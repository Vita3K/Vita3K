// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <renderer/vulkan/surface_cache.h>

#include <gxm/functions.h>
#include <renderer/vulkan/gxm_to_vulkan.h>
#include <renderer/vulkan/state.h>
#include <renderer/vulkan/types.h>
#include <vkutil/vkutil.h>

#include <vulkan/vulkan_format_traits.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <mem/functions.h>
#include <stdexcept>
#include <util/align.h>
#include <util/log.h>
#include <util/vector_utils.h>

extern "C" {
#include <libswscale/swscale.h>
}

static bool format_support_surface_sync(SceGxmColorBaseFormat format) {
    // U2F10F10F10 readback is repacked from the RGBA16F emulation below.
    return true;
}

static bool format_support_swizzle(SceGxmColorBaseFormat format) {
    // do we support something more than the identity swizzle
    // for now we do not support any texture whose component size
    // are all not the same or not a multiple of a byte
    return format != SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10
        && format != SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10
        && format != SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4
        && format != SCE_GXM_COLOR_BASE_FORMAT_U1U5U5U5
        && format != SCE_GXM_COLOR_BASE_FORMAT_SE5M9M9M9
        && format != SCE_GXM_COLOR_BASE_FORMAT_F11F11F10
        && format != SCE_GXM_COLOR_BASE_FORMAT_U5U6U5;
}

static bool format_need_additional_memory(SceGxmColorBaseFormat format) {
    // we are using 4-component surfaces to emulate them
    // so we can't simply use the allocated memory for them
    return format == SCE_GXM_COLOR_BASE_FORMAT_U8U8U8;
}

namespace renderer::vulkan {

static bool surface_sync_needs_u4u4u4u4_repack(const ColorSurfaceCacheInfo &surface) {
    return surface.format == SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4
        && (surface.texture.format == vk::Format::eR8G8B8A8Unorm
            || surface.texture.format == vk::Format::eR8G8B8A8Srgb);
}

static bool surface_sync_needs_f10_repack(const ColorSurfaceCacheInfo &surface) {
    return surface.format == SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10
        && surface.texture.format == vk::Format::eR16G16B16A16Sfloat;
}

static uint8_t unorm8_to_unorm4(uint8_t value) {
    return static_cast<uint8_t>((static_cast<uint32_t>(value) * 15 + 127) / 255);
}

static void pack_rgba8_to_u4u4u4u4(uint8_t *dst, const uint8_t *src, uint32_t pixel_stride, const vk::Rect2D &region, const vk::ComponentMapping &swizzle) {
    const auto mapping = vkutil::color_to_texture_swizzle(swizzle, vkutil::rgba_mapping);
    const std::array components{ mapping.r, mapping.g, mapping.b, mapping.a };
    std::array<uint32_t, 4> channels;
    for (size_t i = 0; i < channels.size(); ++i) {
        if (components[i] < vk::ComponentSwizzle::eR || components[i] > vk::ComponentSwizzle::eA) {
            LOG_ERROR("Unsupported RGBA4444 surface swizzle: {}, {}, {}, {}",
                vk::to_string(swizzle.r), vk::to_string(swizzle.g), vk::to_string(swizzle.b), vk::to_string(swizzle.a));
            return;
        }
        channels[i] = static_cast<uint32_t>(components[i]) - static_cast<uint32_t>(vk::ComponentSwizzle::eR);
    }

    for (uint32_t y = region.offset.y; y < region.offset.y + region.extent.height; ++y) {
        uint8_t *dst_row = dst + static_cast<size_t>(y) * pixel_stride * sizeof(uint16_t);
        const uint8_t *src_row = src + static_cast<size_t>(y) * pixel_stride * 4;

        for (uint32_t x = region.offset.x; x < region.offset.x + region.extent.width; ++x) {
            const uint8_t r = unorm8_to_unorm4(src_row[x * 4 + channels[0]]);
            const uint8_t g = unorm8_to_unorm4(src_row[x * 4 + channels[1]]);
            const uint8_t b = unorm8_to_unorm4(src_row[x * 4 + channels[2]]);
            const uint8_t a = unorm8_to_unorm4(src_row[x * 4 + channels[3]]);
            const uint16_t packed = static_cast<uint16_t>((r << 12) | (g << 8) | (b << 4) | a);
            memcpy(dst_row + x * sizeof(packed), &packed, sizeof(packed));
        }
    }
}

static uint32_t half_to_f10(uint16_t half) {
    return (half & 0x8000) ? 0 : (half >> 5) & 0x3FF;
}

static uint32_t half_to_unorm2(uint16_t half) {
    if (half & 0x8000)
        return 0;
    // Round to the nearest of {0, 1/3, 2/3, 1}.
    return half >= 0x3AAB ? 3 : half >= 0x3800 ? 2
        : half >= 0x3155                       ? 1
                                               : 0;
}

static void pack_rgba16f_to_u2f10f10f10(uint8_t *dst, const uint8_t *src, uint32_t pixel_stride, uint32_t width, uint32_t height, uint32_t dst_stride_bytes) {
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t *src_row = src + static_cast<size_t>(y) * pixel_stride * sizeof(uint64_t);
        uint8_t *dst_row = dst + static_cast<size_t>(y) * dst_stride_bytes;
        for (uint32_t x = 0; x < width; ++x) {
            std::array<uint16_t, 4> components{};
            std::memcpy(components.data(), src_row + static_cast<size_t>(x) * sizeof(uint64_t), sizeof(components));
            const uint32_t packed = half_to_f10(components[0])
                | (half_to_f10(components[1]) << 10)
                | (half_to_f10(components[2]) << 20)
                | (half_to_unorm2(components[3]) << 30);
            std::memcpy(dst_row + static_cast<size_t>(x) * sizeof(packed), &packed, sizeof(packed));
        }
    }
}

static void ensure_surface_sync_buffer(vkutil::Buffer &buffer, vkutil::DestroyQueue &destroy_queue, vk::DeviceSize required_size,
    const vma::AllocationCreateInfo &allocation_info = vkutil::vma_mapped_alloc) {
    if (!buffer.buffer || buffer.size < required_size) {
        destroy_queue.add_buffer(buffer);
        buffer = vkutil::Buffer(required_size);
        buffer.init_buffer(vk::BufferUsageFlagBits::eTransferDst, allocation_info);
    }
}

static void protect_surface(MemState &mem, ColorSurfaceCacheInfo &info) {
    const bool trap_reads = (info.tiling == SurfaceTiling::Linear
        && format_support_surface_sync(info.format));

    uint32_t addr_start = align(info.data.address(), KiB(4));
    uint32_t addr_end = align_down(info.data.address() + info.total_bytes, KiB(4));
    bool small_surface = addr_start >= addr_end;
    if (small_surface) {
        // we still need to protect something, even if it's not completely accurate
        addr_start = align_down(info.data.address(), KiB(4));
        addr_end = align(info.data.address() + info.total_bytes, KiB(4));
    }

    // Use MemPerm::None to trap both reads and writes for surfaces that support sync,
    // MemPerm::ReadOnly to trap only writes for other surfaces
    MemPerm perm = trap_reads ? MemPerm::None : MemPerm::ReadOnly;
    std::shared_ptr<bool> need_sync = trap_reads ? info.need_surface_sync : nullptr;
    // Don't track dirty for small surfaces to avoid false positives from unrelated writes
    std::shared_ptr<bool> dirty = small_surface ? nullptr : info.dirty;

    add_protect(mem, addr_start, addr_end - addr_start, perm,
        [dirty, need_sync](Address, bool write) {
            if (write && dirty && !SurfaceSyncWriteScope::active())
                *dirty = true;
            if (need_sync)
                *need_sync = true;
            return true;
        });
}

ColorSurfaceCacheInfo::~ColorSurfaceCacheInfo() {
    sws_freeContext(sws_context);
}

void VKSurfaceCache::destroy_framebuffers(vk::ImageView view) {
    vkutil::DestroyQueue &destroy_queue = state.frame().destroy_queue;
    for (auto it = framebuffer_array.begin(); it != framebuffer_array.end();) {
        // if the color of depth-stencil match the one of the render_target, this won't be used anymore
        if (it->first.first == view || it->first.second == view) {
            destroy_queue.add(it->second.standard);
            destroy_queue.add(it->second.shader_interlock);
            destroy_queue.add(it->second.storage_color_view);
            it = framebuffer_array.erase(it);
        } else {
            it = std::next(it);
        }
    }
}

void VKSurfaceCache::record_pending_cast(PendingCast &cast, VKContext &context) {
    // Without a macroblock flush, feedback samples the surface as it was before this recording.
    const bool pre_scene = cast.info->last_scene_rendered == context.scene_timestamp
        && context.scene_has_drawn && !context.scene_macroblock_flushed;
    if (!pre_scene && context.in_renderpass)
        context.stop_render_pass();
    cast.record(pre_scene ? context.prerender_cmd : context.render_cmd);
}

void VKSurfaceCache::perform_pending_casts(VKContext &context, uint16_t vert_texture_count, uint16_t frag_texture_count) {
    for (size_t i = 0; i < pending_casts.size();) {
        bool used = false;
        for (uint16_t unit = 0; unit < frag_texture_count; ++unit) {
            if (pending_casts[i].view == context.fragment_textures[unit].imageView) {
                used = true;
                if (pending_casts[i].info->has_phase_view)
                    context.curr_frag_ublock.set_cast_sampler_bit(unit, false, false);
            }
        }
        for (uint16_t unit = 0; unit < vert_texture_count && !used; ++unit)
            used = pending_casts[i].view == context.vertex_textures[unit].imageView;
        if (used) {
            PendingCast cast = std::move(pending_casts[i]);
            pending_casts.erase(pending_casts.begin() + i);
            record_pending_cast(cast, context);
            i = 0;
        } else {
            ++i;
        }
    }
}

void VKSurfaceCache::flush_all_pending_casts() {
    std::vector<PendingCast> casts;
    casts.swap(pending_casts);
    for (auto &cast : casts)
        record_pending_cast(cast, *static_cast<VKContext *>(state.context));
}

void VKSurfaceCache::destroy_surface(ColorSurfaceCacheInfo &info) {
    flush_all_pending_casts();
    vkutil::DestroyQueue &destroy_queue = state.frame().destroy_queue;

    // don't forget to destroy in the right order
    for (auto &casted : info.casted_textures) {
        destroy_queue.add_buffer(casted.transition_buffer);
        destroy_queue.add_image(casted.texture);
    }
    info.casted_textures.clear();

    destroy_framebuffers(info.texture.view);
    destroy_framebuffers(info.alternate_view);
    if (info.gamma_view) {
        destroy_framebuffers(info.gamma_view);
        destroy_queue.add(info.gamma_view);
    }
    destroy_queue.add(info.alternate_view);
    if (info.raw_image) {
        destroy_queue.add_image(*info.raw_image);
        info.raw_image.reset();
    }
    info.raw_image_valid = false;
    destroy_queue.add_image(info.texture);

    if (info.blit_image) {
        destroy_queue.add_image(*info.blit_image);
        info.blit_image.reset();
    }
    if (info.copy_buffer) {
        destroy_queue.add_buffer(*info.copy_buffer);
        info.copy_buffer.reset();
    }
}

void VKSurfaceCache::destroy_surface(DepthStencilSurfaceCacheInfo &info) {
    if (pending_ds_scene == &info)
        pending_ds_scene = nullptr;
    vkutil::DestroyQueue &destroy_queue = state.frame().destroy_queue;

    for (auto &read_only : info.read_surfaces) {
        destroy_queue.add_image(read_only.stencil_view);
        destroy_queue.add_image(read_only.depth_view);
    }
    info.read_surfaces.clear();

    destroy_queue.add(info.depth_view);
    destroy_queue.add(info.stencil_view);

    destroy_framebuffers(info.texture.view);
    destroy_queue.add_image(info.texture);
}

VKSurfaceCache::VKSurfaceCache(VKState &state)
    : state(state) {
    color_surface_queue.init(max_surfaces_allowed);
    ds_surface_queue.init(max_surfaces_allowed);
}

void VKSurfaceCache::cleanup() {
    pending_casts.clear();
    pending_ds_scene = nullptr;
    state.device.destroy(typeless_pipeline);
    state.device.destroy(typeless_pipeline_layout);
    state.device.destroy(typeless_descriptor_layout);
    state.device.destroy(typeless_shader);
    typeless_pipeline = nullptr;
    typeless_pipeline_layout = nullptr;
    typeless_descriptor_layout = nullptr;
    typeless_shader = nullptr;
    for (auto &frame : state.frames) {
        frame.typeless_descriptor.sets.clear();
        frame.typeless_descriptor.descriptors_idx = 0;
    }

    for (auto &[key, fb] : framebuffer_array) {
        state.device.destroy(fb.standard);
        state.device.destroy(fb.shader_interlock);
        state.device.destroy(fb.storage_color_view);
    }
    framebuffer_array.clear();

    for (auto &item : color_surface_queue.items) {
        auto &info = item.content;
        for (auto &casted : info.casted_textures) {
            casted.transition_buffer.destroy();
            casted.texture.destroy();
        }
        info.casted_textures.clear();

        if (info.alternate_view) {
            state.device.destroy(info.alternate_view);
            info.alternate_view = nullptr;
        }
        if (info.gamma_view) {
            state.device.destroy(info.gamma_view);
            info.gamma_view = nullptr;
        }

        if (info.blit_image)
            info.blit_image->destroy();
        if (info.copy_buffer)
            info.copy_buffer->destroy();

        if (info.raw_image)
            info.raw_image->destroy();
        info.raw_image.reset();
        info.raw_image_valid = false;

        info.texture.destroy();
    }

    for (auto &item : ds_surface_queue.items) {
        auto &info = item.content;
        for (auto &read_surface : info.read_surfaces) {
            read_surface.depth_view.destroy();
            read_surface.stencil_view.destroy();
        }
        info.read_surfaces.clear();

        if (info.depth_view) {
            state.device.destroy(info.depth_view);
            info.depth_view = nullptr;
        }
        if (info.stencil_view) {
            state.device.destroy(info.stencil_view);
            info.stencil_view = nullptr;
        }

        info.texture.destroy();
    }

    color_address_lookup.clear();
    depth_address_lookup.clear();
    stencil_address_lookup.clear();
    cpu_surfaces_changed.clear();
    target = nullptr;
    last_written_surface = nullptr;
}

void VKSurfaceCache::ensure_typeless_pipeline() {
    if (typeless_pipeline)
        return;

    const fs::path path = state.static_assets / "shaders-builtin" / "vulkan" / "surface_copy_typeless.comp.spv";
    typeless_shader = vkutil::load_shader(state.device, path);
    if (!typeless_shader) {
        LOG_ERROR("Cannot load typeless surface copy shader: {}", path.string());
        throw std::runtime_error("Cannot load typeless surface copy shader");
    }

    const std::array bindings{
        vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute },
        vk::DescriptorSetLayoutBinding{ 1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute }
    };
    vk::DescriptorSetLayoutCreateInfo descriptor_info{};
    descriptor_info.setBindings(bindings);
    typeless_descriptor_layout = state.device.createDescriptorSetLayout(descriptor_info);

    const vk::PushConstantRange constants{ vk::ShaderStageFlagBits::eCompute, 0, 10 * sizeof(uint32_t) };
    vk::PipelineLayoutCreateInfo layout_info{};
    layout_info.setSetLayouts(typeless_descriptor_layout);
    layout_info.setPushConstantRanges(constants);
    typeless_pipeline_layout = state.device.createPipelineLayout(layout_info);

    const vk::ComputePipelineCreateInfo pipeline_info{
        .stage = { .stage = vk::ShaderStageFlagBits::eCompute, .module = typeless_shader, .pName = "main" },
        .layout = typeless_pipeline_layout
    };
    const auto result = state.device.createComputePipeline(nullptr, pipeline_info);
    if (result.result != vk::Result::eSuccess) {
        LOG_ERROR("Cannot create typeless surface copy pipeline: {}", vk::to_string(result.result));
        throw std::runtime_error("Cannot create typeless surface copy pipeline");
    }
    typeless_pipeline = result.value;
}

vk::DescriptorSet VKSurfaceCache::retrieve_typeless_descriptor() {
    auto &descriptor = state.frame().typeless_descriptor;
    if (descriptor.descriptors_idx == descriptor.sets.size()) {
        constexpr uint32_t pack_size = 16;
        const vk::DescriptorPoolSize pool_size{ vk::DescriptorType::eStorageBuffer, 2 * pack_size * MAX_FRAMES_RENDERING };
        vk::DescriptorPoolCreateInfo pool_info{ .maxSets = pack_size * MAX_FRAMES_RENDERING };
        pool_info.setPoolSizes(pool_size);
        const auto pool = state.device.createDescriptorPool(pool_info);
        state.frame_descriptor_pools.push_back(pool);
        const std::vector<vk::DescriptorSetLayout> layouts(pack_size * MAX_FRAMES_RENDERING, typeless_descriptor_layout);
        vk::DescriptorSetAllocateInfo allocation{ .descriptorPool = pool };
        allocation.setSetLayouts(layouts);
        const auto sets = state.device.allocateDescriptorSets(allocation);
        for (uint32_t i = 0; i < MAX_FRAMES_RENDERING; ++i) {
            auto &frame_sets = state.frames[i].typeless_descriptor.sets;
            frame_sets.insert(frame_sets.end(), sets.begin() + i * pack_size, sets.begin() + (i + 1) * pack_size);
        }
    }
    return descriptor.sets[descriptor.descriptors_idx++];
}

void VKSurfaceCache::copy_typeless_scaled(vk::CommandBuffer cmd, const ColorSurfaceCacheInfo &source, const vkutil::Image &source_image, CastedTexture &dest,
    uint32_t width, uint32_t height, uint32_t data_delta, uint32_t source_bpp, uint32_t dest_bpp) {
    LOG_INFO_ONCE("Using resolution-aware typeless surface copies: {} {}x{} -> {} {}x{}, scale {}, guest byte offset {}, guest stride {}",
        vk::to_string(source_image.format), source.width, source.height, vk::to_string(dest.texture.format), width, height,
        state.res_multiplier, data_delta, source.stride_bytes);
    const auto &limits = state.physical_device_properties.limits;
    const vk::DeviceSize source_size = align(static_cast<vk::DeviceSize>(source.width) * source.height * source_bpp, vk::DeviceSize{ 4 });
    const vk::DeviceSize dest_size = align(static_cast<vk::DeviceSize>(width) * height * dest_bpp, vk::DeviceSize{ 4 });
    if (source_size > limits.maxStorageBufferRange || dest_size > limits.maxStorageBufferRange) {
        LOG_ERROR("Typeless copy requires buffers of {} and {} bytes, exceeding the GPU limit of {}", source_size, dest_size, limits.maxStorageBufferRange);
        throw std::runtime_error("Typeless surface copy exceeds GPU storage buffer range");
    }
    const vk::DeviceSize dest_offset = align(source_size, static_cast<vk::DeviceSize>(limits.minStorageBufferOffsetAlignment));
    const vk::DeviceSize buffer_size = dest_offset + dest_size;
    if (!dest.transition_buffer.buffer || dest.transition_buffer.size < buffer_size) {
        state.frame().destroy_queue.add_buffer(dest.transition_buffer);
        dest.transition_buffer = vkutil::Buffer(buffer_size);
        dest.transition_buffer.init_buffer(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eStorageBuffer);
    }
    ensure_typeless_pipeline();
    const auto buffer = dest.transition_buffer.buffer;
    vk::BufferMemoryBarrier barrier{
        .srcAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eTransferRead,
        .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = buffer,
        .size = VK_WHOLE_SIZE
    };
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer, {}, {}, barrier, {});
    vk::BufferImageCopy copy{
        .imageSubresource = vkutil::color_subresource_layer,
        .imageExtent = { source.width, source.height, 1 }
    };
    cmd.copyImageToBuffer(source_image.image, vk::ImageLayout::eTransferSrcOptimal, buffer, copy);
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eTransferRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, barrier, {});

    const auto descriptor = retrieve_typeless_descriptor();
    const std::array buffers{
        vk::DescriptorBufferInfo{ buffer, 0, source_size },
        vk::DescriptorBufferInfo{ buffer, dest_offset, dest_size }
    };
    std::array<vk::WriteDescriptorSet, 2> writes;
    for (uint32_t i = 0; i < writes.size(); ++i) {
        writes[i] = vk::WriteDescriptorSet{ .dstSet = descriptor, .dstBinding = i, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eStorageBuffer, .pBufferInfo = &buffers[i] };
    }
    state.device.updateDescriptorSets(writes, {});
    const struct {
        uint32_t source_width, source_height, dest_width, dest_height;
        uint32_t source_bpp, dest_bpp, byte_offset, row_offset;
        float scale;
        uint32_t interleave;
    } constants{
        source.width, source.height, width, height, source_bpp, dest_bpp,
        data_delta % source.stride_bytes, data_delta / source.stride_bytes, state.res_multiplier,
        !source.has_phase_view && source_bpp == 8 && dest_bpp == 4
                && width == 2 * source.width && data_delta % source.stride_bytes == 0
            ? 1U
            : 0U
    };
    static_assert(sizeof(constants) == 10 * sizeof(uint32_t));
    if (state.res_multiplier != 1.0f && source_bpp == 8 && dest_bpp == 4)
        LOG_INFO_ONCE("Packed 64-to-32-bit surface copy: interleave={}, explicit phase views={}, byte offset={}, widths={}->{}",
            constants.interleave, source.has_phase_view, constants.byte_offset, source.width, width);
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, typeless_pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, typeless_pipeline_layout, 0, descriptor, {});
    cmd.pushConstants(typeless_pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(constants), &constants);
    const uint32_t groups = static_cast<uint32_t>((dest_size / 4 + 63) / 64);
    const uint32_t groups_x = std::min(groups, limits.maxComputeWorkGroupCount[0]);
    cmd.dispatch(groups_x, (groups + groups_x - 1) / groups_x, 1);
    barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, barrier, {});
    copy.bufferOffset = dest_offset;
    copy.imageExtent = vk::Extent3D{ width, height, 1 };
    cmd.copyBufferToImage(buffer, dest.texture.image, vk::ImageLayout::eTransferDstOptimal, copy);
}

vk::ImageView VKSurfaceCache::create_surface_view(vk::Image image, vk::Format format, vk::ComponentMapping mapping) {
    const vk::ImageViewUsageCreateInfo usage{
        .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment
    };
    const vk::ImageViewCreateInfo info{
        .pNext = state.features.support_shader_interlock && format == vk::Format::eR8G8B8A8Srgb ? &usage : nullptr,
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .components = mapping,
        .subresourceRange = vkutil::color_subresource_range
    };
    return state.device.createImageView(info);
}

vk::ImageView VKSurfaceCache::retrieve_color_surface_view(ColorSurfaceCacheInfo &info, vk::Format format) {
    if (format == info.texture.format)
        return info.texture.view;

    vk::ImageView &view = state.features.support_shader_interlock && format == vk::Format::eR8G8B8A8Srgb
        ? info.gamma_view
        : info.alternate_view;
    if (!view)
        view = create_surface_view(info.texture.image, format, vkutil::default_comp_mapping);
    return view;
}

bool VKSurfaceCache::try_upload_guest_content(ColorSurfaceCacheInfo &info, MemState &mem) {
    const uint32_t bytes_per_pixel = static_cast<uint32_t>(vk::blockSize(info.texture.format));
    if (!state.features.enable_memory_mapping || info.tiling != SurfaceTiling::Linear
        || !format_support_surface_sync(info.format) || info.swizzle.r != vk::ComponentSwizzle::eR
        || bytes_per_pixel != gxm::bits_per_pixel(info.format) / 8 || !bytes_per_pixel
        || info.stride_bytes % bytes_per_pixel || info.stride_bytes / bytes_per_pixel < info.original_width) {
        LOG_WARN_ONCE("Cannot upload CPU-written surface with format {} and tiling {}", vk::to_string(info.texture.format), static_cast<int>(info.tiling));
        return false;
    }

    const uint64_t upload_size = static_cast<uint64_t>(info.stride_bytes) * info.original_height;
    const uint64_t end_address = static_cast<uint64_t>(info.data.address()) + upload_size;
    if (!upload_size || end_address > std::numeric_limits<Address>::max()
        || !is_valid_addr_range(mem, info.data.address(), static_cast<Address>(end_address))) {
        LOG_WARN_ONCE("Cannot upload surface at {}: backing memory is not fully allocated", log_hex(info.data.address()));
        return false;
    }

    vkutil::Buffer upload(upload_size);
    upload.init_buffer(vk::BufferUsageFlagBits::eTransferSrc, vkutil::vma_mapped_alloc);
    memcpy(upload.mapped_data, info.data.get(mem), upload_size);
    state.allocator.flushAllocation(upload.allocation, 0, VK_WHOLE_SIZE);

    const auto cmd = static_cast<VKContext *>(state.context)->prerender_cmd;
    const vk::BufferImageCopy copy{
        .bufferRowLength = info.stride_bytes / bytes_per_pixel,
        .bufferImageHeight = info.original_height,
        .imageSubresource = vkutil::color_subresource_layer,
        .imageExtent = { info.original_width, info.original_height, 1 }
    };
    if (state.res_multiplier == 1.0f) {
        info.texture.transition_to(cmd, vkutil::ImageLayout::TransferDst);
        cmd.copyBufferToImage(upload.buffer, info.texture.image, vk::ImageLayout::eTransferDstOptimal, copy);
    } else {
        if (!info.blit_image) {
            info.blit_image = std::make_unique<vkutil::Image>(info.original_width, info.original_height, info.texture.format);
            info.blit_image->init_image(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);
        }
        auto &native = *info.blit_image;
        native.transition_to_discard(cmd, vkutil::ImageLayout::TransferDst);
        cmd.copyBufferToImage(upload.buffer, native.image, vk::ImageLayout::eTransferDstOptimal, copy);
        native.transition_to(cmd, vkutil::ImageLayout::TransferSrc);
        info.texture.transition_to(cmd, vkutil::ImageLayout::TransferDst);
        const vk::ImageBlit blit{
            .srcSubresource = vkutil::color_subresource_layer,
            .srcOffsets = std::array<vk::Offset3D, 2>{ vk::Offset3D{}, vk::Offset3D{ info.original_width, info.original_height, 1 } },
            .dstSubresource = vkutil::color_subresource_layer,
            .dstOffsets = std::array<vk::Offset3D, 2>{ vk::Offset3D{}, vk::Offset3D{ info.width, info.height, 1 } }
        };
        cmd.blitImage(native.image, vk::ImageLayout::eTransferSrcOptimal, info.texture.image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eNearest);
    }
    info.texture.transition_to(cmd, vkutil::ImageLayout::ColorAttachmentReadWrite);
    // The upload only updates the float image, so its old raw companion is no longer usable.
    info.raw_image_valid = false;
    // Every upload keeps its own staging data until the submitting frame has completed.
    state.frame().destroy_queue.add_buffer(upload);
    return true;
}

SurfaceRetrieveResult VKSurfaceCache::retrieve_color_surface_for_framebuffer(MemState &mem, SceGxmColorSurface *color) {
    // Create the key to access the cache struct
    const uint32_t address = color->data.address();

    const uint32_t original_width = color->width;
    const uint32_t original_height = color->height;

    uint32_t width = static_cast<uint32_t>(original_width * state.res_multiplier);
    uint32_t height = static_cast<uint32_t>(original_height * state.res_multiplier);

    bool overlap = true;

    // Of course, this works under the assumption that range must be unique :D
    auto ite = color_address_lookup.upper_bound(address);
    if (ite == color_address_lookup.begin())
        // no match
        overlap = false;
    else
        --ite;
    // ite is now the first item with an address lower or equal to key

    overlap = (overlap && (ite->first + ite->second->total_bytes) > address);

    while (!overlap && ite != color_address_lookup.begin()) {
        --ite;
        overlap = static_cast<uint64_t>(ite->first) + ite->second->total_bytes > address;
    }

    const SceGxmColorBaseFormat base_format = gxm::get_base_format(color->colorFormat);
    vk::Format vk_format = color::translate_surface_format(base_format,
        state.support_native_rgba4_surface, state.features.support_shader_interlock);

    SurfaceTiling tiling;
    if (color->surfaceType == SCE_GXM_COLOR_SURFACE_LINEAR)
        tiling = SurfaceTiling::Linear;
    else if (color->surfaceType == SCE_GXM_COLOR_SURFACE_SWIZZLED)
        tiling = SurfaceTiling::Swizzled;
    else
        tiling = SurfaceTiling::Tiled;

    const bool is_srgb = color->gamma != 0;
    if (is_srgb) {
        if (vk_format == vk::Format::eR8G8B8A8Unorm) {
            vk_format = vk::Format::eR8G8B8A8Srgb;
        } else {
            LOG_WARN_ONCE("Trying to use gamma correction with non-compatible format {}", vk::to_string(vk_format));
        }
    }

    uint32_t bytes_per_stride = color->strideInPixels * gxm::bits_per_pixel(base_format) / 8;
    uint32_t total_surface_size = bytes_per_stride * original_height;

    VKContext *context = reinterpret_cast<VKContext *>(state.context);

    if (overlap) {
        ColorSurfaceCacheInfo &info = *ite->second;

        // There are four situations I think of:
        // 1. Different base address, lookup for write, in this case, if the cached surface range contains the given address, then
        // probably this cached surface has already been freed GPU-wise. So erase.
        // 2. Same base address, but width and height change to be larger, or format change if write. Remake a new one for both read and write situation.
        // 3. Out of cache range. In write case, create a new one, in read case, lul
        // 4. Read situation with smaller width and height, probably need to extract the needed region out.
        // 5. the surface is a gbuffer and we are currently trying to read the 2nd component, in this case key == ite->first + 4
        const bool addr_in_range_of_cache = ((address + total_surface_size) <= (ite->first + info.total_bytes + 4));
        const bool cache_probably_freed = (ite->first != address) && addr_in_range_of_cache;
        const bool surface_extent_changed = info.height < height || bytes_per_stride != info.stride_bytes || tiling != info.tiling;
        bool surface_stat_changed = false;

        if (ite->first == address)
            surface_stat_changed = surface_extent_changed || info.width < width || base_format != info.format;

        const bool invalidated = cache_probably_freed || surface_stat_changed || !addr_in_range_of_cache;
        if (invalidated) {
            destroy_surface(info);
            color_address_lookup.erase(ite);
            color_surface_queue.set_as_lru(&info);
        } else {
            color_surface_queue.set_as_mru(&info);

            if (info.data && *info.dirty) {
                try_upload_guest_content(info, mem);
                protect_surface(mem, info);
            }
            *info.dirty = false;

            last_written_surface = &info;

            // if this surface has not been rendered to for the last 60 frames, consider it is not safe not to render all shaders to it
            constexpr uint64_t big_delay_between_frames = 60;
            state.pipeline_cache.can_use_deferred_compilation = state.frame_timestamp - info.last_frame_rendered < big_delay_between_frames;
            info.last_frame_rendered = state.frame_timestamp;
            info.last_scene_rendered = context->scene_timestamp;

            return { retrieve_color_surface_view(info, vk_format), &info.texture, &info };
        }
    }

    // get the least recently used (probably unused) color surface
    ColorSurfaceCacheInfo &info_added = *color_surface_queue.get_lru();
    if (info_added.texture.image)
        // deferred destruction of the existing surface
        destroy_surface(info_added);
    if (info_added.data && info_added.data.address() != address)
        info_added.has_phase_view = false;
    if (info_added.data)
        color_address_lookup.erase(info_added.data.address());

    color_surface_queue.set_as_mru(&info_added);
    info_added.last_frame_rendered = state.frame_timestamp;
    info_added.last_scene_rendered = context->scene_timestamp;
    info_added.written_back_scene = 0;

    color_address_lookup[address] = &info_added;

    info_added.width = width;
    info_added.height = height;
    info_added.original_width = original_width;
    info_added.original_height = original_height;
    info_added.stride_bytes = bytes_per_stride;
    info_added.data = color->data;
    info_added.total_bytes = total_surface_size;
    info_added.format = base_format;
    info_added.tiling = tiling;
    // only remember the swizzle here, it will be useful if we get to present or sample from this image with a different swizzle
    info_added.swizzle = color::translate_swizzle(color->colorFormat);

    vkutil::Image &image = info_added.texture;
    image.width = width;
    image.height = height;
    image.format = state.features.support_shader_interlock && vk_format == vk::Format::eR8G8B8A8Srgb
        ? vk::Format::eR8G8B8A8Unorm
        : vk_format;
    image.layout = vkutil::ImageLayout::Undefined;

    // we might have to create a non-srgb/linear view later if this surface is used for presentation
    const bool need_mutable = (vk_format == vk::Format::eR8G8B8A8Unorm || vk_format == vk::Format::eR8G8B8A8Srgb);
    const vk::ImageCreateFlags image_create_flags = need_mutable ? vk::ImageCreateFlagBits::eMutableFormat : vk::ImageCreateFlags();
    const void *image_info_pNext = nullptr;
    if (support_image_format_specifier && need_mutable) {
        static const vk::Format view_formats[] = { vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Srgb };
        static const vk::ImageFormatListCreateInfoKHR image_info_formats{
            .viewFormatCount = 2,
            .pViewFormats = view_formats
        };
        image_info_pNext = &image_info_formats;
    }

    vk::ImageUsageFlags surface_usages = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eInputAttachment;
    if (state.features.support_shader_interlock)
        surface_usages |= vk::ImageUsageFlagBits::eStorage;
    image.init_image(surface_usages, vkutil::default_comp_mapping, image_create_flags, image_info_pNext);

    // do it in the prerender if we read from this texture in the same scene (although this would be useless)
    vk::CommandBuffer cmd_buffer = context->prerender_cmd;
    // must do a first transition to draw the placeholder color
    image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst);

    vk::ClearColorValue clear_color{ std::array<float, 4>({ 0.0f, 0.0f, 0.0f, 0.0f }) };
    cmd_buffer.clearColorImage(image.image, vk::ImageLayout::eTransferDstOptimal, clear_color, vkutil::color_subresource_range);
    image.transition_to(cmd_buffer, vkutil::ImageLayout::ColorAttachmentReadWrite);

    info_added.raw_image_valid = false;
    if (state.features.preserve_f16_nan_as_u16 && base_format == SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16) {
        info_added.raw_image = std::make_unique<vkutil::Image>(width, height, vk::Format::eR16G16B16A16Uint);
        auto &raw = *info_added.raw_image;
        raw.init_image(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment
            | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);
        raw.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst);
        const vk::ClearColorValue raw_zero{ .uint32 = std::array<uint32_t, 4>{} };
        cmd_buffer.clearColorImage(raw.image, vk::ImageLayout::eTransferDstOptimal, raw_zero, vkutil::color_subresource_range);
        raw.transition_to(cmd_buffer, vkutil::ImageLayout::ColorAttachmentReadWrite);
        info_added.raw_image_valid = true;
    }

    last_written_surface = &info_added;
    info_added.need_surface_sync.reset();
    info_added.need_surface_sync = std::make_shared<bool>(false);
    info_added.dirty = std::make_shared<bool>(false);

    if (original_width >= 1024 && original_height >= 1024 && state.features.enable_memory_mapping)
        try_upload_guest_content(info_added, mem);

    // we only support surface sync of linear surfaces for now
    if (!can_mprotect_mapped_memory) {
        // perform surface sync on everything
        // it is slow but well... we can't mprotect the buffer
        *info_added.need_surface_sync = color->surfaceType == SCE_GXM_COLOR_SURFACE_LINEAR;
    } else {
        protect_surface(mem, info_added);
    }

    // it's not impossible that this surface will be rendered once and only used after, so do not skip any shader on it
    state.pipeline_cache.can_use_deferred_compilation = false;

    return { retrieve_color_surface_view(info_added, vk_format), &info_added.texture, &info_added };
}

std::optional<TextureLookupResult> VKSurfaceCache::retrieve_color_surface_as_texture(const SceGxmTexture &texture, const SceGxmColorBaseFormat base_format, TextureViewport *texture_viewport) {
    if (texture.texture_type() == SCE_GXM_TEXTURE_CUBE || texture.texture_type() == SCE_GXM_TEXTURE_CUBE_ARBITRARY)
        return std::nullopt;

    // Create the key to access the cache struct
    const uint32_t address = (texture.data_addr << 2);

    const uint32_t original_width = gxm::get_width(texture);
    const uint32_t original_height = gxm::get_height(texture);

    const uint32_t width = static_cast<uint32_t>(original_width * state.res_multiplier);
    const uint32_t height = static_cast<uint32_t>(original_height * state.res_multiplier);

    uint32_t stride_bytes = 0;
    SurfaceTiling tiling = SurfaceTiling::Swizzled;
    if (texture.texture_type() == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        stride_bytes = gxm::get_stride_in_bytes(texture);
        tiling = SurfaceTiling::Linear;
    } else {
        uint32_t pixel_stride = original_width;
        switch (texture.texture_type()) {
        case SCE_GXM_TEXTURE_LINEAR:
            // when the texture is linear, the stride should be aligned to 8 pixels
            tiling = SurfaceTiling::Linear;
            pixel_stride = align(pixel_stride, 8);
            break;
        case SCE_GXM_TEXTURE_TILED:
            // tiles are 32x32
            tiling = SurfaceTiling::Tiled;
            pixel_stride = align(pixel_stride, 32);
            break;
        case SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY:
            pixel_stride = next_power_of_two(pixel_stride);
            break;
        default:
            break;
        }
        stride_bytes = pixel_stride * gxm::bits_per_pixel(base_format) / 8;
    }
    uint32_t total_surface_size = stride_bytes * original_height;

    auto ite = color_address_lookup.upper_bound(address);
    bool found = false;
    while (ite != color_address_lookup.begin()) {
        --ite;
        const auto &candidate = *ite->second;
        if (static_cast<uint64_t>(ite->first) + candidate.total_bytes > address
            && candidate.tiling == tiling && candidate.stride_bytes == stride_bytes) {
            found = true;
            break;
        }
    }
    if (!found || *ite->second->dirty)
        return std::nullopt;

    const vk::ComponentMapping swizzle = texture::translate_swizzle(gxm::get_format(texture));
    vk::Format vk_format = color::translate_surface_format(base_format,
        state.support_native_rgba4_surface, state.features.support_shader_interlock);
    if (texture.gamma_mode) {
        if (vk_format == vk::Format::eR8G8B8A8Unorm)
            vk_format = vk::Format::eR8G8B8A8Srgb;
        else
            LOG_WARN_ONCE("Trying to use gamma correction with non-compatible format {}", vk::to_string(vk_format));
    }

    ColorSurfaceCacheInfo &info = *ite->second;

    const bool expanded_rgba4 = surface_sync_needs_u4u4u4u4_repack(info)
        || (base_format == SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4 && vk_format != vk::Format::eR4G4B4A4UnormPack16);
    if ((base_format == SCE_GXM_COLOR_BASE_FORMAT_U8U8U8 || info.format == SCE_GXM_COLOR_BASE_FORMAT_U8U8U8
            || expanded_rgba4)
        && base_format != info.format)
        // Expanded surfaces cannot be reinterpreted as a different guest format.
        return std::nullopt;

    if (tiling != info.tiling || info.stride_bytes != stride_bytes)
        // if the tiling is different, also don't try to match them
        // about the strides, I've yet to see a case where the byte stride is different
        return std::nullopt;

    // Check if we can use this surface
    bool addr_in_range_of_cache = ((address + total_surface_size) <= (ite->first + info.total_bytes + 4));

    if (ite->first != address && !addr_in_range_of_cache)
        // persona 4 sample from the top of a texture while the bottom wasn't rendered to, the fact that both the surface and
        // the texture start at the same location should be enough
        return std::nullopt;

    uint32_t bytes_per_pixel_requested = gxm::bits_per_pixel(base_format) / 8;
    uint32_t bytes_per_pixel_in_store = gxm::bits_per_pixel(info.format) / 8;

    if (std::max(bytes_per_pixel_requested, bytes_per_pixel_in_store) % std::min(bytes_per_pixel_requested, bytes_per_pixel_in_store) != 0)
        return std::nullopt;

    // TODO: this is true only for linear textures (and also kind of for tiled textures) (and in this case start_x = 0),
    // for swizzled textures this is different
    const uint32_t data_delta = address - ite->first;
    uint32_t start_sourced_line = static_cast<uint32_t>((data_delta / stride_bytes) * state.res_multiplier);
    uint32_t start_x = static_cast<uint32_t>((data_delta % stride_bytes) / bytes_per_pixel_requested * state.res_multiplier);

    const bool is_typeless_cast = bytes_per_pixel_requested != bytes_per_pixel_in_store;
    const bool cast_phase_hi = is_typeless_cast
        && ((data_delta % stride_bytes) % bytes_per_pixel_in_store) / bytes_per_pixel_requested != 0;
    if (cast_phase_hi && !info.has_phase_view) {
        info.has_phase_view = true;
        for (auto &casted : info.casted_textures)
            casted.scene_timestamp = 0;
    }

    if (start_x >= info.width || start_sourced_line >= info.height)
        return std::nullopt;

    if (static_cast<uint16_t>(start_sourced_line + height) > info.height)
        LOG_WARN_ONCE("Trying to use texture partially in the surface cache");

    // We should be able to use this texture, so set it as mru
    color_surface_queue.set_as_mru(&info);

    const vk::ImageView color_handle_view = reinterpret_cast<VKContext *>(state.context)->current_color_view;
    const bool is_same_image = (color_handle_view == info.texture.view) || (color_handle_view == info.alternate_view)
        || (info.gamma_view && color_handle_view == info.gamma_view);

    const bool native_point_texture = state.res_multiplier != std::floor(state.res_multiplier)
        && original_width <= 256 && original_height <= 256
        && base_format == info.format && vk_format == info.texture.format
        && start_x + width <= info.width && start_sourced_line + height <= info.height
        && texture.min_filter == SCE_GXM_TEXTURE_FILTER_POINT && texture.mag_filter == SCE_GXM_TEXTURE_FILTER_POINT;

    // Resample partial-width reads, but never numerically convert a raw format reinterpretation.
    const bool native_size_cast = native_point_texture
        || (bytes_per_pixel_requested == bytes_per_pixel_in_store && vk_format == info.texture.format
            && (start_x != 0 || width != info.width));

    if (state.features.use_texture_viewport && base_format == info.format && !native_point_texture) {
        // use a texture viewport
        *texture_viewport = {
            .ratio = {
                original_width / static_cast<float>(info.original_width),
                original_height / static_cast<float>(info.original_height) },
            .offset = { start_x / static_cast<float>(info.width), start_sourced_line / static_cast<float>(info.height) }
        };

        // if everything matches
        if (vk_format == info.texture.format && swizzle == info.swizzle)
            return TextureLookupResult{
                info.texture.view,
                info.texture.layout,
                info.texture.format
            };

        // use the other view with the correct swizzle / gamma correction
        if (!info.alternate_view) {
            vk::ComponentMapping resulting_mapping = vkutil::color_to_texture_swizzle(info.swizzle, swizzle);

            info.alternate_view = create_surface_view(info.texture.image, vk_format, resulting_mapping);
        }

        return TextureLookupResult{
            info.alternate_view,
            info.texture.layout,
            info.texture.format
        };
    }

    if (is_same_image || (start_sourced_line != 0) || (start_x != 0) || (info.width != width) || (info.height != height) || (info.format != base_format) || native_point_texture) {
        const uint64_t scene_timestamp = reinterpret_cast<VKContext *>(state.context)->scene_timestamp;

        std::vector<CastedTexture> &casted_vec = info.casted_textures;

        CastedTexture *casted = nullptr;

        // Look in cast cache and grab one. The cache really does not store immediate grab on now, but rather to reduce the synchronization in the pipeline (use different texture)
        for (size_t i = 0; i < casted_vec.size();) {
            if ((casted_vec[i].cropped_height == height) && (casted_vec[i].cropped_width == width) && (casted_vec[i].cropped_y == start_sourced_line) && (casted_vec[i].cropped_x == start_x) && (casted_vec[i].format == base_format) && casted_vec[i].texture.format == vk_format) {
                casted = &casted_vec[i];

                const bool copy_pending = std::any_of(pending_casts.begin(), pending_casts.end(),
                    [casted](const PendingCast &pending) { return pending.view == casted->texture.view; });
                if (casted->scene_timestamp == scene_timestamp && (!is_same_image || copy_pending)) {
                    // Already copied or queued for this scene.
                    return TextureLookupResult{
                        casted->texture.view,
                        vkutil::ImageLayout::SampledImage,
                        casted->texture.format,
                        is_typeless_cast && !info.has_phase_view,
                        cast_phase_hi
                    };
                }

                break;
            } else {
                i++;
            }
        }

        const bool casted_is_new = casted == nullptr;
        if (casted == nullptr) {
            // Try to crop + cast
            casted_vec.resize(casted_vec.size() + 1);
            casted = &casted_vec[casted_vec.size() - 1];
            *casted = CastedTexture{
                .cropped_x = start_x,
                .cropped_y = start_sourced_line,
                .cropped_width = width,
                .cropped_height = height,
                .format = base_format
            };
            casted->texture.width = native_size_cast ? original_width : width;
            casted->texture.height = native_size_cast ? original_height : height;
            casted->texture.format = vk_format;

            // find the swizzle we need to apply
            const std::uint8_t components_in_store = vk::componentCount(info.texture.format);
            const std::uint8_t components_requested = vk::componentCount(vk_format);
            vk::ComponentMapping resulting_swizzle;
            // Only take into consideration the current swizzle when it makes sense
            // (Not perfect but better than doing this all the time)
            if (bytes_per_pixel_requested == bytes_per_pixel_in_store && components_in_store == components_requested)
                resulting_swizzle = vkutil::color_to_texture_swizzle(info.swizzle, swizzle);
            else
                resulting_swizzle = swizzle;

            casted->texture.init_image(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, resulting_swizzle);
        }

        casted->scene_timestamp = scene_timestamp;
        const size_t casted_index = static_cast<size_t>(casted - casted_vec.data());
        vk::Filter cast_filter = vk::Filter::eNearest;
        if (native_size_cast && !native_point_texture
            && (state.physical_device.getFormatProperties(info.texture.format).optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
            cast_filter = vk::Filter::eLinear;
        auto record_cast = [this, info_ptr = &info, casted_index, casted_is_new, native_size_cast, cast_filter,
                               bytes_per_pixel_requested, bytes_per_pixel_in_store, width, height,
                               start_x, start_sourced_line, data_delta, stride_bytes](vk::CommandBuffer cmd_buffer) {
            auto &info = *info_ptr;
            auto *casted = &info.casted_textures[casted_index];
            if (casted_is_new)
                casted->texture.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst);
            else
                casted->texture.transition_to_discard(cmd_buffer, vkutil::ImageLayout::TransferDst);
            auto &source_image = info.raw_image_valid && info.raw_image && info.format != casted->format ? *info.raw_image : info.texture;
            source_image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferSrc);

            if (bytes_per_pixel_requested == bytes_per_pixel_in_store && native_size_cast) {
                const vk::ImageBlit blit{
                    .srcSubresource = vkutil::color_subresource_layer,
                    .srcOffsets = std::array<vk::Offset3D, 2>{ vk::Offset3D{ static_cast<int32_t>(start_x), static_cast<int32_t>(start_sourced_line), 0 }, vk::Offset3D{ static_cast<int32_t>(std::min(start_x + width, static_cast<uint32_t>(info.width))), static_cast<int32_t>(std::min(start_sourced_line + height, static_cast<uint32_t>(info.height))), 1 } },
                    .dstSubresource = vkutil::color_subresource_layer,
                    .dstOffsets = std::array<vk::Offset3D, 2>{ vk::Offset3D{}, vk::Offset3D{ static_cast<int32_t>(casted->texture.width), static_cast<int32_t>(casted->texture.height), 1 } }
                };
                cmd_buffer.blitImage(source_image.image, vk::ImageLayout::eTransferSrcOptimal, casted->texture.image, vk::ImageLayout::eTransferDstOptimal, blit, cast_filter);
            } else if (bytes_per_pixel_requested == bytes_per_pixel_in_store) {
                vk::ImageCopy image_copy{
                    .srcSubresource = vkutil::color_subresource_layer,
                    .srcOffset = { static_cast<int32_t>(start_x), static_cast<int32_t>(start_sourced_line), 0 },
                    .dstSubresource = vkutil::color_subresource_layer,
                    .dstOffset = { 0,
                        0,
                        0 },
                    .extent = {
                        // Don't try to copy what is in the stride
                        std::min<uint32_t>(width, info.width - start_x),
                        std::min<uint32_t>(height, info.height - start_sourced_line),
                        1 }
                };
                cmd_buffer.copyImage(source_image.image, vk::ImageLayout::eTransferSrcOptimal, casted->texture.image, vk::ImageLayout::eTransferDstOptimal, image_copy);
            } else if (state.res_multiplier != 1.0f
                && vk::blockSize(info.texture.format) == bytes_per_pixel_in_store
                && vk::blockSize(casted->texture.format) == bytes_per_pixel_requested) {
                copy_typeless_scaled(cmd_buffer, info, source_image, *casted, width, height, data_delta, bytes_per_pixel_in_store, bytes_per_pixel_requested);
            } else {
                LOG_INFO_ONCE("Game is doing typeless copies");
                // We must use a transition buffer
                vk::DeviceSize buffer_size = stride_bytes * static_cast<size_t>(state.res_multiplier * align(height, 4)) + start_x * bytes_per_pixel_requested;
                if (!casted->transition_buffer.buffer || casted->transition_buffer.size < buffer_size) {
                    // create or re-create the buffer
                    state.frame().destroy_queue.add_buffer(casted->transition_buffer);
                    casted->transition_buffer = vkutil::Buffer(buffer_size);
                    casted->transition_buffer.init_buffer(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc);
                }

                // copy the image to the buffer
                const uint32_t src_pixel_stride = static_cast<uint32_t>((info.stride_bytes / bytes_per_pixel_in_store) * state.res_multiplier);
                vk::BufferImageCopy copy_image_buffer{
                    .bufferOffset = 0,
                    .bufferRowLength = src_pixel_stride,
                    .bufferImageHeight = height,
                    .imageSubresource = vkutil::color_subresource_layer,
                    .imageOffset = { 0,
                        static_cast<int32_t>(start_sourced_line),
                        0 },
                    .imageExtent = { info.width, height, 1 }
                };
                cmd_buffer.copyImageToBuffer(source_image.image, vk::ImageLayout::eTransferSrcOptimal, casted->transition_buffer.buffer, copy_image_buffer);

                const vk::BufferMemoryBarrier barrier{
                    .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                    .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer = casted->transition_buffer.buffer,
                    .size = VK_WHOLE_SIZE
                };
                cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, barrier, {});

                // then the buffer to the image
                const uint32_t dst_pixel_stride = (stride_bytes / bytes_per_pixel_requested) * state.res_multiplier;
                copy_image_buffer
                    .setBufferOffset(start_x * bytes_per_pixel_requested)
                    .setBufferRowLength(dst_pixel_stride)
                    .setImageOffset({ 0, 0, 0 })
                    .setImageExtent({ width, height, 1 });
                cmd_buffer.copyBufferToImage(casted->transition_buffer.buffer, casted->texture.image, vk::ImageLayout::eTransferDstOptimal, copy_image_buffer);
            }
            source_image.transition_to(cmd_buffer, vkutil::ImageLayout::ColorAttachmentReadWrite);
            casted->texture.transition_to(cmd_buffer, vkutil::ImageLayout::SampledImage);
        };
        pending_casts.push_back({ casted->texture.view, &info, std::move(record_cast) });

        return TextureLookupResult{
            casted->texture.view,
            vkutil::ImageLayout::SampledImage,
            casted->texture.format,
            is_typeless_cast && !info.has_phase_view,
            cast_phase_hi
        };
    } else {
        // the renderpass external dependencies should take care of the barrier
        if (swizzle == info.swizzle && vk_format == info.texture.format)
            // we can use the same texture view
            return TextureLookupResult{
                info.texture.view,
                info.texture.layout,
                info.texture.format
            };

        if (!info.alternate_view) {
            vk::ComponentMapping resulting_mapping = vkutil::color_to_texture_swizzle(info.swizzle, swizzle);

            info.alternate_view = create_surface_view(info.texture.image, vk_format, resulting_mapping);
        }

        return TextureLookupResult{
            info.alternate_view,
            vkutil::ImageLayout::ColorAttachmentReadWrite,
            vk_format
        };
    }
}

bool VKSurfaceCache::begin_ds_scene_depth_check(const SceGxmDepthStencilSurface &depth_stencil, bool this_scene_stores, Address scene_color_addr) {
    const auto &lookup = depth_stencil.depth_data ? depth_address_lookup : stencil_address_lookup;
    const Address address = depth_stencil.depth_data ? depth_stencil.depth_data.address() : depth_stencil.stencil_data.address();
    const auto it = lookup.find(address);
    pending_ds_scene = it == lookup.end() ? nullptr : it->second;
    pending_ds_scene_stores = this_scene_stores;
    pending_ds_color_addr = scene_color_addr;
    if (!pending_ds_scene)
        return true;

    const bool continuation = pending_ds_scene->last_scene_color_addr == scene_color_addr;
    pending_ds_scene->last_scene_color_addr = scene_color_addr;
    return pending_ds_scene->depth_content_stored || continuation;
}

void VKSurfaceCache::resolve_ds_scene_end(bool scene_wrote_depth) {
    if (pending_ds_scene && scene_wrote_depth)
        pending_ds_scene->depth_content_stored = pending_ds_scene_stores;
    pending_ds_scene = nullptr;
}

SurfaceRetrieveResult VKSurfaceCache::retrieve_depth_stencil_for_framebuffer(SceGxmDepthStencilSurface *depth_stencil, const uint32_t width, const uint32_t height) {
    // when writing we use the render target size which is already upscaled
    int32_t memory_width = static_cast<int32_t>(width / state.res_multiplier);
    int32_t memory_height = static_cast<int32_t>(height / state.res_multiplier);

    const SurfaceTiling tiling = (depth_stencil->get_type() == SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR) ? SurfaceTiling::Linear : SurfaceTiling::Tiled;

    // check if MSAA is used, the depth buffer is never downscaled
    if (target->multisample_mode != SCE_GXM_MULTISAMPLE_NONE)
        memory_height *= 2;
    if (target->multisample_mode == SCE_GXM_MULTISAMPLE_4X)
        memory_width *= 2;

    const bool is_stencil_only = depth_stencil->depth_data.address() == 0;
    DepthStencilSurfaceCacheInfo *cached_info = nullptr;

    if (!is_stencil_only) {
        auto it = depth_address_lookup.find(depth_stencil->depth_data.address());
        if (it != depth_address_lookup.end())
            cached_info = it->second;
    } else {
        auto it = stencil_address_lookup.find(depth_stencil->stencil_data.address());
        if (it != stencil_address_lookup.end())
            cached_info = it->second;
    }

    if (cached_info)
        pending_ds_scene = cached_info;
    if (cached_info != nullptr) {
        // this the most recently used depth-stencil surface
        ds_surface_queue.set_as_mru(cached_info);

        bool need_remake = cached_info->texture.width < width
            || cached_info->texture.height < height
            || cached_info->stride_samples != depth_stencil->get_stride()
            || cached_info->tiling != tiling;

        if (!need_remake)
            return {
                cached_info->texture.view,
                &cached_info->texture
            };
    } else {
        // retrieve a new depth stencil
        cached_info = ds_surface_queue.get_lru();
    }

    // erase it if it was used previously
    if (cached_info->surface.depth_data)
        depth_address_lookup.erase(cached_info->surface.depth_data.address());
    if (cached_info->surface.stencil_data)
        stencil_address_lookup.erase(cached_info->surface.stencil_data.address());
    if (cached_info->texture.image)
        destroy_surface(*cached_info);

    // update the lookup info
    ds_surface_queue.set_as_mru(cached_info);
    if (depth_stencil->depth_data)
        depth_address_lookup[depth_stencil->depth_data.address()] = cached_info;
    if (depth_stencil->stencil_data)
        stencil_address_lookup[depth_stencil->stencil_data.address()] = cached_info;

    cached_info->surface = *depth_stencil;
    cached_info->memory_width = memory_width;
    cached_info->memory_height = memory_height;
    cached_info->multisample_mode = target->multisample_mode;
    cached_info->stride_samples = depth_stencil->get_stride();
    cached_info->tiling = tiling;
    cached_info->depth_content_stored = true;
    cached_info->last_scene_color_addr = pending_ds_color_addr;
    pending_ds_scene = cached_info;

    uint32_t bytes_per_sample;
    switch (depth_stencil->get_format()) {
    case SCE_GXM_DEPTH_STENCIL_FORMAT_S8:
        bytes_per_sample = 1;
        break;
    case SCE_GXM_DEPTH_STENCIL_FORMAT_D16:
        bytes_per_sample = 2;
        break;
    default:
        bytes_per_sample = 4;
        break;
    }
    cached_info->total_bytes = bytes_per_sample * depth_stencil->get_stride() * memory_height;

    vkutil::Image &image = cached_info->texture;

    // use prerender cmd in case we read from the depth buffer (although I really doubt this could happen)
    VKContext *context = reinterpret_cast<VKContext *>(state.context);
    vk::CommandBuffer cmd_buffer = context->prerender_cmd;

    image.width = width;
    image.height = height;
    image.format = state.deep_stencil_use;
    image.layout = vkutil::ImageLayout::Undefined;
    image.init_image(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled);

    image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst, vkutil::ds_subresource_range);
    vk::ClearDepthStencilValue clear_value{
        .depth = 1.0,
        .stencil = 0
    };
    cmd_buffer.clearDepthStencilImage(image.image, vk::ImageLayout::eTransferDstOptimal, clear_value, vkutil::ds_subresource_range);
    image.transition_to(cmd_buffer, vkutil::ImageLayout::DepthStencilReadOnly, vkutil::ds_subresource_range);

    return {
        image.view,
        &image
    };
}

std::optional<TextureLookupResult> VKSurfaceCache::retrieve_depth_stencil_as_texture(const SceGxmTexture &texture, TextureViewport *texture_viewport) {
    if (texture.texture_type() == SCE_GXM_TEXTURE_CUBE || texture.texture_type() == SCE_GXM_TEXTURE_CUBE_ARBITRARY)
        return std::nullopt;

    SceGxmTextureBaseFormat base_format = gxm::get_base_format(gxm::get_format(texture));
    bool can_be_depth = false;
    bool can_be_stencil = false;

    uint32_t bytes_per_sample = 4;
    switch (base_format) {
        // 8bit stencil
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
        bytes_per_sample = 1;
        can_be_stencil = true;
        break;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
        bytes_per_sample = 2;
        [[fallthrough]];
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
        can_be_depth = true;
        break;
    default:
        break;
    }
    int32_t memory_width = gxm::get_width(texture);
    int32_t memory_height = gxm::get_height(texture);

    SurfaceTiling tiling;
    uint32_t stride_samples;

    switch (texture.texture_type()) {
    case SCE_GXM_TEXTURE_LINEAR:
        tiling = SurfaceTiling::Linear;
        stride_samples = align(memory_width, 8);
        break;
    case SCE_GXM_TEXTURE_LINEAR_STRIDED:
        tiling = SurfaceTiling::Linear;
        stride_samples = (gxm::get_stride_in_bytes(texture) * 8) / gxm::bits_per_pixel(base_format);
        break;
    case SCE_GXM_TEXTURE_TILED:
        tiling = SurfaceTiling::Tiled;
        stride_samples = align(memory_width, 32);
        break;
    default:
        // a depth/stencil is never swizzled
        return std::nullopt;
    }

    if (stride_samples % 32 != 0)
        // a depth/stencil always has a stride which is a multiple of the tile size
        return std::nullopt;

    // take upscaling into account
    uint32_t width = static_cast<uint32_t>(memory_width * state.res_multiplier);
    uint32_t height = static_cast<uint32_t>(memory_height * state.res_multiplier);
    const uint64_t total_bytes = uint64_t{ bytes_per_sample } * stride_samples * memory_height;

    const uint32_t address = texture.data_addr << 2;
    uint32_t surface_address = 0;
    DepthStencilSurfaceCacheInfo *found_info = nullptr;

    const auto allocated_rows = [](const DepthStencilSurfaceCacheInfo &info) -> uint32_t {
        if (info.memory_height <= 0)
            return 0;
        const uint32_t rows = static_cast<uint32_t>(info.memory_height);
        return info.tiling == SurfaceTiling::Tiled ? align(rows, 32U) : rows;
    };

    if (can_be_depth) {
        // get the first depth surface with an address lower or equal to address
        auto it = depth_address_lookup.upper_bound(address);
        if (it != depth_address_lookup.begin()) {
            --it;

            const auto &info = *it->second;
            const uint64_t surface_bytes = info.memory_height > 0
                ? uint64_t{ info.total_bytes } / info.memory_height * allocated_rows(info)
                : 0;
            // the texture must be contained entirely in the depth surface allocation
            if (uint64_t{ address } + total_bytes <= uint64_t{ it->first } + surface_bytes) {
                surface_address = it->first;
                found_info = it->second;
            }
        }
    }
    if (!found_info && can_be_stencil) {
        // get the first stencil surface with an address lower or equal to address
        auto it = stencil_address_lookup.upper_bound(address);
        if (it != stencil_address_lookup.begin()) {
            --it;

            // note: we don't support sampling the stencil from a D24S8 depth-stencil
            // so we can assume any stencil uses only 1 byte per sample
            const uint64_t surface_bytes = uint64_t{ it->second->stride_samples } * allocated_rows(*it->second);

            // the texture must be contained entirely in the stencil surface
            if (uint64_t{ address } + total_bytes <= uint64_t{ it->first } + surface_bytes) {
                surface_address = it->first;
                found_info = it->second;
            }
        }
    }

    if (found_info == nullptr)
        return std::nullopt;

    DepthStencilSurfaceCacheInfo &cached_info = *found_info;
    if (tiling != cached_info.tiling || stride_samples != cached_info.stride_samples)
        return std::nullopt;

    // we sample from it, set the surface as most recently used
    ds_surface_queue.set_as_mru(found_info);

    // take MSAA into account
    if (cached_info.multisample_mode != SCE_GXM_MULTISAMPLE_NONE)
        height /= 2;
    if (cached_info.multisample_mode == SCE_GXM_MULTISAMPLE_4X)
        width /= 2;

    const bool is_stencil = can_be_stencil;

    const uint32_t delta_samples = (address - surface_address) / bytes_per_sample;
    uint32_t delta_col_samples = delta_samples % stride_samples;
    uint32_t delta_row_samples = delta_samples / stride_samples;

    if (delta_col_samples * state.res_multiplier >= cached_info.texture.width
        || delta_row_samples * state.res_multiplier >= cached_info.texture.height) {
        LOG_WARN_ONCE("Depth/stencil texture begins outside the rendered surface, using guest memory");
        return std::nullopt;
    }

    vk::ImageView ds_attachment = reinterpret_cast<VKContext *>(state.context)->current_ds_view;
    const bool reading_ds_attachment = cached_info.texture.view == ds_attachment;
    const bool same_dimension = memory_width == cached_info.memory_width
        && memory_height == cached_info.memory_height
        && delta_col_samples == 0
        && delta_row_samples == 0;

    if (!reading_ds_attachment && (state.features.use_texture_viewport || same_dimension)) {
        // we can just sample from the surface itself

        // we must create a new read-only view if it is not already present
        vk::ImageView &img_view = is_stencil ? cached_info.stencil_view : cached_info.depth_view;
        if (!img_view) {
            vk::ImageSubresourceRange range = vkutil::ds_subresource_range;
            range.aspectMask = is_stencil ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eDepth;
            vk::ImageViewCreateInfo view_info{
                .image = cached_info.texture.image,
                .viewType = vk::ImageViewType::e2D,
                .format = state.deep_stencil_use,
                .components = {},
                .subresourceRange = range
            };
            img_view = state.device.createImageView(view_info);
        }

        const float inv_surface_width = 1 / static_cast<float>(cached_info.memory_width);
        const float inv_surface_height = 1 / static_cast<float>(cached_info.memory_height);
        if (state.features.use_texture_viewport) {
            texture_viewport->offset = {
                delta_col_samples * inv_surface_width,
                delta_row_samples * inv_surface_height
            };
            texture_viewport->ratio = {
                memory_width * inv_surface_width,
                memory_height * inv_surface_height
            };
        }

        return TextureLookupResult{
            img_view,
            vkutil::ImageLayout::DepthStencilReadOnly,
            state.deep_stencil_use
        };
    }

    const uint64_t scene_timestamp = reinterpret_cast<VKContext *>(state.context)->scene_timestamp;

    int read_surface_idx = -1;
    for (int i = 0; i < cached_info.read_surfaces.size(); i++) {
        auto &read_surface = cached_info.read_surfaces[i];
        if (read_surface.depth_view.width == width
            && read_surface.depth_view.height == height
            && read_surface.delta_row == delta_row_samples
            && read_surface.delta_col == delta_col_samples) {
            read_surface_idx = i;
            break;
        }
    }

    if (read_surface_idx == -1) {
        // no compatible read surface found

        DepthSurfaceView read_only{
            .depth_view = vkutil::Image(width, height, state.deep_stencil_use),
            .scene_timestamp = 0,
            .delta_col = delta_col_samples,
            .delta_row = delta_row_samples,
        };
        read_only.depth_view.init_image(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
        // we want a texture view with only the depth or stencil aspect bit
        // TODO: not efficient
        state.device.destroy(read_only.depth_view.view);
        read_only.depth_view.view = nullptr;

        read_surface_idx = cached_info.read_surfaces.size();
        cached_info.read_surfaces.emplace_back(std::move(read_only));
    }

    DepthSurfaceView &read_only = cached_info.read_surfaces[read_surface_idx];
    vkutil::Image &img_view = is_stencil ? read_only.stencil_view : read_only.depth_view;

    if (!img_view.view) {
        vk::ImageSubresourceRange range = vkutil::ds_subresource_range;
        range.aspectMask = is_stencil ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eDepth;
        vk::ImageViewCreateInfo view_info{
            .image = read_only.depth_view.image,
            .viewType = vk::ImageViewType::e2D,
            .format = state.deep_stencil_use,
            .components = {},
            .subresourceRange = range
        };
        img_view.view = state.device.createImageView(view_info);
        img_view.layout = vkutil::ImageLayout::SampledImage;
    }

    // An active attachment may have changed since the previous snapshot in this scene.
    if (read_only.scene_timestamp == scene_timestamp && !reading_ds_attachment)
        return TextureLookupResult{
            img_view.view,
            img_view.layout,
            img_view.format
        };

    read_only.scene_timestamp = scene_timestamp;

    // Order a feedback snapshot after the draws that produced its source.
    VKContext *context = reinterpret_cast<VKContext *>(state.context);
    if (reading_ds_attachment && context->has_recorded_render_pass) {
        const SceGxmNotification notification{};
        context->stop_recording(notification, notification, false);
        context->start_recording();
    }
    vk::CommandBuffer cmd_buffer = context->prerender_cmd;

    delta_row_samples *= state.res_multiplier;
    delta_col_samples *= state.res_multiplier;

    read_only.depth_view.transition_to_discard(cmd_buffer, vkutil::ImageLayout::TransferDst, vkutil::ds_subresource_range);

    cached_info.texture.transition_to(cmd_buffer, vkutil::ImageLayout::TransferSrc, vkutil::ds_subresource_range);
    vk::ImageSubresourceLayers layers = vkutil::color_subresource_layer;
    layers.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    vk::ImageCopy image_copy{
        .srcSubresource = layers,
        .srcOffset = { static_cast<int>(delta_col_samples), static_cast<int>(delta_row_samples), 0 },
        .dstSubresource = layers,
        .dstOffset = { 0, 0, 0 },
        .extent = { std::min(width, cached_info.texture.width - delta_col_samples), std::min(height, cached_info.texture.height - delta_row_samples), 1U }
    };
    cmd_buffer.copyImage(cached_info.texture.image, vk::ImageLayout::eTransferSrcOptimal, read_only.depth_view.image, vk::ImageLayout::eTransferDstOptimal, image_copy);

    // transition back
    cached_info.texture.transition_to(cmd_buffer, vkutil::ImageLayout::DepthStencilReadOnly, vkutil::ds_subresource_range);
    read_only.depth_view.transition_to(cmd_buffer, vkutil::ImageLayout::SampledImage, vkutil::ds_subresource_range);

    return TextureLookupResult{
        img_view.view,
        img_view.layout,
        img_view.format
    };
}

static Framebuffer empty_framebuffer{};
Framebuffer &VKSurfaceCache::retrieve_framebuffer_handle(MemState &mem, SceGxmColorSurface *color, SceGxmDepthStencilSurface *depth_stencil,
    vk::RenderPass standard_render_pass, vk::RenderPass interlock_render_pass, vk::ImageView &color_view, vk::ImageView &ds_view) {
    if (!target) {
        LOG_ERROR("Unable to retrieve framebuffer with no active render target!");
        return empty_framebuffer;
    }

    if (!color && !depth_stencil)
        LOG_INFO_ONCE("Using transient attachments for a scene without color or depth/stencil backing");

    // might get modified by retrieve_color_surface_for_framebuffer
    state.pipeline_cache.can_use_deferred_compilation = true;

    // First retrieve separately the color surface and ds surface
    SurfaceRetrieveResult color_result;
    SurfaceRetrieveResult ds_result;

    if (color) {
        color_result = retrieve_color_surface_for_framebuffer(mem, color);
    } else {
        color_result.view = target->color.view;
        color_result.base_image = &target->color;
    }

    if (depth_stencil) {
        ds_result = retrieve_depth_stencil_for_framebuffer(depth_stencil, target->width, target->height);
    } else {
        ds_result.view = target->depthstencil.view;
        ds_result.base_image = &target->depthstencil;
    }

    color_view = color_result.view;
    ds_view = ds_result.view;

    std::pair<vk::ImageView, vk::ImageView> key = { color_view, ds_view };
    auto it = framebuffer_array.find(key);

    if (it != framebuffer_array.end()) {
        // we already created a framebuffer for this pair
        return it->second;
    }

    // make the framebuffer as big as possible
    const uint32_t framebuffer_width = std::min(color_result.base_image->width, ds_result.base_image->width);
    const uint32_t framebuffer_height = std::min(color_result.base_image->height, ds_result.base_image->height);

    vk::FramebufferCreateInfo fb_info{
        .renderPass = standard_render_pass,
        .width = framebuffer_width,
        .height = framebuffer_height,
        .layers = 1
    };
    const auto raw = color_result.color_surface ? color_result.color_surface->raw_image.get() : nullptr;
    vk::ImageView attachments[] = { color_result.view, raw ? raw->view : ds_result.view, ds_result.view };
    fb_info.setAttachments(attachments);
    fb_info.attachmentCount = raw ? 3 : 2;
    vk::Framebuffer fb_standard = state.device.createFramebuffer(fb_info);

    vk::Framebuffer fb_interlock = nullptr;
    vk::ImageView storage_color_view;
    if (state.features.support_shader_interlock) {
        // we also need to create the framebuffer for shader interlock
        fb_info.renderPass = interlock_render_pass;
        fb_info.pAttachments = &attachments[2];
        fb_info.attachmentCount = 1;
        fb_interlock = state.device.createFramebuffer(fb_info);

        if (color && color->gamma && gxm::get_base_format(color->colorFormat) == SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8) {
            // Storage accesses use UNORM; the interlock shader handles gamma conversion.
            const vk::ImageViewCreateInfo view_info{
                .image = color_result.base_image->image,
                .viewType = vk::ImageViewType::e2D,
                .format = vk::Format::eR8G8B8A8Unorm,
                .components = vkutil::default_comp_mapping,
                .subresourceRange = vkutil::color_subresource_range
            };
            storage_color_view = state.device.createImageView(view_info);
        }
    }

    return (framebuffer_array[key] = { fb_standard, fb_interlock, color_result.base_image, storage_color_view, { framebuffer_width, framebuffer_height }, color_result.color_surface });
}

bool VKSurfaceCache::has_newer_overlapping_surface(const ColorSurfaceCacheInfo &surface) const {
    const uint64_t begin = surface.data.address();
    const uint64_t end = begin + surface.total_bytes;
    for (const auto &[base, info] : color_address_lookup) {
        if (info != &surface && base<end &&static_cast<uint64_t>(base) + info->total_bytes> begin
            && info->last_scene_rendered > surface.last_scene_rendered)
            return true;
    }
    return false;
}

bool VKSurfaceCache::check_for_surface(MemState &mem, Address source_address, CallbackRequestFunction &callback, Address target_address) {
    if (!state.features.enable_memory_mapping || state.disable_surface_sync)
        return false;

    if (vector_utils::find_index(cpu_surfaces_changed, source_address) != -1) {
        // there is a transfer operation pending on this surface, just add the callback after and we are done
        state.request_queue.push(CallbackRequest{ new CallbackRequestFunction(std::move(callback)) });

        if (target_address)
            cpu_surfaces_changed.push_back(target_address);
        return true;
    }

    // for now, only look if the address matches exactly a color surface
    auto it = color_address_lookup.find(source_address);
    if (it == color_address_lookup.end())
        return false;

    auto &surface = *it->second;
    VKContext &context = *static_cast<VKContext *>(state.context);
    const bool stale = surface.last_frame_rendered + MAX_FRAMES_RENDERING <= state.frame_timestamp;
    const bool needs_writeback = surface.written_back_scene < surface.last_scene_rendered;
    const bool sync_stale = stale && needs_writeback && surface.tiling == SurfaceTiling::Linear
        && format_support_surface_sync(surface.format) && !*surface.dirty && !has_newer_overlapping_surface(surface);
    if (stale && needs_writeback && !sync_stale)
        return false;

    // A recorded writeback may still be pending on the wait thread: keep callbacks ordered even for old surfaces.
    if ((!stale && !*surface.need_surface_sync) || sync_stale) {
        // first send the command to sync the surface with the GPU
        *surface.need_surface_sync = true;

        // we shouldn't have a command buffer being used, but just in case
        vk::CommandBuffer prev_cmd = context.render_cmd;

        // for the time being, just create a temp command buffer / fence
        // That's not the best approach but I guess it works
        vk::CommandBuffer surface_cmd = nullptr;
        vk::Fence fence = state.device.createFence({});
        ColorSurfaceCacheInfo *returned_info = nullptr;
        {
            std::lock_guard<std::mutex> lock(state.multithread_pool_mutex);
            surface_cmd = vkutil::create_single_time_command(state.device, state.multithread_command_pool);

            context.render_cmd = surface_cmd;
            ColorSurfaceCacheInfo *const prev_last_written = last_written_surface;
            last_written_surface = &surface;
            returned_info = perform_surface_sync(true);
            last_written_surface = prev_last_written == &surface ? nullptr : prev_last_written;
            context.render_cmd = prev_cmd;

            surface_cmd.end();
        }
        // submit this command
        vk::SubmitInfo submit_info{};
        submit_info.setCommandBuffers(surface_cmd);
        try {
            std::lock_guard<std::mutex> lock(state.queue_mutex);
            state.general_queue.submit(submit_info, fence);
        } catch (const vk::DeviceLostError &) {
            state.report_device_lost("surface readback queue submit");
            throw;
        }

        // now we need to wait for the fence, then destroy it along with the command buffer
        // to prevent memory leaks
        CallbackRequestFunction vk_callback = [&state = this->state, fence, surface_cmd]() {
            auto result = state.wait_for_fences(fence, std::numeric_limits<uint64_t>::max());
            if (result != vk::Result::eSuccess)
                LOG_ERROR("Could not wait for fences.");

            // destroy the objects
            state.device.destroyFence(fence);

            std::lock_guard<std::mutex> lock(state.multithread_pool_mutex);
            state.device.freeCommandBuffers(state.multithread_command_pool, surface_cmd);
        };
        state.request_queue.push(CallbackRequest{ new CallbackRequestFunction(std::move(vk_callback)) });

        queue_surface_sync_completion(returned_info);
    }

    // now push the callback
    state.request_queue.push(CallbackRequest{ new CallbackRequestFunction(std::move(callback)) });

    if (target_address)
        cpu_surfaces_changed.push_back(target_address);

    return true;
}

void VKSurfaceCache::queue_surface_sync_completion(ColorSurfaceCacheInfo *surface) {
    if (!surface)
        return;

    if (state.mapping_method == MappingMethod::DoubleBuffer && surface->need_buffer_sync) {
        const auto &region = surface->sync_region;
        const uint32_t bytes_per_pixel = gxm::bits_per_pixel(surface->format) / 8;
        const uint32_t row_bytes = region.extent.width * bytes_per_pixel;
        const uint32_t size = (region.extent.height - 1) * surface->stride_bytes + row_bytes;
        const Address location = surface->data.address() + region.offset.y * surface->stride_bytes + region.offset.x * bytes_per_pixel;
        state.request_queue.push(BufferSyncRequest{ location, size, surface->stride_bytes, row_bytes, region.extent.height });
    }
    if (surface->need_post_surface_sync)
        state.request_queue.push(PostSurfaceSyncRequest{ surface });
}

ColorSurfaceCacheInfo *VKSurfaceCache::perform_surface_sync(bool full_surface) {
    // surface sync is supported only if memory mapping is enabled
    if (!state.features.enable_memory_mapping)
        return nullptr;

    if (last_written_surface == nullptr || !*last_written_surface->need_surface_sync)
        return nullptr;

    VKContext *context = reinterpret_cast<VKContext *>(state.context);
    vk::CommandBuffer cmd_buffer = context->render_cmd;

    auto &sync_source = last_written_surface->raw_image_valid && last_written_surface->raw_image
        ? *last_written_surface->raw_image
        : last_written_surface->texture;
    sync_source.transition_to(cmd_buffer, vkutil::ImageLayout::TransferSrc);
    vk::Image image_to_copy = sync_source.image;
    vk::ImageLayout image_layout = vk::ImageLayout::eTransferSrcOptimal;

    // this works for surface swizzles
    bool is_swizzle_identity = last_written_surface->swizzle.r == vk::ComponentSwizzle::eR;
    if (!is_swizzle_identity && !format_support_swizzle(last_written_surface->format)
        && !surface_sync_needs_u4u4u4u4_repack(*last_written_surface)) {
        LOG_WARN_ONCE("Surface sync with swizzle not support on {}", vk::to_string(last_written_surface->texture.format));

        is_swizzle_identity = true;
    }

    vk::Rect2D region{ { 0, 0 }, { last_written_surface->original_width, last_written_surface->original_height } };
    vk::Offset3D src_begin{};
    vk::Offset3D src_end{ last_written_surface->width, last_written_surface->height, 1 };
    const bool needs_f10_repack = surface_sync_needs_f10_repack(*last_written_surface);
    const bool can_sync_region = !needs_f10_repack && (is_swizzle_identity || surface_sync_needs_u4u4u4u4_repack(*last_written_surface));
    if (!full_surface && state.res_multiplier != 1.0f && context->render_target->has_macroblock_sync && context->rendered_area
        && can_sync_region && !format_need_additional_memory(last_written_surface->format)) {
        const auto &area = *context->rendered_area;
        src_begin.x = std::clamp(area.offset.x, 0, static_cast<int32_t>(last_written_surface->width));
        src_begin.y = std::clamp(area.offset.y, 0, static_cast<int32_t>(last_written_surface->height));
        src_end.x = std::clamp(area.offset.x + static_cast<int32_t>(area.extent.width), src_begin.x, static_cast<int32_t>(last_written_surface->width));
        src_end.y = std::clamp(area.offset.y + static_cast<int32_t>(area.extent.height), src_begin.y, static_cast<int32_t>(last_written_surface->height));
        region.offset = { static_cast<int32_t>(src_begin.x / state.res_multiplier), static_cast<int32_t>(src_begin.y / state.res_multiplier) };
        const uint32_t x1 = std::min<uint32_t>(src_end.x / state.res_multiplier, last_written_surface->original_width);
        const uint32_t y1 = std::min<uint32_t>(src_end.y / state.res_multiplier, last_written_surface->original_height);
        region.extent = { x1 - region.offset.x, y1 - region.offset.y };
        if (!region.extent.width || !region.extent.height) {
            LOG_WARN_ONCE("Surface sync region is empty after resolution scaling");
            sync_source.transition_to(cmd_buffer, vkutil::ImageLayout::ColorAttachmentReadWrite);
            return nullptr;
        }
    }
    last_written_surface->sync_region = region;

    if (state.res_multiplier != 1.0f) {
        // scale back the image using a blit command first

        if (!last_written_surface->blit_image)
            last_written_surface->blit_image = std::make_unique<vkutil::Image>();

        vkutil::Image &blit_image = *last_written_surface->blit_image;

        if (blit_image.image && blit_image.format != sync_source.format) {
            state.frame().destroy_queue.add_image(blit_image);
            blit_image = vkutil::Image();
        }

        if (!blit_image.image) {
            blit_image.format = sync_source.format;
            blit_image.width = last_written_surface->original_width;
            blit_image.height = last_written_surface->original_height;

            blit_image.init_image(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);
            blit_image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst);
        } else {
            blit_image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst);
        }

        vk::ImageBlit blit{
            .srcSubresource = vkutil::color_subresource_layer,
            .srcOffsets = std::array<vk::Offset3D, 2>{ src_begin, src_end },
            .dstSubresource = vkutil::color_subresource_layer,
            .dstOffsets = std::array<vk::Offset3D, 2>{ vk::Offset3D{ region.offset.x, region.offset.y, 0 }, vk::Offset3D{ region.offset.x + static_cast<int32_t>(region.extent.width), region.offset.y + static_cast<int32_t>(region.extent.height), 1 } },
        };
        const bool linear_supported = static_cast<bool>(state.physical_device.getFormatProperties(blit_image.format).optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear);
        cmd_buffer.blitImage(image_to_copy, image_layout, blit_image.image, vk::ImageLayout::eTransferDstOptimal, blit, linear_supported ? vk::Filter::eLinear : vk::Filter::eNearest);

        blit_image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferSrc);
        image_to_copy = blit_image.image;
        image_layout = vk::ImageLayout::eTransferSrcOptimal;
    }

    vk::Buffer buffer;
    uint32_t offset;
    const uint32_t pixel_stride = (last_written_surface->stride_bytes * 8) / gxm::bits_per_pixel(last_written_surface->format);
    const uint32_t transfer_pixel_size = static_cast<uint32_t>(vk::blockSize(last_written_surface->texture.format));
    const bool needs_copy_buffer = format_need_additional_memory(last_written_surface->format)
        || surface_sync_needs_u4u4u4u4_repack(*last_written_surface) || needs_f10_repack;
    if (needs_copy_buffer) {
        if (!last_written_surface->copy_buffer)
            last_written_surface->copy_buffer = std::make_unique<vkutil::Buffer>();

        vkutil::Buffer &copy_buffer = *last_written_surface->copy_buffer;

        const vk::DeviceSize required_size = static_cast<vk::DeviceSize>(pixel_stride) * last_written_surface->original_height * transfer_pixel_size;
        ensure_surface_sync_buffer(copy_buffer, state.frame().destroy_queue, required_size,
            needs_f10_repack ? vkutil::vma_readback_alloc : vkutil::vma_mapped_alloc);

        buffer = copy_buffer.buffer;
        offset = 0;

        last_written_surface->need_buffer_sync = false;
        last_written_surface->need_post_surface_sync = true;
    } else {
        last_written_surface->need_buffer_sync = true;
        last_written_surface->need_post_surface_sync = !is_swizzle_identity;
        std::tie(buffer, offset) = state.get_matching_mapping(last_written_surface->data);
    }
    vk::BufferImageCopy copy{
        .bufferOffset = offset + (static_cast<vk::DeviceSize>(region.offset.y) * pixel_stride + region.offset.x) * transfer_pixel_size,
        .bufferRowLength = pixel_stride,
        .bufferImageHeight = last_written_surface->original_height,
        .imageSubresource = vkutil::color_subresource_layer,
        .imageOffset = { region.offset.x, region.offset.y, 0 },
        .imageExtent = { region.extent.width, region.extent.height, 1 }
    };
    cmd_buffer.copyImageToBuffer(image_to_copy, image_layout, buffer, copy);
    sync_source.transition_to(cmd_buffer, vkutil::ImageLayout::ColorAttachmentReadWrite);

    if (region.offset.x == 0 && region.offset.y == 0
        && region.extent.width == last_written_surface->original_width && region.extent.height == last_written_surface->original_height)
        last_written_surface->written_back_scene = last_written_surface->last_scene_rendered;

    ColorSurfaceCacheInfo *return_value = last_written_surface;
    last_written_surface = nullptr;

    return return_value;
}

template <typename T>
static void swizzle_text_T_2(T *pixels, uint32_t nb_pixel) {
    for (uint32_t i = 0; i < nb_pixel; i++) {
        std::swap(pixels[2 * i], pixels[2 * i + 1]);
    }
}

template <typename T, size_t type>
static void swizzle_text_T_4(T *pixels, uint32_t nb_pixel) {
    for (uint32_t i = 0; i < nb_pixel; i++) {
        if constexpr (type == 0) {
            // BGRA
            std::swap(pixels[4 * i], pixels[4 * i + 2]);
        } else if constexpr (type == 1) {
            // ABGR
            std::swap(pixels[4 * i], pixels[4 * i + 3]);
            std::swap(pixels[4 * i + 1], pixels[4 * i + 2]);
        } else {
            // ARGB
            T copy[] = { pixels[4 * i],
                pixels[4 * i + 1],
                pixels[4 * i + 2],
                pixels[4 * i + 3] };
            pixels[4 * i] = copy[3];
            pixels[4 * i + 1] = copy[0];
            pixels[4 * i + 2] = copy[1];
            pixels[4 * i + 3] = copy[2];
        }
    }
}

template <typename T>
static void swizzle_text_T(T *pixels, uint32_t nb_pixel, ColorSurfaceCacheInfo *surface) {
    // there can only be 2 or 4 component textures here
    if (vk::componentCount(surface->texture.format) == 2) {
        swizzle_text_T_2<T>(pixels, nb_pixel);
    } else {
        // find the swizzle
        // swizzles are inversed
        switch (surface->swizzle.r) {
        case vk::ComponentSwizzle::eB:
            // BGRA
            swizzle_text_T_4<T, 0>(pixels, nb_pixel);
            break;
        case vk::ComponentSwizzle::eA:
            // ABGR
            swizzle_text_T_4<T, 1>(pixels, nb_pixel);
            break;
        case vk::ComponentSwizzle::eG:
            // ARGB
            swizzle_text_T_4<T, 2>(pixels, nb_pixel);
            break;
        default:
            // RGBA, nothing to swap
            break;
        }
    }
}

void VKSurfaceCache::perform_post_surface_sync(const MemState &mem, ColorSurfaceCacheInfo *surface) {
    if (surface == nullptr)
        return;

    const SurfaceSyncWriteScope internal_write;
    const uint32_t pixel_stride = (surface->stride_bytes * 8) / gxm::bits_per_pixel(surface->format);
    const uint32_t nb_pixels = pixel_stride * surface->original_height;
    uint8_t *pixels = surface->data.cast<uint8_t>().get(mem);

    if (surface_sync_needs_u4u4u4u4_repack(*surface)) {
        pack_rgba8_to_u4u4u4u4(pixels, static_cast<const uint8_t *>(surface->copy_buffer->mapped_data), pixel_stride, surface->sync_region, surface->swizzle);
        return;
    }

    if (surface_sync_needs_f10_repack(*surface)) {
        pack_rgba16f_to_u2f10f10f10(pixels, static_cast<const uint8_t *>(surface->copy_buffer->mapped_data), pixel_stride,
            surface->original_width, surface->original_height, surface->stride_bytes);
        return;
    }

    if (format_need_additional_memory(surface->format)) {
        // special case, use a custom function
        const bool is_swizzle_identity = surface->swizzle.r == vk::ComponentSwizzle::eR;
        if (!surface->sws_context) {
            const AVPixelFormat dst_fmt = is_swizzle_identity ? AV_PIX_FMT_RGB24 : AV_PIX_FMT_BGR24;
            surface->sws_context = sws_getContext(surface->original_width, surface->original_height, AV_PIX_FMT_RGB0, surface->original_width, surface->original_height, dst_fmt, 0, nullptr, nullptr, nullptr);
            assert(surface->sws_context != nullptr);
        }

        int src_stride = pixel_stride * 4;
        int dst_stride = pixel_stride * 3;
        sws_scale(surface->sws_context, reinterpret_cast<const uint8_t *const *>(&surface->copy_buffer->mapped_data), &src_stride, 0, surface->original_height, &pixels, &dst_stride);
        return;
    }

    switch (vk::componentBits(surface->texture.format, 0)) {
    case 8:
        swizzle_text_T<uint8_t>(pixels, nb_pixels, surface);
        break;
    case 16:
        swizzle_text_T<uint16_t>(reinterpret_cast<uint16_t *>(pixels), nb_pixels, surface);
        break;
    case 32:
        swizzle_text_T<uint32_t>(reinterpret_cast<uint32_t *>(pixels), nb_pixels, surface);
        break;
    }
}

void VKSurfaceCache::destroy_associated_framebuffers(const VKRenderTarget *render_target) {
    if (!render_target)
        return;

    destroy_framebuffers(render_target->color.view);
    destroy_framebuffers(render_target->depthstencil.view);
}

vk::ImageView VKSurfaceCache::sourcing_color_surface_for_presentation(Ptr<const void> address, uint32_t pitch, Viewport &viewport) {
    // get closest surface with an address below address
    auto ite = color_address_lookup.upper_bound(address.address());
    if (ite == color_address_lookup.begin()) {
        return nullptr;
    }
    --ite;

    ColorSurfaceCacheInfo &info = *ite->second;
    if (info.data.address() + info.total_bytes <= address.address())
        // they do not overlap
        return nullptr;

    if (info.stride_bytes == pitch * 4) {
        // In assumption the format is RGBA8
        const size_t data_delta = address.address() - ite->first;
        uint32_t limited_height = viewport.height;
        if ((data_delta % (pitch * 4)) == 0) {
            uint32_t start_sourced_line = static_cast<uint32_t>((data_delta / (pitch * 4)) * state.res_multiplier);
            if ((start_sourced_line + viewport.height) > info.height) {
                // Sometimes the surface is just missing a little bit of lines
                if (start_sourced_line < info.height) {
                    // Just limit the height and display it
                    limited_height = info.height - start_sourced_line;
                } else {
                    LOG_ERROR("Trying to present non-existent segment in cached color surface!");
                    return nullptr;
                }
            }

            // Compute position in texture
            viewport.offset_x = 0;
            viewport.offset_y = start_sourced_line;
            viewport.width = std::min(viewport.width, static_cast<uint32_t>(info.width));
            viewport.height = limited_height;
            viewport.texture_width = info.width;
            viewport.texture_height = info.height;

            if (info.swizzle == vkutil::rgba_mapping && info.texture.format == vk::Format::eR8G8B8A8Unorm)
                return info.texture.view;

            if (!info.alternate_view) {
                // create a view with the right swizzle and without gamma correction
                vk::ImageViewCreateInfo view_info{
                    .image = info.texture.image,
                    .viewType = vk::ImageViewType::e2D,
                    .format = vk::Format::eR8G8B8A8Unorm,
                    .components = vkutil::color_to_texture_swizzle(info.swizzle, vkutil::rgba_mapping),
                    .subresourceRange = vkutil::color_subresource_range
                };
                info.alternate_view = state.device.createImageView(view_info);
            }

            return info.alternate_view;
        }
    }

    return nullptr;
}

std::vector<uint32_t> VKSurfaceCache::dump_frame(Ptr<const void> address, uint32_t width, uint32_t height, uint32_t pitch) {
    // get closest surface with an address below address
    auto ite = color_address_lookup.upper_bound(address.address());
    if (ite == color_address_lookup.begin()) {
        return {};
    }
    --ite;

    const ColorSurfaceCacheInfo &info = *ite->second;

    const uint32_t data_delta = address.address() - ite->first;
    const uint32_t pitch_byte = pitch * 4;
    if (info.stride_bytes != pitch_byte || data_delta % pitch_byte != 0)
        return {};

    const uint32_t line_delta = static_cast<uint32_t>((data_delta / pitch_byte) * state.res_multiplier);
    if (line_delta >= info.height)
        return {};

    const uint32_t real_height = std::min(height, info.height - line_delta);

    std::vector<uint32_t> frame(width * height, 0);

    // we need a temporary buffer and command buffer for this
    // this is a raii buffer, it will be destroyed at the end of this function
    vkutil::Buffer temp_buff(width * height * 4);
    temp_buff.init_buffer(vk::BufferUsageFlagBits::eTransferDst, vkutil::vma_mapped_alloc);

    // Capture runs outside the render thread, so it must not record into its command pool.
    const vk::CommandPoolCreateInfo pool_info{
        .flags = vk::CommandPoolCreateFlagBits::eTransient,
        .queueFamilyIndex = state.general_family_index
    };
    const auto command_pool = state.device.createCommandPoolUnique(pool_info);
    vk::CommandBuffer cmd_buffer = vkutil::create_single_time_command(state.device, *command_pool);

    // layout is general, we can directly copy from it
    vk::BufferImageCopy image_copy{
        .bufferOffset = 0,
        .bufferRowLength = width,
        .bufferImageHeight = height,
        .imageSubresource = vkutil::color_subresource_layer,
        .imageOffset = { 0, static_cast<int>(line_delta), 0 },
        .imageExtent = { width, real_height, 1 }
    };
    cmd_buffer.copyImageToBuffer(info.texture.image, vk::ImageLayout::eGeneral, temp_buff.buffer, image_copy);

    // this will cause a waitIdle, not an issue
    {
        std::lock_guard<std::mutex> lock(state.queue_mutex);
        vkutil::end_single_time_command(state.device, state.general_queue, *command_pool, cmd_buffer);
    }

    memcpy(frame.data(), temp_buff.mapped_data, frame.size() * 4);

    return frame;
}

} // namespace renderer::vulkan
