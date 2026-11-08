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

#include <renderer/metal/functions.h>
#include <renderer/metal/state.h>

#include "gxm_to_metal.h"
#include "mtl_types.h"

#include <config/state.h>
#include <display/state.h>
#include <renderer/functions.h>
#include <renderer/shaders.h>
#include <shader/spirv_recompiler.h>
#include <util/align.h>
#include <util/log.h>

#include <xxh3.h>

#include <chrono>

namespace renderer::metal {

id<MTLTexture> create_texture(id<MTLDevice> device, MTLPixelFormat format, uint32_t width, uint32_t height, MTLTextureUsage usage,
    uint32_t mip_count, bool is_cube, uint32_t array_length, MTLStorageMode storage, const char *label) {
    MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
    desc.textureType = is_cube ? MTLTextureTypeCube : (array_length > 1 ? MTLTextureType2DArray : MTLTextureType2D);
    desc.pixelFormat = format;
    desc.width = std::max<uint32_t>(width, 1);
    desc.height = std::max<uint32_t>(height, 1);
    desc.depth = 1;
    desc.mipmapLevelCount = std::max<uint32_t>(mip_count, 1);
    desc.arrayLength = is_cube ? 1 : std::max<uint32_t>(array_length, 1);
    desc.sampleCount = 1;
    desc.usage = usage;
    desc.storageMode = storage;
    desc.cpuCacheMode = MTLCPUCacheModeDefaultCache;

    id<MTLTexture> texture = [device newTextureWithDescriptor:desc];
    if (!texture) {
        LOG_ERROR("Failed to create a {}x{} Metal texture with format {}", width, height, format_name(format));
        return nil;
    }
    if (label)
        texture.label = [NSString stringWithUTF8String:label];
    return texture;
}

id<MTLTexture> create_texture_view(id<MTLTexture> texture, MTLPixelFormat format, const MTLTextureSwizzleChannels &swizzle) {
    if (!texture)
        return nil;
    return [texture newTextureViewWithPixelFormat:format
                                      textureType:texture.textureType
                                           levels:NSMakeRange(0, texture.mipmapLevelCount)
                                           slices:NSMakeRange(0, texture.arrayLength)
                                          swizzle:swizzle];
}

id<MTLBuffer> create_shared_buffer(id<MTLDevice> device, size_t size, const char *label) {
    id<MTLBuffer> buffer = [device newBufferWithLength:std::max<size_t>(size, 16) options:MTLResourceStorageModeShared];
    if (!buffer) {
        LOG_ERROR("Failed to create a Metal buffer of {} bytes", size);
        return nil;
    }
    if (label)
        buffer.label = [NSString stringWithUTF8String:label];
    return buffer;
}

void RingBuffer::create(id<MTLDevice> device, uint32_t new_capacity, NSString *label) {
    capacity = new_capacity;
    cursor = 0;
    buffer = [device newBufferWithLength:capacity options:MTLResourceStorageModeShared];
    buffer.label = label;
}

void RingBuffer::allocate(const uint32_t data_size) {
    if (cursor + data_size > capacity)
        cursor = 0;

    data_offset = cursor;
    cursor += data_size;
    cursor = align(cursor, alignment);
}

void RingBuffer::copy(const uint32_t size, const void *data, const uint32_t offset) {
    memcpy(static_cast<uint8_t *>(buffer.contents) + data_offset + offset, data, size);
}

MetalDevice::MetalDevice(MetalState &state)
    : state(state)
    , texture_cache(state)
    , surface_cache(state)
    , pipeline_cache(state)
    , screen_renderer(state) {
}

void MetalDevice::wait_idle() {
    if (!queue)
        return;
    // the queue executes command buffers in order, waiting for an empty one waits for everything submitted before
    id<MTLCommandBuffer> cmd = [queue commandBuffer];
    [cmd commit];
    [cmd waitUntilCompleted];

    for (auto &frame : frames)
        frame.rendered_cmds.clear();
}

MetalState::MetalState() = default;

MetalState::~MetalState() = default;

bool MetalState::init() {
    shader_version = fmt::format("v{}", shader::CURRENT_VERSION);
    // the mask is a read-write rgba8 image in the fragment shader
    features.use_mask_bit = device && device->support_rw_texture;
    return true;
}

static id<MTLDevice> select_device(int gpu_idx) {
    NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
    if (gpu_idx > 0 && static_cast<NSUInteger>(gpu_idx) <= devices.count)
        return devices[gpu_idx - 1];

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device && devices.count > 0)
        device = devices[0];
    return device;
}

bool MetalState::create(std::unique_ptr<renderer::State> &state, const Config &config) {
    @autoreleasepool {
        device = std::make_unique<MetalDevice>(*this);
        MetalDevice &dev = *device;

        dev.device = select_device(config.current_config.gpu_idx);
        if (!dev.device) {
            LOG_ERROR("No Metal device is available on this system");
            return false;
        }

        dev.queue = [dev.device newCommandQueueWithMaxCommandBufferCount:256];
        if (!dev.queue) {
            LOG_ERROR("Failed to create the Metal command queue");
            return false;
        }
        dev.queue.label = @"Vita3K";

        gpu_name = dev.device.name.UTF8String;
        dev.is_apple_gpu = [dev.device supportsFamily:MTLGPUFamilyApple1];
        dev.support_framebuffer_fetch = dev.is_apple_gpu;
        dev.support_packed16 = dev.is_apple_gpu;
        dev.support_bc = dev.device.supportsBCTextureCompression;
        dev.support_astc = dev.is_apple_gpu;
        dev.support_rw_texture = dev.device.readWriteTextureSupport == MTLReadWriteTextureTier2;
        dev.max_texture_size = ([dev.device supportsFamily:MTLGPUFamilyApple3] || [dev.device supportsFamily:MTLGPUFamilyMac2]) ? 16384 : 8192;
        dev.max_anisotropy = 16;

        LOG_INFO("Metal device: {}", gpu_name);
        LOG_INFO("Metal features: apple GPU: {}, framebuffer fetch: {}, packed 16-bit formats: {}, BC textures: {}, ASTC textures: {}, read-write textures: {}",
            dev.is_apple_gpu, dev.support_framebuffer_fetch, dev.support_packed16, dev.support_bc, dev.support_astc, dev.support_rw_texture);

        // default texture (white), sampler and buffer
        {
            dev.default_texture = create_texture(dev.device, MTLPixelFormatRGBA8Unorm, 1, 1, MTLTextureUsageShaderRead, 1, false, 1, MTLStorageModeManaged, "default texture");
            if (!dev.default_texture)
                return false;
            const uint32_t white = 0xFFFFFFFF;
            [dev.default_texture replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&white bytesPerRow:4];

            MTLSamplerDescriptor *sampler_desc = [[MTLSamplerDescriptor alloc] init];
            sampler_desc.minFilter = MTLSamplerMinMagFilterLinear;
            sampler_desc.magFilter = MTLSamplerMinMagFilterLinear;
            sampler_desc.mipFilter = MTLSamplerMipFilterNotMipmapped;
            sampler_desc.sAddressMode = MTLSamplerAddressModeRepeat;
            sampler_desc.tAddressMode = MTLSamplerAddressModeRepeat;
            sampler_desc.rAddressMode = MTLSamplerAddressModeRepeat;
            dev.default_sampler = [dev.device newSamplerStateWithDescriptor:sampler_desc];

            dev.default_buffer = create_shared_buffer(dev.device, KiB(4), "default buffer");
            memset(dev.default_buffer.contents, 0, KiB(4));
        }

        if (!dev.screen_renderer.create())
            return false;

        init_overlay_font_dirs();

        if (!dev.overlay_renderer.init(*this)) {
            LOG_WARN("Failed to initialize Metal overlay renderer, overlays will be disabled");
        }

        return init();
    }
}

void MetalState::late_init(const Config &cfg, const std::string_view game_id, MemState &mem) {
    this->mem = &mem;

    const bool use_high_accuracy = cfg.current_config.high_accuracy;

    // the color attachment is read directly in the fragment shader (framebuffer fetch on Apple GPUs)
    features.direct_fragcolor = true;
    features.support_shader_interlock = false;
    features.support_texture_barrier = false;
    // Metal has no scaled vertex formats, the conversion is done in the shader
    features.support_scaled_attribute_formats = false;
    features.support_rgb_attributes = true;
    features.enable_memory_mapping = false;
    mapping_method = MappingMethod::Disabled;
    supported_mapping_methods_mask = (1 << static_cast<int>(MappingMethod::Disabled));

    if (device->support_framebuffer_fetch)
        LOG_INFO("Using framebuffer fetch for programmable blending");
    else
        LOG_INFO("Using color attachment reads for programmable blending");

    // texture viewport is faster but not entirely accurate
    if (!use_high_accuracy) {
        LOG_INFO("The Metal renderer is using texture viewport for better performance");
        features.use_texture_viewport = true;
    }

    if (!features.use_mask_bit)
        LOG_WARN("The GPU does not support read-write rgba8 textures, mask emulation is disabled");

    device->pipeline_cache.init();
    device->texture_cache.init(true, texture_folder(), game_id);
}

void MetalState::cleanup() {
    if (!device)
        return;

    @autoreleasepool {
        device->wait_idle();

        context = nullptr;

        device->pipeline_cache.cleanup();
        device->screen_renderer.cleanup();
        device->overlay_renderer.destroy();
        device->surface_cache.cleanup();
        device->texture_cache.cleanup();

        for (auto &frame : device->frames)
            frame.rendered_cmds.clear();

        device->default_texture = nil;
        device->default_sampler = nil;
        device->default_buffer = nil;
        device->queue = nil;
        device->device = nil;
        device.reset();
    }

    gxp_ptr_map.clear();
    shaders_cache_hashs.clear();
    last_scene_id = 0;
    shaders_count_compiled = 0;
    programs_count_pre_compiled = 0;
    should_display = false;
    render_abort = false;
}

TextureCache *MetalState::get_texture_cache() {
    return device ? &device->texture_cache : nullptr;
}

void MetalState::render_frame(DisplayState &display, const GxmState &gxm, MemState &mem) {
    // we are displaying this frame, wait for a new one
    should_display = false;

    DisplayFrameInfo frame;
    {
        std::lock_guard<std::mutex> guard(display.display_info_mutex);
        frame = display.next_rendered_frame;
    }

    update_overlays();
    bool has_overlays = false;
    if (overlay_manager) {
        overlay_manager->lock_shared();
        has_overlays = overlay_manager->has_visible();
        overlay_manager->unlock_shared();
    }

    if (!frame.base && !has_overlays)
        return;

    @autoreleasepool {
        ScreenRenderer &screen = device->screen_renderer;
        if (!screen.acquire_drawable())
            return;

        // store viewport for touch
        {
            const float fb_w = static_cast<float>(screen.width);
            const float fb_h = static_cast<float>(screen.height);
            display.viewport_drawable_w = static_cast<int>(fb_w);
            display.viewport_drawable_h = static_cast<int>(fb_h);
            if (fb_h > 0.0f) {
                const float window_aspect = fb_w / fb_h;
                constexpr float vita_aspect = static_cast<float>(DEFAULT_RES_WIDTH) / DEFAULT_RES_HEIGHT;
                const bool pixel_perfect = fullscreen_hd_res_pixel_perfect && fullscreen
                    && !(screen.width % DEFAULT_RES_WIDTH)
                    && !(screen.height % (DEFAULT_RES_HEIGHT - 4));
                if (stretch_the_display_area && !pixel_perfect) {
                    display.viewport_x = 0.0f;
                    display.viewport_y = 0.0f;
                    display.viewport_w = fb_w;
                    display.viewport_h = fb_h;
                } else if ((window_aspect > vita_aspect) && !pixel_perfect) {
                    display.viewport_w = fb_h * vita_aspect;
                    display.viewport_h = fb_h;
                    display.viewport_x = (fb_w - display.viewport_w) / 2.0f;
                    display.viewport_y = 0.0f;
                } else {
                    display.viewport_w = fb_w;
                    display.viewport_h = fb_w / vita_aspect;
                    display.viewport_x = 0.0f;
                    display.viewport_y = (fb_h - display.viewport_h) / 2.0f;
                }
            }
        }

        if (has_overlays && screen.current_cmd_buffer) {
            device->overlay_renderer.prepare(screen.current_cmd_buffer, *overlay_manager,
                display.viewport_w, display.viewport_h, screen.current_slot);
        }

        if (frame.base) {
            Viewport viewport;
            viewport.width = static_cast<uint32_t>(frame.image_size.x * res_multiplier);
            viewport.height = static_cast<uint32_t>(frame.image_size.y * res_multiplier);

            id<MTLTexture> surface = device->surface_cache.sourcing_color_surface_for_presentation(frame.base, frame.pitch, viewport);

            if (!surface) {
                const uint32_t width = static_cast<uint32_t>(frame.image_size.x);
                const uint32_t height = static_cast<uint32_t>(frame.image_size.y);
                if (width > 0 && height > 0 && frame.pitch >= width) {
                    id<MTLTexture> __strong &vita_surface = screen.vita_surface[screen.current_slot];
                    if (!vita_surface || vita_surface.width != width || vita_surface.height != height) {
                        // re-create the texture
                        vita_surface = create_texture(device->device, MTLPixelFormatRGBA8Unorm, width, height, MTLTextureUsageShaderRead,
                            1, false, 1, MTLStorageModeManaged, "vita surface");
                    }

                    if (vita_surface) {
                        // the previous use of this texture is done, see acquire_drawable
                        [vita_surface replaceRegion:MTLRegionMake2D(0, 0, width, height)
                                        mipmapLevel:0
                                          withBytes:frame.base.get(mem)
                                        bytesPerRow:frame.pitch * 4];
                        surface = vita_surface;
                        viewport = {
                            .offset_x = 0,
                            .offset_y = 0,
                            .width = width,
                            .height = height,
                            .texture_width = width,
                            .texture_height = height
                        };
                    }
                }
            }

            if (surface)
                screen.render(surface, viewport);
            else
                screen.begin_render_pass(true);
        } else if (has_overlays) {
            screen.begin_render_pass(true);
        }

        if (has_overlays && screen.current_encoder) {
            device->overlay_renderer.render(screen.current_encoder,
                display.viewport_x, display.viewport_y,
                display.viewport_w, display.viewport_h);
        }
    }
}

void MetalState::swap_window() {
    @autoreleasepool {
        device->screen_renderer.swap_window();
    }

    // look once a frame if we need to save the shader cache manifest
    const auto time_s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (static_cast<uint64_t>(time_s) >= device->pipeline_cache.next_pipeline_cache_save) {
        device->pipeline_cache.save_pipeline_cache();
        device->pipeline_cache.next_pipeline_cache_save = std::numeric_limits<uint64_t>::max();
    }
}

std::vector<uint32_t> MetalState::dump_frame(DisplayState &display, uint32_t &width, uint32_t &height) {
    DisplayFrameInfo frame;
    {
        std::lock_guard<std::mutex> guard(display.display_info_mutex);
        frame = display.next_rendered_frame;
    }

    width = static_cast<uint32_t>(frame.image_size.x * res_multiplier);
    height = static_cast<uint32_t>(frame.image_size.y * res_multiplier);
    @autoreleasepool {
        return device->surface_cache.dump_frame(frame.base, width, height, frame.pitch);
    }
}

uint32_t MetalState::get_features_mask() {
    union {
        struct {
            bool use_texture_viewport : 1;
            bool use_mask_bit : 1;
            bool use_framebuffer_fetch : 1;
        };
        uint32_t value;
    } features_mask;
    static_assert(sizeof(features_mask) == sizeof(uint32_t));

    features_mask.value = 0;
    features_mask.use_texture_viewport = features.use_texture_viewport;
    features_mask.use_mask_bit = features.use_mask_bit;
    features_mask.use_framebuffer_fetch = device && device->support_framebuffer_fetch;

    return features_mask.value;
}

int MetalState::get_supported_filters() {
    return static_cast<int>(Filter::NEAREST) | static_cast<int>(Filter::BILINEAR) | static_cast<int>(Filter::BICUBIC)
        | static_cast<int>(Filter::FXAA) | static_cast<int>(Filter::FSR);
}

void MetalState::set_screen_filter(const std::string_view &filter) {
    renderer::send_single_command(*this, nullptr, renderer::CommandOpcode::SetScreenFilter, false, new std::string(filter));
}

void apply_screen_filter(MetalState &state, std::string_view filter) {
    @autoreleasepool {
        state.device->screen_renderer.set_filter(filter);
    }
}

int MetalState::get_max_anisotropic_filtering() {
    return device ? static_cast<int>(device->max_anisotropy) : 1;
}

void MetalState::set_anisotropic_filtering(int anisotropic_filtering) {
    device->texture_cache.anisotropic_filtering = anisotropic_filtering;
}

int MetalState::get_max_2d_texture_width() {
    return device ? static_cast<int>(device->max_texture_size) : 8192;
}

void MetalState::set_async_compilation(bool enable) {
    device->pipeline_cache.set_async_compilation(enable);
}

std::string_view MetalState::get_gpu_name() {
    return gpu_name;
}

uint32_t MetalState::get_gpu_version() {
    return 0;
}

void MetalState::precompile_shader(const ShadersHash &hash) {
    Sha256Hash empty_hash{};
    @autoreleasepool {
        if (hash.vert != empty_hash)
            device->pipeline_cache.precompile_shader(hash.vert);
        if (hash.frag != empty_hash)
            device->pipeline_cache.precompile_shader(hash.frag);
    }

    programs_count_pre_compiled++;
    LOG_INFO("Program Compiled {}/{}", programs_count_pre_compiled, shaders_cache_hashs.size());
}

void MetalState::preclose_action() {
    // make sure we are in a game
    if (shaders_path.empty() || !device)
        return;

    device->pipeline_cache.save_pipeline_cache();
}

bool create(std::unique_ptr<renderer::State> &state, const Config &config) {
    auto &metal_state = dynamic_cast<MetalState &>(*state);

    try {
        return metal_state.create(state, config);
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to initialise the Metal renderer: {}", e.what());
    }
    return false;
}

bool create(std::unique_ptr<VertexProgram> &vp, MetalState &state, const SceGxmProgram &program) {
    vp = std::make_unique<VertexProgram>();
    return true;
}

bool create(std::unique_ptr<FragmentProgram> &fp, MetalState &state, const SceGxmProgram &program, const SceGxmBlendInfo *blend) {
    fp = std::make_unique<MetalFragmentProgram>();

    MetalFragmentProgram *fp_mtl = static_cast<MetalFragmentProgram *>(fp.get());

    // Translate blending.
    // programs using native color can't use traditional blending
    if (blend != nullptr && !program.is_native_color()) {
        uint32_t color_mask = MTLColorWriteMaskNone;
        if (blend->colorMask & SCE_GXM_COLOR_MASK_R)
            color_mask |= MTLColorWriteMaskRed;
        if (blend->colorMask & SCE_GXM_COLOR_MASK_G)
            color_mask |= MTLColorWriteMaskGreen;
        if (blend->colorMask & SCE_GXM_COLOR_MASK_B)
            color_mask |= MTLColorWriteMaskBlue;
        if (blend->colorMask & SCE_GXM_COLOR_MASK_A)
            color_mask |= MTLColorWriteMaskAlpha;

        fp_mtl->blending = BlendState{
            .enable = (blend->colorFunc != SCE_GXM_BLEND_FUNC_NONE) || (blend->alphaFunc != SCE_GXM_BLEND_FUNC_NONE),
            .src_color = static_cast<uint32_t>(translate_blend_factor(blend->colorSrc)),
            .dst_color = static_cast<uint32_t>(translate_blend_factor(blend->colorDst)),
            .color_op = static_cast<uint32_t>(translate_blend_func(blend->colorFunc)),
            .src_alpha = static_cast<uint32_t>(translate_blend_factor(blend->alphaSrc)),
            .dst_alpha = static_cast<uint32_t>(translate_blend_factor(blend->alphaDst)),
            .alpha_op = static_cast<uint32_t>(translate_blend_func(blend->alphaFunc)),
            .write_mask = color_mask
        };
    } else {
        fp_mtl->blending = BlendState{
            .enable = 0,
            .src_color = MTLBlendFactorOne,
            .dst_color = MTLBlendFactorZero,
            .color_op = MTLBlendOperationAdd,
            .src_alpha = MTLBlendFactorOne,
            .dst_alpha = MTLBlendFactorZero,
            .alpha_op = MTLBlendOperationAdd,
            .write_mask = MTLColorWriteMaskAll
        };
    }

    // compute blending hash, as it will be used for the pipeline hash
    fp_mtl->blending_hash = XXH_INLINE_XXH3_64bits(&fp_mtl->blending, sizeof(BlendState));
    return true;
}

} // namespace renderer::metal
