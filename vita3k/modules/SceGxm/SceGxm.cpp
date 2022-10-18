// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include "SceGxm.h"

#include <modules/module_parent.h>

#include <xxh3.h>

#include <gxm/functions.h>
#include <gxm/state.h>
#include <gxm/types.h>
#include <immintrin.h>
#include <kernel/state.h>
#include <mem/state.h>

#include <SDL.h>
#include <io/state.h>
#include <mem/allocator.h>
#include <mem/mempool.h>
#include <renderer/functions.h>
#include <renderer/state.h>
#include <renderer/types.h>
#include <util/bytes.h>
#include <util/lock_and_find.h>
#include <util/log.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceGxm);

static Ptr<void> gxmRunDeferredMemoryCallback(KernelState &kernel, const MemState &mem, std::mutex &global_lock, std::uint32_t &return_size, Ptr<SceGxmDeferredContextCallback> callback, Ptr<void> userdata,
    const std::uint32_t size, const SceUID thread_id) {
    const std::lock_guard<std::mutex> guard(global_lock);

    const ThreadStatePtr thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);
    const Address final_size_addr = stack_alloc(*thread->cpu, 4);

    Ptr<void> result(static_cast<Address>(thread->run_callback(callback.address(),
        { userdata.address(), size, final_size_addr })));

    return_size = *Ptr<std::uint32_t>(final_size_addr).get(mem);
    stack_free(*thread->cpu, 4);

    return result;
}

struct SceGxmCommandList;

// Represent the data range in the vita memory that is taken by a command list
// If the command list goes through multiple vdm buffers there will be multiple command ranges
struct CommandListRange {
    Address start;
    Address end;
    SceGxmCommandList *command_list;

    bool operator<(const CommandListRange &other) const {
        // the ranges are all disjoints, so no problem for ordering
        return start < other.start;
    }
};

typedef std::set<CommandListRange>::iterator RangeIterator;

struct SceGxmCommandList {
    renderer::CommandList *list;

    // the locations on the vita memory that correspond to this command list
    // this part is not copied in the command list given to the game by endCommandList
    std::stack<RangeIterator> memory_ranges;
};

// Seems on real vita, this is the maximum size, I got stack corrupt if try to write more
static_assert(sizeof(SceGxmCommandList) - sizeof(std::stack<CommandListRange>) <= 32);

struct SceGxmContext {
    GxmContextState state;

    std::unique_ptr<renderer::Context> renderer;

    std::mutex lock;
    std::mutex &callback_lock;

    uint8_t *alloc_space = nullptr;
    uint8_t *alloc_space_end = nullptr;

    BitmapAllocator command_allocator;
    bool last_precomputed = false;

    // this is used for deferred contexts
    uint8_t *alloc_space_start = nullptr;
    std::set<CommandListRange> command_list_ranges;
    SceGxmCommandList *curr_command_list = nullptr;

    // tell if a call to set_texture must be made
    gxp::TextureInfo is_vert_texture_dirty;
    gxp::TextureInfo is_frag_texture_dirty;

    explicit SceGxmContext(std::mutex &callback_lock_)
        : callback_lock(callback_lock_) {
    }

    void reset_recording() {
        last_precomputed = false;
    }

    void free_command_list(SceGxmCommandList *command_list) {
        // command list has been overwritten, free the memory
        // everything except the command_list except was allocated using malloc
        renderer::Command *cmd = command_list->list->first;
        while (cmd != command_list->list->last) {
            renderer::Command *next = cmd->next;
            memset(cmd, 0, sizeof(renderer::Command));
            free(cmd);
            cmd = next;
        }
        free(cmd);
        free(command_list->list);

        // we also need to delete all ranges occupied by this list
        while (!command_list->memory_ranges.empty()) {
            command_list_ranges.erase(command_list->memory_ranges.top());
            command_list->memory_ranges.pop();
        }

        delete command_list;
    }

    // check if there are any command list which have been overwritten, meaning we can free their content
    void deferred_check_for_free(const CommandListRange &new_range) {
        // delete all command list that overlaps
        RangeIterator it = command_list_ranges.upper_bound(new_range);
        if (it != command_list_ranges.begin()) {
            // get the first element before
            it--;
            if (it->end <= new_range.start) {
                it++;
            }
        }

        // now we can go from left to right and stop when it doesn't overlap anymore;
        while (it != command_list_ranges.end() && it->start < new_range.end) {
            RangeIterator to_delete = it;
            // we must keep it not invalidated
            while (it != command_list_ranges.end() && it->command_list == to_delete->command_list)
                it++;

            free_command_list(to_delete->command_list);
        }
    }

    // insert new memory range used by a command list
    void insert_new_memory_range(const MemState &mem) {
        CommandListRange range = {
            Ptr<void>(alloc_space_start, mem).address(),
            Ptr<void>(alloc_space, mem).address(),
            curr_command_list
        };
        deferred_check_for_free(range);
        RangeIterator it = command_list_ranges.emplace(std::move(range)).first;
        curr_command_list->memory_ranges.push(it);
    }

    bool make_new_alloc_space(KernelState &kern, const MemState &mem, const SceUID thread_id, bool force = false) {
        if (alloc_space && (state.type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE)) {
            return false;
        }

        if (!force && alloc_space && alloc_space < alloc_space_end) {
            // we already have spare space
            return true;
        }

        if (state.active && state.type == SCE_GXM_CONTEXT_TYPE_DEFERRED) {
            // update memory ranges
            insert_new_memory_range(mem);
        }

        std::uint32_t actual_size = 0;

        if (state.vdm_buffer && state.vdm_buffer_size > 0) {
            alloc_space = state.vdm_buffer.cast<std::uint8_t>().get(mem);
            actual_size = state.vdm_buffer_size;

            if (state.type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
                command_allocator.reset();
                command_allocator.set_maximum(state.vdm_buffer_size / sizeof(renderer::Command));
            } else {
                // setting the vdm buffer size to 0 means we are using it
                state.vdm_buffer_size = 0;
            }
        } else {
            static constexpr std::uint32_t DEFAULT_SIZE = 1024;

            Ptr<void> space = gxmRunDeferredMemoryCallback(kern, mem, callback_lock, actual_size, state.vdm_memory_callback,
                state.memory_callback_userdata, DEFAULT_SIZE, thread_id);

            if (!space) {
                LOG_ERROR("VDM callback runs out of memory!");
                return false;
            }

            alloc_space = space.cast<std::uint8_t>().get(mem);
        }

        alloc_space_start = alloc_space;
        alloc_space_end = alloc_space + actual_size;
        return true;
    }

    std::uint8_t *linearly_allocate(KernelState &kern, const MemState &mem, const SceUID thread_id, const std::uint32_t size) {
        if (state.type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {
            return nullptr;
        }

        // allocate 8 bytes in the vdm memory to make it look like the vdm buffer is getting used
        // otherwise we would never know when to free our command lists
        constexpr uint32_t allocated_on_vdm = 8;

        if (alloc_space + allocated_on_vdm > alloc_space_end) {
            if (!make_new_alloc_space(kern, mem, thread_id, true)) {
                return nullptr;
            }
        }

        alloc_space += allocated_on_vdm;

        // the data returned is not part of the vita memory (our commands are too big and do not fit)
        return reinterpret_cast<uint8_t *>(malloc(size));
    }

    template <typename T>
    T *linearly_allocate(KernelState &kern, const MemState &mem, const SceUID thread_id) {
        return reinterpret_cast<T *>(linearly_allocate(kern, mem, thread_id, sizeof(T)));
    }

    renderer::Command *allocate_new_command(KernelState &kern, const MemState &mem, SceUID current_thread_id) {
        const std::lock_guard<std::mutex> guard(lock);
        renderer::Command *new_command = nullptr;

        if (state.type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
            int size = 1;

            int offset = command_allocator.allocate_from(0, size);

            if (offset < 0) {
                new_command = new renderer::Command;
                new_command->flags |= renderer::Command::FLAG_FROM_HOST;
            } else {
                new_command = reinterpret_cast<renderer::Command *>(alloc_space) + offset;
                new (new_command) renderer::Command;
            }
        } else {
            new_command = linearly_allocate<renderer::Command>(kern, mem, current_thread_id);

            new (new_command) renderer::Command;
            new_command->flags |= renderer::Command::FLAG_NO_FREE;
        }

        return new_command;
    }

    void free_new_command(renderer::Command *cmd) {
        if (!(cmd->flags & renderer::Command::FLAG_NO_FREE)) {
            if (cmd->flags & renderer::Command::FLAG_FROM_HOST) {
                delete cmd;
            } else {
                const std::lock_guard<std::mutex> guard(lock);

                const std::uint32_t offset = static_cast<std::uint32_t>(cmd - reinterpret_cast<renderer::Command *>(alloc_space));
                command_allocator.free(offset, 1);
            }
        }
    }
};

struct SceGxmRenderTarget {
    std::unique_ptr<renderer::RenderTarget> renderer;
    std::uint16_t width;
    std::uint16_t height;
    std::uint16_t scenesPerFrame;
    SceUID driverMemBlock;
};

typedef std::uint32_t VertexCacheHash;

struct VertexProgramCacheKey {
    SceGxmRegisteredProgram vertex_program;
    VertexCacheHash hash;
};

typedef std::map<VertexProgramCacheKey, Ptr<SceGxmVertexProgram>> VertexProgramCache;

struct FragmentProgramCacheKey {
    SceGxmRegisteredProgram fragment_program;
    SceGxmBlendInfo blend_info;
};

typedef std::map<FragmentProgramCacheKey, Ptr<SceGxmFragmentProgram>> FragmentProgramCache;

struct SceGxmShaderPatcher {
    VertexProgramCache vertex_program_cache;
    FragmentProgramCache fragment_program_cache;
    SceGxmShaderPatcherParams params;
};

// clang-format off
static const size_t size_mask_gxp = 228;
static const uint8_t mask_gxp[] = {
    0x47, 0x58, 0x50, 0x00, 0x01, 0x05, 0x50, 0x03, 0xE1, 0x00, 0x00, 0x00, 0xF6, 0x94, 0xF3, 0x74, 
	0x73, 0x30, 0xEE, 0xE2, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xA4, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 
	0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 
	0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x5C, 0x00, 0x00, 0x00, 0xC0, 0x3D, 0x03, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 
	0x01, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x44, 0xFA, 0x00, 0x00, 0x00, 0xE0, 
	0x7E, 0x0D, 0x81, 0x40, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x10, 0x00, 0x00, 0x00, 
	0x01, 0xE1, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6D, 0x61, 0x73, 0x6B, 
	0x00, 0x00, 0x00, 0x00, 
};
// clang-format on

static constexpr std::uint32_t DEFAULT_RING_SIZE = 4096;

static VertexCacheHash hash_data(const void *data, size_t size) {
    auto hash = XXH_INLINE_XXH3_64bits(data, size);
    return VertexCacheHash(hash);
}

static bool operator<(const SceGxmRegisteredProgram &a, const SceGxmRegisteredProgram &b) {
    return a.program < b.program;
}

static bool operator<(const VertexProgramCacheKey &a, const VertexProgramCacheKey &b) {
    if (a.vertex_program < b.vertex_program) {
        return true;
    }
    if (b.vertex_program < a.vertex_program) {
        return false;
    }
    return b.hash < a.hash;
}

static bool operator<(const SceGxmBlendInfo &a, const SceGxmBlendInfo &b) {
    return memcmp(&a, &b, sizeof(a)) < 0;
}

static bool operator<(const FragmentProgramCacheKey &a, const FragmentProgramCacheKey &b) {
    if (a.fragment_program < b.fragment_program) {
        return true;
    }
    if (b.fragment_program < a.fragment_program) {
        return false;
    }
    return b.blend_info < a.blend_info;
}

static int init_texture_base(const char *export_name, SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat tex_format, uint32_t width, uint32_t height, uint32_t mipCount,
    const SceGxmTextureType &texture_type) {
    if (width > 4096 || height > 4096 || mipCount > 13) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }
    // data can be empty to be filled out later.

    texture->mip_count = std::min<std::uint32_t>(15, mipCount - 1);
    texture->format0 = (tex_format & 0x80000000) >> 31;
    texture->lod_bias = 31;
    texture->gamma_mode = 0;

    if ((texture_type == SCE_GXM_TEXTURE_SWIZZLED) || (texture_type == SCE_GXM_TEXTURE_CUBE)) {
        // Find highest set bit of width and height. It's also the 2^? for width and height
        static auto highest_set_bit = [](const std::uint32_t num) -> std::uint32_t {
            for (std::int32_t i = 12; i >= 0; i--) {
                if (num & (1 << i)) {
                    return static_cast<std::uint32_t>(i);
                }
            }

            return 0;
        };

        texture->uaddr_mode = texture->vaddr_mode = SCE_GXM_TEXTURE_ADDR_MIRROR;
        texture->height_base2 = highest_set_bit(height);
        texture->width_base2 = highest_set_bit(width);
    } else {
        texture->uaddr_mode = texture->vaddr_mode = SCE_GXM_TEXTURE_ADDR_CLAMP;
        texture->height = height - 1;
        texture->width = width - 1;
    }

    texture->base_format = (tex_format & 0x1F000000) >> 24;
    texture->type = texture_type >> 29;
    texture->data_addr = data.address() >> 2;
    texture->swizzle_format = (tex_format & 0x7000) >> 12;
    texture->normalize_mode = 1;
    texture->min_filter = SCE_GXM_TEXTURE_FILTER_POINT;
    texture->mag_filter = SCE_GXM_TEXTURE_FILTER_POINT;
    texture->lod_min0 = 0;
    texture->lod_min1 = 0;

    return 0;
}

EXPORT(int, _sceGxmBeginScene) {
    TRACY_FUNC(_sceGxmBeginScene);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmAddRazorGpuCaptureBuffer) {
    TRACY_FUNC(sceGxmAddRazorGpuCaptureBuffer);
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetDefaultRegionClipAndViewport, SceGxmContext *context, uint32_t xMax, uint32_t yMax) {
    TRACY_FUNC(sceGxmSetDefaultRegionClipAndViewport, context, xMax, yMax);
    const std::uint32_t xMin = 0;
    const std::uint32_t yMin = 0;

    context->state.viewport.offset.x = 0.5f * static_cast<float>(1.0f + xMin + xMax);
    context->state.viewport.offset.y = 0.5f * (static_cast<float>(1.0 + yMin + yMax));
    context->state.viewport.offset.z = 0.5f;
    context->state.viewport.scale.x = 0.5f * static_cast<float>(1.0f + xMax - xMin);
    context->state.viewport.scale.y = -0.5f * static_cast<float>(1.0f + yMax - yMin);
    context->state.viewport.scale.z = 0.5f;

    context->state.region_clip_min.x = xMin;
    context->state.region_clip_max.x = xMax;
    context->state.region_clip_min.y = yMin;
    context->state.region_clip_max.y = yMax;
    context->state.region_clip_mode = SCE_GXM_REGION_CLIP_OUTSIDE;

    if (context->alloc_space) {
        // Set default region clip and viewport
        renderer::set_region_clip(*emuenv.renderer, context->renderer.get(), SCE_GXM_REGION_CLIP_OUTSIDE,
            xMin, xMax, yMin, yMax);

        if (context->state.viewport.enable == SCE_GXM_VIEWPORT_ENABLED) {
            renderer::set_viewport_real(*emuenv.renderer, context->renderer.get(), context->state.viewport.offset.x,
                context->state.viewport.offset.y, context->state.viewport.offset.z, context->state.viewport.scale.x,
                context->state.viewport.scale.y, context->state.viewport.scale.z);
        } else {
            renderer::set_viewport_flat(*emuenv.renderer, context->renderer.get());
        }
    }
}

static void gxmContextStateRestore(renderer::State &state, MemState &mem, SceGxmContext *context, const bool sync_viewport_and_clip) {
    if (sync_viewport_and_clip) {
        renderer::set_region_clip(state, context->renderer.get(), SCE_GXM_REGION_CLIP_OUTSIDE,
            context->state.region_clip_min.x, context->state.region_clip_max.x, context->state.region_clip_min.y,
            context->state.region_clip_max.y);

        if (context->state.viewport.enable == SCE_GXM_VIEWPORT_ENABLED) {
            renderer::set_viewport_real(state, context->renderer.get(), context->state.viewport.offset.x,
                context->state.viewport.offset.y, context->state.viewport.offset.z, context->state.viewport.scale.x,
                context->state.viewport.scale.y, context->state.viewport.scale.z);
        } else {
            renderer::set_viewport_flat(state, context->renderer.get());
        }
    }

    renderer::set_cull_mode(state, context->renderer.get(), context->state.cull_mode);
    renderer::set_depth_bias(state, context->renderer.get(), true, context->state.front_depth_bias_factor, context->state.front_depth_bias_units);
    renderer::set_depth_bias(state, context->renderer.get(), false, context->state.back_depth_bias_factor, context->state.back_depth_bias_units);
    renderer::set_depth_func(state, context->renderer.get(), true, context->state.front_depth_func);
    renderer::set_depth_func(state, context->renderer.get(), false, context->state.back_depth_func);
    renderer::set_depth_write_enable_mode(state, context->renderer.get(), true, context->state.front_depth_write_enable);
    renderer::set_depth_write_enable_mode(state, context->renderer.get(), false, context->state.back_depth_write_enable);
    renderer::set_point_line_width(state, context->renderer.get(), true, context->state.front_point_line_width);
    renderer::set_point_line_width(state, context->renderer.get(), false, context->state.back_point_line_width);
    renderer::set_polygon_mode(state, context->renderer.get(), true, context->state.front_polygon_mode);
    renderer::set_polygon_mode(state, context->renderer.get(), false, context->state.back_polygon_mode);
    renderer::set_two_sided_enable(state, context->renderer.get(), context->state.two_sided);
    renderer::set_stencil_func(state, context->renderer.get(), true, context->state.front_stencil.func, context->state.front_stencil.stencil_fail,
        context->state.front_stencil.depth_fail, context->state.front_stencil.depth_pass, context->state.front_stencil.compare_mask,
        context->state.front_stencil.write_mask);
    renderer::set_stencil_func(state, context->renderer.get(), false, context->state.back_stencil.func, context->state.back_stencil.stencil_fail,
        context->state.back_stencil.depth_fail, context->state.back_stencil.depth_pass, context->state.back_stencil.compare_mask,
        context->state.back_stencil.write_mask);
    renderer::set_stencil_ref(state, context->renderer.get(), true, context->state.front_stencil.ref);
    renderer::set_stencil_ref(state, context->renderer.get(), false, context->state.back_stencil.ref);

    if (context->state.vertex_program) {
        renderer::set_program(state, context->renderer.get(), context->state.vertex_program, false);

        context->is_vert_texture_dirty.set();
    }

    // The uniform buffer, vertex stream will be uploaded later, for now only need to resync de textures
    if (context->state.fragment_program) {
        renderer::set_program(state, context->renderer.get(), context->state.fragment_program, true);

        context->is_frag_texture_dirty.set();
    }
}

EXPORT(int, sceGxmBeginCommandList, SceGxmContext *deferredContext) {
    TRACY_FUNC(sceGxmBeginCommandList, deferredContext);
    if (!deferredContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (deferredContext->state.type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (deferredContext->state.active) {
        return RET_ERROR(SCE_GXM_ERROR_WITHIN_COMMAND_LIST);
    }

    deferredContext->state.fragment_ring_buffer_used = 0;
    deferredContext->state.vertex_ring_buffer_used = 0;

    deferredContext->curr_command_list = new SceGxmCommandList();

    if (!deferredContext->make_new_alloc_space(emuenv.kernel, emuenv.mem, thread_id)) {
        return RET_ERROR(SCE_GXM_ERROR_RESERVE_FAILED);
    }

    // in case the same vdm buffer was used for two consecutive command lists
    deferredContext->alloc_space_start = deferredContext->alloc_space;

    if (!deferredContext->state.vertex_ring_buffer) {
        deferredContext->state.vertex_ring_buffer = gxmRunDeferredMemoryCallback(emuenv.kernel, emuenv.mem, emuenv.gxm.callback_lock, deferredContext->state.vertex_ring_buffer_size,
            deferredContext->state.vertex_memory_callback, deferredContext->state.memory_callback_userdata, DEFAULT_RING_SIZE, thread_id);

        if (!deferredContext->state.vertex_ring_buffer) {
            return RET_ERROR(SCE_GXM_ERROR_RESERVE_FAILED);
        }
    }

    if (!deferredContext->state.fragment_ring_buffer) {
        deferredContext->state.fragment_ring_buffer = gxmRunDeferredMemoryCallback(emuenv.kernel, emuenv.mem, emuenv.gxm.callback_lock, deferredContext->state.fragment_ring_buffer_size,
            deferredContext->state.fragment_memory_callback, deferredContext->state.memory_callback_userdata, DEFAULT_RING_SIZE, thread_id);

        if (!deferredContext->state.fragment_ring_buffer) {
            return RET_ERROR(SCE_GXM_ERROR_RESERVE_FAILED);
        }
    }

    // Set command allocate functions
    KernelState *kernel = &emuenv.kernel;
    MemState *mem = &emuenv.mem;

    deferredContext->renderer->alloc_func = [deferredContext, kernel, mem, thread_id]() {
        return deferredContext->allocate_new_command(*kernel, *mem, thread_id);
    };

    deferredContext->renderer->free_func = [](renderer::Command *cmd) {
        // do not delete here, commands will be deleted when they are overwritten
    };

    // Begin the command list by white washing previous command list, and restoring deferred state
    renderer::reset_command_list(deferredContext->renderer->command_list);
    gxmContextStateRestore(*emuenv.renderer, emuenv.mem, deferredContext, false);

    deferredContext->state.active = true;

    return 0;
}

EXPORT(int, sceGxmBeginScene, SceGxmContext *context, uint32_t flags, const SceGxmRenderTarget *renderTarget, const SceGxmValidRegion *validRegion, SceGxmSyncObject *vertexSyncObject, Ptr<SceGxmSyncObject> fragmentSyncObject, const SceGxmColorSurface *colorSurface, const SceGxmDepthStencilSurface *depthStencil) {
    TRACY_FUNC(sceGxmBeginScene, context, flags, renderTarget, validRegion, vertexSyncObject, fragmentSyncObject, colorSurface, depthStencil);
    if (!context) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (flags & 0xFFFFFFF0) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!renderTarget || (vertexSyncObject != nullptr)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (context->state.type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (context->state.active) {
        return RET_ERROR(SCE_GXM_ERROR_WITHIN_SCENE);
    }

    context->state.fragment_sync_object = fragmentSyncObject;

    if (fragmentSyncObject) {
        SceGxmSyncObject *sync = fragmentSyncObject.get(emuenv.mem);

        // Wait for the display queue to be done.
        // If it's offline render, the sync object already has the display queue subject done, so don't worry.
        renderer::add_command(context->renderer.get(), renderer::CommandOpcode::WaitSyncObject,
            nullptr, fragmentSyncObject, renderTarget->renderer.get(), sync->last_display);
    }

    // It's legal to set at client.
    context->state.active = true;
    context->last_precomputed = false;

    // set all textures as dirty, in case their content is modified (even though I really don't think it would)
    // or some texture is the current framebuffer
    context->is_vert_texture_dirty.set();
    context->is_frag_texture_dirty.set();

    SceGxmColorSurface *color_surface_copy = nullptr;
    SceGxmDepthStencilSurface *depth_stencil_surface_copy = nullptr;

    if (colorSurface) {
        color_surface_copy = new SceGxmColorSurface;
        *color_surface_copy = *colorSurface;
    }

    if (depthStencil) {
        depth_stencil_surface_copy = new SceGxmDepthStencilSurface;
        *depth_stencil_surface_copy = *depthStencil;
    }

    renderer::set_context(*emuenv.renderer, context->renderer.get(), renderTarget->renderer.get(), color_surface_copy,
        depth_stencil_surface_copy);

    const std::uint32_t xmax = (validRegion ? validRegion->xMax : renderTarget->width - 1);
    const std::uint32_t ymax = (validRegion ? validRegion->yMax : renderTarget->height - 1);

    CALL_EXPORT(sceGxmSetDefaultRegionClipAndViewport, context, xmax, ymax);
    return 0;
}

EXPORT(int, sceGxmBeginSceneEx, SceGxmContext *immediateContext, uint32_t flags, const SceGxmRenderTarget *renderTarget, const SceGxmValidRegion *validRegion, SceGxmSyncObject *vertexSyncObject, Ptr<SceGxmSyncObject> fragmentSyncObject, const SceGxmColorSurface *colorSurface, const SceGxmDepthStencilSurface *loadDepthStencilSurface, const SceGxmDepthStencilSurface *storeDepthStencilSurface) {
    TRACY_FUNC(sceGxmBeginSceneEx, immediateContext, flags, renderTarget, validRegion, vertexSyncObject, fragmentSyncObject, colorSurface, loadDepthStencilSurface, storeDepthStencilSurface);
    // storeDepthStencilSurface
    if (!immediateContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (flags & 0xFFFFFFF0) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!renderTarget || (vertexSyncObject != nullptr)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (immediateContext->state.type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (immediateContext->state.active) {
        return RET_ERROR(SCE_GXM_ERROR_WITHIN_SCENE);
    }

    STUBBED("Using sceGxmBeginScene");
    return CALL_EXPORT(sceGxmBeginScene, immediateContext, flags, renderTarget, validRegion, vertexSyncObject, fragmentSyncObject, colorSurface, loadDepthStencilSurface);
}

EXPORT(void, sceGxmColorSurfaceGetClip, const SceGxmColorSurface *surface, uint32_t *xMin, uint32_t *yMin, uint32_t *xMax, uint32_t *yMax) {
    TRACY_FUNC(sceGxmColorSurfaceGetClip, surface, xMin, yMin, xMax, yMax);
    assert(surface);
    UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceGxmColorSurfaceGetData, const SceGxmColorSurface *surface) {
    TRACY_FUNC(sceGxmColorSurfaceGetData, surface);
    assert(surface);
    return surface->data;
}

EXPORT(SceGxmColorSurfaceDitherMode, sceGxmColorSurfaceGetDitherMode, const SceGxmColorSurface *surface) {
    TRACY_FUNC(sceGxmColorSurfaceGetDitherMode, surface);
    assert(surface);
    STUBBED("SCE_GXM_COLOR_SURFACE_DITHER_DISABLED");
    return SceGxmColorSurfaceDitherMode::SCE_GXM_COLOR_SURFACE_DITHER_DISABLED;
}

EXPORT(SceGxmColorFormat, sceGxmColorSurfaceGetFormat, const SceGxmColorSurface *surface) {
    TRACY_FUNC(sceGxmColorSurfaceGetFormat, surface);
    assert(surface);
    return surface->colorFormat;
}

EXPORT(SceGxmColorSurfaceGammaMode, sceGxmColorSurfaceGetGammaMode, const SceGxmColorSurface *surface) {
    TRACY_FUNC(sceGxmColorSurfaceGetGammaMode, surface);
    assert(surface);
    return static_cast<SceGxmColorSurfaceGammaMode>(surface->gamma << 12);
}

EXPORT(SceGxmColorSurfaceScaleMode, sceGxmColorSurfaceGetScaleMode, const SceGxmColorSurface *surface) {
    TRACY_FUNC(sceGxmColorSurfaceGetScaleMode, surface);
    assert(surface);
    return surface->downscale ? SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE : SCE_GXM_COLOR_SURFACE_SCALE_NONE;
}

EXPORT(uint32_t, sceGxmColorSurfaceGetStrideInPixels, const SceGxmColorSurface *surface) {
    TRACY_FUNC(sceGxmColorSurfaceGetStrideInPixels, surface);
    assert(surface);
    return surface->strideInPixels;
}

EXPORT(SceGxmColorSurfaceType, sceGxmColorSurfaceGetType, const SceGxmColorSurface *surface) {
    TRACY_FUNC(sceGxmColorSurfaceGetType, surface);
    assert(surface);
    return surface->surfaceType;
}

EXPORT(int, sceGxmColorSurfaceInit, SceGxmColorSurface *surface, SceGxmColorFormat colorFormat, SceGxmColorSurfaceType surfaceType, SceGxmColorSurfaceScaleMode scaleMode, SceGxmOutputRegisterSize outputRegisterSize, uint32_t width, uint32_t height, uint32_t strideInPixels, Ptr<void> data) {
    TRACY_FUNC(sceGxmColorSurfaceInit, surface, colorFormat, surfaceType, scaleMode, outputRegisterSize, width, height, strideInPixels, data);
    if (!surface || !data)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (width > 4096 || height > 4096)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    if (strideInPixels & 1)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_ALIGNMENT);

    if ((strideInPixels < width) || ((data.address() & 3) != 0))
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    memset(surface, 0, sizeof(SceGxmColorSurface));
    surface->disabled = 0;
    surface->downscale = scaleMode == SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE;
    surface->width = width;
    surface->height = height;
    surface->strideInPixels = strideInPixels;
    surface->data = data;
    surface->colorFormat = colorFormat;
    surface->surfaceType = surfaceType;
    surface->outputRegisterSize = outputRegisterSize;

    SceGxmTextureFormat tex_format;
    if (!gxm::convert_color_format_to_texture_format(colorFormat, tex_format)) {
        LOG_WARN("Unable to convert color surface type 0x{:X} to texture format enum for background texture of color surface!", static_cast<std::uint32_t>(colorFormat));
    }

    // Create background object, for here don't return an error
    if (init_texture_base(export_name, &surface->backgroundTex, surface->data, tex_format, surface->width, surface->height, 1, SCE_GXM_TEXTURE_LINEAR) != SCE_KERNEL_OK) {
        LOG_WARN("Unable to initialize background object control texture!");
    }

    return 0;
}

EXPORT(int, sceGxmColorSurfaceInitDisabled, SceGxmColorSurface *surface) {
    TRACY_FUNC(sceGxmColorSurfaceInitDisabled, surface);
    if (!surface)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    surface->disabled = 1;
    return 0;
}

EXPORT(bool, sceGxmColorSurfaceIsEnabled, const SceGxmColorSurface *surface) {
    TRACY_FUNC(sceGxmColorSurfaceIsEnabled, surface);
    assert(surface);
    return !surface->disabled;
}

EXPORT(void, sceGxmColorSurfaceSetClip, SceGxmColorSurface *surface, uint32_t xMin, uint32_t yMin, uint32_t xMax, uint32_t yMax) {
    TRACY_FUNC(sceGxmColorSurfaceSetClip, surface, xMin, yMin, xMax, yMax);
    assert(surface);
    UNIMPLEMENTED();
}

EXPORT(int, sceGxmColorSurfaceSetData, SceGxmColorSurface *surface, Ptr<void> data) {
    TRACY_FUNC(sceGxmColorSurfaceSetData, surface, data);
    if (!surface) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (data.address() & 3) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    surface->data = data;
    surface->backgroundTex.data_addr = data.address() >> 2;

    return 0;
}

EXPORT(int, sceGxmColorSurfaceSetDitherMode, SceGxmColorSurface *surface, SceGxmColorSurfaceDitherMode ditherMode) {
    TRACY_FUNC(sceGxmColorSurfaceSetDitherMode, surface, ditherMode);
    if (!surface) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTextureSetFormat, SceGxmTexture *tex, SceGxmTextureFormat format);

EXPORT(int, sceGxmColorSurfaceSetFormat, SceGxmColorSurface *surface, SceGxmColorFormat format) {
    TRACY_FUNC(sceGxmColorSurfaceSetFormat, surface, format);
    if (!surface) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    surface->colorFormat = format;

    SceGxmTextureFormat tex_format;
    if (!gxm::convert_color_format_to_texture_format(format, tex_format)) {
        LOG_WARN("Unable to convert color surface type 0x{:X} to texture format enum for background texture of color surface!", static_cast<std::uint32_t>(format));
    }

    return CALL_EXPORT(sceGxmTextureSetFormat, &surface->backgroundTex, tex_format);
}

EXPORT(int, sceGxmColorSurfaceSetGammaMode, SceGxmColorSurface *surface, SceGxmColorSurfaceGammaMode gammaMode) {
    TRACY_FUNC(sceGxmColorSurfaceSetGammaMode, surface, gammaMode);
    if (!surface) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    surface->gamma = static_cast<uint32_t>(gammaMode) >> 12;

    return 0;
}

EXPORT(void, sceGxmColorSurfaceSetScaleMode, SceGxmColorSurface *surface, SceGxmColorSurfaceScaleMode scaleMode) {
    TRACY_FUNC(sceGxmColorSurfaceSetScaleMode, surface, scaleMode);
    assert(surface);
    UNIMPLEMENTED();
}

EXPORT(int, sceGxmCreateContext, const SceGxmContextParams *params, Ptr<SceGxmContext> *context) {
    TRACY_FUNC(sceGxmCreateContext, params, context);
    if (!params || !context)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (params->hostMemSize < sizeof(SceGxmContext)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    *context = params->hostMem.cast<SceGxmContext>();

    SceGxmContext *const ctx = context->get(emuenv.mem);
    new (ctx) SceGxmContext(emuenv.gxm.callback_lock);

    ctx->state.fragment_ring_buffer = params->fragmentRingBufferMem;
    ctx->state.vertex_ring_buffer = params->vertexRingBufferMem;
    ctx->state.fragment_ring_buffer_size = params->fragmentRingBufferMemSize;
    ctx->state.vertex_ring_buffer_size = params->vertexRingBufferMemSize;

    ctx->state.type = SCE_GXM_CONTEXT_TYPE_IMMEDIATE;

    if (!renderer::create_context(*emuenv.renderer, ctx->renderer)) {
        context->reset();
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }

    // Set VDM buffer space
    ctx->state.vdm_buffer = params->vdmRingBufferMem;
    ctx->state.vdm_buffer_size = params->vdmRingBufferMemSize;

    ctx->make_new_alloc_space(emuenv.kernel, emuenv.mem, thread_id);

    // Set command allocate functions
    // The command buffer will not be reallocated, so this is fine to use this thread ID
    KernelState *kernel = &emuenv.kernel;
    MemState *mem = &emuenv.mem;

    ctx->renderer->alloc_func = [ctx, kernel, mem, thread_id]() {
        return ctx->allocate_new_command(*kernel, *mem, thread_id);
    };

    ctx->renderer->free_func = [ctx](renderer::Command *cmd) {
        return ctx->free_new_command(cmd);
    };

    return 0;
}

EXPORT(int, sceGxmCreateDeferredContext, SceGxmDeferredContextParams *params, Ptr<SceGxmContext> *deferredContext) {
    TRACY_FUNC(sceGxmCreateDeferredContext, params, deferredContext);
    if (!params || !deferredContext)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (params->hostMemSize < sizeof(SceGxmContext)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    *deferredContext = params->hostMem.cast<SceGxmContext>();
    SceGxmContext *const ctx = deferredContext->get(emuenv.mem);
    new (ctx) SceGxmContext(emuenv.gxm.callback_lock);

    ctx->state.vertex_memory_callback = params->vertexCallback;
    ctx->state.fragment_memory_callback = params->fragmentCallback;
    ctx->state.vdm_memory_callback = params->vdmCallback;
    ctx->state.memory_callback_userdata = params->userData;

    ctx->state.type = SCE_GXM_CONTEXT_TYPE_DEFERRED;

    // Create a generic context. This is only used for storing command list
    ctx->renderer = std::make_unique<renderer::Context>();

    return 0;
}

EXPORT(int, sceGxmCreateRenderTarget, const SceGxmRenderTargetParams *params, Ptr<SceGxmRenderTarget> *renderTarget) {
    TRACY_FUNC(sceGxmCreateRenderTarget, params, renderTarget);
    if (!params) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (params->flags & 0xFFFF00EC) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!renderTarget) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    *renderTarget = alloc<SceGxmRenderTarget>(emuenv.mem, __FUNCTION__);
    if (!*renderTarget) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    SceGxmRenderTarget *const rt = renderTarget->get(emuenv.mem);
    if (!renderer::create_render_target(*emuenv.renderer, rt->renderer, params)) {
        free(emuenv.mem, *renderTarget);
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }

    rt->width = params->width;
    rt->height = params->height;
    rt->scenesPerFrame = params->scenesPerFrame;
    rt->driverMemBlock = params->driverMemBlock;

    return 0;
}

EXPORT(float, sceGxmDepthStencilSurfaceGetBackgroundDepth, const SceGxmDepthStencilSurface *surface) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceGetBackgroundDepth, surface);
    assert(surface);
    return surface->backgroundDepth;
}

EXPORT(bool, sceGxmDepthStencilSurfaceGetBackgroundMask, const SceGxmDepthStencilSurface *surface) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceGetBackgroundMask, surface);
    assert(surface);
    return (surface->control.content & SceGxmDepthStencilControl::mask_bit) != 0;
}

EXPORT(uint8_t, sceGxmDepthStencilSurfaceGetBackgroundStencil, const SceGxmDepthStencilSurface *surface) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceGetBackgroundStencil, surface);
    assert(surface);
    return surface->control.content & SceGxmDepthStencilControl::stencil_bits;
}

EXPORT(SceGxmDepthStencilForceLoadMode, sceGxmDepthStencilSurfaceGetForceLoadMode, const SceGxmDepthStencilSurface *surface) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceGetForceLoadMode, surface);
    assert(surface);
    return static_cast<SceGxmDepthStencilForceLoadMode>(surface->zlsControl & SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED);
}

EXPORT(SceGxmDepthStencilForceStoreMode, sceGxmDepthStencilSurfaceGetForceStoreMode, const SceGxmDepthStencilSurface *surface) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceGetForceStoreMode, surface);
    assert(surface);
    return static_cast<SceGxmDepthStencilForceStoreMode>(surface->zlsControl & SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED);
}

EXPORT(int, sceGxmDepthStencilSurfaceGetFormat, const SceGxmDepthStencilSurface *surface) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceGetFormat, surface);
    assert(surface);
    return UNIMPLEMENTED();
}

EXPORT(uint32_t, sceGxmDepthStencilSurfaceGetStrideInSamples, const SceGxmDepthStencilSurface *surface) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceGetStrideInSamples, surface);
    assert(surface);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDepthStencilSurfaceInit, SceGxmDepthStencilSurface *surface, SceGxmDepthStencilFormat depthStencilFormat, SceGxmDepthStencilSurfaceType surfaceType, uint32_t strideInSamples, Ptr<void> depthData, Ptr<void> stencilData) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceInit, surface, depthStencilFormat, surfaceType, strideInSamples, depthData, stencilData);
    if (!surface) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((strideInSamples == 0) || ((strideInSamples % SCE_GXM_TILE_SIZEX) != 0)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    *surface = SceGxmDepthStencilSurface();
    surface->depthData = depthData;
    surface->stencilData = stencilData;
    surface->zlsControl = SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_DISABLED | SCE_GXM_DEPTH_STENCIL_FORCE_STORE_DISABLED;

    surface->control.content = static_cast<uint32_t>(depthStencilFormat) | SceGxmDepthStencilControl::mask_bit;
    return 0;
}

EXPORT(int, sceGxmDepthStencilSurfaceInitDisabled, SceGxmDepthStencilSurface *surface) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceInitDisabled, surface);
    if (!surface) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    *surface = SceGxmDepthStencilSurface();

    surface->control.content = SceGxmDepthStencilControl::disabled_bit | SceGxmDepthStencilControl::mask_bit;
    return 0;
}

EXPORT(bool, sceGxmDepthStencilSurfaceIsEnabled, const SceGxmDepthStencilSurface *surface) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceIsEnabled, surface);
    assert(surface);
    return (surface->control.content & SceGxmDepthStencilControl::disabled_bit) == 0;
}

EXPORT(void, sceGxmDepthStencilSurfaceSetBackgroundDepth, SceGxmDepthStencilSurface *surface, float depth) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceSetBackgroundDepth, surface, depth);
    assert(surface);
    surface->backgroundDepth = depth;
}

EXPORT(void, sceGxmDepthStencilSurfaceSetBackgroundMask, SceGxmDepthStencilSurface *surface, bool mask) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceSetBackgroundMask, surface, mask);
    assert(surface);
    if (mask)
        surface->control.content |= SceGxmDepthStencilControl::mask_bit;
    else
        surface->control.content &= ~SceGxmDepthStencilControl::mask_bit;
}

EXPORT(void, sceGxmDepthStencilSurfaceSetBackgroundStencil, SceGxmDepthStencilSurface *surface, uint8_t stencil) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceSetBackgroundStencil, surface, stencil);
    assert(surface);
    surface->control.content &= ~SceGxmDepthStencilControl::stencil_bits;
    surface->control.content |= stencil;
}

EXPORT(void, sceGxmDepthStencilSurfaceSetForceLoadMode, SceGxmDepthStencilSurface *surface, SceGxmDepthStencilForceLoadMode forceLoad) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceSetForceLoadMode, surface, forceLoad);
    assert(surface);
    surface->zlsControl = (forceLoad & SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED) | (surface->zlsControl & ~SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED);
}

EXPORT(void, sceGxmDepthStencilSurfaceSetForceStoreMode, SceGxmDepthStencilSurface *surface, SceGxmDepthStencilForceStoreMode forceStore) {
    TRACY_FUNC(sceGxmDepthStencilSurfaceSetForceStoreMode, surface, forceStore);
    assert(surface);
    surface->zlsControl = (forceStore & SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED) | (surface->zlsControl & ~SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED);
}

EXPORT(int, sceGxmDestroyContext, Ptr<SceGxmContext> context) {
    TRACY_FUNC(sceGxmDestroyContext, context);
    if (!context)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    renderer::destroy_context(*emuenv.renderer, context.get(emuenv.mem)->renderer);

    return 0;
}

EXPORT(int, sceGxmDestroyDeferredContext, SceGxmContext *deferredContext) {
    TRACY_FUNC(sceGxmDestroyDeferredContext, deferredContext);
    if (!deferredContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDestroyRenderTarget, Ptr<SceGxmRenderTarget> renderTarget) {
    TRACY_FUNC(sceGxmDestroyRenderTarget, renderTarget);
    MemState &mem = emuenv.mem;

    if (!renderTarget)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    renderer::destroy_render_target(*emuenv.renderer, renderTarget.get(mem)->renderer);

    free(mem, renderTarget);

    return 0;
}

EXPORT(int, sceGxmDisplayQueueAddEntry, Ptr<SceGxmSyncObject> oldBuffer, Ptr<SceGxmSyncObject> newBuffer, Ptr<const void> callbackData) {
    TRACY_FUNC(sceGxmDisplayQueueAddEntry, oldBuffer, newBuffer, callbackData);
    if (!oldBuffer || !newBuffer)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    DisplayCallback display_callback;

    const Address address = alloc(emuenv.mem, emuenv.gxm.params.displayQueueCallbackDataSize, __FUNCTION__);
    const Ptr<void> ptr(address);
    memcpy(ptr.get(emuenv.mem), callbackData.get(emuenv.mem), emuenv.gxm.params.displayQueueCallbackDataSize);

    // Block future rendering by setting value2 of sync object
    SceGxmSyncObject *oldBufferSync = oldBuffer.get(emuenv.mem);
    SceGxmSyncObject *newBufferSync = newBuffer.get(emuenv.mem);

    display_callback.data = address;
    display_callback.pc = emuenv.gxm.params.displayQueueCallback.address();
    display_callback.old_buffer = oldBuffer;
    display_callback.new_buffer = newBuffer;
    display_callback.new_buffer_timestamp = newBufferSync->timestamp_ahead++;

    if (newBuffer == emuenv.gxm.last_fbo_sync_object) {
        // don't know why, some games like NFS send twice in a row the same buffer to the front...
        // act like it is not displaying anymore
        renderer::subject_done(newBufferSync, newBufferSync->last_display);
    }

    newBufferSync->last_display = newBufferSync->timestamp_ahead;
    emuenv.gxm.last_fbo_sync_object = newBuffer;

    // needed the first time the sync object is used as the old front buffer
    if (oldBufferSync->last_display == 0) {
        // resogun draws to the front buffer using the fact that the sync object prevents
        // it from doing so until it is swapped, the first time it happens must be handled
        // as a special case
        renderer::wishlist(oldBufferSync, oldBufferSync->timestamp_ahead);

        oldBufferSync->last_display = ++oldBufferSync->timestamp_ahead;
    }

    // function may be blocking here (expected behavior)
    emuenv.gxm.display_queue.push(display_callback);

    renderer::send_single_command(*emuenv.renderer, nullptr, renderer::CommandOpcode::NewFrame, false);

    return 0;
}

EXPORT(int, sceGxmDisplayQueueFinish) {
    TRACY_FUNC(sceGxmDisplayQueueFinish);
    emuenv.gxm.display_queue.wait_empty();

    return 0;
}

static void gxmSetUniformBuffers(renderer::State &state, GxmState &gxm, SceGxmContext *context, const SceGxmProgram &program, const UniformBuffers &buffers, const UniformBufferSizes &sizes, KernelState &kern, const MemState &mem, const SceUID current_thread) {
    for (std::size_t i = 0; i < buffers.size(); i++) {
        if (!buffers[i] || sizes.at(i) == 0) {
            continue;
        }

        std::uint32_t bytes_to_copy = sizes.at(i) * 4;
        if (sizes.at(i) == SCE_GXM_MAX_UB_IN_FLOAT_UNIT) {
            auto ite = gxm.memory_mapped_regions.lower_bound(buffers[i].address());
            if ((ite != gxm.memory_mapped_regions.end()) && ((ite->first + ite->second.size) > buffers[i].address())) {
                // Bound the size
                bytes_to_copy = std::min<std::uint32_t>(ite->first + ite->second.size - buffers[i].address(), bytes_to_copy);
            }

            // Check other UB friends and bound the size
            for (std::size_t j = 0; j < SCE_GXM_MAX_UNIFORM_BUFFERS; j++) {
                if (i == j) {
                    continue;
                }

                if (buffers[j].address() > buffers[i].address()) {
                    bytes_to_copy = std::min<std::uint32_t>(buffers[j].address() - buffers[i].address(), bytes_to_copy);
                }
            }
        }
        renderer::set_uniform_buffer(state, context->renderer.get(), !program.is_fragment(), i, bytes_to_copy, buffers[i]);
    }
}

static int gxmDrawElementGeneral(EmuEnvState &emuenv, const char *export_name, const SceUID thread_id, SceGxmContext *context, SceGxmPrimitiveType primType, SceGxmIndexFormat indexType, const void *indexData, uint32_t indexCount, uint32_t instanceCount) {
    if (!context || !indexData)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (!context->state.active) {
        if (context->state.type == SCE_GXM_CONTEXT_TYPE_DEFERRED) {
            return RET_ERROR(SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST);
        } else {
            return RET_ERROR(SCE_GXM_ERROR_NOT_WITHIN_SCENE);
        }
    }

    if (!context->state.fragment_program || !context->state.vertex_program) {
        return RET_ERROR(SCE_GXM_ERROR_NULL_PROGRAM);
    }

    const SceGxmFragmentProgram &gxm_fragment_program = *context->state.fragment_program.get(emuenv.mem);
    const SceGxmVertexProgram &gxm_vertex_program = *context->state.vertex_program.get(emuenv.mem);

    // Set uniforms
    const SceGxmProgram &vertex_program_gxp = *gxm_vertex_program.program.get(emuenv.mem);
    const SceGxmProgram &fragment_program_gxp = *gxm_fragment_program.program.get(emuenv.mem);

    gxmSetUniformBuffers(*emuenv.renderer, emuenv.gxm, context, vertex_program_gxp, context->state.vertex_uniform_buffers, gxm_vertex_program.renderer_data->uniform_buffer_sizes,
        emuenv.kernel, emuenv.mem, thread_id);
    gxmSetUniformBuffers(*emuenv.renderer, emuenv.gxm, context, fragment_program_gxp, context->state.fragment_uniform_buffers, gxm_fragment_program.renderer_data->uniform_buffer_sizes,
        emuenv.kernel, emuenv.mem, thread_id);

    if (context->last_precomputed) {
        // Need to re-set the data
        const auto frag_paramters = gxp::program_parameters(fragment_program_gxp);
        const auto vert_paramters = gxp::program_parameters(vertex_program_gxp);

        renderer::set_program(*emuenv.renderer, context->renderer.get(), context->state.vertex_program, false);
        renderer::set_program(*emuenv.renderer, context->renderer.get(), context->state.fragment_program, true);

        context->last_precomputed = false;
    }

    // set textures that are dirty
    const gxp::TextureInfo vert_textures_sync = gxm_vertex_program.renderer_data->textures_used & context->is_vert_texture_dirty;
    context->is_vert_texture_dirty &= ~vert_textures_sync;
    const gxp::TextureInfo frag_textures_sync = gxm_fragment_program.renderer_data->textures_used & context->is_frag_texture_dirty;
    context->is_frag_texture_dirty &= ~frag_textures_sync;
    const auto &textures = context->state.textures;
    for (uint16_t texture_index = 0; texture_index < SCE_GXM_MAX_TEXTURE_UNITS; texture_index++) {
        if (vert_textures_sync[texture_index]) {
            const uint16_t index_position = SCE_GXM_MAX_TEXTURE_UNITS + texture_index;
            renderer::set_texture(*emuenv.renderer, context->renderer.get(), index_position, textures[index_position]);
        }

        if (frag_textures_sync[texture_index])
            renderer::set_texture(*emuenv.renderer, context->renderer.get(), texture_index, textures[texture_index]);
    }

    // Update vertex data. We should stores a copy of the data to pass it to GPU later, since another scene
    // may start to overwrite stuff when this scene is being processed in our queue (in case of OpenGL).
    size_t max_index = 0;
    if (indexType == SCE_GXM_INDEX_FORMAT_U16) {
        const uint16_t *const data = static_cast<const uint16_t *>(indexData);
        max_index = *std::max_element(&data[0], &data[indexCount]);
    } else {
        const uint32_t *const data = static_cast<const uint32_t *>(indexData);
        max_index = *std::max_element(&data[0], &data[indexCount]);
    }

    size_t max_data_length[SCE_GXM_MAX_VERTEX_STREAMS] = {};
    std::uint32_t stream_used = 0;
    for (const SceGxmVertexAttribute &attribute : gxm_vertex_program.attributes) {
        const SceGxmAttributeFormat attribute_format = static_cast<SceGxmAttributeFormat>(attribute.format);
        const size_t attribute_size = gxm::attribute_format_size(attribute_format) * attribute.componentCount;
        const SceGxmVertexStream &stream = gxm_vertex_program.streams[attribute.streamIndex];
        const SceGxmIndexSource index_source = static_cast<SceGxmIndexSource>(stream.indexSource);
        const size_t data_passed_length = gxm::is_stream_instancing(index_source) ? ((instanceCount - 1) * stream.stride) : (max_index * stream.stride);
        const size_t data_length = attribute.offset + data_passed_length + attribute_size;
        max_data_length[attribute.streamIndex] = std::max<size_t>(max_data_length[attribute.streamIndex], data_length);
        stream_used |= (1 << attribute.streamIndex);
    }

    // Copy and queue upload
    for (size_t stream_index = 0; stream_index < SCE_GXM_MAX_VERTEX_STREAMS; ++stream_index) {
        // Upload it
        if (stream_used & (1 << static_cast<std::uint16_t>(stream_index))) {
            const size_t data_length = max_data_length[stream_index];
            const Ptr<const void> data = context->state.stream_data[stream_index];

            renderer::set_vertex_stream(*emuenv.renderer, context->renderer.get(), stream_index,
                data_length, data);
        }
    }

    renderer::draw(*emuenv.renderer, context->renderer.get(), primType, indexType, indexData, indexCount, instanceCount);

    return 0;
}

EXPORT(int, sceGxmDraw, SceGxmContext *context, SceGxmPrimitiveType primType, SceGxmIndexFormat indexType, const void *indexData, uint32_t indexCount) {
    TRACY_FUNC(sceGxmDraw, context, primType, indexType, indexData, indexCount);
    return gxmDrawElementGeneral(emuenv, export_name, thread_id, context, primType, indexType, indexData, indexCount, 1);
}

EXPORT(int, sceGxmDrawInstanced, SceGxmContext *context, SceGxmPrimitiveType primType, SceGxmIndexFormat indexType, const void *indexData, uint32_t indexCount, uint32_t indexWrap) {
    TRACY_FUNC(sceGxmDrawInstanced, context, primType, indexType, indexData, indexCount, indexWrap);
    if (indexCount % indexWrap != 0) {
        LOG_WARN("Extra vertexes are requested to be drawn (ignored)");
    }

    return gxmDrawElementGeneral(emuenv, export_name, thread_id, context, primType, indexType, indexData, indexWrap, indexCount / indexWrap);
}

EXPORT(int, sceGxmDrawPrecomputed, SceGxmContext *context, SceGxmPrecomputedDraw *draw) {
    TRACY_FUNC(sceGxmDrawPrecomputed, context, draw);
    if (!context) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (!context->state.active) {
        if (context->state.type == SCE_GXM_CONTEXT_TYPE_DEFERRED) {
            return RET_ERROR(SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST);
        } else {
            return RET_ERROR(SCE_GXM_ERROR_NOT_WITHIN_SCENE);
        }
    }

    if (!draw) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    SceGxmPrecomputedVertexState *vertex_state = context->state.precomputed_vertex_state.cast<SceGxmPrecomputedVertexState>().get(emuenv.mem);
    SceGxmPrecomputedFragmentState *fragment_state = context->state.precomputed_fragment_state.cast<SceGxmPrecomputedFragmentState>().get(emuenv.mem);

    // not sure if precomputed uses current program... maybe it does?
    // anyway states have to be made on a program to program basis so this should be safe
    const Ptr<const SceGxmFragmentProgram> fragment_program_gptr = fragment_state ? fragment_state->program : context->state.fragment_program;
    const Ptr<const SceGxmVertexProgram> vertex_program_gptr = vertex_state ? vertex_state->program : context->state.vertex_program;

    const SceGxmFragmentProgram *fragment_program = fragment_program_gptr.get(emuenv.mem);
    const SceGxmVertexProgram *vertex_program = vertex_program_gptr.get(emuenv.mem);

    if (!vertex_program || !fragment_program) {
        return RET_ERROR(SCE_GXM_ERROR_NULL_PROGRAM);
    }

    renderer::set_program(*emuenv.renderer, context->renderer.get(), fragment_program_gptr, true);
    renderer::set_program(*emuenv.renderer, context->renderer.get(), vertex_program_gptr, false);

    // Set uniforms
    const SceGxmProgram &vertex_program_gxp = *vertex_program->program.get(emuenv.mem);
    const SceGxmProgram &fragment_program_gxp = *fragment_program->program.get(emuenv.mem);

    gxmSetUniformBuffers(*emuenv.renderer, emuenv.gxm, context, vertex_program_gxp, (vertex_state ? (*vertex_state->uniform_buffers.get(emuenv.mem)) : context->state.vertex_uniform_buffers), vertex_program->renderer_data->uniform_buffer_sizes,
        emuenv.kernel, emuenv.mem, thread_id);

    gxmSetUniformBuffers(*emuenv.renderer, emuenv.gxm, context, fragment_program_gxp, (fragment_state ? (*fragment_state->uniform_buffers.get(emuenv.mem)) : context->state.fragment_uniform_buffers), fragment_program->renderer_data->uniform_buffer_sizes,
        emuenv.kernel, emuenv.mem, thread_id);

    // Update vertex data. We should stores a copy of the data to pass it to GPU later, since another scene
    // may start to overwrite stuff when this scene is being processed in our queue (in case of OpenGL).
    size_t max_index = 0;
    if (draw->index_format == SCE_GXM_INDEX_FORMAT_U16) {
        const uint16_t *const data = draw->index_data.cast<const uint16_t>().get(emuenv.mem);
        max_index = *std::max_element(&data[0], &data[draw->vertex_count]);
    } else {
        const uint32_t *const data = draw->index_data.cast<const uint32_t>().get(emuenv.mem);
        max_index = *std::max_element(&data[0], &data[draw->vertex_count]);
    }

    // set all textures that are used and mark them as dirty
    const gxp::TextureInfo vert_textures_sync = vertex_program->renderer_data->textures_used;
    context->is_vert_texture_dirty |= vert_textures_sync;
    const gxp::TextureInfo frag_textures_sync = fragment_program->renderer_data->textures_used;
    context->is_frag_texture_dirty |= frag_textures_sync;
    const SceGxmTexture *frag_textures = fragment_state ? fragment_state->textures.get(emuenv.mem) : context->state.textures.data();
    SceGxmTexture *vert_textures = vertex_state ? vertex_state->textures.get(emuenv.mem) : (context->state.textures.data() + SCE_GXM_MAX_TEXTURE_UNITS);
    for (uint16_t texture_index = 0; texture_index < SCE_GXM_MAX_TEXTURE_UNITS; texture_index++) {
        if (vert_textures_sync[texture_index]) {
            const uint16_t index_position = SCE_GXM_MAX_TEXTURE_UNITS + texture_index;
            renderer::set_texture(*emuenv.renderer, context->renderer.get(), index_position, vert_textures[texture_index]);
        }

        if (frag_textures_sync[texture_index])
            renderer::set_texture(*emuenv.renderer, context->renderer.get(), texture_index, frag_textures[texture_index]);
    }

    size_t max_data_length[SCE_GXM_MAX_VERTEX_STREAMS] = {};
    std::uint32_t stream_used = 0;
    for (const SceGxmVertexAttribute &attribute : vertex_program->attributes) {
        const SceGxmAttributeFormat attribute_format = static_cast<SceGxmAttributeFormat>(attribute.format);
        const size_t attribute_size = gxm::attribute_format_size(attribute_format) * attribute.componentCount;
        const SceGxmVertexStream &stream = vertex_program->streams[attribute.streamIndex];
        const SceGxmIndexSource index_source = static_cast<SceGxmIndexSource>(stream.indexSource);
        const size_t data_passed_length = gxm::is_stream_instancing(index_source) ? ((draw->instance_count - 1) * stream.stride) : (max_index * stream.stride);
        const size_t data_length = attribute.offset + data_passed_length + attribute_size;
        max_data_length[attribute.streamIndex] = std::max<size_t>(max_data_length[attribute.streamIndex], data_length);
        stream_used |= (1 << attribute.streamIndex);
    }

    auto stream_data = draw->stream_data.get(emuenv.mem);
    // Copy and queue upload
    for (size_t stream_index = 0; stream_index < SCE_GXM_MAX_VERTEX_STREAMS; ++stream_index) {
        // Upload it
        if (stream_used & (1 << static_cast<std::uint16_t>(stream_index))) {
            const size_t data_length = max_data_length[stream_index];
            const Ptr<const void> data = stream_data[stream_index];

            renderer::set_vertex_stream(*emuenv.renderer, context->renderer.get(), stream_index,
                data_length, data);
        }
    }

    renderer::draw(*emuenv.renderer, context->renderer.get(), draw->type, draw->index_format, draw->index_data.get(emuenv.mem), draw->vertex_count, draw->instance_count);

    context->last_precomputed = true;
    return 0;
}

EXPORT(int, sceGxmEndCommandList, SceGxmContext *deferredContext, SceGxmCommandList *commandList) {
    TRACY_FUNC(sceGxmEndCommandList, deferredContext, commandList);
    if (deferredContext->state.type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!deferredContext->state.active) {
        return RET_ERROR(SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST);
    }

    // only set the first two fields for commandList (its size is assumed to be 32 bytes by the game)
    commandList->list = deferredContext->linearly_allocate<renderer::CommandList>(emuenv.kernel, emuenv.mem,
        thread_id);

    // also update our own command list
    deferredContext->curr_command_list->list = commandList->list;

    *commandList->list = deferredContext->renderer->command_list;

    // insert last memory range
    deferredContext->insert_new_memory_range(emuenv.mem);
    deferredContext->curr_command_list = nullptr;

    // Reset active state
    deferredContext->state.active = false;
    deferredContext->reset_recording();

    renderer::reset_command_list(deferredContext->renderer->command_list);

    return 0;
}

EXPORT(int, sceGxmEndScene, SceGxmContext *context, SceGxmNotification *vertexNotification, SceGxmNotification *fragmentNotification) {
    TRACY_FUNC(sceGxmEndScene, context, vertexNotification, fragmentNotification);
    const MemState &mem = emuenv.mem;

    if (!context) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (context->state.type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!context->state.active) {
        return RET_ERROR(SCE_GXM_ERROR_WITHIN_SCENE);
    }

    // Add command to end the scene
    renderer::sync_surface_data(*emuenv.renderer, context->renderer.get());

    if (vertexNotification) {
        renderer::add_command(context->renderer.get(), renderer::CommandOpcode::SignalNotification,
            nullptr, *vertexNotification, true);
    }

    if (fragmentNotification) {
        renderer::add_command(context->renderer.get(), renderer::CommandOpcode::SignalNotification,
            nullptr, *fragmentNotification, false);
    }

    if (context->state.fragment_sync_object) {
        SceGxmSyncObject *sync = context->state.fragment_sync_object.get(mem);
        uint32_t cmd_timestamp = ++sync->timestamp_ahead;
        renderer::add_command(context->renderer.get(), renderer::CommandOpcode::SignalSyncObject,
            nullptr, context->state.fragment_sync_object, cmd_timestamp);
    }

    // Submit our command list
    renderer::submit_command_list(*emuenv.renderer, context->renderer.get(), context->renderer->command_list);
    renderer::reset_command_list(context->renderer->command_list);

    context->state.active = false;
    return 0;
}

EXPORT(int, sceGxmExecuteCommandList, SceGxmContext *context, SceGxmCommandList *commandList) {
    TRACY_FUNC(sceGxmExecuteCommandList, context, commandList);
    if (!context) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (context->state.type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!context->state.active) {
        return RET_ERROR(SCE_GXM_ERROR_NOT_WITHIN_SCENE);
    }

    // Emit a jump to the first command of given command list
    // Since only one immediate context exists per process, direct linking like this should be fine! (I hope)
    renderer::CommandList &imm_cmds = context->renderer->command_list;

    imm_cmds.last->next = commandList->list->first;
    imm_cmds.last = commandList->list->last;

    // Restore back our GXM state
    gxmContextStateRestore(*emuenv.renderer, emuenv.mem, context, true);

    return 0;
}

EXPORT(int, sceGxmFinish, SceGxmContext *context) {
    TRACY_FUNC(sceGxmFinish, context);
    assert(context);

    if (!context)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_THREAD);

    // Wait on this context's rendering finish code.
    renderer::finish(*emuenv.renderer, context->renderer.get());

    return 0;
}

EXPORT(SceGxmPassType, sceGxmFragmentProgramGetPassType, const SceGxmFragmentProgram *fragmentProgram) {
    TRACY_FUNC(sceGxmFragmentProgramGetPassType, fragmentProgram);
    assert(fragmentProgram);
    STUBBED("SCE_GXM_PASS_TYPE_OPAQUE");
    return SceGxmPassType::SCE_GXM_PASS_TYPE_OPAQUE;
}

EXPORT(Ptr<const SceGxmProgram>, sceGxmFragmentProgramGetProgram, const SceGxmFragmentProgram *fragmentProgram) {
    TRACY_FUNC(sceGxmFragmentProgramGetProgram, fragmentProgram);
    assert(fragmentProgram);
    return fragmentProgram->program;
}

EXPORT(bool, sceGxmFragmentProgramIsEnabled, const SceGxmFragmentProgram *fragmentProgram) {
    TRACY_FUNC(sceGxmFragmentProgramIsEnabled, fragmentProgram);
    assert(fragmentProgram);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmGetContextType, const SceGxmContext *context, SceGxmContextType *type) {
    TRACY_FUNC(sceGxmGetContextType, context, type);
    if (!context || !type) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    *type = context->state.type;

    return 0;
}

EXPORT(int, sceGxmGetDeferredContextFragmentBuffer, const SceGxmContext *deferredContext, Ptr<void> *mem) {
    TRACY_FUNC(sceGxmGetDeferredContextFragmentBuffer, deferredContext, mem);
    if (!deferredContext || !mem) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (deferredContext->state.type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (deferredContext->state.active) {
        return RET_ERROR(SCE_GXM_ERROR_WITHIN_COMMAND_LIST);
    }

    *mem = deferredContext->state.fragment_ring_buffer;
    return 0;
}

EXPORT(int, sceGxmGetDeferredContextVdmBuffer, const SceGxmContext *deferredContext, Ptr<void> *mem) {
    TRACY_FUNC(sceGxmGetDeferredContextVdmBuffer, deferredContext, mem);
    if (!deferredContext || !mem) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (deferredContext->state.type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (deferredContext->state.active) {
        return RET_ERROR(SCE_GXM_ERROR_WITHIN_COMMAND_LIST);
    }

    *mem = deferredContext->state.vdm_buffer;
    return 0;
}

EXPORT(int, sceGxmGetDeferredContextVertexBuffer, const SceGxmContext *deferredContext, Ptr<void> *mem) {
    TRACY_FUNC(sceGxmGetDeferredContextVertexBuffer, deferredContext, mem);
    if (!deferredContext || !mem) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (deferredContext->state.type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (deferredContext->state.active) {
        return RET_ERROR(SCE_GXM_ERROR_WITHIN_COMMAND_LIST);
    }

    *mem = deferredContext->state.vertex_ring_buffer;
    return 0;
}

EXPORT(Ptr<uint32_t>, sceGxmGetNotificationRegion) {
    TRACY_FUNC(sceGxmGetNotificationRegion);
    return emuenv.gxm.notification_region;
}

EXPORT(int, sceGxmGetParameterBufferThreshold, uint32_t *parameterBufferSize) {
    TRACY_FUNC(sceGxmGetParameterBufferThreshold, parameterBufferSize);
    if (!parameterBufferSize) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(uint32_t, sceGxmGetPrecomputedDrawSize, const SceGxmVertexProgram *vertexProgram) {
    TRACY_FUNC(sceGxmGetPrecomputedDrawSize, vertexProgram);
    assert(vertexProgram);

    uint16_t max_stream_index = 0;
    for (const SceGxmVertexAttribute &attribute : vertexProgram->attributes) {
        max_stream_index = std::max(attribute.streamIndex, max_stream_index);
    }

    return (max_stream_index + 1) * sizeof(StreamData);
}

EXPORT(uint32_t, sceGxmGetPrecomputedFragmentStateSize, const SceGxmFragmentProgram *fragmentProgram) {
    TRACY_FUNC(sceGxmGetPrecomputedFragmentStateSize, fragmentProgram);
    assert(fragmentProgram);

    const uint16_t texture_count = fragmentProgram->renderer_data->texture_count;
    return texture_count * sizeof(TextureData) + sizeof(UniformBuffers);
}

EXPORT(uint32_t, sceGxmGetPrecomputedVertexStateSize, const SceGxmVertexProgram *vertexProgram) {
    TRACY_FUNC(sceGxmGetPrecomputedVertexStateSize, vertexProgram);
    assert(vertexProgram);

    const uint16_t texture_count = vertexProgram->renderer_data->texture_count;
    return texture_count * sizeof(TextureData) + sizeof(UniformBuffers);
}

EXPORT(int, sceGxmGetRenderTargetMemSize, const SceGxmRenderTargetParams *params, uint32_t *hostMemSize) {
    TRACY_FUNC(sceGxmGetRenderTargetMemSize, params, hostMemSize);
    if (!params) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    *hostMemSize = uint32_t(MiB(2));
    return STUBBED("2MiB emuenv mem");
}

struct GxmThreadParams {
    KernelState *kernel = nullptr;
    MemState *mem = nullptr;
    SceUID thid = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
    GxmState *gxm = nullptr;
    renderer::State *renderer = nullptr;
    std::shared_ptr<SDL_semaphore> emuenv_may_destroy_params = std::shared_ptr<SDL_semaphore>(SDL_CreateSemaphore(0), SDL_DestroySemaphore);
};

static int SDLCALL thread_function(void *data) {
    const GxmThreadParams params = *static_cast<const GxmThreadParams *>(data);
    SDL_SemPost(params.emuenv_may_destroy_params.get());
    while (true) {
        auto display_callback = params.gxm->display_queue.top();
        if (!display_callback)
            break;

        SceGxmSyncObject *oldBuffer = Ptr<SceGxmSyncObject>(display_callback->old_buffer).get(*params.mem);
        SceGxmSyncObject *newBuffer = Ptr<SceGxmSyncObject>(display_callback->new_buffer).get(*params.mem);

        // Wait for fragment on the new buffer to finish
        renderer::wishlist(newBuffer, display_callback->new_buffer_timestamp);
        // now we can remove the thread from the display queue
        params.gxm->display_queue.pop();

        // Now run callback
        const ThreadStatePtr display_thread = util::find(params.thid, params.kernel->threads);
        display_thread->run_guest_function(*params.kernel, display_callback->pc, display_callback->data);

        free(*params.mem, display_callback->data);

        // The only thing old buffer should be waiting for is to stop being displayed
        renderer::subject_done(oldBuffer, std::min(oldBuffer->timestamp_current + 1, oldBuffer->timestamp_ahead.load()));
    }

    return 0;
}

EXPORT(int, sceGxmInitialize, const SceGxmInitializeParams *params) {
    TRACY_FUNC(sceGxmInitialize, params);
    if (!params) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (((params->flags != 0x00000000U) && (params->flags != 0x00010000U) && (params->flags != 0x00020000U)) || (params->parameterBufferSize & 0x3FFFF)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if ((params->displayQueueMaxPendingCount * params->displayQueueCallbackDataSize) > 0x200) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    emuenv.gxm.params = *params;
    // hack, limit the number of frame rendering at the same time to at most 3
    // moreover, it looks like one of the frame of displayQueueMaxPendingCount is the one going to be added to the display queue
    const uint32_t max_queue_size = std::min(std::max(params->displayQueueMaxPendingCount, 2U), 3U) - 1;
    emuenv.gxm.display_queue.maxPendingCount_ = max_queue_size;

    const ThreadStatePtr main_thread = util::find(thread_id, emuenv.kernel.threads);
    const ThreadStatePtr display_queue_thread = emuenv.kernel.create_thread(emuenv.mem, "SceGxmDisplayQueue", Ptr<void>(0), SCE_KERNEL_HIGHEST_PRIORITY_USER, SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT, SCE_KERNEL_STACK_SIZE_USER_DEFAULT, nullptr);
    if (!display_queue_thread) {
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }
    emuenv.gxm.display_queue_thread = display_queue_thread->id;

    GxmThreadParams gxm_params;
    gxm_params.mem = &emuenv.mem;
    gxm_params.kernel = &emuenv.kernel;
    gxm_params.thid = emuenv.gxm.display_queue_thread;
    gxm_params.gxm = &emuenv.gxm;
    gxm_params.renderer = emuenv.renderer.get();

    // Reset the queue in case sceGxmTerminate was called earlier
    emuenv.gxm.display_queue.reset();
    emuenv.gxm.sdl_thread = SDL_CreateThread(&thread_function, "SceGxmDisplayQueue", &gxm_params);
    SDL_SemWait(gxm_params.emuenv_may_destroy_params.get());
    emuenv.gxm.notification_region = Ptr<uint32_t>(alloc(emuenv.mem, MiB(1), "SceGxmNotificationRegion"));
    memset(emuenv.gxm.notification_region.get(emuenv.mem), 0, MiB(1));
    return 0;
}

EXPORT(int, sceGxmIsDebugVersion) {
    TRACY_FUNC(sceGxmIsDebugVersion);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmMapFragmentUsseMemory, Ptr<void> base, uint32_t size, uint32_t *offset) {
    TRACY_FUNC(sceGxmMapFragmentUsseMemory, base, size, offset);
    STUBBED("always return success");

    if (!base || !offset) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    // TODO What should this be?
    *offset = base.address();

    return 0;
}

EXPORT(int, sceGxmMapMemory, Ptr<void> base, uint32_t size, uint32_t attribs) {
    TRACY_FUNC(sceGxmMapMemory, base, size, attribs);
    if (!base) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    // Check if it has already been mapped
    // Some games intentionally overlapping mapped region. Nothing we can do. Allow it, bear your own consequences.
    GxmState &gxm = emuenv.gxm;

    auto ite = gxm.memory_mapped_regions.lower_bound(base.address());
    if ((ite == gxm.memory_mapped_regions.end()) || (ite->first != base.address())) {
        gxm.memory_mapped_regions.emplace(base.address(), MemoryMapInfo{ base.address(), size, attribs });
        return 0;
    }

    return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
}

EXPORT(int, sceGxmMapVertexUsseMemory, Ptr<void> base, uint32_t size, uint32_t *offset) {
    TRACY_FUNC(sceGxmMapVertexUsseMemory, base, size, offset);
    STUBBED("always return success");

    if (!base || !offset) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    // TODO What should this be?
    *offset = base.address();

    return 0;
}

EXPORT(int, sceGxmMidSceneFlush, SceGxmContext *immediateContext, uint32_t flags, SceGxmSyncObject *vertexSyncObject, const SceGxmNotification *vertexNotification) {
    TRACY_FUNC(sceGxmMidSceneFlush, immediateContext, flags, vertexSyncObject, vertexNotification);
    STUBBED("Surfaces not flushed back to memory");

    if (!immediateContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((flags & 0xFFFFFFFE) || (immediateContext->state.type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (vertexSyncObject != nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (vertexNotification)
        renderer::add_command(immediateContext->renderer.get(), renderer::CommandOpcode::SignalNotification,
            nullptr, *vertexNotification, true);

    // send the commands recorded up to now
    renderer::submit_command_list(*emuenv.renderer, immediateContext->renderer.get(), immediateContext->renderer->command_list);
    renderer::reset_command_list(immediateContext->renderer->command_list);

    return 0;
}

EXPORT(int, _sceGxmMidSceneFlush, SceGxmContext *immediateContext, uint32_t flags, SceGxmSyncObject *vertexSyncObject, const SceGxmNotification *vertexNotification) {
    TRACY_FUNC(_sceGxmMidSceneFlush, immediateContext, flags, vertexSyncObject, vertexNotification);
    return CALL_EXPORT(sceGxmMidSceneFlush, immediateContext, flags, vertexSyncObject, vertexNotification);
}

EXPORT(int, sceGxmNotificationWait, const SceGxmNotification *notification) {
    TRACY_FUNC(sceGxmNotificationWait, notification);
    if (!notification) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    std::uint32_t *value = notification->address.get(emuenv.mem);
    const std::uint32_t target_value = notification->value;

    std::unique_lock<std::mutex> lock(emuenv.renderer->notification_mutex);
    if (*value != target_value) {
        emuenv.renderer->notification_ready.wait(lock, [&]() { return *value == target_value; });
    }

    return 0;
}

EXPORT(int, sceGxmPadHeartbeat, const SceGxmColorSurface *displaySurface, SceGxmSyncObject *displaySyncObject) {
    TRACY_FUNC(sceGxmPadHeartbeat, displaySurface, displaySyncObject);
    if (!displaySurface || !displaySyncObject)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    return 0;
}

EXPORT(int, sceGxmPadTriggerGpuPaTrace) {
    TRACY_FUNC(sceGxmPadTriggerGpuPaTrace);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPopUserMarker) {
    TRACY_FUNC(sceGxmPopUserMarker);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedDrawInit, SceGxmPrecomputedDraw *state, Ptr<const SceGxmVertexProgram> program, Ptr<void> extra_data) {
    TRACY_FUNC(sceGxmPrecomputedDrawInit, state, program, extra_data);
    if (!state || !program || !extra_data) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (extra_data.address() & 0xF) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_ALIGNMENT);
    }

    SceGxmPrecomputedDraw new_draw;

    new_draw.program = program;

    uint16_t max_stream_index = 0;
    const auto &gxm_vertex_program = *program.get(emuenv.mem);
    for (const SceGxmVertexAttribute &attribute : gxm_vertex_program.attributes) {
        max_stream_index = std::max(attribute.streamIndex, max_stream_index);
    }

    new_draw.stream_count = max_stream_index + 1;
    new_draw.stream_data = extra_data.cast<StreamData>();

    *state = new_draw;

    return 0;
}

EXPORT(int, sceGxmPrecomputedDrawSetAllVertexStreams, SceGxmPrecomputedDraw *state, const Ptr<const void> *stream_data) {
    TRACY_FUNC(sceGxmPrecomputedDrawSetAllVertexStreams, state, stream_data);
    if (!state) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const auto state_stream_data = state->stream_data.get(emuenv.mem);
    for (int i = 0; i < state->stream_count; ++i) {
        state_stream_data[i] = stream_data[i];
    }

    return 0;
}

EXPORT(int, sceGxmPrecomputedDrawSetParams, SceGxmPrecomputedDraw *state, SceGxmPrimitiveType type, SceGxmIndexFormat index_format, Ptr<const void> index_data, uint32_t vertex_count) {
    TRACY_FUNC(sceGxmPrecomputedDrawSetParams, state, type, index_format, index_data, vertex_count);
    if (!state || !index_data) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    state->type = type;
    state->index_format = index_format;
    state->index_data = index_data;
    state->vertex_count = vertex_count;
    state->instance_count = 1;

    return 0;
}

EXPORT(int, sceGxmPrecomputedDrawSetParamsInstanced, SceGxmPrecomputedDraw *precomputedDraw, SceGxmPrimitiveType primType, SceGxmIndexFormat indexType, Ptr<const void> indexData, uint32_t indexCount, uint32_t indexWrap) {
    TRACY_FUNC(sceGxmPrecomputedDrawSetParamsInstanced, precomputedDraw, primType, indexType, indexData, indexCount, indexWrap);
    if (!precomputedDraw || !indexData) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    precomputedDraw->type = primType;
    precomputedDraw->index_format = indexType;
    precomputedDraw->index_data = indexData;
    if (indexWrap == 0) {
        precomputedDraw->vertex_count = 0;
        precomputedDraw->instance_count = 0;
    } else {
        precomputedDraw->vertex_count = indexWrap;
        precomputedDraw->instance_count = indexCount / indexWrap;
    }

    return 0;
}

EXPORT(int, sceGxmPrecomputedDrawSetVertexStream, SceGxmPrecomputedDraw *state, uint32_t streamIndex, Ptr<const void> streamData) {
    TRACY_FUNC(sceGxmPrecomputedDrawSetVertexStream, state, streamIndex, streamData);
    if (!state) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (streamIndex > (SCE_GXM_MAX_VERTEX_STREAMS - 1)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!streamData) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const auto stream_data = state->stream_data.get(emuenv.mem);
    stream_data[streamIndex] = streamData;

    return 0;
}

EXPORT(Ptr<const void>, sceGxmPrecomputedFragmentStateGetDefaultUniformBuffer, const SceGxmPrecomputedFragmentState *state) {
    TRACY_FUNC(sceGxmPrecomputedFragmentStateGetDefaultUniformBuffer, state);
    UniformBuffers &uniform_buffers = *state->uniform_buffers.get(emuenv.mem);
    return uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX];
}

EXPORT(int, sceGxmPrecomputedFragmentStateInit, SceGxmPrecomputedFragmentState *state, Ptr<const SceGxmFragmentProgram> program, Ptr<void> extra_data) {
    TRACY_FUNC(sceGxmPrecomputedFragmentStateInit, state, program, extra_data);
    if (!state || !program || !extra_data) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (extra_data.address() & 0xF) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_ALIGNMENT);
    }

    SceGxmPrecomputedFragmentState new_state;
    new_state.program = program;

    new_state.texture_count = program.get(emuenv.mem)->renderer_data->texture_count;

    new_state.textures = extra_data.cast<TextureData>();
    new_state.uniform_buffers = (extra_data.cast<TextureData>() + new_state.texture_count).cast<UniformBuffers>();

    *state = new_state;

    return 0;
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetAllAuxiliarySurfaces) {
    TRACY_FUNC(sceGxmPrecomputedFragmentStateSetAllAuxiliarySurfaces);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetAllTextures, SceGxmPrecomputedFragmentState *state, Ptr<const SceGxmTexture> textures) {
    TRACY_FUNC(sceGxmPrecomputedFragmentStateSetAllTextures, state, textures);
    if (!state || !textures) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const auto state_textures = state->textures.get(emuenv.mem);
    for (int i = 0; i < state->texture_count; ++i) {
        state_textures[i] = textures.get(emuenv.mem)[i];
    }

    return 0;
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetAllUniformBuffers, SceGxmPrecomputedFragmentState *precomputedState, Ptr<const void> const *bufferDataArray) {
    TRACY_FUNC(sceGxmPrecomputedFragmentStateSetAllUniformBuffers, precomputedState, bufferDataArray);
    if (!precomputedState || !precomputedState->uniform_buffers || !bufferDataArray)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    UniformBuffers *uniform_buffers = precomputedState->uniform_buffers.get(emuenv.mem);
    if (!uniform_buffers)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    for (auto b = 0; b < SCE_GXM_MAX_UNIFORM_BUFFERS; b++)
        (*uniform_buffers)[b] = bufferDataArray[b];

    return 0;
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetDefaultUniformBuffer, SceGxmPrecomputedFragmentState *state, Ptr<const void> buffer) {
    TRACY_FUNC(sceGxmPrecomputedFragmentStateSetDefaultUniformBuffer, state, buffer);
    if (!state || !buffer) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    UniformBuffers &uniform_buffers = *state->uniform_buffers.get(emuenv.mem);
    uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX] = buffer;

    return 0;
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetTexture, SceGxmPrecomputedFragmentState *state, uint32_t index, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmPrecomputedFragmentStateSetTexture, state, index, texture);
    if (!state)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (index >= SCE_GXM_MAX_TEXTURE_UNITS)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (index < state->texture_count) {
        const auto state_textures = state->textures.get(emuenv.mem);
        state_textures[index] = *texture;
    }

    return 0;
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetUniformBuffer, SceGxmPrecomputedFragmentState *precomputedState, uint32_t bufferIndex, Ptr<const void> bufferData) {
    TRACY_FUNC(sceGxmPrecomputedFragmentStateSetUniformBuffer, precomputedState, bufferIndex, bufferData);
    if (!precomputedState) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (bufferIndex > (SCE_GXM_MAX_UNIFORM_BUFFERS - 1)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!bufferData) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    auto &state_uniform_buffers = *precomputedState->uniform_buffers.get(emuenv.mem);
    state_uniform_buffers[bufferIndex] = bufferData;

    return 0;
}

EXPORT(Ptr<const void>, sceGxmPrecomputedVertexStateGetDefaultUniformBuffer, SceGxmPrecomputedVertexState *state) {
    TRACY_FUNC(sceGxmPrecomputedVertexStateGetDefaultUniformBuffer, state);
    UniformBuffers &uniform_buffers = *state->uniform_buffers.get(emuenv.mem);
    return uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX];
}

EXPORT(int, sceGxmPrecomputedVertexStateInit, SceGxmPrecomputedVertexState *state, Ptr<const SceGxmVertexProgram> program, Ptr<void> extra_data) {
    TRACY_FUNC(sceGxmPrecomputedVertexStateInit, state, program, extra_data);
    if (!state || !program || !extra_data) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (extra_data.address() & 0xF) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_ALIGNMENT);
    }

    SceGxmPrecomputedVertexState new_state;
    new_state.program = program;

    new_state.texture_count = program.get(emuenv.mem)->renderer_data->texture_count;

    new_state.textures = extra_data.cast<TextureData>();
    new_state.uniform_buffers = (extra_data.cast<TextureData>() + new_state.texture_count).cast<UniformBuffers>();

    *state = new_state;

    return 0;
}

EXPORT(int, sceGxmPrecomputedVertexStateSetAllTextures, SceGxmPrecomputedVertexState *precomputedState, Ptr<const SceGxmTexture> textureArray) {
    TRACY_FUNC(sceGxmPrecomputedVertexStateSetAllTextures, precomputedState, textureArray);
    if (!precomputedState || !textureArray) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const auto state_textures = precomputedState->textures.get(emuenv.mem);
    for (int i = 0; i < precomputedState->texture_count; ++i) {
        state_textures[i] = textureArray.get(emuenv.mem)[i];
    }

    return 0;
}

EXPORT(int, sceGxmPrecomputedVertexStateSetAllUniformBuffers, SceGxmPrecomputedVertexState *precomputedState, Ptr<const void> const *bufferDataArray) {
    TRACY_FUNC(sceGxmPrecomputedVertexStateSetAllUniformBuffers, precomputedState, bufferDataArray);
    if (!precomputedState || !precomputedState->uniform_buffers || !bufferDataArray)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    UniformBuffers *uniform_buffers = precomputedState->uniform_buffers.get(emuenv.mem);
    if (!uniform_buffers)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    for (auto b = 0; b < SCE_GXM_MAX_UNIFORM_BUFFERS; b++)
        (*uniform_buffers)[b] = bufferDataArray[b];

    return 0;
}

EXPORT(int, sceGxmPrecomputedVertexStateSetDefaultUniformBuffer, SceGxmPrecomputedVertexState *state, Ptr<const void> buffer) {
    TRACY_FUNC(sceGxmPrecomputedVertexStateSetDefaultUniformBuffer, state, buffer);
    if (!state || !buffer) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    UniformBuffers &uniform_buffers = *state->uniform_buffers.get(emuenv.mem);
    uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX] = buffer;

    return 0;
}

EXPORT(int, sceGxmPrecomputedVertexStateSetTexture, SceGxmPrecomputedVertexState *precomputedState, uint32_t textureIndex, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmPrecomputedVertexStateSetTexture, precomputedState, textureIndex, texture);
    if (!precomputedState)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (textureIndex >= SCE_GXM_MAX_TEXTURE_UNITS)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (textureIndex < precomputedState->texture_count) {
        const auto state_textures = precomputedState->textures.get(emuenv.mem);
        state_textures[textureIndex] = *texture;
    }

    return 0;
}

EXPORT(int, sceGxmPrecomputedVertexStateSetUniformBuffer, SceGxmPrecomputedVertexState *precomputedState, uint32_t bufferIndex, Ptr<const void> bufferData) {
    TRACY_FUNC(sceGxmPrecomputedVertexStateSetUniformBuffer, precomputedState, bufferIndex, bufferData);
    if (!precomputedState) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (bufferIndex > (SCE_GXM_MAX_UNIFORM_BUFFERS - 1)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!bufferData) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    UniformBuffers &uniform_buffers = *precomputedState->uniform_buffers.get(emuenv.mem);
    uniform_buffers[bufferIndex] = bufferData;

    return 0;
}

EXPORT(int, sceGxmProgramCheck, const SceGxmProgram *program) {
    TRACY_FUNC(sceGxmProgramCheck, program);
    if (!program)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    if (memcmp(&program->magic, "GXP", 4) != 0)
        return RET_ERROR(SCE_GXM_ERROR_NULL_PROGRAM);

    return 0;
}

EXPORT(Ptr<SceGxmProgramParameter>, sceGxmProgramFindParameterByName, const SceGxmProgram *program, const char *name) {
    TRACY_FUNC(sceGxmProgramFindParameterByName, program, name);
    const MemState &mem = emuenv.mem;
    assert(program);
    if (!program || !name)
        return Ptr<SceGxmProgramParameter>();

    const SceGxmProgramParameter *const parameters = reinterpret_cast<const SceGxmProgramParameter *>(reinterpret_cast<const uint8_t *>(&program->parameters_offset) + program->parameters_offset);
    for (uint32_t i = 0; i < program->parameter_count; ++i) {
        const SceGxmProgramParameter *const parameter = &parameters[i];
        const uint8_t *const parameter_bytes = reinterpret_cast<const uint8_t *>(parameter);
        const char *const parameter_name = reinterpret_cast<const char *>(parameter_bytes + parameter->name_offset);
        if (strcmp(parameter_name, name) == 0) {
            const Address parameter_address = static_cast<Address>(parameter_bytes - &mem.memory[0]);
            return Ptr<SceGxmProgramParameter>(parameter_address);
        }
    }

    return Ptr<SceGxmProgramParameter>();
}

EXPORT(Ptr<SceGxmProgramParameter>, sceGxmProgramFindParameterBySemantic, const SceGxmProgram *program, SceGxmParameterSemantic semantic, uint32_t index) {
    TRACY_FUNC(sceGxmProgramFindParameterBySemantic, program, semantic, index);
    const MemState &mem = emuenv.mem;

    if (semantic == SCE_GXM_PARAMETER_SEMANTIC_NONE) {
        return Ptr<SceGxmProgramParameter>();
    }

    assert(program);
    if (!program)
        return Ptr<SceGxmProgramParameter>();

    const SceGxmProgramParameter *const parameters = reinterpret_cast<const SceGxmProgramParameter *>(reinterpret_cast<const uint8_t *>(&program->parameters_offset) + program->parameters_offset);
    for (uint32_t i = 0; i < program->parameter_count; ++i) {
        const SceGxmProgramParameter *const parameter = &parameters[i];
        const uint8_t *const parameter_bytes = reinterpret_cast<const uint8_t *>(parameter);
        if ((parameter->semantic == semantic) && (parameter->semantic_index == index)) {
            const Address parameter_address = static_cast<Address>(parameter_bytes - &mem.memory[0]);
            return Ptr<SceGxmProgramParameter>(parameter_address);
        }
    }

    return Ptr<SceGxmProgramParameter>();
}

EXPORT(Ptr<SceGxmProgramParameter>, _sceGxmProgramFindParameterBySemantic, const SceGxmProgram *program, SceGxmParameterSemantic semantic, uint32_t index) {
    TRACY_FUNC(_sceGxmProgramFindParameterBySemantic, program, semantic, index);
    return export_sceGxmProgramFindParameterBySemantic(emuenv, thread_id, export_name, program, semantic, index);
}

EXPORT(uint32_t, sceGxmProgramGetDefaultUniformBufferSize, const SceGxmProgram *program) {
    TRACY_FUNC(sceGxmProgramGetDefaultUniformBufferSize, program);
    return program->default_uniform_buffer_count * sizeof(Ptr<const void>);
}

EXPORT(uint32_t, sceGxmProgramGetFragmentProgramInputs, Ptr<const SceGxmProgram> program_) {
    TRACY_FUNC(sceGxmProgramGetFragmentProgramInputs, program_);
    const auto program = program_.get(emuenv.mem);

    return static_cast<uint32_t>(gxp::get_fragment_inputs(*program));
}

EXPORT(int, sceGxmProgramGetOutputRegisterFormat, const SceGxmProgram *program, SceGxmParameterType *type, uint32_t *componentCount) {
    TRACY_FUNC(sceGxmProgramGetOutputRegisterFormat, program, type, componentCount);
    if (!program || !type || !componentCount)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (!program->is_fragment())
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    return UNIMPLEMENTED();
}

EXPORT(Ptr<SceGxmProgramParameter>, sceGxmProgramGetParameter, const SceGxmProgram *program, uint32_t index) {
    TRACY_FUNC(sceGxmProgramGetParameter, program, index);
    const SceGxmProgramParameter *const parameters = reinterpret_cast<const SceGxmProgramParameter *>(reinterpret_cast<const uint8_t *>(&program->parameters_offset) + program->parameters_offset);

    const SceGxmProgramParameter *const parameter = &parameters[index];
    const uint8_t *const parameter_bytes = reinterpret_cast<const uint8_t *>(parameter);

    const Address parameter_address = static_cast<Address>(parameter_bytes - &emuenv.mem.memory[0]);
    return Ptr<SceGxmProgramParameter>(parameter_address);
}

EXPORT(uint32_t, sceGxmProgramGetParameterCount, const SceGxmProgram *program) {
    TRACY_FUNC(sceGxmProgramGetParameterCount, program);
    assert(program);
    return program->parameter_count;
}

EXPORT(uint32_t, sceGxmProgramGetSize, const SceGxmProgram *program) {
    TRACY_FUNC(sceGxmProgramGetSize, program);
    assert(program);
    return program->size;
}

EXPORT(SceGxmProgramType, sceGxmProgramGetType, const SceGxmProgram *program) {
    TRACY_FUNC(sceGxmProgramGetType, program);
    assert(program);
    return program->get_type();
}

EXPORT(uint32_t, sceGxmProgramGetVertexProgramOutputs, Ptr<const SceGxmProgram> program_) {
    TRACY_FUNC(sceGxmProgramGetVertexProgramOutputs, program_);
    const auto program = program_.get(emuenv.mem);

    return static_cast<uint32_t>(gxp::get_vertex_outputs(*program));
}

EXPORT(bool, sceGxmProgramIsDepthReplaceUsed, const SceGxmProgram *program) {
    TRACY_FUNC(sceGxmProgramIsDepthReplaceUsed, program);
    assert(program);
    return program->is_depth_replace_used();
}

EXPORT(bool, sceGxmProgramIsDiscardUsed, const SceGxmProgram *program) {
    TRACY_FUNC(sceGxmProgramIsDiscardUsed, program);
    assert(program);
    return program->is_discard_used();
}

EXPORT(bool, sceGxmProgramIsEquivalent, const SceGxmProgram *programA, const SceGxmProgram *programB) {
    TRACY_FUNC(sceGxmProgramIsEquivalent, programA, programB);
    if (!programA || !programB) {
        LOG_ERROR("SCE_GXM_ERROR_INVALID_POINTER");
        return false;
    }

    return (programA->size == programB->size) && (memcmp(programA, programB, programA->size) == 0);
}

EXPORT(bool, sceGxmProgramIsFragColorUsed, const SceGxmProgram *program) {
    TRACY_FUNC(sceGxmProgramIsFragColorUsed, program);
    assert(program);
    return program->is_frag_color_used();
}

EXPORT(bool, sceGxmProgramIsNativeColorUsed, const SceGxmProgram *program) {
    TRACY_FUNC(sceGxmProgramIsNativeColorUsed, program);
    assert(program);
    return program->is_native_color();
}

EXPORT(bool, sceGxmProgramIsSpriteCoordUsed, const SceGxmProgram *program) {
    TRACY_FUNC(sceGxmProgramIsSpriteCoordUsed, program);
    assert(program);
    return program->is_sprite_coord_used();
}

EXPORT(uint32_t, sceGxmProgramParameterGetArraySize, const SceGxmProgramParameter *parameter) {
    TRACY_FUNC(sceGxmProgramParameterGetArraySize, parameter);
    assert(parameter);
    return parameter->array_size;
}

EXPORT(int, sceGxmProgramParameterGetCategory, const SceGxmProgramParameter *parameter) {
    TRACY_FUNC(sceGxmProgramParameterGetCategory, parameter);
    assert(parameter);
    return parameter->category;
}

EXPORT(uint32_t, sceGxmProgramParameterGetComponentCount, const SceGxmProgramParameter *parameter) {
    TRACY_FUNC(sceGxmProgramParameterGetComponentCount, parameter);
    assert(parameter);
    return parameter->component_count;
}

EXPORT(uint32_t, sceGxmProgramParameterGetContainerIndex, const SceGxmProgramParameter *parameter) {
    TRACY_FUNC(sceGxmProgramParameterGetContainerIndex, parameter);
    assert(parameter);
    return parameter->container_index;
}

EXPORT(uint32_t, sceGxmProgramParameterGetIndex, const SceGxmProgram *program, const SceGxmProgramParameter *parameter) {
    TRACY_FUNC(sceGxmProgramParameterGetIndex, program, parameter);
    uint64_t parameter_offset = program->parameters_offset;

    if (parameter_offset > 0)
        parameter_offset += (uint64_t)&program->parameters_offset;
    return (uint32_t)((uint64_t)parameter - parameter_offset) >> 4;
}

EXPORT(Ptr<const char>, sceGxmProgramParameterGetName, Ptr<const SceGxmProgramParameter> parameter) {
    TRACY_FUNC(sceGxmProgramParameterGetName, parameter);
    if (!parameter)
        return {};
    return Ptr<const char>(parameter.address() + parameter.get(emuenv.mem)->name_offset);
}

EXPORT(uint32_t, sceGxmProgramParameterGetResourceIndex, const SceGxmProgramParameter *parameter) {
    TRACY_FUNC(sceGxmProgramParameterGetResourceIndex, parameter);
    assert(parameter);
    return parameter->resource_index;
}

EXPORT(int, sceGxmProgramParameterGetSemantic, const SceGxmProgramParameter *parameter) {
    TRACY_FUNC(sceGxmProgramParameterGetSemantic, parameter);
    assert(parameter);

    if (parameter->category != SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE)
        return SCE_GXM_PARAMETER_SEMANTIC_NONE;

    return parameter->semantic;
}

EXPORT(int, _sceGxmProgramParameterGetSemantic, const SceGxmProgramParameter *parameter) {
    TRACY_FUNC(_sceGxmProgramParameterGetSemantic, parameter);
    return export_sceGxmProgramParameterGetSemantic(emuenv, thread_id, export_name, parameter);
}

EXPORT(uint32_t, sceGxmProgramParameterGetSemanticIndex, const SceGxmProgramParameter *parameter) {
    TRACY_FUNC(sceGxmProgramParameterGetSemanticIndex, parameter);
    return parameter->semantic_index & 0xf;
}

EXPORT(int, sceGxmProgramParameterGetType, const SceGxmProgramParameter *parameter) {
    TRACY_FUNC(sceGxmProgramParameterGetType, parameter);
    return parameter->type;
}

EXPORT(bool, sceGxmProgramParameterIsRegFormat, const SceGxmProgram *program, const SceGxmProgramParameter *parameter) {
    TRACY_FUNC(sceGxmProgramParameterIsRegFormat, program, parameter);
    if (program->is_fragment()) {
        return false;
    }

    if (parameter->category != SceGxmParameterCategory::SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) {
        return false;
    }

    const uint64_t regformat_indices = program->vertex_varyings()->untyped_pa_regs;
    return (regformat_indices & (1ULL << parameter->resource_index)) != 0;
}

EXPORT(bool, sceGxmProgramParameterIsSamplerCube, const SceGxmProgramParameter *parameter) {
    TRACY_FUNC(sceGxmProgramParameterIsSamplerCube, parameter);
    return parameter->is_sampler_cube();
}

EXPORT(int, sceGxmPushUserMarker) {
    TRACY_FUNC(sceGxmPushUserMarker);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmRemoveRazorGpuCaptureBuffer) {
    TRACY_FUNC(sceGxmRemoveRazorGpuCaptureBuffer);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmRenderTargetGetDriverMemBlock, const SceGxmRenderTarget *renderTarget, SceUID *driverMemBlock) {
    TRACY_FUNC(sceGxmRenderTargetGetDriverMemBlock, renderTarget, driverMemBlock);
    if (!renderTarget || !driverMemBlock) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    *driverMemBlock = renderTarget->driverMemBlock;

    return 0;
}

EXPORT(int, sceGxmReserveFragmentDefaultUniformBuffer, SceGxmContext *context, Ptr<void> *uniformBuffer) {
    TRACY_FUNC(sceGxmReserveFragmentDefaultUniformBuffer, context, uniformBuffer);
    if (!context || !uniformBuffer)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    *uniformBuffer = context->state.fragment_ring_buffer.cast<uint8_t>() + static_cast<int32_t>(context->state.fragment_ring_buffer_used);

    const auto fragment_program = context->state.fragment_program.get(emuenv.mem);
    const auto program = fragment_program->program.get(emuenv.mem);

    const size_t size = (size_t)program->default_uniform_buffer_count * 4;
    const size_t next_used = context->state.fragment_ring_buffer_used + size;
    assert(next_used <= context->state.fragment_ring_buffer_size);
    if (next_used > context->state.fragment_ring_buffer_size) {
        if (context->state.type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
            // TODO: I think this should cause a force flush
            context->state.fragment_ring_buffer_used = 0;
        } else {
            context->state.fragment_ring_buffer = gxmRunDeferredMemoryCallback(emuenv.kernel, emuenv.mem, emuenv.gxm.callback_lock, context->state.fragment_ring_buffer_size,
                context->state.fragment_memory_callback, context->state.memory_callback_userdata, DEFAULT_RING_SIZE, thread_id);

            if (!context->state.fragment_ring_buffer) {
                return RET_ERROR(SCE_GXM_ERROR_RESERVE_FAILED);
            }

            context->state.fragment_ring_buffer_used = 0;
        }
    } else {
        context->state.fragment_ring_buffer_used = next_used;
    }

    context->state.fragment_uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX] = *uniformBuffer;

    return 0;
}

EXPORT(int, sceGxmRenderTargetGetHostMem) {
    TRACY_FUNC(sceGxmRenderTargetGetHostMem);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmReserveVertexDefaultUniformBuffer, SceGxmContext *context, Ptr<void> *uniformBuffer) {
    TRACY_FUNC(sceGxmReserveVertexDefaultUniformBuffer, context, uniformBuffer);
    if (!context || !uniformBuffer)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    *uniformBuffer = context->state.vertex_ring_buffer.cast<uint8_t>() + static_cast<int32_t>(context->state.vertex_ring_buffer_used);

    const auto vertex_program = context->state.vertex_program.get(emuenv.mem);
    const auto program = vertex_program->program.get(emuenv.mem);

    const size_t size = (size_t)program->default_uniform_buffer_count * 4;
    const size_t next_used = context->state.vertex_ring_buffer_used + size;
    assert(next_used <= context->state.vertex_ring_buffer_size);
    if (next_used > context->state.vertex_ring_buffer_size) {
        if (context->state.type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
            // TODO: I think this should cause a force flush
            context->state.vertex_ring_buffer_used = 0;
        } else {
            context->state.vertex_ring_buffer = gxmRunDeferredMemoryCallback(emuenv.kernel, emuenv.mem, emuenv.gxm.callback_lock, context->state.vertex_ring_buffer_size,
                context->state.vertex_memory_callback, context->state.memory_callback_userdata, DEFAULT_RING_SIZE, thread_id);

            if (!context->state.vertex_ring_buffer) {
                return RET_ERROR(SCE_GXM_ERROR_RESERVE_FAILED);
            }

            context->state.vertex_ring_buffer_used = 0;
        }
    } else {
        context->state.vertex_ring_buffer_used = next_used;
    }

    // context->state.vertex_last_reserve_status = SceGxmLastReserveStatus::Available;

    context->state.vertex_uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX] = *uniformBuffer;

    return 0;
}

EXPORT(int, sceGxmSetAuxiliarySurface) {
    TRACY_FUNC(sceGxmSetAuxiliarySurface);
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetBackDepthBias, SceGxmContext *context, int32_t factor, int32_t units) {
    TRACY_FUNC(sceGxmSetBackDepthBias, context, factor, units);
    if ((context->state.back_depth_bias_factor != factor) || (context->state.back_depth_bias_units != units)) {
        context->state.back_depth_bias_factor = factor;
        context->state.back_depth_bias_units = units;

        if (context->alloc_space) {
            renderer::set_depth_bias(*emuenv.renderer, context->renderer.get(), false, factor, units);
        }
    }
}

EXPORT(void, sceGxmSetBackDepthFunc, SceGxmContext *context, SceGxmDepthFunc depthFunc) {
    TRACY_FUNC(sceGxmSetBackDepthFunc, context, depthFunc);
    if (context->state.back_depth_func != depthFunc) {
        context->state.back_depth_func = depthFunc;

        if (context->alloc_space) {
            renderer::set_depth_func(*emuenv.renderer, context->renderer.get(), false, depthFunc);
        }
    }
}

EXPORT(void, sceGxmSetBackDepthWriteEnable, SceGxmContext *context, SceGxmDepthWriteMode enable) {
    TRACY_FUNC(sceGxmSetBackDepthWriteEnable, context, enable);
    if (context->state.back_depth_write_enable != enable) {
        context->state.back_depth_write_enable = enable;

        if (context->alloc_space) {
            renderer::set_depth_write_enable_mode(*emuenv.renderer, context->renderer.get(), false, enable);
        }
    }
}

EXPORT(void, sceGxmSetBackFragmentProgramEnable, SceGxmContext *context, SceGxmFragmentProgramMode enable) {
    TRACY_FUNC(sceGxmSetBackFragmentProgramEnable, context, enable);
    renderer::set_side_fragment_program_enable(*emuenv.renderer, context->renderer.get(), false, enable);
}

EXPORT(void, sceGxmSetBackLineFillLastPixelEnable, SceGxmContext *context, SceGxmLineFillLastPixelMode enable) {
    TRACY_FUNC(sceGxmSetBackLineFillLastPixelEnable, context, enable);
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetBackPointLineWidth, SceGxmContext *context, uint32_t width) {
    TRACY_FUNC(sceGxmSetBackPointLineWidth, context, width);
    if (context->state.back_point_line_width != width) {
        context->state.back_point_line_width = width;

        if (context->alloc_space) {
            renderer::set_point_line_width(*emuenv.renderer, context->renderer.get(), false, width);
        }
    }
}

EXPORT(void, sceGxmSetBackPolygonMode, SceGxmContext *context, SceGxmPolygonMode mode) {
    TRACY_FUNC(sceGxmSetBackPolygonMode, context, mode);
    if (context->state.back_polygon_mode != mode) {
        context->state.back_polygon_mode = mode;

        if (context->alloc_space) {
            renderer::set_polygon_mode(*emuenv.renderer, context->renderer.get(), false, mode);
        }
    }
}

EXPORT(void, sceGxmSetBackStencilFunc, SceGxmContext *context, SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail, SceGxmStencilOp depthPass, int32_t compareMask, uint32_t writeMask) {
    TRACY_FUNC(sceGxmSetBackStencilFunc, context, func, stencilFail, depthFail, depthPass, compareMask, writeMask);
    // compareMask and depthMask should be uint8_t, however the compiler optimizes the call if this is the case...
    const uint8_t compare_mask = static_cast<uint8_t>(compareMask);
    const uint8_t write_mask = static_cast<uint8_t>(writeMask);
    if ((context->state.back_stencil.func != func)
        || (context->state.back_stencil.stencil_fail != stencilFail)
        || (context->state.back_stencil.depth_fail != depthFail)
        || (context->state.back_stencil.depth_pass != depthPass)
        || (context->state.back_stencil.compare_mask != compare_mask)
        || (context->state.back_stencil.write_mask != write_mask)) {
        context->state.back_stencil.func = func;
        context->state.back_stencil.stencil_fail = stencilFail;
        context->state.back_stencil.depth_fail = depthFail;
        context->state.back_stencil.depth_pass = depthPass;
        context->state.back_stencil.compare_mask = compare_mask;
        context->state.back_stencil.write_mask = write_mask;

        if (context->alloc_space) {
            renderer::set_stencil_func(*emuenv.renderer, context->renderer.get(), false, func, stencilFail, depthFail, depthPass, compare_mask, write_mask);
        }
    }
}

EXPORT(void, sceGxmSetBackStencilRef, SceGxmContext *context, uint8_t sref) {
    TRACY_FUNC(sceGxmSetBackStencilRef, context, sref);
    if (context->state.back_stencil.ref != sref) {
        context->state.back_stencil.ref = sref;

        if (context->alloc_space)
            renderer::set_stencil_ref(*emuenv.renderer, context->renderer.get(), false, sref);
    }
}

EXPORT(void, sceGxmSetBackVisibilityTestEnable, SceGxmContext *context, SceGxmVisibilityTestMode enable) {
    TRACY_FUNC(sceGxmSetBackVisibilityTestEnable, context, enable);
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetBackVisibilityTestIndex, SceGxmContext *context, uint32_t index) {
    TRACY_FUNC(sceGxmSetBackVisibilityTestIndex, context, index);
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetBackVisibilityTestOp, SceGxmContext *context, SceGxmVisibilityTestOp op) {
    TRACY_FUNC(sceGxmSetBackVisibilityTestOp, context, op);
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetCullMode, SceGxmContext *context, SceGxmCullMode mode) {
    TRACY_FUNC(sceGxmSetCullMode, context, mode);
    if (context->state.cull_mode != mode) {
        context->state.cull_mode = mode;

        if (context->alloc_space)
            renderer::set_cull_mode(*emuenv.renderer, context->renderer.get(), mode);
    }
}

static constexpr const std::uint32_t SCE_GXM_DEFERRED_CONTEXT_MINIMUM_BUFFER_SIZE = 1024;

EXPORT(int, sceGxmSetDeferredContextFragmentBuffer, SceGxmContext *deferredContext, Ptr<void> mem, uint32_t size) {
    TRACY_FUNC(sceGxmSetDeferredContextFragmentBuffer, deferredContext, mem, size);
    if (!deferredContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (deferredContext->state.type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (deferredContext->state.active) {
        return RET_ERROR(SCE_GXM_ERROR_WITHIN_COMMAND_LIST);
    }

    if ((size != 0) && (size < SCE_GXM_DEFERRED_CONTEXT_MINIMUM_BUFFER_SIZE)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (mem && !size) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    // Use the one specified
    deferredContext->state.fragment_ring_buffer = mem;
    deferredContext->state.fragment_ring_buffer_size = size;

    return 0;
}

EXPORT(int, sceGxmSetDeferredContextVdmBuffer, SceGxmContext *deferredContext, Ptr<void> mem, uint32_t size) {
    TRACY_FUNC(sceGxmSetDeferredContextVdmBuffer, deferredContext, mem, size);
    if (!deferredContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (deferredContext->state.type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (deferredContext->state.active) {
        return RET_ERROR(SCE_GXM_ERROR_WITHIN_COMMAND_LIST);
    }

    // Use the one specified
    deferredContext->state.vdm_buffer = mem;
    deferredContext->state.vdm_buffer_size = size;

    // make sure the next call will use the new vdm buffer
    deferredContext->alloc_space = deferredContext->alloc_space_end;

    return 0;
}

EXPORT(int, sceGxmSetDeferredContextVertexBuffer, SceGxmContext *deferredContext, Ptr<void> mem, uint32_t size) {
    TRACY_FUNC(sceGxmSetDeferredContextVertexBuffer, deferredContext, mem, size);
    if (!deferredContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (deferredContext->state.type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (deferredContext->state.active) {
        return RET_ERROR(SCE_GXM_ERROR_WITHIN_COMMAND_LIST);
    }

    if ((size != 0) && (size < SCE_GXM_DEFERRED_CONTEXT_MINIMUM_BUFFER_SIZE)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (mem && !size) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    // Use the one specified
    deferredContext->state.vertex_ring_buffer = mem;
    deferredContext->state.vertex_ring_buffer_size = size;

    return 0;
}

EXPORT(int, sceGxmSetFragmentDefaultUniformBuffer, SceGxmContext *context, Ptr<const void> bufferData) {
    TRACY_FUNC(sceGxmSetFragmentDefaultUniformBuffer, context, bufferData);
    if (!context || !bufferData) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    context->state.fragment_uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX] = bufferData;
    return 0;
}

EXPORT(void, sceGxmSetFragmentProgram, SceGxmContext *context, Ptr<const SceGxmFragmentProgram> fragmentProgram) {
    TRACY_FUNC(sceGxmSetFragmentProgram, context, fragmentProgram);
    if (!context || !fragmentProgram)
        return;

    context->state.fragment_program = fragmentProgram;
    renderer::set_program(*emuenv.renderer, context->renderer.get(), fragmentProgram, true);
}

EXPORT(int, sceGxmSetFragmentTexture, SceGxmContext *context, uint32_t textureIndex, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmSetFragmentTexture, context, textureIndex, texture);
    if (!context || !texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (textureIndex > (SCE_GXM_MAX_TEXTURE_UNITS - 1)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    context->state.textures[textureIndex] = *texture;
    context->is_frag_texture_dirty[textureIndex] = true;

    return 0;
}

EXPORT(int, sceGxmSetFragmentUniformBuffer, SceGxmContext *context, uint32_t bufferIndex, Ptr<const void> bufferData) {
    TRACY_FUNC(sceGxmSetFragmentUniformBuffer, context, bufferIndex, bufferData);
    if (!context || !bufferData) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (bufferIndex > (SCE_GXM_MAX_UNIFORM_BUFFERS - 1)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    context->state.fragment_uniform_buffers[bufferIndex] = bufferData;
    return 0;
}

EXPORT(void, sceGxmSetFrontDepthBias, SceGxmContext *context, int32_t factor, int32_t units) {
    TRACY_FUNC(sceGxmSetFrontDepthBias, context, factor, units);
    if ((context->state.front_depth_bias_factor != factor) || (context->state.front_depth_bias_units != units)) {
        context->state.front_depth_bias_factor = factor;
        context->state.front_depth_bias_units = units;

        if (context->alloc_space)
            renderer::set_depth_bias(*emuenv.renderer, context->renderer.get(), true, factor, units);
    }
}

EXPORT(void, sceGxmSetFrontDepthFunc, SceGxmContext *context, SceGxmDepthFunc depthFunc) {
    TRACY_FUNC(sceGxmSetFrontDepthFunc, context, depthFunc);
    if (context->state.front_depth_func != depthFunc) {
        context->state.front_depth_func = depthFunc;

        if (context->alloc_space) {
            renderer::set_depth_func(*emuenv.renderer, context->renderer.get(), true, depthFunc);
        }
    }
}

EXPORT(void, sceGxmSetFrontDepthWriteEnable, SceGxmContext *context, SceGxmDepthWriteMode enable) {
    TRACY_FUNC(sceGxmSetFrontDepthWriteEnable, context, enable);
    if (context->state.front_depth_write_enable != enable) {
        context->state.front_depth_write_enable = enable;

        if (context->alloc_space) {
            renderer::set_depth_write_enable_mode(*emuenv.renderer, context->renderer.get(), true, enable);
        }
    }
}

EXPORT(void, sceGxmSetFrontFragmentProgramEnable, SceGxmContext *context, SceGxmFragmentProgramMode enable) {
    TRACY_FUNC(sceGxmSetFrontFragmentProgramEnable, context, enable);
    renderer::set_side_fragment_program_enable(*emuenv.renderer, context->renderer.get(), true, enable);
}

EXPORT(void, sceGxmSetFrontLineFillLastPixelEnable, SceGxmContext *context, SceGxmLineFillLastPixelMode enable) {
    TRACY_FUNC(sceGxmSetFrontLineFillLastPixelEnable, context, enable);
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetFrontPointLineWidth, SceGxmContext *context, uint32_t width) {
    TRACY_FUNC(sceGxmSetFrontPointLineWidth, context, width);
    if (context->state.front_point_line_width != width) {
        context->state.front_point_line_width = width;

        if (context->alloc_space) {
            renderer::set_point_line_width(*emuenv.renderer, context->renderer.get(), true, width);
        }
    }
}

EXPORT(void, sceGxmSetFrontPolygonMode, SceGxmContext *context, SceGxmPolygonMode mode) {
    TRACY_FUNC(sceGxmSetFrontPolygonMode, context, mode);
    if (context->state.front_polygon_mode != mode) {
        context->state.front_polygon_mode = mode;

        if (context->alloc_space) {
            renderer::set_polygon_mode(*emuenv.renderer, context->renderer.get(), true, mode);
        }
    }
}

EXPORT(void, sceGxmSetFrontStencilFunc, SceGxmContext *context, SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail, SceGxmStencilOp depthPass, int32_t compareMask, uint32_t writeMask) {
    TRACY_FUNC(sceGxmSetFrontStencilFunc, context, func, stencilFail, depthFail, depthPass, compareMask, writeMask);
    // compareMask and depthMask should be uint8_t, however the compiler optimizes the call if this is the case...
    const uint8_t compare_mask = static_cast<uint8_t>(compareMask);
    const uint8_t write_mask = static_cast<uint8_t>(writeMask);
    if ((context->state.front_stencil.func != func)
        || (context->state.front_stencil.stencil_fail != stencilFail)
        || (context->state.front_stencil.depth_fail != depthFail)
        || (context->state.front_stencil.depth_pass != depthPass)
        || (context->state.front_stencil.compare_mask != compare_mask)
        || (context->state.front_stencil.write_mask != write_mask)) {
        context->state.front_stencil.func = func;
        context->state.front_stencil.depth_fail = depthFail;
        context->state.front_stencil.depth_pass = depthPass;
        context->state.front_stencil.stencil_fail = stencilFail;
        context->state.front_stencil.compare_mask = compare_mask;
        context->state.front_stencil.write_mask = write_mask;

        if (context->alloc_space)
            renderer::set_stencil_func(*emuenv.renderer, context->renderer.get(), true, func, stencilFail, depthFail, depthPass, compare_mask, write_mask);
    }
}

EXPORT(void, sceGxmSetFrontStencilRef, SceGxmContext *context, uint8_t sref) {
    TRACY_FUNC(sceGxmSetFrontStencilRef, context, sref);
    if (context->state.front_stencil.ref != sref) {
        context->state.front_stencil.ref = sref;
        if (context->alloc_space) {
            renderer::set_stencil_ref(*emuenv.renderer, context->renderer.get(), true, sref);
        }
    }
}

EXPORT(void, sceGxmSetFrontVisibilityTestEnable, SceGxmContext *context, SceGxmVisibilityTestMode enable) {
    TRACY_FUNC(sceGxmSetFrontVisibilityTestEnable, context, enable);
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetFrontVisibilityTestIndex, SceGxmContext *context, uint32_t index) {
    TRACY_FUNC(sceGxmSetFrontVisibilityTestIndex, context, index);
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetFrontVisibilityTestOp, SceGxmContext *context, SceGxmVisibilityTestOp op) {
    TRACY_FUNC(sceGxmSetFrontVisibilityTestOp, context, op);
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetPrecomputedFragmentState, SceGxmContext *context, Ptr<SceGxmPrecomputedFragmentState> state) {
    TRACY_FUNC(sceGxmSetPrecomputedFragmentState, context, state);
    if (!state) {
        context->state.precomputed_fragment_state.reset();
        return;
    }

    context->state.precomputed_fragment_state = state;
}

EXPORT(void, sceGxmSetPrecomputedVertexState, SceGxmContext *context, Ptr<SceGxmPrecomputedVertexState> state) {
    TRACY_FUNC(sceGxmSetPrecomputedVertexState, context, state);
    if (!state) {
        context->state.precomputed_vertex_state.reset();
        return;
    }

    context->state.precomputed_vertex_state = state;
}

EXPORT(void, sceGxmSetRegionClip, SceGxmContext *context, SceGxmRegionClipMode mode, uint32_t xMin, uint32_t yMin, uint32_t xMax, uint32_t yMax) {
    TRACY_FUNC(sceGxmSetRegionClip, context, mode, xMin, yMin, xMax, yMax);
    bool change_detected = false;

    if (context->state.region_clip_mode != mode) {
        context->state.region_clip_mode = mode;
        change_detected = true;
    }
    // Set it right here now
    if ((context->state.region_clip_min.x != xMin) || (context->state.region_clip_min.y != yMin) || (context->state.region_clip_max.x != xMax)
        || (context->state.region_clip_max.y != yMax)) {
        context->state.region_clip_min.x = xMin;
        context->state.region_clip_min.y = yMin;
        context->state.region_clip_max.x = xMax;
        context->state.region_clip_max.y = yMax;

        change_detected = true;
    }

    if (change_detected && context->alloc_space)
        renderer::set_region_clip(*emuenv.renderer, context->renderer.get(), mode, xMin, xMax, yMin, yMax);
}

EXPORT(void, sceGxmSetTwoSidedEnable, SceGxmContext *context, SceGxmTwoSidedMode mode) {
    TRACY_FUNC(sceGxmSetTwoSidedEnable, context, mode);
    if (context->state.two_sided != mode) {
        context->state.two_sided = mode;

        if (context->alloc_space) {
            renderer::set_two_sided_enable(*emuenv.renderer, context->renderer.get(), mode);
        }
    }
}

template <typename T>
static void convert_uniform_data(std::vector<std::uint8_t> &converted_data, const float *sourceData, uint32_t componentCount) {
    converted_data.resize(componentCount * sizeof(T));
    for (std::uint32_t i = 0; i < componentCount; ++i) {
        T converted = static_cast<T>(sourceData[i]);
        std::memcpy(&converted_data[i * sizeof(T)], &converted, sizeof(T));
    }
}

EXPORT(int, sceGxmSetUniformDataF, void *uniformBuffer, const SceGxmProgramParameter *parameter, uint32_t componentOffset, uint32_t componentCount, const float *sourceData) {
    TRACY_FUNC(sceGxmSetUniformDataF, uniformBuffer, parameter, componentOffset, componentCount, sourceData);
    assert(parameter);

    if (!uniformBuffer || !parameter || !sourceData)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (parameter->category != SceGxmParameterCategory::SCE_GXM_PARAMETER_CATEGORY_UNIFORM)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    size_t size = 0;
    size_t offset = 0;
    bool is_float = false;

    const std::uint16_t param_type = parameter->type;

    // Component size is in bytes
    int comp_size = gxp::get_parameter_type_size(static_cast<SceGxmParameterType>(param_type));
    const std::uint8_t *source = reinterpret_cast<const std::uint8_t *>(sourceData);
    std::vector<std::uint8_t> converted_data;

    switch (parameter->type) {
    case SCE_GXM_PARAMETER_TYPE_S8: {
        convert_uniform_data<int8_t>(converted_data, sourceData, componentCount);
        source = converted_data.data();
        break;
    }

    case SCE_GXM_PARAMETER_TYPE_U8: {
        convert_uniform_data<uint8_t>(converted_data, sourceData, componentCount);
        source = converted_data.data();
        break;
    }

    case SCE_GXM_PARAMETER_TYPE_U16: {
        convert_uniform_data<uint16_t>(converted_data, sourceData, componentCount);
        source = converted_data.data();
        break;
    }

    case SCE_GXM_PARAMETER_TYPE_S16: {
        convert_uniform_data<int16_t>(converted_data, sourceData, componentCount);
        source = converted_data.data();
        break;
    }

    case SCE_GXM_PARAMETER_TYPE_U32: {
        convert_uniform_data<uint32_t>(converted_data, sourceData, componentCount);
        source = converted_data.data();
        break;
    }

    case SCE_GXM_PARAMETER_TYPE_S32: {
        convert_uniform_data<int32_t>(converted_data, sourceData, componentCount);
        source = converted_data.data();
        break;
    }

    case SCE_GXM_PARAMETER_TYPE_F16: {
        converted_data.resize(((componentCount + 7) / 8) * 8 * 2);
        float_to_half(sourceData, reinterpret_cast<std::uint16_t *>(converted_data.data()), componentCount);

        source = converted_data.data();
        is_float = true;
        break;
    }

    case SCE_GXM_PARAMETER_TYPE_F32: {
        is_float = true;
        break;
    }
    default:
        assert(false);
    }

    if (parameter->array_size == 1 || parameter->component_count == 1) {
        // Case 1: No array. Only a single vector. Don't apply any alignment
        // Case 2: Array but component count equals to 1. This case, a scalar array, align it to 32-bit bound
        if (parameter->component_count == 1) {
            // Apply 32 bit alignment, by making each component has 4 bytes
            comp_size = 4;
        }

        size = componentCount * comp_size;
        offset = parameter->resource_index * sizeof(float) + componentOffset * comp_size;

        memcpy(static_cast<uint8_t *>(uniformBuffer) + offset, source, size);
    } else {
        // This is the size of each element.
        size = parameter->component_count * comp_size;
        int align_bytes = 0;

        if (is_float) {
            // Align it to 64-bit boundary (8 bytes)
            if ((size & 7) != 0) {
                align_bytes = 8 - (size & 7);
            }
        } else {
            // Align it to 32-bit boundary (4 bytes)
            if ((size & 3) != 0) {
                align_bytes = 4 - (size & 3);
            }
        }

        // wtf
        // wtf
        const int vec_to_start_write = componentOffset / parameter->component_count;
        int component_cursor_inside_vector = (componentOffset % parameter->component_count);
        std::uint8_t *dest = reinterpret_cast<uint8_t *>(uniformBuffer) + parameter->resource_index * sizeof(float)
            + vec_to_start_write * (size + align_bytes) + component_cursor_inside_vector * comp_size;

        int component_to_copy_remain_per_elem = parameter->component_count - component_cursor_inside_vector;
        int component_left_to_copy = componentCount;

        while (component_left_to_copy > 0) {
            memcpy(dest, source, component_to_copy_remain_per_elem * comp_size);

            // Add and align destination
            dest += comp_size * component_to_copy_remain_per_elem + align_bytes;
            source += component_to_copy_remain_per_elem * comp_size;

            component_left_to_copy -= component_to_copy_remain_per_elem;
            component_to_copy_remain_per_elem = std::min<int>(4, component_to_copy_remain_per_elem);
        }
    }

    return 0;
}

EXPORT(int, sceGxmSetUserMarker) {
    TRACY_FUNC(sceGxmSetUserMarker);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetValidationEnable) {
    TRACY_FUNC(sceGxmSetValidationEnable);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetVertexDefaultUniformBuffer, SceGxmContext *context, Ptr<const void> bufferData) {
    TRACY_FUNC(sceGxmSetVertexDefaultUniformBuffer, context, bufferData);
    if (!context || !bufferData) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    context->state.vertex_uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX] = bufferData;
    return 0;
}

EXPORT(void, sceGxmSetVertexProgram, SceGxmContext *context, Ptr<const SceGxmVertexProgram> vertexProgram) {
    TRACY_FUNC(sceGxmSetVertexProgram, context, vertexProgram);
    if (!context || !vertexProgram)
        return;

    context->state.vertex_program = vertexProgram;
    renderer::set_program(*emuenv.renderer, context->renderer.get(), vertexProgram, false);
}

EXPORT(int, sceGxmSetVertexStream, SceGxmContext *context, uint32_t streamIndex, Ptr<const void> streamData) {
    TRACY_FUNC(sceGxmSetVertexStream, context, streamIndex, streamData);
    if (!context || !streamData)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (streamIndex > (SCE_GXM_MAX_VERTEX_STREAMS - 1))
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    context->state.stream_data[streamIndex] = streamData;
    return 0;
}

EXPORT(int, sceGxmSetVertexTexture, SceGxmContext *context, uint32_t textureIndex, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmSetVertexTexture, context, textureIndex, texture);
    if (!context || !texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (textureIndex > (SCE_GXM_MAX_TEXTURE_UNITS - 1)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    context->state.textures[textureIndex + SCE_GXM_MAX_TEXTURE_UNITS] = *texture;
    context->is_vert_texture_dirty[textureIndex] = true;

    return 0;
}

EXPORT(int, _sceGxmSetVertexTexture, SceGxmContext *context, uint32_t textureIndex, const SceGxmTexture *texture) {
    TRACY_FUNC(_sceGxmSetVertexTexture, context, textureIndex, texture);
    return CALL_EXPORT(sceGxmSetVertexTexture, context, textureIndex, texture);
}

EXPORT(int, sceGxmSetVertexUniformBuffer, SceGxmContext *context, uint32_t bufferIndex, Ptr<const void> bufferData) {
    TRACY_FUNC(sceGxmSetVertexUniformBuffer, context, bufferIndex, bufferData);
    if (!context || !bufferData)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (bufferIndex > (SCE_GXM_MAX_UNIFORM_BUFFERS - 1))
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    context->state.vertex_uniform_buffers[bufferIndex] = bufferData;
    return 0;
}

EXPORT(void, sceGxmSetViewport, SceGxmContext *context, float xOffset, float xScale, float yOffset, float yScale, float zOffset, float zScale) {
    TRACY_FUNC(sceGxmSetViewport, context, xOffset, xScale, yOffset, yScale, zOffset, zScale);
    // Set viewport to enable, enable more offset and scale to set
    if (context->state.viewport.offset.x != xOffset || (context->state.viewport.offset.y != yOffset) || (context->state.viewport.offset.z != zOffset)
        || (context->state.viewport.scale.x != xScale) || (context->state.viewport.scale.y != yScale) || (context->state.viewport.scale.z != zScale)) {
        context->state.viewport.offset.x = xOffset;
        context->state.viewport.offset.y = yOffset;
        context->state.viewport.offset.z = zOffset;
        context->state.viewport.scale.x = xScale;
        context->state.viewport.scale.y = yScale;
        context->state.viewport.scale.z = zScale;

        if (context->alloc_space) {
            if (context->state.viewport.enable == SCE_GXM_VIEWPORT_ENABLED) {
                renderer::set_viewport_real(*emuenv.renderer, context->renderer.get(), context->state.viewport.offset.x,
                    context->state.viewport.offset.y, context->state.viewport.offset.z, context->state.viewport.scale.x, context->state.viewport.scale.y,
                    context->state.viewport.scale.z);
            } else {
                renderer::set_viewport_flat(*emuenv.renderer, context->renderer.get());
            }
        }
    }
}

EXPORT(void, sceGxmSetViewportEnable, SceGxmContext *context, SceGxmViewportMode enable) {
    TRACY_FUNC(sceGxmSetViewportEnable, context, enable);
    // Set viewport to enable/disable, no additional offset and scale to set.
    if (context->state.viewport.enable != enable) {
        context->state.viewport.enable = enable;

        if (context->alloc_space) {
            if (context->state.viewport.enable == SCE_GXM_VIEWPORT_DISABLED) {
                renderer::set_viewport_flat(*emuenv.renderer, context->renderer.get());
            } else {
                renderer::set_viewport_real(*emuenv.renderer, context->renderer.get(), context->state.viewport.offset.x,
                    context->state.viewport.offset.y, context->state.viewport.offset.z, context->state.viewport.scale.x, context->state.viewport.scale.y,
                    context->state.viewport.scale.z);
            }
        }
    }
}

EXPORT(int, sceGxmSetVisibilityBuffer, SceGxmContext *immediateContext, Ptr<void> bufferBase, uint32_t stridePerCore) {
    TRACY_FUNC(sceGxmSetVisibilityBuffer, immediateContext, bufferBase, stridePerCore);
    if (!immediateContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (immediateContext->state.type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    if (bufferBase.address() & (SCE_GXM_VISIBILITY_ALIGNMENT - 1))
        return RET_ERROR(SCE_GXM_ERROR_INVALID_ALIGNMENT);

    STUBBED("Set all visible");

    memset(bufferBase.get(emuenv.mem), 0xFF, SCE_GXM_GPU_CORE_COUNT * stridePerCore);

    return 0;
}

EXPORT(void, sceGxmSetWBufferEnable) {
    TRACY_FUNC(sceGxmSetWBufferEnable);
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetWClampEnable) {
    TRACY_FUNC(sceGxmSetWClampEnable);
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetWClampValue, SceGxmContext *context, float clampValue) {
    TRACY_FUNC(sceGxmSetWClampValue, context, clampValue);
    UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetWarningEnabled) {
    TRACY_FUNC(sceGxmSetWarningEnabled);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetYuvProfile) {
    TRACY_FUNC(sceGxmSetYuvProfile);
    return UNIMPLEMENTED();
}

Address alloc_callbacked(EmuEnvState &emuenv, SceUID thread_id, const SceGxmShaderPatcherParams &shaderPatcherParams, unsigned int size) {
    if (!shaderPatcherParams.hostAllocCallback) {
        LOG_ERROR("Empty hostAllocCallback");
    }
    const auto thread = emuenv.kernel.get_thread(thread_id);
    auto result = thread->run_callback(shaderPatcherParams.hostAllocCallback.address(), { shaderPatcherParams.userData.address(), size });
    return result;
}

template <typename T>
Ptr<T> alloc_callbacked(EmuEnvState &emuenv, SceUID thread_id, const SceGxmShaderPatcherParams &shaderPatcherParams) {
    const Address address = alloc_callbacked(emuenv, thread_id, shaderPatcherParams, sizeof(T));
    const Ptr<T> ptr(address);
    if (!ptr) {
        return ptr;
    }
    T *const memory = ptr.get(emuenv.mem);
    new (memory) T;
    return ptr;
}

template <typename T>
Ptr<T> alloc_callbacked(EmuEnvState &emuenv, SceUID thread_id, SceGxmShaderPatcher *shaderPatcher) {
    return alloc_callbacked<T>(emuenv, thread_id, shaderPatcher->params);
}

void free_callbacked(EmuEnvState &emuenv, SceUID thread_id, SceGxmShaderPatcher *shaderPatcher, Address data) {
    if (!shaderPatcher->params.hostFreeCallback) {
        LOG_ERROR("Empty hostFreeCallback");
    }
    const auto thread = emuenv.kernel.get_thread(thread_id);
    thread->run_callback(shaderPatcher->params.hostFreeCallback.address(), { shaderPatcher->params.userData.address(), data });
}

template <typename T>
void free_callbacked(EmuEnvState &emuenv, SceUID thread_id, SceGxmShaderPatcher *shaderPatcher, Ptr<T> data) {
    free_callbacked(emuenv, thread_id, shaderPatcher, data.address());
}

EXPORT(int, sceGxmShaderPatcherAddRefFragmentProgram, SceGxmShaderPatcher *shaderPatcher, SceGxmFragmentProgram *fragmentProgram) {
    TRACY_FUNC(sceGxmShaderPatcherAddRefFragmentProgram, shaderPatcher, fragmentProgram);
    if (!shaderPatcher || !fragmentProgram)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    ++fragmentProgram->reference_count;

    return 0;
}

EXPORT(int, sceGxmShaderPatcherAddRefVertexProgram, SceGxmShaderPatcher *shaderPatcher, SceGxmVertexProgram *vertexProgram) {
    TRACY_FUNC(sceGxmShaderPatcherAddRefVertexProgram, shaderPatcher, vertexProgram);
    if (!shaderPatcher || !vertexProgram)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    ++vertexProgram->reference_count;

    return 0;
}

EXPORT(int, sceGxmShaderPatcherCreate, const SceGxmShaderPatcherParams *params, Ptr<SceGxmShaderPatcher> *shaderPatcher) {
    TRACY_FUNC(sceGxmShaderPatcherCreate, params, shaderPatcher);
    if (!params || !shaderPatcher)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    *shaderPatcher = alloc_callbacked<SceGxmShaderPatcher>(emuenv, thread_id, *params);
    assert(*shaderPatcher);
    if (!*shaderPatcher) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }
    shaderPatcher->get(emuenv.mem)->params = *params;
    return 0;
}

EXPORT(int, sceGxmShaderPatcherCreateFragmentProgram, SceGxmShaderPatcher *shaderPatcher, const SceGxmRegisteredProgram *programId, SceGxmOutputRegisterFormat outputFormat, SceGxmMultisampleMode multisampleMode, const SceGxmBlendInfo *blendInfo, Ptr<const SceGxmProgram> vertexProgram, Ptr<SceGxmFragmentProgram> *fragmentProgram) {
    TRACY_FUNC(sceGxmShaderPatcherCreateFragmentProgram, shaderPatcher, programId, outputFormat, multisampleMode, blendInfo, vertexProgram, fragmentProgram);
    MemState &mem = emuenv.mem;

    if (!shaderPatcher || !programId || !fragmentProgram)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    static const SceGxmBlendInfo default_blend_info = {
        SCE_GXM_COLOR_MASK_ALL,
        SCE_GXM_BLEND_FUNC_NONE,
        SCE_GXM_BLEND_FUNC_NONE,
        SCE_GXM_BLEND_FACTOR_ONE,
        SCE_GXM_BLEND_FACTOR_ZERO,
        SCE_GXM_BLEND_FACTOR_ONE,
        SCE_GXM_BLEND_FACTOR_ZERO
    };
    const FragmentProgramCacheKey key = {
        *programId,
        (blendInfo != nullptr) ? *blendInfo : default_blend_info
    };
    FragmentProgramCache::const_iterator cached = shaderPatcher->fragment_program_cache.find(key);
    if (cached != shaderPatcher->fragment_program_cache.end()) {
        ++cached->second.get(mem)->reference_count;
        *fragmentProgram = cached->second;
        return 0;
    }

    *fragmentProgram = alloc_callbacked<SceGxmFragmentProgram>(emuenv, thread_id, shaderPatcher);
    assert(*fragmentProgram);
    if (!*fragmentProgram) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    SceGxmFragmentProgram *const fp = fragmentProgram->get(mem);
    fp->is_maskupdate = false;
    fp->program = programId->program;

    if (!renderer::create(fp->renderer_data, *emuenv.renderer, *programId->program.get(mem), blendInfo, emuenv.renderer->gxp_ptr_map, emuenv.base_path.c_str(), emuenv.io.title_id.c_str())) {
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }

    shaderPatcher->fragment_program_cache.emplace(key, *fragmentProgram);

    return 0;
}

EXPORT(int, sceGxmShaderPatcherCreateMaskUpdateFragmentProgram, SceGxmShaderPatcher *shaderPatcher, Ptr<SceGxmFragmentProgram> *fragmentProgram) {
    TRACY_FUNC(sceGxmShaderPatcherCreateMaskUpdateFragmentProgram, shaderPatcher, fragmentProgram);
    MemState &mem = emuenv.mem;

    if (!shaderPatcher || !fragmentProgram)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    *fragmentProgram = alloc_callbacked<SceGxmFragmentProgram>(emuenv, thread_id, shaderPatcher);
    assert(*fragmentProgram);
    if (!*fragmentProgram) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    SceGxmFragmentProgram *const fp = fragmentProgram->get(mem);
    fp->is_maskupdate = true;
    fp->program = Ptr<const SceGxmProgram>(alloc_callbacked(emuenv, thread_id, shaderPatcher->params, size_mask_gxp));
    memcpy(const_cast<SceGxmProgram *>(fp->program.get(mem)), mask_gxp, size_mask_gxp);

    if (!renderer::create(fp->renderer_data, *emuenv.renderer, *fp->program.get(mem), nullptr, emuenv.renderer->gxp_ptr_map, emuenv.base_path.c_str(), emuenv.io.title_id.c_str())) {
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }

    return 0;
}

EXPORT(int, sceGxmShaderPatcherCreateVertexProgram, SceGxmShaderPatcher *shaderPatcher, const SceGxmRegisteredProgram *programId, const SceGxmVertexAttribute *attributes, uint32_t attributeCount, const SceGxmVertexStream *streams, uint32_t streamCount, Ptr<SceGxmVertexProgram> *vertexProgram) {
    TRACY_FUNC(sceGxmShaderPatcherCreateVertexProgram, shaderPatcher, programId, attributes, attributeCount, streams, streamCount, vertexProgram);
    MemState &mem = emuenv.mem;

    if (!shaderPatcher || !programId || !vertexProgram)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    VertexProgramCacheKey key = {
        *programId,
        0
    };

    if (attributes) {
        key.hash = hash_data(attributes, sizeof(SceGxmVertexAttribute) * attributeCount);
    }

    if (streams) {
        key.hash ^= hash_data(streams, sizeof(SceGxmVertexStream) * streamCount);
    }

    VertexProgramCache::const_iterator cached = shaderPatcher->vertex_program_cache.find(key);
    if (cached != shaderPatcher->vertex_program_cache.end()) {
        ++cached->second.get(mem)->reference_count;
        *vertexProgram = cached->second;
        return 0;
    }

    *vertexProgram = alloc_callbacked<SceGxmVertexProgram>(emuenv, thread_id, shaderPatcher);
    assert(*vertexProgram);
    if (!*vertexProgram) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    SceGxmVertexProgram *const vp = vertexProgram->get(mem);
    vp->program = programId->program;
    vp->key_hash = key.hash;

    if (streams && streamCount > 0) {
        vp->streams.insert(vp->streams.end(), &streams[0], &streams[streamCount]);
    }

    if (attributes && attributeCount > 0) {
        vp->attributes.insert(vp->attributes.end(), &attributes[0], &attributes[attributeCount]);
    }

    if (!renderer::create(vp->renderer_data, *emuenv.renderer, *programId->program.get(mem), emuenv.renderer->gxp_ptr_map, emuenv.base_path.c_str(), emuenv.io.title_id.c_str())) {
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }

    shaderPatcher->vertex_program_cache.emplace(key, *vertexProgram);

    return 0;
}

EXPORT(int, sceGxmShaderPatcherDestroy, Ptr<SceGxmShaderPatcher> shaderPatcher) {
    TRACY_FUNC(sceGxmShaderPatcherDestroy, shaderPatcher);
    if (!shaderPatcher)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    free_callbacked(emuenv, thread_id, shaderPatcher.get(emuenv.mem), shaderPatcher);

    return 0;
}

EXPORT(int, sceGxmShaderPatcherForceUnregisterProgram, SceGxmShaderPatcher *shaderPatcher, SceGxmShaderPatcherId programId) {
    TRACY_FUNC(sceGxmShaderPatcherForceUnregisterProgram, shaderPatcher, programId);
    if (!shaderPatcher || !programId) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(uint32_t, sceGxmShaderPatcherGetBufferMemAllocated, const SceGxmShaderPatcher *shaderPatcher) {
    TRACY_FUNC(sceGxmShaderPatcherGetBufferMemAllocated, shaderPatcher);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherGetFragmentProgramRefCount, const SceGxmShaderPatcher *shaderPatcher, const SceGxmFragmentProgram *fragmentProgram, uint32_t *refCount) {
    TRACY_FUNC(sceGxmShaderPatcherGetFragmentProgramRefCount, shaderPatcher, fragmentProgram, refCount);
    if (!shaderPatcher || !fragmentProgram || !refCount) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    *refCount = fragmentProgram->reference_count;
    return 0;
}

EXPORT(uint32_t, sceGxmShaderPatcherGetFragmentUsseMemAllocated, const SceGxmShaderPatcher *shaderPatcher) {
    TRACY_FUNC(sceGxmShaderPatcherGetFragmentUsseMemAllocated, shaderPatcher);
    return UNIMPLEMENTED();
}

EXPORT(uint32_t, sceGxmShaderPatcherGetHostMemAllocated, const SceGxmShaderPatcher *shaderPatcher) {
    TRACY_FUNC(sceGxmShaderPatcherGetHostMemAllocated, shaderPatcher);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<const SceGxmProgram>, sceGxmShaderPatcherGetProgramFromId, SceGxmShaderPatcherId programId) {
    TRACY_FUNC(sceGxmShaderPatcherGetProgramFromId, programId);
    if (!programId) {
        return Ptr<const SceGxmProgram>();
    }

    return programId.get(emuenv.mem)->program;
}

EXPORT(int, sceGxmShaderPatcherGetUserData) {
    TRACY_FUNC(sceGxmShaderPatcherGetUserData);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherGetVertexProgramRefCount, const SceGxmShaderPatcher *shaderPatcher, const SceGxmVertexProgram *vertexProgram, uint32_t *refCount) {
    TRACY_FUNC(sceGxmShaderPatcherGetVertexProgramRefCount, shaderPatcher, vertexProgram, refCount);
    if (!shaderPatcher || !vertexProgram || !refCount) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    *refCount = vertexProgram->reference_count;
    return 0;
}

EXPORT(uint32_t, sceGxmShaderPatcherGetVertexUsseMemAllocated, const SceGxmShaderPatcher *shaderPatcher) {
    TRACY_FUNC(sceGxmShaderPatcherGetVertexUsseMemAllocated, shaderPatcher);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherRegisterProgram, SceGxmShaderPatcher *shaderPatcher, Ptr<const SceGxmProgram> programHeader, SceGxmShaderPatcherId *programId) {
    TRACY_FUNC(sceGxmShaderPatcherRegisterProgram, shaderPatcher, programHeader, programId);
    if (!shaderPatcher || !programHeader || !programId)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    *programId = alloc_callbacked<SceGxmRegisteredProgram>(emuenv, thread_id, shaderPatcher);
    assert(*programId);
    if (!*programId) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    SceGxmRegisteredProgram *const rp = programId->get(emuenv.mem);
    rp->program = programHeader;

    return 0;
}

EXPORT(int, sceGxmShaderPatcherReleaseFragmentProgram, SceGxmShaderPatcher *shaderPatcher, Ptr<SceGxmFragmentProgram> fragmentProgram) {
    TRACY_FUNC(sceGxmShaderPatcherReleaseFragmentProgram, shaderPatcher, fragmentProgram);
    if (!shaderPatcher || !fragmentProgram)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    SceGxmFragmentProgram *const fp = fragmentProgram.get(emuenv.mem);
    --fp->reference_count;
    if (fp->reference_count == 0) {
        for (FragmentProgramCache::const_iterator it = shaderPatcher->fragment_program_cache.begin(); it != shaderPatcher->fragment_program_cache.end(); ++it) {
            if (it->second == fragmentProgram) {
                shaderPatcher->fragment_program_cache.erase(it);
                break;
            }
        }
        free_callbacked(emuenv, thread_id, shaderPatcher, fragmentProgram);
    }

    return 0;
}

EXPORT(int, sceGxmShaderPatcherReleaseVertexProgram, SceGxmShaderPatcher *shaderPatcher, Ptr<SceGxmVertexProgram> vertexProgram) {
    TRACY_FUNC(sceGxmShaderPatcherReleaseVertexProgram, shaderPatcher, vertexProgram);
    if (!shaderPatcher || !vertexProgram)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    SceGxmVertexProgram *const vp = vertexProgram.get(emuenv.mem);
    --vp->reference_count;
    if (vp->reference_count == 0) {
        for (VertexProgramCache::const_iterator it = shaderPatcher->vertex_program_cache.begin(); it != shaderPatcher->vertex_program_cache.end(); ++it) {
            if (it->second == vertexProgram) {
                shaderPatcher->vertex_program_cache.erase(it);
                break;
            }
        }
        free_callbacked(emuenv, thread_id, shaderPatcher, vertexProgram);
    }

    return 0;
}

EXPORT(int, sceGxmShaderPatcherSetAuxiliarySurface) {
    TRACY_FUNC(sceGxmShaderPatcherSetAuxiliarySurface);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherSetUserData, SceGxmShaderPatcher *shaderPatcher, Ptr<void> userData) {
    TRACY_FUNC(sceGxmShaderPatcherSetUserData, shaderPatcher, userData);
    if (!shaderPatcher) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    shaderPatcher->params.userData = userData;
    return 0;
}

EXPORT(int, sceGxmShaderPatcherUnregisterProgram, SceGxmShaderPatcher *shaderPatcher, SceGxmShaderPatcherId programId) {
    TRACY_FUNC(sceGxmShaderPatcherUnregisterProgram, shaderPatcher, programId);
    if (!shaderPatcher || !programId)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    SceGxmRegisteredProgram *const rp = programId.get(emuenv.mem);
    rp->program.reset();

    free_callbacked(emuenv, thread_id, shaderPatcher, programId);

    return 0;
}

EXPORT(int, sceGxmSyncObjectCreate, Ptr<SceGxmSyncObject> *syncObject) {
    TRACY_FUNC(sceGxmSyncObjectCreate, syncObject);
    if (!syncObject)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    *syncObject = alloc<SceGxmSyncObject>(emuenv.mem, __FUNCTION__);
    if (!*syncObject) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    renderer::create(syncObject->get(emuenv.mem), *emuenv.renderer);

    return 0;
}

EXPORT(int, sceGxmSyncObjectDestroy, Ptr<SceGxmSyncObject> syncObject) {
    TRACY_FUNC(sceGxmSyncObjectDestroy, syncObject);
    if (!syncObject)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    renderer::destroy(syncObject.get(emuenv.mem), *emuenv.renderer);
    free(emuenv.mem, syncObject);

    return 0;
}

EXPORT(int, sceGxmTerminate) {
    TRACY_FUNC(sceGxmTerminate);
    // Make sure everything is done in SDL side before killing Vita thread
    emuenv.gxm.display_queue.wait_empty();
    emuenv.gxm.display_queue.abort();
    SDL_WaitThread(emuenv.gxm.sdl_thread, nullptr);
    emuenv.gxm.sdl_thread = nullptr;
    emuenv.kernel.get_thread(emuenv.gxm.display_queue_thread)->exit_delete();
    return 0;
}

EXPORT(Ptr<void>, sceGxmTextureGetData, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetData, texture);
    assert(texture);
    return Ptr<void>(texture->data_addr << 2);
}

EXPORT(SceGxmTextureFormat, sceGxmTextureGetFormat, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetFormat, texture);
    assert(texture);
    return gxm::get_format(texture);
}

EXPORT(int, sceGxmTextureGetGammaMode, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetGammaMode, texture);
    assert(texture);
    return (texture->gamma_mode << 27);
}

EXPORT(uint32_t, sceGxmTextureGetHeight, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetHeight, texture);
    assert(texture);
    return static_cast<uint32_t>(gxm::get_height(texture));
}

EXPORT(uint32_t, sceGxmTextureGetLodBias, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetLodBias, texture);
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return 0;
    }

    return texture->lod_bias;
}

EXPORT(uint32_t, sceGxmTextureGetLodMin, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetLodMin, texture);
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return 0;
    }

    return texture->lod_min0 | (texture->lod_min1 << 2);
}

EXPORT(int, sceGxmTextureGetMagFilter, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetMagFilter, texture);
    assert(texture);
    return texture->mag_filter;
}

EXPORT(int, sceGxmTextureGetMinFilter, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetMinFilter, texture);
    assert(texture);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return texture->mag_filter;
    }
    return texture->min_filter;
}

EXPORT(SceGxmTextureMipFilter, sceGxmTextureGetMipFilter, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetMipFilter, texture);
    assert(texture);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
    }
    return texture->mip_filter ? SCE_GXM_TEXTURE_MIP_FILTER_ENABLED : SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
}

EXPORT(uint32_t, sceGxmTextureGetMipmapCount, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetMipmapCount, texture);
    assert(texture);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return 0;
    }
    return (texture->mip_count + 1) & 0xf;
}

EXPORT(uint32_t, sceGxmTextureGetMipmapCountUnsafe, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetMipmapCountUnsafe, texture);
    assert(texture);
    return (texture->mip_count + 1) & 0xf;
}

EXPORT(int, sceGxmTextureGetNormalizeMode, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetNormalizeMode, texture);
    assert(texture);
    return texture->normalize_mode << 31;
}

EXPORT(Ptr<void>, sceGxmTextureGetPalette, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetPalette, texture);
    const auto base_format = gxm::get_base_format(gxm::get_format(texture));

    return gxm::is_paletted_format(base_format) ? Ptr<void>(texture->palette_addr << 6) : Ptr<void>();
}

EXPORT(uint32_t, sceGxmTextureGetStride, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetStride, texture);
    assert(texture);
    if (texture->texture_type() != SCE_GXM_TEXTURE_LINEAR_STRIDED)
        return 0;

    return uint32_t(gxm::get_stride_in_bytes(texture));
}

EXPORT(int, sceGxmTextureGetType, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetType, texture);
    assert(texture);
    return (texture->type << 29);
}

EXPORT(int, sceGxmTextureGetUAddrMode, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetUAddrMode, texture);
    assert(texture);
    return texture->uaddr_mode;
}

EXPORT(int, sceGxmTextureGetUAddrModeSafe, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetUAddrModeSafe, texture);
    assert(texture);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return SCE_GXM_TEXTURE_ADDR_CLAMP;
    }
    return texture->uaddr_mode;
}

EXPORT(int, sceGxmTextureGetVAddrMode, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetVAddrMode, texture);
    assert(texture);
    return texture->vaddr_mode;
}

EXPORT(int, sceGxmTextureGetVAddrModeSafe, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetVAddrModeSafe, texture);
    assert(texture);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return SCE_GXM_TEXTURE_ADDR_CLAMP;
    }
    return texture->vaddr_mode;
}

EXPORT(uint32_t, sceGxmTextureGetWidth, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureGetWidth, texture);
    assert(texture);
    return static_cast<uint32_t>(gxm::get_width(texture));
}

EXPORT(int, sceGxmTextureInitCube, SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, uint32_t width, uint32_t height, uint32_t mipCount) {
    TRACY_FUNC(sceGxmTextureInitCube, texture, data, texFormat, width, height, mipCount);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const int result = init_texture_base(export_name, texture, data, texFormat, width, height, mipCount, SCE_GXM_TEXTURE_CUBE);

    return result;
}

EXPORT(int, sceGxmTextureInitCubeArbitrary, SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, uint32_t width, uint32_t height, uint32_t mipCount) {
    TRACY_FUNC(sceGxmTextureInitCubeArbitrary, texture, data, texFormat, width, height, mipCount);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const int result = init_texture_base(export_name, texture, data, texFormat, width, height, mipCount, SCE_GXM_TEXTURE_CUBE_ARBITRARY);

    return result;
}

EXPORT(int, sceGxmTextureInitLinear, SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, uint32_t width, uint32_t height, uint32_t mipCount) {
    TRACY_FUNC(sceGxmTextureInitLinear, texture, data, texFormat, width, height, mipCount);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const int result = init_texture_base(export_name, texture, data, texFormat, width, height, mipCount, SCE_GXM_TEXTURE_LINEAR);

    return result;
}

EXPORT(int, sceGxmTextureInitLinearStrided, SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, uint32_t width, uint32_t height, uint32_t byteStride) {
    TRACY_FUNC(sceGxmTextureInitLinearStrided, texture, data, texFormat, width, height, byteStride);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((width > 4096) || (height > 4096)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (byteStride & 3) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_ALIGNMENT);
    }

    if ((byteStride < 4) || (byteStride > 131072)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    const uint32_t stride_compressed = (byteStride >> 2) - 1;
    texture->mip_filter = stride_compressed & 1;
    texture->min_filter = (stride_compressed & 0b0000110) >> 1;
    texture->mip_count = (stride_compressed & 0b1111000) >> 3;
    texture->lod_bias = (stride_compressed & 0b1111110000000) >> 7;
    texture->base_format = (texFormat & 0x1F000000) >> 24;
    texture->type = SCE_GXM_TEXTURE_LINEAR_STRIDED >> 29;
    texture->data_addr = data.address() >> 2;
    texture->swizzle_format = (texFormat & 0x7000) >> 12;
    texture->normalize_mode = 1;
    texture->format0 = (texFormat & 0x80000000) >> 31;
    texture->uaddr_mode = texture->vaddr_mode = SCE_GXM_TEXTURE_ADDR_CLAMP;
    texture->height = height - 1;
    texture->width = width - 1;

    return 0;
}

EXPORT(int, sceGxmTextureInitSwizzled, SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, uint32_t width, uint32_t height, uint32_t mipCount) {
    TRACY_FUNC(sceGxmTextureInitSwizzled, texture, data, texFormat, width, height, mipCount);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const int result = init_texture_base(export_name, texture, data, texFormat, width, height, mipCount, SCE_GXM_TEXTURE_SWIZZLED);

    return result;
}

EXPORT(int, sceGxmTextureInitSwizzledArbitrary, SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, uint32_t width, uint32_t height, uint32_t mipCount) {
    TRACY_FUNC(sceGxmTextureInitSwizzledArbitrary, texture, data, texFormat, width, height, mipCount);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const auto result = init_texture_base(export_name, texture, data, texFormat, width, height, mipCount, SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY);

    return result;
}

EXPORT(int, sceGxmTextureInitTiled, SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, uint32_t width, uint32_t height, uint32_t mipCount) {
    TRACY_FUNC(sceGxmTextureInitTiled, texture, data, texFormat, width, height, mipCount);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const int result = init_texture_base(export_name, texture, data, texFormat, width, height, mipCount, SCE_GXM_TEXTURE_TILED);

    return result;
}

EXPORT(int, sceGxmTextureSetData, SceGxmTexture *texture, Ptr<const void> data) {
    TRACY_FUNC(sceGxmTextureSetData, texture, data);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    texture->data_addr = data.address() >> 2;
    return 0;
}

EXPORT(int, sceGxmTextureSetFormat, SceGxmTexture *texture, SceGxmTextureFormat texFormat) {
    TRACY_FUNC(sceGxmTextureSetFormat, texture, texFormat);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    texture->base_format = (texFormat & 0x1F000000) >> 24;
    texture->swizzle_format = (texFormat & 0x7000) >> 12;
    texture->format0 = (texFormat & 0x80000000) >> 31;

    return SCE_KERNEL_OK;
}

EXPORT(int, sceGxmTextureSetGammaMode, SceGxmTexture *texture, SceGxmTextureGammaMode gammaMode) {
    TRACY_FUNC(sceGxmTextureSetGammaMode, texture, gammaMode);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    texture->gamma_mode = (static_cast<std::uint32_t>(gammaMode) >> 27);
    return 0;
}

EXPORT(int, sceGxmTextureSetHeight, SceGxmTexture *texture, uint32_t height) {
    TRACY_FUNC(sceGxmTextureSetHeight, texture, height);
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    if (height > 4096)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    if ((texture->type << 29) == SCE_GXM_TEXTURE_TILED) {
        if (texture->mip_count > 1) {
            if (height >> (texture->mip_count - 1) >> 0x1F) {
                goto LINEAR;
            }
        }
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (((texture->type << 29) != SCE_GXM_TEXTURE_SWIZZLED) && ((texture->type << 29) != SCE_GXM_TEXTURE_TILED)) {
    LINEAR:
        texture->height = height - 1;
        return 0;
    }

    // TODO: Add support for swizzled textures
    LOG_WARN("Unimplemented texture format detected in sceGxmTextureSetHeight call.");

    return 0;
}

EXPORT(int, _sceGxmTextureSetHeight, SceGxmTexture *texture, uint32_t height) {
    TRACY_FUNC(_sceGxmTextureSetHeight, texture, height);
    return CALL_EXPORT(sceGxmTextureSetHeight, texture, height);
}

EXPORT(int, sceGxmTextureSetLodBias, SceGxmTexture *texture, uint32_t bias) {
    TRACY_FUNC(sceGxmTextureSetLodBias, texture, bias);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }

    if (bias > 63) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    texture->lod_bias = bias;

    return 0;
}

EXPORT(int, sceGxmTextureSetLodMin, SceGxmTexture *texture, uint32_t lodMin) {
    TRACY_FUNC(sceGxmTextureSetLodMin, texture, lodMin);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }

    texture->lod_min0 = lodMin & 3;
    texture->lod_min1 = lodMin >> 2;

    return 0;
}

EXPORT(int, sceGxmTextureSetMagFilter, SceGxmTexture *texture, SceGxmTextureFilter magFilter) {
    TRACY_FUNC(sceGxmTextureSetMagFilter, texture, magFilter);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    texture->mag_filter = (uint32_t)magFilter;
    return 0;
}

EXPORT(int, sceGxmTextureSetMinFilter, SceGxmTexture *texture, SceGxmTextureFilter minFilter) {
    TRACY_FUNC(sceGxmTextureSetMinFilter, texture, minFilter);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }

    texture->min_filter = (uint32_t)minFilter;
    return 0;
}

EXPORT(int, sceGxmTextureSetMipFilter, SceGxmTexture *texture, SceGxmTextureMipFilter mipFilter) {
    TRACY_FUNC(sceGxmTextureSetMipFilter, texture, mipFilter);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }

    texture->mip_filter = static_cast<bool>(mipFilter);
    return 0;
}

EXPORT(int, sceGxmTextureSetMipmapCount, SceGxmTexture *texture, uint32_t mipCount) {
    TRACY_FUNC(sceGxmTextureSetMipmapCount, texture, mipCount);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }

    if (mipCount > 13) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    texture->mip_count = std::min<std::uint32_t>(15, mipCount - 1);
    return 0;
}

EXPORT(int, sceGxmTextureSetNormalizeMode, SceGxmTexture *texture, SceGxmTextureNormalizeMode normalizeMode) {
    TRACY_FUNC(sceGxmTextureSetNormalizeMode, texture, normalizeMode);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    texture->normalize_mode = (static_cast<std::uint32_t>(normalizeMode) >> 31);
    return 0;
}

EXPORT(int, sceGxmTextureSetPalette, SceGxmTexture *texture, Ptr<const void> paletteData) {
    TRACY_FUNC(sceGxmTextureSetPalette, texture, paletteData);
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    if (paletteData.address() & 0x3F)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_ALIGNMENT);

    texture->palette_addr = (paletteData.address() >> 6);
    return 0;
}

EXPORT(int, sceGxmTextureSetStride, SceGxmTexture *texture, uint32_t byteStride) {
    TRACY_FUNC(sceGxmTextureSetStride, texture, byteStride);
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    if ((texture->type << 29) != SCE_GXM_TEXTURE_LINEAR_STRIDED)
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    if (byteStride & 3)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_ALIGNMENT);
    if ((byteStride < 4) || (byteStride > 131072))
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    return UNIMPLEMENTED();
}

static bool verify_texture_mode(SceGxmTexture *texture, SceGxmTextureAddrMode mode) {
    if ((texture->type << 29) == SCE_GXM_TEXTURE_CUBE || (texture->type << 29) == SCE_GXM_TEXTURE_CUBE_ARBITRARY) {
        if (mode != SCE_GXM_TEXTURE_ADDR_CLAMP) {
            return false;
        }
    } else {
        if (mode <= SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER && mode >= SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER) {
            if ((texture->type << 29) != SCE_GXM_TEXTURE_SWIZZLED) {
                return false;
            }
        }
        if (mode == SCE_GXM_TEXTURE_ADDR_MIRROR && ((texture->type << 29) != SCE_GXM_TEXTURE_SWIZZLED)) {
            return false;
        }
    }
    return true;
}

EXPORT(int, sceGxmTextureSetUAddrMode, SceGxmTexture *texture, SceGxmTextureAddrMode mode) {
    TRACY_FUNC(sceGxmTextureSetUAddrMode, texture, mode);
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (!verify_texture_mode(texture, mode))
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);

    texture->uaddr_mode = mode;
    return 0;
}

EXPORT(int, sceGxmTextureSetUAddrModeSafe, SceGxmTexture *texture, SceGxmTextureAddrMode mode) {
    TRACY_FUNC(sceGxmTextureSetUAddrModeSafe, texture, mode);
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (!verify_texture_mode(texture, mode))
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);

    texture->uaddr_mode = mode;
    return 0;
}

EXPORT(int, sceGxmTextureSetVAddrMode, SceGxmTexture *texture, SceGxmTextureAddrMode mode) {
    TRACY_FUNC(sceGxmTextureSetVAddrMode, texture, mode);
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (!verify_texture_mode(texture, mode))
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);

    texture->vaddr_mode = mode;
    return 0;
}

EXPORT(int, sceGxmTextureSetVAddrModeSafe, SceGxmTexture *texture, SceGxmTextureAddrMode mode) {
    TRACY_FUNC(sceGxmTextureSetVAddrModeSafe, texture, mode);
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (!verify_texture_mode(texture, mode))
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);

    texture->vaddr_mode = mode;
    return 0;
}

EXPORT(int, sceGxmTextureSetWidth, SceGxmTexture *texture, uint32_t width) {
    TRACY_FUNC(sceGxmTextureSetWidth, texture, width);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    } else if (width > 4096) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if ((texture->type << 29) == SCE_GXM_TEXTURE_TILED) {
        if (texture->mip_count > 1) {
            if (width >> (texture->mip_count - 1) >> 0x1F) {
                goto LINEAR;
            }
        }
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (((texture->type << 29) != SCE_GXM_TEXTURE_SWIZZLED) && ((texture->type << 29) != SCE_GXM_TEXTURE_TILED)) {
    LINEAR:
        texture->width = width - 1;
        return 0;
    }

    // TODO: Add support for swizzled textures
    LOG_WARN("Unimplemented texture format detected in sceGxmTextureSetWidth call.");

    return 0;
}

EXPORT(int, _sceGxmTextureSetWidth, SceGxmTexture *texture, uint32_t width) {
    TRACY_FUNC(_sceGxmTextureSetWidth, texture, width);
    return CALL_EXPORT(sceGxmTextureSetWidth, texture, width);
}

EXPORT(int, sceGxmTextureValidate, const SceGxmTexture *texture) {
    TRACY_FUNC(sceGxmTextureValidate, texture);
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTransferCopy, uint32_t width, uint32_t height, uint32_t colorKeyValue, uint32_t colorKeyMask, SceGxmTransferColorKeyMode colorKeyMode,
    SceGxmTransferFormat srcFormat, SceGxmTransferType srcType, Ptr<void> srcAddress, uint32_t srcX, uint32_t srcY, int32_t srcStride,
    SceGxmTransferFormat destFormat, SceGxmTransferType destType, Ptr<void> destAddress, uint32_t destX, uint32_t destY, int32_t destStride,
    Ptr<SceGxmSyncObject> syncObject, SceGxmTransferFlags syncFlags, const SceGxmNotification *notification) {
    TRACY_FUNC(sceGxmTransferCopy, width, height, colorKeyValue, colorKeyMask, colorKeyMode, srcFormat, srcType, srcAddress, srcX);
#ifdef TRACY_ENABLE
    if (_tracy_activation_state) {
        __TRACY_LOG_ARG_IF(srcY)
        __TRACY_LOG_ARG_IF(srcStride)
        __TRACY_LOG_ARG_IF(destFormat)
        __TRACY_LOG_ARG_IF(destType)
        __TRACY_LOG_ARG_IF(destAddress)
        __TRACY_LOG_ARG_IF(destX)
        __TRACY_LOG_ARG_IF(destY)
        __TRACY_LOG_ARG_IF(destStride)
        __TRACY_LOG_ARG_IF(syncObject)
        __TRACY_LOG_ARG_IF(syncFlags)
        __TRACY_LOG_ARG_IF(notification)
    }
#endif // TRACY_ENABLE

    if (!srcAddress || !destAddress)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    const auto src_type_is_linear = srcType == SCE_GXM_TRANSFER_LINEAR;
    const auto src_type_is_tiled = srcType == SCE_GXM_TRANSFER_TILED;
    const auto src_type_is_swizzled = srcType == SCE_GXM_TRANSFER_SWIZZLED;
    const auto dest_type_is_linear = destType == SCE_GXM_TRANSFER_LINEAR;
    const auto dest_type_is_tiled = destType == SCE_GXM_TRANSFER_TILED;
    const auto dest_type_is_swizzled = destType == SCE_GXM_TRANSFER_SWIZZLED;

    const auto is_invalid_value = (src_type_is_tiled && dest_type_is_swizzled) || (src_type_is_swizzled && dest_type_is_tiled) || (src_type_is_swizzled && dest_type_is_swizzled);
    if (is_invalid_value)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    uint32_t cmd_timestamp;
    if (syncObject) {
        SceGxmSyncObject *sync = syncObject.get(emuenv.mem);
        renderer::send_single_command(*emuenv.renderer, nullptr, renderer::CommandOpcode::WaitSyncObject, false,
            syncObject, nullptr, sync->last_display);
        cmd_timestamp = ++sync->timestamp_ahead;
    }

    // needed, otherwise the command is not big enough
    SceGxmTransferImage *images = new SceGxmTransferImage[2];

    SceGxmTransferImage *src = &images[0];
    src->format = srcFormat;
    src->address = srcAddress;
    src->x = srcX;
    src->y = srcY;
    src->width = width;
    src->height = height;
    src->stride = srcStride;

    SceGxmTransferImage *dest = &images[1];
    dest->format = destFormat;
    dest->address = destAddress;
    dest->x = destX;
    dest->y = destY;
    dest->width = width;
    dest->height = height;
    dest->stride = destStride;

    renderer::transfer_copy(*emuenv.renderer, colorKeyValue, colorKeyMask, colorKeyMode, images, srcType, destType);

    if (notification)
        renderer::send_single_command(*emuenv.renderer, nullptr, renderer::CommandOpcode::SignalNotification, false, *notification, true);

    if (syncObject) {
        SceGxmSyncObject *sync = syncObject.get(emuenv.mem);
        renderer::send_single_command(*emuenv.renderer, nullptr, renderer::CommandOpcode::SignalSyncObject, false,
            syncObject, cmd_timestamp);
    }

    return 0;
}

EXPORT(int, sceGxmTransferDownscale, SceGxmTransferFormat srcFormat, Ptr<void> srcAddress,
    uint32_t srcX, uint32_t srcY, uint32_t srcWidth, uint32_t srcHeight, int32_t srcStride,
    SceGxmTransferFormat destFormat, Ptr<void> destAddress, uint32_t destX, uint32_t destY, int32_t destStride,
    Ptr<SceGxmSyncObject> syncObject, SceGxmTransferFlags syncFlags, const SceGxmNotification *notification) {
    TRACY_FUNC(sceGxmTransferDownscale, srcFormat, srcX, srcY, srcWidth, srcHeight, srcStride, destFormat, destAddress, destX);
#ifdef TRACY_ENABLE
    if (_tracy_activation_state) {
        __TRACY_LOG_ARG_IF(destY)
        __TRACY_LOG_ARG_IF(destStride)
        __TRACY_LOG_ARG_IF(syncObject)
        __TRACY_LOG_ARG_IF(syncFlags)
        __TRACY_LOG_ARG_IF(notification)
    }
#endif // TRACY_ENABLE

    if (!srcAddress || !destAddress)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    uint32_t cmd_timestamp;
    if (syncObject) {
        SceGxmSyncObject *sync = syncObject.get(emuenv.mem);
        renderer::send_single_command(*emuenv.renderer, nullptr, renderer::CommandOpcode::WaitSyncObject, false,
            syncObject, nullptr, sync->last_display);
        cmd_timestamp = ++sync->timestamp_ahead;
    }

    SceGxmTransferImage *src = new SceGxmTransferImage;
    src->format = srcFormat;
    src->address = srcAddress;
    src->x = srcX;
    src->y = srcY;
    src->width = srcWidth;
    src->height = srcHeight;
    src->stride = srcStride;

    SceGxmTransferImage *dest = new SceGxmTransferImage;
    dest->format = destFormat;
    dest->address = destAddress;
    dest->x = destX;
    dest->y = destY;
    dest->stride = destStride;

    renderer::transfer_downscale(*emuenv.renderer, src, dest);

    if (notification)
        renderer::send_single_command(*emuenv.renderer, nullptr, renderer::CommandOpcode::SignalNotification, false, *notification, true);

    if (syncObject) {
        SceGxmSyncObject *sync = syncObject.get(emuenv.mem);
        renderer::send_single_command(*emuenv.renderer, nullptr, renderer::CommandOpcode::SignalSyncObject, false,
            syncObject, cmd_timestamp);
    }

    return 0;
}

EXPORT(int, sceGxmTransferFill, uint32_t fillColor, SceGxmTransferFormat destFormat, Ptr<void> destAddress,
    uint32_t destX, uint32_t destY, uint32_t destWidth, uint32_t destHeight, int32_t destStride,
    Ptr<SceGxmSyncObject> syncObject, SceGxmTransferFlags syncFlags, const SceGxmNotification *notification) {
    TRACY_FUNC(sceGxmTransferFill, fillColor, destFormat, destAddress, destX, destY, destWidth, destHeight, destStride, syncObject);
#ifdef TRACY_ENABLE
    if (_tracy_activation_state) {
        __TRACY_LOG_ARG_IF(syncFlags)
        __TRACY_LOG_ARG_IF(notification)
    }
#endif // TRACY_ENABLE

    if (!destAddress)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    uint32_t cmd_timestamp;
    if (syncObject) {
        SceGxmSyncObject *sync = syncObject.get(emuenv.mem);
        renderer::send_single_command(*emuenv.renderer, nullptr, renderer::CommandOpcode::WaitSyncObject, false,
            syncObject, nullptr, sync->last_display);
        cmd_timestamp = ++sync->timestamp_ahead;
    }

    SceGxmTransferImage *dest = new SceGxmTransferImage;
    dest->format = destFormat;
    dest->address = destAddress;
    dest->x = destX;
    dest->y = destY;
    dest->width = destWidth;
    dest->height = destHeight;
    dest->stride = destStride;
    renderer::transfer_fill(*emuenv.renderer, fillColor, dest);

    if (notification)
        renderer::send_single_command(*emuenv.renderer, nullptr, renderer::CommandOpcode::SignalNotification, false, *notification, true);

    if (syncObject) {
        SceGxmSyncObject *sync = syncObject.get(emuenv.mem);
        renderer::send_single_command(*emuenv.renderer, nullptr, renderer::CommandOpcode::SignalSyncObject, false,
            syncObject, cmd_timestamp);
    }

    return 0;
}

EXPORT(int, sceGxmTransferFinish) {
    TRACY_FUNC(sceGxmTransferFinish);
    // same as sceGxmFinish
    renderer::finish(*emuenv.renderer, nullptr);

    return 0;
}

EXPORT(int, sceGxmUnmapFragmentUsseMemory, void *base) {
    TRACY_FUNC(sceGxmUnmapFragmentUsseMemory, base);
    if (!base) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    return 0;
}

EXPORT(int, sceGxmUnmapMemory, Ptr<void> base) {
    TRACY_FUNC(sceGxmUnmapMemory, base);
    if (!base) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    auto ite = emuenv.gxm.memory_mapped_regions.find(base.address());
    if (ite == emuenv.gxm.memory_mapped_regions.end()) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    emuenv.gxm.memory_mapped_regions.erase(ite);
    return 0;
}

EXPORT(int, sceGxmUnmapVertexUsseMemory, void *base) {
    TRACY_FUNC(sceGxmUnmapVertexUsseMemory, base);
    if (!base) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    return 0;
}

EXPORT(int, sceGxmVertexFence) {
    TRACY_FUNC(sceGxmVertexFence);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<const SceGxmProgram>, sceGxmVertexProgramGetProgram, const SceGxmVertexProgram *vertexProgram) {
    TRACY_FUNC(sceGxmVertexProgramGetProgram, vertexProgram);
    return vertexProgram->program;
}

EXPORT(int, sceGxmWaitEvent) {
    TRACY_FUNC(sceGxmWaitEvent);
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_sceGxmBeginScene)
BRIDGE_IMPL(_sceGxmProgramFindParameterBySemantic)
BRIDGE_IMPL(_sceGxmProgramParameterGetSemantic)
BRIDGE_IMPL(_sceGxmSetVertexTexture)
BRIDGE_IMPL(_sceGxmTextureSetHeight)
BRIDGE_IMPL(_sceGxmTextureSetWidth)
BRIDGE_IMPL(sceGxmAddRazorGpuCaptureBuffer)
BRIDGE_IMPL(sceGxmBeginCommandList)
BRIDGE_IMPL(sceGxmBeginScene)
BRIDGE_IMPL(sceGxmBeginSceneEx)
BRIDGE_IMPL(sceGxmColorSurfaceGetClip)
BRIDGE_IMPL(sceGxmColorSurfaceGetData)
BRIDGE_IMPL(sceGxmColorSurfaceGetDitherMode)
BRIDGE_IMPL(sceGxmColorSurfaceGetFormat)
BRIDGE_IMPL(sceGxmColorSurfaceGetGammaMode)
BRIDGE_IMPL(sceGxmColorSurfaceGetScaleMode)
BRIDGE_IMPL(sceGxmColorSurfaceGetStrideInPixels)
BRIDGE_IMPL(sceGxmColorSurfaceGetType)
BRIDGE_IMPL(sceGxmColorSurfaceInit)
BRIDGE_IMPL(sceGxmColorSurfaceInitDisabled)
BRIDGE_IMPL(sceGxmColorSurfaceIsEnabled)
BRIDGE_IMPL(sceGxmColorSurfaceSetClip)
BRIDGE_IMPL(sceGxmColorSurfaceSetData)
BRIDGE_IMPL(sceGxmColorSurfaceSetDitherMode)
BRIDGE_IMPL(sceGxmColorSurfaceSetFormat)
BRIDGE_IMPL(sceGxmColorSurfaceSetGammaMode)
BRIDGE_IMPL(sceGxmColorSurfaceSetScaleMode)
BRIDGE_IMPL(sceGxmCreateContext)
BRIDGE_IMPL(sceGxmCreateDeferredContext)
BRIDGE_IMPL(sceGxmCreateRenderTarget)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceGetBackgroundDepth)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceGetBackgroundMask)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceGetBackgroundStencil)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceGetForceLoadMode)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceGetForceStoreMode)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceGetFormat)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceGetStrideInSamples)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceInit)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceInitDisabled)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceIsEnabled)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceSetBackgroundDepth)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceSetBackgroundMask)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceSetBackgroundStencil)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceSetForceLoadMode)
BRIDGE_IMPL(sceGxmDepthStencilSurfaceSetForceStoreMode)
BRIDGE_IMPL(sceGxmDestroyContext)
BRIDGE_IMPL(sceGxmDestroyDeferredContext)
BRIDGE_IMPL(sceGxmDestroyRenderTarget)
BRIDGE_IMPL(sceGxmDisplayQueueAddEntry)
BRIDGE_IMPL(sceGxmDisplayQueueFinish)
BRIDGE_IMPL(sceGxmDraw)
BRIDGE_IMPL(sceGxmDrawInstanced)
BRIDGE_IMPL(sceGxmDrawPrecomputed)
BRIDGE_IMPL(sceGxmEndCommandList)
BRIDGE_IMPL(sceGxmEndScene)
BRIDGE_IMPL(sceGxmExecuteCommandList)
BRIDGE_IMPL(sceGxmFinish)
BRIDGE_IMPL(sceGxmFragmentProgramGetPassType)
BRIDGE_IMPL(sceGxmFragmentProgramGetProgram)
BRIDGE_IMPL(sceGxmFragmentProgramIsEnabled)
BRIDGE_IMPL(sceGxmGetContextType)
BRIDGE_IMPL(sceGxmGetDeferredContextFragmentBuffer)
BRIDGE_IMPL(sceGxmGetDeferredContextVdmBuffer)
BRIDGE_IMPL(sceGxmGetDeferredContextVertexBuffer)
BRIDGE_IMPL(sceGxmGetNotificationRegion)
BRIDGE_IMPL(sceGxmGetParameterBufferThreshold)
BRIDGE_IMPL(sceGxmGetPrecomputedDrawSize)
BRIDGE_IMPL(sceGxmGetPrecomputedFragmentStateSize)
BRIDGE_IMPL(sceGxmGetPrecomputedVertexStateSize)
BRIDGE_IMPL(sceGxmGetRenderTargetMemSize)
BRIDGE_IMPL(sceGxmInitialize)
BRIDGE_IMPL(sceGxmIsDebugVersion)
BRIDGE_IMPL(sceGxmMapFragmentUsseMemory)
BRIDGE_IMPL(sceGxmMapMemory)
BRIDGE_IMPL(sceGxmMapVertexUsseMemory)
BRIDGE_IMPL(sceGxmMidSceneFlush)
BRIDGE_IMPL(_sceGxmMidSceneFlush)
BRIDGE_IMPL(sceGxmNotificationWait)
BRIDGE_IMPL(sceGxmPadHeartbeat)
BRIDGE_IMPL(sceGxmPadTriggerGpuPaTrace)
BRIDGE_IMPL(sceGxmPopUserMarker)
BRIDGE_IMPL(sceGxmPrecomputedDrawInit)
BRIDGE_IMPL(sceGxmPrecomputedDrawSetAllVertexStreams)
BRIDGE_IMPL(sceGxmPrecomputedDrawSetParams)
BRIDGE_IMPL(sceGxmPrecomputedDrawSetParamsInstanced)
BRIDGE_IMPL(sceGxmPrecomputedDrawSetVertexStream)
BRIDGE_IMPL(sceGxmPrecomputedFragmentStateGetDefaultUniformBuffer)
BRIDGE_IMPL(sceGxmPrecomputedFragmentStateInit)
BRIDGE_IMPL(sceGxmPrecomputedFragmentStateSetAllAuxiliarySurfaces)
BRIDGE_IMPL(sceGxmPrecomputedFragmentStateSetAllTextures)
BRIDGE_IMPL(sceGxmPrecomputedFragmentStateSetAllUniformBuffers)
BRIDGE_IMPL(sceGxmPrecomputedFragmentStateSetDefaultUniformBuffer)
BRIDGE_IMPL(sceGxmPrecomputedFragmentStateSetTexture)
BRIDGE_IMPL(sceGxmPrecomputedFragmentStateSetUniformBuffer)
BRIDGE_IMPL(sceGxmPrecomputedVertexStateGetDefaultUniformBuffer)
BRIDGE_IMPL(sceGxmPrecomputedVertexStateInit)
BRIDGE_IMPL(sceGxmPrecomputedVertexStateSetAllTextures)
BRIDGE_IMPL(sceGxmPrecomputedVertexStateSetAllUniformBuffers)
BRIDGE_IMPL(sceGxmPrecomputedVertexStateSetDefaultUniformBuffer)
BRIDGE_IMPL(sceGxmPrecomputedVertexStateSetTexture)
BRIDGE_IMPL(sceGxmPrecomputedVertexStateSetUniformBuffer)
BRIDGE_IMPL(sceGxmProgramCheck)
BRIDGE_IMPL(sceGxmProgramFindParameterByName)
BRIDGE_IMPL(sceGxmProgramFindParameterBySemantic)
BRIDGE_IMPL(sceGxmProgramGetDefaultUniformBufferSize)
BRIDGE_IMPL(sceGxmProgramGetFragmentProgramInputs)
BRIDGE_IMPL(sceGxmProgramGetOutputRegisterFormat)
BRIDGE_IMPL(sceGxmProgramGetParameter)
BRIDGE_IMPL(sceGxmProgramGetParameterCount)
BRIDGE_IMPL(sceGxmProgramGetSize)
BRIDGE_IMPL(sceGxmProgramGetType)
BRIDGE_IMPL(sceGxmProgramGetVertexProgramOutputs)
BRIDGE_IMPL(sceGxmProgramIsDepthReplaceUsed)
BRIDGE_IMPL(sceGxmProgramIsDiscardUsed)
BRIDGE_IMPL(sceGxmProgramIsEquivalent)
BRIDGE_IMPL(sceGxmProgramIsFragColorUsed)
BRIDGE_IMPL(sceGxmProgramIsNativeColorUsed)
BRIDGE_IMPL(sceGxmProgramIsSpriteCoordUsed)
BRIDGE_IMPL(sceGxmProgramParameterGetArraySize)
BRIDGE_IMPL(sceGxmProgramParameterGetCategory)
BRIDGE_IMPL(sceGxmProgramParameterGetComponentCount)
BRIDGE_IMPL(sceGxmProgramParameterGetContainerIndex)
BRIDGE_IMPL(sceGxmProgramParameterGetIndex)
BRIDGE_IMPL(sceGxmProgramParameterGetName)
BRIDGE_IMPL(sceGxmProgramParameterGetResourceIndex)
BRIDGE_IMPL(sceGxmProgramParameterGetSemantic)
BRIDGE_IMPL(sceGxmProgramParameterGetSemanticIndex)
BRIDGE_IMPL(sceGxmProgramParameterGetType)
BRIDGE_IMPL(sceGxmProgramParameterIsRegFormat)
BRIDGE_IMPL(sceGxmProgramParameterIsSamplerCube)
BRIDGE_IMPL(sceGxmPushUserMarker)
BRIDGE_IMPL(sceGxmRemoveRazorGpuCaptureBuffer)
BRIDGE_IMPL(sceGxmRenderTargetGetDriverMemBlock)
BRIDGE_IMPL(sceGxmRenderTargetGetHostMem)
BRIDGE_IMPL(sceGxmReserveFragmentDefaultUniformBuffer)
BRIDGE_IMPL(sceGxmReserveVertexDefaultUniformBuffer)
BRIDGE_IMPL(sceGxmSetAuxiliarySurface)
BRIDGE_IMPL(sceGxmSetBackDepthBias)
BRIDGE_IMPL(sceGxmSetBackDepthFunc)
BRIDGE_IMPL(sceGxmSetBackDepthWriteEnable)
BRIDGE_IMPL(sceGxmSetBackFragmentProgramEnable)
BRIDGE_IMPL(sceGxmSetBackLineFillLastPixelEnable)
BRIDGE_IMPL(sceGxmSetBackPointLineWidth)
BRIDGE_IMPL(sceGxmSetBackPolygonMode)
BRIDGE_IMPL(sceGxmSetBackStencilFunc)
BRIDGE_IMPL(sceGxmSetBackStencilRef)
BRIDGE_IMPL(sceGxmSetBackVisibilityTestEnable)
BRIDGE_IMPL(sceGxmSetBackVisibilityTestIndex)
BRIDGE_IMPL(sceGxmSetBackVisibilityTestOp)
BRIDGE_IMPL(sceGxmSetCullMode)
BRIDGE_IMPL(sceGxmSetDefaultRegionClipAndViewport)
BRIDGE_IMPL(sceGxmSetDeferredContextFragmentBuffer)
BRIDGE_IMPL(sceGxmSetDeferredContextVdmBuffer)
BRIDGE_IMPL(sceGxmSetDeferredContextVertexBuffer)
BRIDGE_IMPL(sceGxmSetFragmentDefaultUniformBuffer)
BRIDGE_IMPL(sceGxmSetFragmentProgram)
BRIDGE_IMPL(sceGxmSetFragmentTexture)
BRIDGE_IMPL(sceGxmSetFragmentUniformBuffer)
BRIDGE_IMPL(sceGxmSetFrontDepthBias)
BRIDGE_IMPL(sceGxmSetFrontDepthFunc)
BRIDGE_IMPL(sceGxmSetFrontDepthWriteEnable)
BRIDGE_IMPL(sceGxmSetFrontFragmentProgramEnable)
BRIDGE_IMPL(sceGxmSetFrontLineFillLastPixelEnable)
BRIDGE_IMPL(sceGxmSetFrontPointLineWidth)
BRIDGE_IMPL(sceGxmSetFrontPolygonMode)
BRIDGE_IMPL(sceGxmSetFrontStencilFunc)
BRIDGE_IMPL(sceGxmSetFrontStencilRef)
BRIDGE_IMPL(sceGxmSetFrontVisibilityTestEnable)
BRIDGE_IMPL(sceGxmSetFrontVisibilityTestIndex)
BRIDGE_IMPL(sceGxmSetFrontVisibilityTestOp)
BRIDGE_IMPL(sceGxmSetPrecomputedFragmentState)
BRIDGE_IMPL(sceGxmSetPrecomputedVertexState)
BRIDGE_IMPL(sceGxmSetRegionClip)
BRIDGE_IMPL(sceGxmSetTwoSidedEnable)
BRIDGE_IMPL(sceGxmSetUniformDataF)
BRIDGE_IMPL(sceGxmSetUserMarker)
BRIDGE_IMPL(sceGxmSetValidationEnable)
BRIDGE_IMPL(sceGxmSetVertexDefaultUniformBuffer)
BRIDGE_IMPL(sceGxmSetVertexProgram)
BRIDGE_IMPL(sceGxmSetVertexStream)
BRIDGE_IMPL(sceGxmSetVertexTexture)
BRIDGE_IMPL(sceGxmSetVertexUniformBuffer)
BRIDGE_IMPL(sceGxmSetViewport)
BRIDGE_IMPL(sceGxmSetViewportEnable)
BRIDGE_IMPL(sceGxmSetVisibilityBuffer)
BRIDGE_IMPL(sceGxmSetWBufferEnable)
BRIDGE_IMPL(sceGxmSetWClampEnable)
BRIDGE_IMPL(sceGxmSetWClampValue)
BRIDGE_IMPL(sceGxmSetWarningEnabled)
BRIDGE_IMPL(sceGxmSetYuvProfile)
BRIDGE_IMPL(sceGxmShaderPatcherAddRefFragmentProgram)
BRIDGE_IMPL(sceGxmShaderPatcherAddRefVertexProgram)
BRIDGE_IMPL(sceGxmShaderPatcherCreate)
BRIDGE_IMPL(sceGxmShaderPatcherCreateFragmentProgram)
BRIDGE_IMPL(sceGxmShaderPatcherCreateMaskUpdateFragmentProgram)
BRIDGE_IMPL(sceGxmShaderPatcherCreateVertexProgram)
BRIDGE_IMPL(sceGxmShaderPatcherDestroy)
BRIDGE_IMPL(sceGxmShaderPatcherForceUnregisterProgram)
BRIDGE_IMPL(sceGxmShaderPatcherGetBufferMemAllocated)
BRIDGE_IMPL(sceGxmShaderPatcherGetFragmentProgramRefCount)
BRIDGE_IMPL(sceGxmShaderPatcherGetFragmentUsseMemAllocated)
BRIDGE_IMPL(sceGxmShaderPatcherGetHostMemAllocated)
BRIDGE_IMPL(sceGxmShaderPatcherGetProgramFromId)
BRIDGE_IMPL(sceGxmShaderPatcherGetUserData)
BRIDGE_IMPL(sceGxmShaderPatcherGetVertexProgramRefCount)
BRIDGE_IMPL(sceGxmShaderPatcherGetVertexUsseMemAllocated)
BRIDGE_IMPL(sceGxmShaderPatcherRegisterProgram)
BRIDGE_IMPL(sceGxmShaderPatcherReleaseFragmentProgram)
BRIDGE_IMPL(sceGxmShaderPatcherReleaseVertexProgram)
BRIDGE_IMPL(sceGxmShaderPatcherSetAuxiliarySurface)
BRIDGE_IMPL(sceGxmShaderPatcherSetUserData)
BRIDGE_IMPL(sceGxmShaderPatcherUnregisterProgram)
BRIDGE_IMPL(sceGxmSyncObjectCreate)
BRIDGE_IMPL(sceGxmSyncObjectDestroy)
BRIDGE_IMPL(sceGxmTerminate)
BRIDGE_IMPL(sceGxmTextureGetData)
BRIDGE_IMPL(sceGxmTextureGetFormat)
BRIDGE_IMPL(sceGxmTextureGetGammaMode)
BRIDGE_IMPL(sceGxmTextureGetHeight)
BRIDGE_IMPL(sceGxmTextureGetLodBias)
BRIDGE_IMPL(sceGxmTextureGetLodMin)
BRIDGE_IMPL(sceGxmTextureGetMagFilter)
BRIDGE_IMPL(sceGxmTextureGetMinFilter)
BRIDGE_IMPL(sceGxmTextureGetMipFilter)
BRIDGE_IMPL(sceGxmTextureGetMipmapCount)
BRIDGE_IMPL(sceGxmTextureGetMipmapCountUnsafe)
BRIDGE_IMPL(sceGxmTextureGetNormalizeMode)
BRIDGE_IMPL(sceGxmTextureGetPalette)
BRIDGE_IMPL(sceGxmTextureGetStride)
BRIDGE_IMPL(sceGxmTextureGetType)
BRIDGE_IMPL(sceGxmTextureGetUAddrMode)
BRIDGE_IMPL(sceGxmTextureGetUAddrModeSafe)
BRIDGE_IMPL(sceGxmTextureGetVAddrMode)
BRIDGE_IMPL(sceGxmTextureGetVAddrModeSafe)
BRIDGE_IMPL(sceGxmTextureGetWidth)
BRIDGE_IMPL(sceGxmTextureInitCube)
BRIDGE_IMPL(sceGxmTextureInitCubeArbitrary)
BRIDGE_IMPL(sceGxmTextureInitLinear)
BRIDGE_IMPL(sceGxmTextureInitLinearStrided)
BRIDGE_IMPL(sceGxmTextureInitSwizzled)
BRIDGE_IMPL(sceGxmTextureInitSwizzledArbitrary)
BRIDGE_IMPL(sceGxmTextureInitTiled)
BRIDGE_IMPL(sceGxmTextureSetData)
BRIDGE_IMPL(sceGxmTextureSetFormat)
BRIDGE_IMPL(sceGxmTextureSetGammaMode)
BRIDGE_IMPL(sceGxmTextureSetHeight)
BRIDGE_IMPL(sceGxmTextureSetLodBias)
BRIDGE_IMPL(sceGxmTextureSetLodMin)
BRIDGE_IMPL(sceGxmTextureSetMagFilter)
BRIDGE_IMPL(sceGxmTextureSetMinFilter)
BRIDGE_IMPL(sceGxmTextureSetMipFilter)
BRIDGE_IMPL(sceGxmTextureSetMipmapCount)
BRIDGE_IMPL(sceGxmTextureSetNormalizeMode)
BRIDGE_IMPL(sceGxmTextureSetPalette)
BRIDGE_IMPL(sceGxmTextureSetStride)
BRIDGE_IMPL(sceGxmTextureSetUAddrMode)
BRIDGE_IMPL(sceGxmTextureSetUAddrModeSafe)
BRIDGE_IMPL(sceGxmTextureSetVAddrMode)
BRIDGE_IMPL(sceGxmTextureSetVAddrModeSafe)
BRIDGE_IMPL(sceGxmTextureSetWidth)
BRIDGE_IMPL(sceGxmTextureValidate)
BRIDGE_IMPL(sceGxmTransferCopy)
BRIDGE_IMPL(sceGxmTransferDownscale)
BRIDGE_IMPL(sceGxmTransferFill)
BRIDGE_IMPL(sceGxmTransferFinish)
BRIDGE_IMPL(sceGxmUnmapFragmentUsseMemory)
BRIDGE_IMPL(sceGxmUnmapMemory)
BRIDGE_IMPL(sceGxmUnmapVertexUsseMemory)
BRIDGE_IMPL(sceGxmVertexFence)
BRIDGE_IMPL(sceGxmVertexProgramGetProgram)
BRIDGE_IMPL(sceGxmWaitEvent)
