// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <crypto/hash.h>
#include <gxm/functions.h>
#include <gxm/types.h>
#include <kernel/thread/thread_functions.h>
#include <renderer/functions.h>
#include <renderer/types.h>
#include <util/lock_and_find.h>
#include <util/log.h>

#include <psp2/kernel/error.h>

struct SceGxmContext {
    GxmContextState state;
    GXMRecordState record;
    std::unique_ptr<renderer::Context> renderer;
};

struct SceGxmRenderTarget {
    std::unique_ptr<renderer::RenderTarget> renderer;
    std::uint16_t width;
    std::uint16_t height;
};

struct FragmentProgramCacheKey {
    SceGxmRegisteredProgram fragment_program;
    emu::SceGxmBlendInfo blend_info;
};

typedef std::map<FragmentProgramCacheKey, Ptr<SceGxmFragmentProgram>> FragmentProgramCache;

struct SceGxmShaderPatcher {
    FragmentProgramCache fragment_program_cache;
};

// clang-format off
static const size_t size_mask_gxp = 176;
static const uint8_t mask_gxp[] = {
    0x47, 0x58, 0x50, 0x00, 0x01, 0x05, 0x60, 0x03, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};
// clang-format on

static bool operator<(const SceGxmRegisteredProgram &a, const SceGxmRegisteredProgram &b) {
    return a.program < b.program;
}

static bool operator<(const emu::SceGxmBlendInfo &a, const emu::SceGxmBlendInfo &b) {
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

EXPORT(int, sceGxmAddRazorGpuCaptureBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmBeginCommandList) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmBeginScene, SceGxmContext *context, unsigned int flags, const SceGxmRenderTarget *renderTarget, const SceGxmValidRegion *validRegion, SceGxmSyncObject *vertexSyncObject, Ptr<SceGxmSyncObject> fragmentSyncObject, const emu::SceGxmColorSurface *colorSurface, const emu::SceGxmDepthStencilSurface *depthStencil) {
    assert(context != nullptr);
    assert(flags == 0);
    assert(renderTarget != nullptr);
    assert(validRegion == nullptr);
    assert(vertexSyncObject == nullptr);

    if (host.gxm.is_in_scene) {
        return RET_ERROR(SCE_GXM_ERROR_WITHIN_SCENE);
    }
    if (depthStencil == nullptr && colorSurface == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    context->state.fragment_sync_object = fragmentSyncObject;
    if (fragmentSyncObject.get(host.mem) != nullptr) {
        SceGxmSyncObject *sync = fragmentSyncObject.get(host.mem);
        // Wait for both display queue and fragment stage to be done.
        // If it's offline render, the sync object already has the display queue subject done, so don't worry.
        renderer::wishlist(sync, (renderer::SyncObjectSubject)(renderer::SyncObjectSubject::DisplayQueue | renderer::SyncObjectSubject::Fragment));
    }

    // TODO This may not be right.
    context->state.fragment_ring_buffer_used = 0;
    context->state.vertex_ring_buffer_used = 0;

    // It's legal to set at client.
    host.gxm.is_in_scene = true;

    // Reset command list and finish status
    renderer::reset_command_list(context->renderer->command_list);

    emu::SceGxmColorSurface *color_surface_copy = nullptr;
    emu::SceGxmDepthStencilSurface *depth_stencil_surface_copy = nullptr;

    if (colorSurface) {
        color_surface_copy = new emu::SceGxmColorSurface;
        *color_surface_copy = *colorSurface;
    }

    if (depthStencil) {
        depth_stencil_surface_copy = new emu::SceGxmDepthStencilSurface;
        *depth_stencil_surface_copy = *depthStencil;
    }

    const std::uint32_t xmin = 0;
    const std::uint32_t ymin = 0;
    const std::uint32_t xmax = (validRegion ? validRegion->xMax : renderTarget->width - 1);
    const std::uint32_t ymax = (validRegion ? validRegion->yMax : renderTarget->height - 1);

    renderer::set_context(*host.renderer, context->renderer.get(), &context->state, renderTarget->renderer.get(),
        color_surface_copy, depth_stencil_surface_copy);

    // Set default region clip and viewport
    renderer::set_region_clip(*host.renderer, context->renderer.get(), &context->state, SCE_GXM_REGION_CLIP_OUTSIDE,
        xmin, xmax, ymin, ymax);

    renderer::set_viewport(*host.renderer, context->renderer.get(), &context->state,
        0.5f * static_cast<float>(1.0f + xmin + xmax), 0.5f * (static_cast<float>(1.0 + ymin + ymax)),
        0.5f, 0.5f * static_cast<float>(1.0f + xmax - xmin), -0.5f * static_cast<float>(1.0f + ymax - ymin), 0.5f);

    return 0;
}

EXPORT(int, sceGxmBeginSceneEx) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmColorSurfaceGetClip) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceGxmColorSurfaceGetData, emu::SceGxmColorSurface *surface) {
    return Ptr<void>(surface->pbeEmitWords[3]);
}

EXPORT(SceGxmColorSurfaceDitherMode, sceGxmColorSurfaceGetDitherMode, emu::SceGxmColorSurface *surface) {
    STUBBED("SCE_GXM_COLOR_SURFACE_DITHER_DISABLED");
    return SceGxmColorSurfaceDitherMode::SCE_GXM_COLOR_SURFACE_DITHER_DISABLED;
}

EXPORT(SceGxmColorFormat, sceGxmColorSurfaceGetFormat, emu::SceGxmColorSurface *surface) {
    STUBBED("SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR");
    return SceGxmColorFormat::SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR;
}

EXPORT(SceGxmColorSurfaceGammaMode, sceGxmColorSurfaceGetGammaMode, emu::SceGxmColorSurface *surface) {
    STUBBED("SCE_GXM_COLOR_SURFACE_GAMMA_NONE");
    return SceGxmColorSurfaceGammaMode::SCE_GXM_COLOR_SURFACE_GAMMA_NONE;
}

EXPORT(SceGxmColorSurfaceScaleMode, sceGxmColorSurfaceGetScaleMode, emu::SceGxmColorSurface *surface) {
    STUBBED("SCE_GXM_COLOR_SURFACE_SCALE_NONE");
    return SceGxmColorSurfaceScaleMode::SCE_GXM_COLOR_SURFACE_SCALE_NONE;
}

EXPORT(int, sceGxmColorSurfaceGetStrideInPixels, emu::SceGxmColorSurface *surface) {
    return surface->pbeEmitWords[2];
}

EXPORT(SceGxmColorSurfaceType, sceGxmColorSurfaceGetType, emu::SceGxmColorSurface *surface) {
    STUBBED("SCE_GXM_COLOR_SURFACE_LINEAR");
    return SceGxmColorSurfaceType::SCE_GXM_COLOR_SURFACE_LINEAR;
}

EXPORT(int, sceGxmColorSurfaceInit, emu::SceGxmColorSurface *surface, SceGxmColorFormat colorFormat, SceGxmColorSurfaceType surfaceType, SceGxmColorSurfaceScaleMode scaleMode, SceGxmOutputRegisterSize outputRegisterSize, unsigned int width, unsigned int height, unsigned int strideInPixels, Ptr<void> data) {
    assert(surface != nullptr);
    assert(colorFormat == SCE_GXM_COLOR_FORMAT_A8B8G8R8);
    assert(surfaceType == SCE_GXM_COLOR_SURFACE_LINEAR);
    assert(scaleMode == SCE_GXM_COLOR_SURFACE_SCALE_NONE);
    assert(outputRegisterSize == SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT);
    assert(width > 0);
    assert(height > 0);
    assert(strideInPixels > 0);
    assert(data);

    memset(surface, 0, sizeof(*surface));
    surface->pbeEmitWords[0] = width;
    surface->pbeEmitWords[1] = height;
    surface->pbeEmitWords[2] = strideInPixels;
    surface->pbeEmitWords[3] = data.address();
    surface->outputRegisterSize = outputRegisterSize;

    return 0;
}

EXPORT(int, sceGxmColorSurfaceInitDisabled) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmColorSurfaceIsEnabled) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmColorSurfaceSetClip) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmColorSurfaceSetData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmColorSurfaceSetDitherMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmColorSurfaceSetFormat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmColorSurfaceSetGammaMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmColorSurfaceSetScaleMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmCreateContext, const emu::SceGxmContextParams *params, Ptr<SceGxmContext> *context) {
    assert(params != nullptr);
    assert(context != nullptr);

    *context = alloc<SceGxmContext>(host.mem, __FUNCTION__);
    if (!*context) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    SceGxmContext *const ctx = context->get(host.mem);
    ctx->state.params = *params;

    if (!renderer::create_context(*host.renderer, ctx->renderer)) {
        free(host.mem, *context);
        context->reset();
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }

    return 0;
}

EXPORT(int, sceGxmCreateDeferredContext) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmCreateRenderTarget, const SceGxmRenderTargetParams *params, Ptr<SceGxmRenderTarget> *renderTarget) {
    assert(params != nullptr);
    assert(renderTarget != nullptr);

    *renderTarget = alloc<SceGxmRenderTarget>(host.mem, __FUNCTION__);
    if (!*renderTarget) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    SceGxmRenderTarget *const rt = renderTarget->get(host.mem);
    if (!renderer::create_render_target(*host.renderer, rt->renderer, params)) {
        free(host.mem, *renderTarget);
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }

    rt->width = params->width;
    rt->height = params->height;

    return 0;
}

EXPORT(int, sceGxmDepthStencilSurfaceGetBackgroundDepth) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDepthStencilSurfaceGetBackgroundMask) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDepthStencilSurfaceGetBackgroundStencil) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDepthStencilSurfaceGetForceLoadMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDepthStencilSurfaceGetForceStoreMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDepthStencilSurfaceGetFormat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDepthStencilSurfaceGetStrideInSamples) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDepthStencilSurfaceInit, emu::SceGxmDepthStencilSurface *surface, SceGxmDepthStencilFormat depthStencilFormat, SceGxmDepthStencilSurfaceType surfaceType, unsigned int strideInSamples, Ptr<void> depthData, Ptr<void> stencilData) {
    assert(surface != nullptr);
    assert(surfaceType == SCE_GXM_DEPTH_STENCIL_SURFACE_TILED);
    assert(strideInSamples > 0);
    assert(depthData);

    // TODO What to do here?
    memset(surface, 0, sizeof(*surface));
    surface->depthData = depthData;
    surface->stencilData = stencilData;

    return 0;
}

EXPORT(int, sceGxmDepthStencilSurfaceInitDisabled) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDepthStencilSurfaceIsEnabled) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDepthStencilSurfaceSetBackgroundDepth) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDepthStencilSurfaceSetBackgroundMask) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDepthStencilSurfaceSetBackgroundStencil) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmDepthStencilSurfaceSetForceLoadMode, emu::SceGxmDepthStencilSurface *surface,
    SceGxmDepthStencilForceLoadMode forceLoad) {
    surface->zlsControl = (forceLoad & 2) | (surface->zlsControl & 0xFFFFFFFD);
}

EXPORT(void, sceGxmDepthStencilSurfaceSetForceStoreMode, emu::SceGxmDepthStencilSurface *surface,
    SceGxmDepthStencilForceStoreMode forceStore) {
    surface->zlsControl = (forceStore & 4) | (surface->zlsControl & 0xFFFFFFFB);
}

EXPORT(int, sceGxmDestroyContext, Ptr<SceGxmContext> context) {
    assert(context);

    free(host.mem, context);

    return 0;
}

EXPORT(int, sceGxmDestroyDeferredContext) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDestroyRenderTarget, Ptr<SceGxmRenderTarget> renderTarget) {
    MemState &mem = host.mem;
    assert(renderTarget);

    renderer::destroy_render_target(*host.renderer, renderTarget.get(mem)->renderer);

    free(mem, renderTarget);

    return 0;
}

EXPORT(int, sceGxmDisplayQueueAddEntry, Ptr<SceGxmSyncObject> oldBuffer, Ptr<SceGxmSyncObject> newBuffer, Ptr<const void> callbackData) {
    if (!oldBuffer || !newBuffer || !callbackData) {
        return SCE_GXM_ERROR_INVALID_POINTER;
    }

    DisplayCallback display_callback;

    const Address address = alloc(host.mem, host.gxm.params.displayQueueCallbackDataSize, __FUNCTION__);
    const Ptr<void> ptr(address);
    memcpy(ptr.get(host.mem), callbackData.get(host.mem), host.gxm.params.displayQueueCallbackDataSize);

    // Block future rendering by setting value2 of sync object
    SceGxmSyncObject *newBufferSync = Ptr<SceGxmSyncObject>(newBuffer).get(host.mem);

    renderer::subject_in_progress(newBufferSync, renderer::SyncObjectSubject::DisplayQueue);

    display_callback.data = address;
    display_callback.pc = host.gxm.params.displayQueueCallback.address();
    display_callback.old_buffer = oldBuffer.address();
    display_callback.new_buffer = newBuffer.address();
    host.gxm.display_queue.push(display_callback);

    return 0;
}

EXPORT(int, sceGxmDisplayQueueFinish) {
    return 0;
}

EXPORT(int, sceGxmDraw, SceGxmContext *context, SceGxmPrimitiveType primType, SceGxmIndexFormat indexType, const void *indexData, unsigned int indexCount) {
    assert(context != nullptr);
    assert(indexData != nullptr);

    if (!host.gxm.is_in_scene) {
        return RET_ERROR(SCE_GXM_ERROR_NOT_WITHIN_SCENE);
    }

    if (!context->record.fragment_program || !context->record.vertex_program) {
        return RET_ERROR(SCE_GXM_ERROR_NULL_PROGRAM);
    }

    if (context->state.vertex_last_reserve_status == SceGxmLastReserveStatus::Reserved) {
        const auto vertex_program = context->record.vertex_program.get(host.mem);
        const auto program = vertex_program->program.get(host.mem);

        const size_t size = program->default_uniform_buffer_count * 4;
        const size_t next_used = context->state.vertex_ring_buffer_used + size;
        assert(next_used <= context->state.params.vertexRingBufferMemSize);
        if (next_used > context->state.params.vertexRingBufferMemSize) {
            return RET_ERROR(SCE_GXM_ERROR_RESERVE_FAILED); // TODO: Does not actually return this on immediate context
        }

        context->state.vertex_ring_buffer_used = next_used;
        context->state.vertex_last_reserve_status = SceGxmLastReserveStatus::Available;
    }
    if (context->state.fragment_last_reserve_status == SceGxmLastReserveStatus::Reserved) {
        const auto fragment_program = context->record.fragment_program.get(host.mem);
        const auto program = fragment_program->program.get(host.mem);

        const size_t size = program->default_uniform_buffer_count * 4;
        const size_t next_used = context->state.fragment_ring_buffer_used + size;
        assert(next_used <= context->state.params.fragmentRingBufferMemSize);
        if (next_used > context->state.params.fragmentRingBufferMemSize) {
            return RET_ERROR(SCE_GXM_ERROR_RESERVE_FAILED); // TODO: Does not actually return this on immediate context
        }

        context->state.fragment_ring_buffer_used = next_used;
        context->state.fragment_last_reserve_status = SceGxmLastReserveStatus::Available;
    }

    const SceGxmFragmentProgram &gxm_fragment_program = *context->record.fragment_program.get(host.mem);
    const SceGxmVertexProgram &gxm_vertex_program = *context->record.vertex_program.get(host.mem);

    // Set uniforms
    const SceGxmProgram &vertex_program_gxp = *gxm_vertex_program.program.get(host.mem);
    const SceGxmProgram &fragment_program_gxp = *gxm_fragment_program.program.get(host.mem);

    // Update uniforms from this side. We should pass the pointer to parameter struct, since from there it can
    // find the name and other things based on the pointer in memory. The pointer should be persists until
    // the fragment is done, so we are guranteed to be safe.
    renderer::set_uniforms(*host.renderer, context->renderer.get(), vertex_program_gxp, context->state.vertex_uniform_buffers, host.mem);
    renderer::set_uniforms(*host.renderer, context->renderer.get(), fragment_program_gxp, context->state.fragment_uniform_buffers, host.mem);

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
    for (const emu::SceGxmVertexAttribute &attribute : gxm_vertex_program.attributes) {
        const SceGxmAttributeFormat attribute_format = static_cast<SceGxmAttributeFormat>(attribute.format);
        const size_t attribute_size = gxm::attribute_format_size(attribute_format) * attribute.componentCount;
        const SceGxmVertexStream &stream = gxm_vertex_program.streams[attribute.streamIndex];
        const size_t data_length = attribute.offset + (max_index * stream.stride) + attribute_size;
        max_data_length[attribute.streamIndex] = std::max<size_t>(max_data_length[attribute.streamIndex], data_length);
        stream_used |= (1 << attribute.streamIndex);
    }

    // Copy and queue upload
    for (size_t stream_index = 0; stream_index < SCE_GXM_MAX_VERTEX_STREAMS; ++stream_index) {
        // Upload it
        if (stream_used & (1 << static_cast<std::uint16_t>(stream_index))) {
            const size_t data_length = max_data_length[stream_index];
            const std::uint8_t *const data = context->state.stream_data[stream_index].cast<const std::uint8_t>().get(host.mem);

            std::uint8_t *a_copy = new std::uint8_t[data_length];
            std::copy(data, data + data_length, a_copy);

            renderer::set_vertex_stream(*host.renderer, context->renderer.get(), &context->state, stream_index,
                data_length, a_copy);
        }
    }

    // Fragment texture is copied so no need to set it here.
    // Add draw command
    renderer::draw(*host.renderer, context->renderer.get(), &context->state, primType, indexType, indexData, indexCount);

    return 0;
}

EXPORT(int, sceGxmDrawInstanced) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDrawPrecomputed) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmEndCommandList) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmEndScene, SceGxmContext *context, const emu::SceGxmNotification *vertexNotification, const emu::SceGxmNotification *fragmentNotification) {
    const MemState &mem = host.mem;
    assert(context != nullptr);
    assert(vertexNotification == nullptr);
    //assert(fragmentNotification == nullptr);

    if (!host.gxm.is_in_scene) {
        return RET_ERROR(SCE_GXM_ERROR_NOT_WITHIN_SCENE);
    }

    // Add command to end the scene
    renderer::sync_surface_data(*host.renderer, context->renderer.get(), &context->state);

    // Add NOP for SceGxmFinish
    context->renderer->render_finish_status = renderer::CommandErrorCodePending;
    renderer::add_command(context->renderer.get(), renderer::CommandOpcode::Nop, &context->renderer->render_finish_status,
        (int)0);

    if (context->state.fragment_sync_object) {
        // Add NOP for our sync object
        SceGxmSyncObject *sync = context->state.fragment_sync_object.get(mem);
        renderer::add_command(context->renderer.get(), renderer::CommandOpcode::SignalSyncObject,
            nullptr, context->state.fragment_sync_object);

        renderer::subject_in_progress(sync, renderer::SyncObjectSubject::Fragment);
    }

    if (fragmentNotification) {
        volatile uint32_t *fragment_address = fragmentNotification->address.get(host.mem);
        *fragment_address = fragmentNotification->value;
    }

    // Submit our command list
    renderer::submit_command_list(*host.renderer, context->renderer.get(), &context->state,
        context->renderer->command_list);

    host.gxm.is_in_scene = false;
    host.renderer->scene_processed_since_last_frame++;

    return 0;
}

EXPORT(int, sceGxmExecuteCommandList) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmFinish, SceGxmContext *context) {
    assert(context != nullptr);

    // Wait on this context's rendering finish code. There is one for sync object and one specifically
    // for SceGxmFinish.
    renderer::finish(*host.renderer, *context->renderer);
}

EXPORT(int, sceGxmFragmentProgramGetPassType) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmFragmentProgramGetProgram) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmFragmentProgramIsEnabled) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmGetContextType) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmGetDeferredContextFragmentBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmGetDeferredContextVdmBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmGetDeferredContextVertexBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<uint32_t>, sceGxmGetNotificationRegion) {
    return host.gxm.notification_region;
}

EXPORT(int, sceGxmGetParameterBufferThreshold) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmGetPrecomputedDrawSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmGetPrecomputedFragmentStateSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmGetPrecomputedVertexStateSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmGetRenderTargetMemSizes, const SceGxmRenderTargetParams *params, uint32_t *hostMemSize) {
    *hostMemSize = 2 * 1024 * 1024;
    return STUBBED("2MB host mem");
}

struct GxmThreadParams {
    KernelState *kernel = nullptr;
    MemState *mem = nullptr;
    SceUID thid = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
    GxmState *gxm = nullptr;
    renderer::State *renderer = nullptr;
    std::shared_ptr<SDL_semaphore> host_may_destroy_params = std::shared_ptr<SDL_semaphore>(SDL_CreateSemaphore(0), SDL_DestroySemaphore);
};

static int SDLCALL thread_function(void *data) {
    const GxmThreadParams params = *static_cast<const GxmThreadParams *>(data);
    SDL_SemPost(params.host_may_destroy_params.get());
    while (true) {
        auto display_callback = params.gxm->display_queue.pop();
        if (!display_callback)
            break;

        SceGxmSyncObject *newBuffer = Ptr<SceGxmSyncObject>(display_callback->new_buffer).get(*params.mem);

        // Wait for fragment on the new buffer to finish
        renderer::wishlist(newBuffer, renderer::SyncObjectSubject::Fragment);

        // Now run callback
        {
            const ThreadStatePtr thread = lock_and_find(params.thid, params.kernel->threads, params.kernel->mutex);
            if ((!thread) || thread->to_do == ThreadToDo::exit)
                break;
        }
        const ThreadStatePtr display_thread = util::find(params.thid, params.kernel->threads);
        run_callback(*display_thread, display_callback->pc, display_callback->data);

        free(*params.mem, display_callback->data);

        // Should not block anymore.
        renderer::subject_done(newBuffer, renderer::SyncObjectSubject::DisplayQueue);
    }

    return 0;
}

EXPORT(int, sceGxmInitialize, const emu::SceGxmInitializeParams *params) {
    assert(params != nullptr);

    host.gxm.params = *params;
    host.gxm.display_queue.maxPendingCount_ = params->displayQueueMaxPendingCount;

    const ThreadStatePtr main_thread = util::find(thread_id, host.kernel.threads);

    const CallImport call_import = [&host](CPUState &cpu, uint32_t nid, SceUID thread_id) {
        ::call_import(host, cpu, nid, thread_id);
    };

    const auto stack_size = SCE_KERNEL_STACK_SIZE_USER_DEFAULT; // TODO: Verify this is the correct stack size

    host.gxm.display_queue_thread = create_thread(Ptr<void>(read_pc(*main_thread->cpu)), host.kernel, host.mem, "SceGxmDisplayQueue", SCE_KERNEL_HIGHEST_PRIORITY_USER, stack_size, call_import, false);

    if (host.gxm.display_queue_thread < 0) {
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }

    const ThreadStatePtr display_thread = util::find(host.gxm.display_queue_thread, host.kernel.threads);

    const std::function<void(SDL_Thread *)> delete_thread = [display_thread](SDL_Thread *running_thread) {
        {
            const std::lock_guard<std::mutex> lock(display_thread->mutex);
            display_thread->to_do = ThreadToDo::exit;
        }
        display_thread->something_to_do.notify_all(); // TODO Should this be notify_one()?
    };

    GxmThreadParams gxm_params;
    gxm_params.mem = &host.mem;
    gxm_params.kernel = &host.kernel;
    gxm_params.thid = host.gxm.display_queue_thread;
    gxm_params.gxm = &host.gxm;
    gxm_params.renderer = host.renderer.get();

    const ThreadPtr running_thread(SDL_CreateThread(&thread_function, "SceGxmDisplayQueue", &gxm_params), delete_thread);
    SDL_SemWait(gxm_params.host_may_destroy_params.get());
    host.kernel.running_threads.emplace(host.gxm.display_queue_thread, running_thread);
    host.gxm.notification_region = Ptr<uint32_t>(alloc(host.mem, MB(1), "SceGxmNotificationRegion"));
    memset(host.gxm.notification_region.get(host.mem), 0, MB(1));
    return 0;
}

EXPORT(int, sceGxmIsDebugVersion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmMapFragmentUsseMemory, Ptr<void> base, SceSize size, unsigned int *offset) {
    assert(base);
    assert(size > 0);
    assert(offset != nullptr);

    // TODO What should this be?
    *offset = base.address();

    return 0;
}

EXPORT(int, sceGxmMapMemory, void *base, SceSize size, SceGxmMemoryAttribFlags attr) {
    assert(base != nullptr);
    assert(size > 0);
    assert((attr == SCE_GXM_MEMORY_ATTRIB_READ) || (attr == SCE_GXM_MEMORY_ATTRIB_RW));

    return 0;
}

EXPORT(int, sceGxmMapVertexUsseMemory, Ptr<void> base, SceSize size, unsigned int *offset) {
    assert(base);
    assert(size > 0);
    assert(offset != nullptr);

    // TODO What should this be?
    *offset = base.address();

    return 0;
}

EXPORT(int, sceGxmMidSceneFlush) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmNotificationWait) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPadHeartbeat, const emu::SceGxmColorSurface *displaySurface, SceGxmSyncObject *displaySyncObject) {
    assert(displaySurface != nullptr);
    assert(displaySyncObject != nullptr);

    host.renderer->average_scene_per_frame = (host.renderer->average_scene_per_frame + host.renderer->scene_processed_since_last_frame + 1) >> 1;

    host.renderer->scene_processed_since_last_frame = 0;

    return 0;
}

EXPORT(int, sceGxmPadTriggerGpuPaTrace) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPopUserMarker) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedDrawInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedDrawSetAllVertexStreams) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedDrawSetParams) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedDrawSetParamsInstanced) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedDrawSetVertexStream) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedFragmentStateGetDefaultUniformBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedFragmentStateInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetAllAuxiliarySurfaces) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetAllTextures) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetAllUniformBuffers) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetDefaultUniformBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetTexture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetUniformBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedVertexStateGetDefaultUniformBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedVertexStateInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedVertexStateSetAllTextures) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedVertexStateSetAllUniformBuffers) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedVertexStateSetDefaultUniformBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedVertexStateSetTexture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedVertexStateSetUniformBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmProgramCheck, const SceGxmProgram *program) {
    assert(program != nullptr);

    assert(memcmp(&program->magic, "GXP", 4) == 0);

    return 0;
}

EXPORT(Ptr<SceGxmProgramParameter>, sceGxmProgramFindParameterByName, const SceGxmProgram *program, const char *name) {
    const MemState &mem = host.mem;
    assert(program != nullptr);
    assert(name != nullptr);

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
    const MemState &mem = host.mem;
    assert(program != nullptr);

    const SceGxmProgramParameter *const parameters = reinterpret_cast<const SceGxmProgramParameter *>(reinterpret_cast<const uint8_t *>(&program->parameters_offset) + program->parameters_offset);
    uint32_t current_index = 0;
    for (uint32_t i = 0; i < program->parameter_count; ++i) {
        const SceGxmProgramParameter *const parameter = &parameters[i];
        const uint8_t *const parameter_bytes = reinterpret_cast<const uint8_t *>(parameter);
        if (parameter->semantic == semantic) {
            if (current_index == index) {
                const Address parameter_address = static_cast<Address>(parameter_bytes - &mem.memory[0]);
                return Ptr<SceGxmProgramParameter>(parameter_address);
            }
            current_index++;
        }
    }

    return Ptr<SceGxmProgramParameter>();
}

EXPORT(int, sceGxmProgramGetDefaultUniformBufferSize, const SceGxmProgram *program) {
    return program->default_uniform_buffer_count * 4;
}

EXPORT(int, sceGxmProgramGetFragmentProgramInputs, Ptr<const SceGxmProgram> program_) {
    const auto program = program_.get(host.mem);

    return static_cast<int>(gxp::get_fragment_inputs(*program));
}

EXPORT(int, sceGxmProgramGetOutputRegisterFormat, const SceGxmProgram *program, SceGxmParameterType *type, std::uint32_t *component_count) {
    if (!program || !type || !component_count)
        return SCE_GXM_ERROR_INVALID_POINTER;

    if (!program->is_fragment())
        return SCE_GXM_ERROR_INVALID_VALUE;

    // TODO

    return STUBBED("only error checking");
}

EXPORT(Ptr<SceGxmProgramParameter>, sceGxmProgramGetParameter, const SceGxmProgram *program, unsigned int index) {
    const SceGxmProgramParameter *const parameters = reinterpret_cast<const SceGxmProgramParameter *>(reinterpret_cast<const uint8_t *>(&program->parameters_offset) + program->parameters_offset);

    const SceGxmProgramParameter *const parameter = &parameters[index];
    const uint8_t *const parameter_bytes = reinterpret_cast<const uint8_t *>(parameter);

    const Address parameter_address = static_cast<Address>(parameter_bytes - &host.mem.memory[0]);
    return Ptr<SceGxmProgramParameter>(parameter_address);
}

EXPORT(int, sceGxmProgramGetParameterCount, const SceGxmProgram *program) {
    return program->parameter_count;
}

EXPORT(int, sceGxmProgramGetSize, const SceGxmProgram *program) {
    return program->size;
}

EXPORT(int, sceGxmProgramGetType, const SceGxmProgram *program) {
    return program->get_type();
}

EXPORT(int, sceGxmProgramGetVertexProgramOutputs, Ptr<const SceGxmProgram> program_) {
    const auto program = program_.get(host.mem);

    return static_cast<int>(gxp::get_vertex_outputs(*program));
}

EXPORT(int, sceGxmProgramIsDepthReplaceUsed) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmProgramIsDiscardUsed) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmProgramIsEquivalent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmProgramIsFragColorUsed) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmProgramIsNativeColorUsed, Ptr<const SceGxmProgram> program_) {
    const auto program = program_.get(host.mem);

    return program->is_native_color();
}

EXPORT(int, sceGxmProgramIsSpriteCoordUsed) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmProgramParameterGetArraySize, const SceGxmProgramParameter *parameter) {
    return parameter->array_size;
}

EXPORT(int, sceGxmProgramParameterGetCategory, const SceGxmProgramParameter *parameter) {
    return parameter->category;
}

EXPORT(int, sceGxmProgramParameterGetComponentCount, const SceGxmProgramParameter *parameter) {
    return (int32_t)parameter->component_count;
}

EXPORT(int, sceGxmProgramParameterGetContainerIndex, const SceGxmProgramParameter *parameter) {
    return parameter->container_index;
}

EXPORT(uint32_t, sceGxmProgramParameterGetIndex, const SceGxmProgram *program, const SceGxmProgramParameter *parameter) {
    uint64_t parameter_offset = program->parameters_offset;

    if (parameter_offset > 0)
        parameter_offset += (uint64_t)&program->parameters_offset;
    return (uint32_t)((uint64_t)parameter - parameter_offset) >> 4;
}

EXPORT(Ptr<const char>, sceGxmProgramParameterGetName, Ptr<const SceGxmProgramParameter> parameter) {
    if (!parameter)
        return {};
    return Ptr<const char>(parameter.address() + parameter.get(host.mem)->name_offset);
}

EXPORT(unsigned int, sceGxmProgramParameterGetResourceIndex, const SceGxmProgramParameter *parameter) {
    return parameter->resource_index;
}

EXPORT(int, sceGxmProgramParameterGetSemantic, const SceGxmProgramParameter *parameter) {
    if (parameter->category != SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE)
        return 0;

    return parameter->semantic;
}

EXPORT(int, sceGxmProgramParameterGetSemanticIndex, const SceGxmProgramParameter *parameter) {
    return parameter->semantic & 0xF00;
}

EXPORT(int, sceGxmProgramParameterGetType, const SceGxmProgramParameter *parameter) {
    return parameter->type;
}

EXPORT(int, sceGxmProgramParameterIsRegFormat) {
    return UNIMPLEMENTED();
}

EXPORT(bool, sceGxmProgramParameterIsSamplerCube, const SceGxmProgramParameter *parameter) {
    return parameter->is_sampler_cube();
}

EXPORT(int, sceGxmPushUserMarker) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmRemoveRazorGpuCaptureBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmRenderTargetGetDriverMemBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmReserveFragmentDefaultUniformBuffer, SceGxmContext *context, Ptr<void> *uniformBuffer) {
    if (!context || !uniformBuffer)
        return SCE_GXM_ERROR_INVALID_POINTER;

    *uniformBuffer = context->state.params.fragmentRingBufferMem.cast<uint8_t>() + static_cast<int32_t>(context->state.fragment_ring_buffer_used);
    context->state.fragment_last_reserve_status = SceGxmLastReserveStatus::Reserved;
    context->state.fragment_uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX] = *uniformBuffer;

    return 0;
}

EXPORT(int, sceGxmRenderTargetGetHostMem) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmReserveVertexDefaultUniformBuffer, SceGxmContext *context, Ptr<void> *uniformBuffer) {
    if (!context || !uniformBuffer)
        return SCE_GXM_ERROR_INVALID_POINTER;

    *uniformBuffer = context->state.params.vertexRingBufferMem.cast<uint8_t>() + static_cast<int32_t>(context->state.vertex_ring_buffer_used);

    context->state.vertex_last_reserve_status = SceGxmLastReserveStatus::Reserved;
    context->state.vertex_uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX] = *uniformBuffer;

    return 0;
}

EXPORT(int, sceGxmSetAuxiliarySurface) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetBackDepthBias, SceGxmContext *context, int factor, int units) {
    renderer::set_depth_bias(*host.renderer, context->renderer.get(), &context->state, false, factor, units);
}

EXPORT(void, sceGxmSetBackDepthFunc, SceGxmContext *context, SceGxmDepthFunc depthFunc) {
    renderer::set_depth_func(*host.renderer, context->renderer.get(), &context->state, false, depthFunc);
}

EXPORT(void, sceGxmSetBackDepthWriteEnable, SceGxmContext *context, SceGxmDepthWriteMode enable) {
    renderer::set_depth_write_enable_mode(*host.renderer, context->renderer.get(), &context->state, false, enable);
}

EXPORT(int, sceGxmSetBackFragmentProgramEnable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetBackLineFillLastPixelEnable) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetBackPointLineWidth, SceGxmContext *context, unsigned int width) {
    renderer::set_point_line_width(*host.renderer, context->renderer.get(), &context->state, false, width);
}

EXPORT(void, sceGxmSetBackPolygonMode, SceGxmContext *context, SceGxmPolygonMode mode) {
    renderer::set_polygon_mode(*host.renderer, context->renderer.get(), &context->state, false, mode);
}

EXPORT(void, sceGxmSetBackStencilFunc, SceGxmContext *context, SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail, SceGxmStencilOp depthPass, unsigned char compareMask, unsigned char writeMask) {
    renderer::set_stencil_func(*host.renderer, context->renderer.get(), &context->state, false, func, stencilFail, depthFail, depthPass, compareMask, writeMask);
}

EXPORT(void, sceGxmSetBackStencilRef, SceGxmContext *context, unsigned char sref) {
    renderer::set_stencil_ref(*host.renderer, context->renderer.get(), &context->state, false, sref);
}

EXPORT(int, sceGxmSetBackVisibilityTestEnable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetBackVisibilityTestIndex) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetBackVisibilityTestOp) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetCullMode, SceGxmContext *context, SceGxmCullMode mode) {
    renderer::set_cull_mode(*host.renderer, context->renderer.get(), &context->state, mode);
}

EXPORT(int, sceGxmSetDefaultRegionClipAndViewport) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetDeferredContextFragmentBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetDeferredContextVdmBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetDeferredContextVertexBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetFragmentDefaultUniformBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetFragmentProgram, SceGxmContext *context, Ptr<const SceGxmFragmentProgram> fragmentProgram) {
    assert(context != nullptr);
    assert(fragmentProgram);

    context->record.fragment_program = fragmentProgram;
    renderer::set_program(*host.renderer, context->renderer.get(), &context->state, fragmentProgram, true);
}

EXPORT(int, sceGxmSetFragmentTexture, SceGxmContext *context, unsigned int textureIndex, const emu::SceGxmTexture *texture) {
    assert(context != nullptr);
    assert(texture != nullptr);

    renderer::set_fragment_texture(*host.renderer, context->renderer.get(), &context->state, textureIndex, *texture);

    return 0;
}

EXPORT(int, sceGxmSetFragmentUniformBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetFrontDepthBias, SceGxmContext *context, int factor, int units) {
    renderer::set_depth_bias(*host.renderer, context->renderer.get(), &context->state, true, factor, units);
}

EXPORT(void, sceGxmSetFrontDepthFunc, SceGxmContext *context, SceGxmDepthFunc depthFunc) {
    renderer::set_depth_func(*host.renderer, context->renderer.get(), &context->state, true, depthFunc);
}

EXPORT(void, sceGxmSetFrontDepthWriteEnable, SceGxmContext *context, SceGxmDepthWriteMode enable) {
    renderer::set_depth_write_enable_mode(*host.renderer, context->renderer.get(), &context->state, true, enable);
}

EXPORT(int, sceGxmSetFrontFragmentProgramEnable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetFrontLineFillLastPixelEnable) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetFrontPointLineWidth, SceGxmContext *context, unsigned int width) {
    renderer::set_point_line_width(*host.renderer, context->renderer.get(), &context->state, true, width);
}

EXPORT(void, sceGxmSetFrontPolygonMode, SceGxmContext *context, SceGxmPolygonMode mode) {
    renderer::set_polygon_mode(*host.renderer, context->renderer.get(), &context->state, true, mode);
}

EXPORT(void, sceGxmSetFrontStencilFunc, SceGxmContext *context, SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail, SceGxmStencilOp depthPass, unsigned char compareMask, unsigned char writeMask) {
    renderer::set_stencil_func(*host.renderer, context->renderer.get(), &context->state, true, func, stencilFail, depthFail, depthPass, compareMask, writeMask);
}

EXPORT(void, sceGxmSetFrontStencilRef, SceGxmContext *context, unsigned char sref) {
    renderer::set_stencil_ref(*host.renderer, context->renderer.get(), &context->state, true, sref);
}

EXPORT(int, sceGxmSetFrontVisibilityTestEnable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetFrontVisibilityTestIndex) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetFrontVisibilityTestOp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetPrecomputedFragmentState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetPrecomputedVertexState) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetRegionClip, SceGxmContext *context, SceGxmRegionClipMode mode, unsigned int xMin, unsigned int yMin, unsigned int xMax, unsigned int yMax) {
    renderer::set_region_clip(*host.renderer, context->renderer.get(), &context->state, mode, xMin, xMax, yMin, yMax);
}

EXPORT(void, sceGxmSetTwoSidedEnable, SceGxmContext *context, SceGxmTwoSidedMode mode) {
    renderer::set_two_sided_enable(*host.renderer, context->renderer.get(), &context->state, mode);
}

EXPORT(int, sceGxmSetUniformDataF, void *uniformBuffer, const SceGxmProgramParameter *parameter, unsigned int componentOffset, unsigned int componentCount, const float *sourceData) {
    //    assert(uniformBuffer != nullptr);
    assert(parameter != nullptr);
    assert(parameter->container_index == SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX);
    assert(componentCount > 0);
    assert(sourceData != nullptr);

    size_t size = componentCount * sizeof(float);
    size_t offset = (parameter->resource_index + componentOffset) * sizeof(float);
    memcpy(static_cast<uint8_t *>(uniformBuffer) + offset, sourceData, size);
    return 0;
}

EXPORT(int, sceGxmSetUserMarker) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetValidationEnable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetVertexDefaultUniformBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetVertexProgram, SceGxmContext *context, Ptr<const SceGxmVertexProgram> vertexProgram) {
    assert(context != nullptr);
    assert(vertexProgram);

    context->record.vertex_program = vertexProgram;
    renderer::set_program(*host.renderer, context->renderer.get(), &context->state, vertexProgram, false);
}

EXPORT(int, sceGxmSetVertexStream, SceGxmContext *context, unsigned int streamIndex, Ptr<const void> streamData) {
    assert(context != nullptr);
    assert(streamData);

    context->state.stream_data[streamIndex] = streamData;

    return 0;
}

EXPORT(int, sceGxmSetVertexTexture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetVertexUniformBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetViewport, SceGxmContext *context, float xOffset, float xScale, float yOffset, float yScale, float zOffset, float zScale) {
    // Set viewport to enable, enable more offset and scale to set
    renderer::set_viewport(*host.renderer, context->renderer.get(), &context->state, xOffset, yOffset, zOffset, xScale, yScale, zScale);
}

EXPORT(void, sceGxmSetViewportEnable, SceGxmContext *context, SceGxmViewportMode enable) {
    // Set viewport to enable/disable, no additional offset and scale to set.
    renderer::set_viewport_enable(*host.renderer, context->renderer.get(), &context->state, enable);
}

EXPORT(int, sceGxmSetVisibilityBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetWBufferEnable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetWClampEnable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetWClampValue) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetWarningEnabled) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetYuvProfile) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherAddRefFragmentProgram, SceGxmShaderPatcher *shaderPatcher, SceGxmFragmentProgram *fragmentProgram) {
    assert(shaderPatcher != nullptr);
    assert(fragmentProgram != nullptr);

    ++fragmentProgram->reference_count;

    return 0;
}

EXPORT(int, sceGxmShaderPatcherAddRefVertexProgram, SceGxmShaderPatcher *shaderPatcher, SceGxmVertexProgram *vertexProgram) {
    assert(shaderPatcher != nullptr);
    assert(vertexProgram != nullptr);

    ++vertexProgram->reference_count;

    return 0;
}

EXPORT(int, sceGxmShaderPatcherCreate, const emu::SceGxmShaderPatcherParams *params, Ptr<SceGxmShaderPatcher> *shaderPatcher) {
    assert(params != nullptr);
    assert(shaderPatcher != nullptr);

    *shaderPatcher = alloc<SceGxmShaderPatcher>(host.mem, __FUNCTION__);
    assert(*shaderPatcher);
    if (!*shaderPatcher) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    return 0;
}

EXPORT(int, sceGxmShaderPatcherCreateFragmentProgram, SceGxmShaderPatcher *shaderPatcher, const SceGxmRegisteredProgram *programId, SceGxmOutputRegisterFormat outputFormat, SceGxmMultisampleMode multisampleMode, const emu::SceGxmBlendInfo *blendInfo, Ptr<const SceGxmProgram> vertexProgram, Ptr<SceGxmFragmentProgram> *fragmentProgram) {
    MemState &mem = host.mem;
    assert(shaderPatcher != nullptr);
    assert(programId != nullptr);
    assert(multisampleMode == SCE_GXM_MULTISAMPLE_NONE);
    assert(fragmentProgram != nullptr);

    static const emu::SceGxmBlendInfo default_blend_info = {
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

    *fragmentProgram = alloc<SceGxmFragmentProgram>(mem, __FUNCTION__);
    assert(*fragmentProgram);
    if (!*fragmentProgram) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    SceGxmFragmentProgram *const fp = fragmentProgram->get(mem);
    fp->program = programId->program;

    if (!renderer::create(fp->renderer_data, *host.renderer, *programId->program.get(mem), blendInfo, host.renderer->gxp_ptr_map, host.base_path.c_str(), host.io.title_id.c_str())) {
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }

    shaderPatcher->fragment_program_cache.emplace(key, *fragmentProgram);

    return 0;
}

EXPORT(int, sceGxmShaderPatcherCreateMaskUpdateFragmentProgram, SceGxmShaderPatcher *shaderPatcher, Ptr<SceGxmFragmentProgram> *fragmentProgram) {
    MemState &mem = host.mem;
    assert(shaderPatcher != nullptr);
    assert(fragmentProgram != nullptr);

    *fragmentProgram = alloc<SceGxmFragmentProgram>(mem, __FUNCTION__);
    assert(*fragmentProgram);
    if (!*fragmentProgram) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    SceGxmFragmentProgram *const fp = fragmentProgram->get(mem);
    fp->program = alloc(mem, size_mask_gxp, __FUNCTION__);
    memcpy(const_cast<SceGxmProgram *>(fp->program.get(mem)), mask_gxp, size_mask_gxp);

    if (!renderer::create(fp->renderer_data, *host.renderer, *fp->program.get(mem), nullptr, host.renderer->gxp_ptr_map, host.base_path.c_str(), host.io.title_id.c_str())) {
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }

    return STUBBED("Using a null shader");
}

EXPORT(int, sceGxmShaderPatcherCreateVertexProgram, SceGxmShaderPatcher *shaderPatcher, const SceGxmRegisteredProgram *programId, const emu::SceGxmVertexAttribute *attributes, unsigned int attributeCount, const SceGxmVertexStream *streams, unsigned int streamCount, Ptr<SceGxmVertexProgram> *vertexProgram) {
    MemState &mem = host.mem;
    assert(shaderPatcher != nullptr);
    assert(programId != nullptr);
    assert(vertexProgram != nullptr);

    *vertexProgram = alloc<SceGxmVertexProgram>(mem, __FUNCTION__);
    assert(*vertexProgram);
    if (!*vertexProgram) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    SceGxmVertexProgram *const vp = vertexProgram->get(mem);
    vp->program = programId->program;

    if (streams && streamCount > 0) {
        vp->streams.insert(vp->streams.end(), &streams[0], &streams[streamCount]);
    }

    if (attributes && attributeCount > 0) {
        vp->attributes.insert(vp->attributes.end(), &attributes[0], &attributes[attributeCount]);
    }

    if (!renderer::create(vp->renderer_data, *host.renderer, *programId->program.get(mem), host.renderer->gxp_ptr_map, host.base_path.c_str(), host.io.title_id.c_str())) {
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }

    return 0;
}

EXPORT(int, sceGxmShaderPatcherDestroy, Ptr<SceGxmShaderPatcher> shaderPatcher) {
    assert(shaderPatcher);

    free(host.mem, shaderPatcher);

    return 0;
}

EXPORT(int, sceGxmShaderPatcherForceUnregisterProgram) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherGetBufferMemAllocated) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherGetFragmentProgramRefCount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherGetFragmentUsseMemAllocated) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherGetHostMemAllocated) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<const SceGxmProgram>, sceGxmShaderPatcherGetProgramFromId, emu::SceGxmShaderPatcherId programId) {
    if (!programId) {
        return Ptr<const SceGxmProgram>();
    }

    return programId.get(host.mem)->program;
}

EXPORT(int, sceGxmShaderPatcherGetUserData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherGetVertexProgramRefCount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherGetVertexUsseMemAllocated) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherRegisterProgram, SceGxmShaderPatcher *shaderPatcher, Ptr<const SceGxmProgram> programHeader, emu::SceGxmShaderPatcherId *programId) {
    assert(shaderPatcher != nullptr);
    assert(programHeader);
    assert(programId != nullptr);

    *programId = alloc<SceGxmRegisteredProgram>(host.mem, __FUNCTION__);
    assert(*programId);
    if (!*programId) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    SceGxmRegisteredProgram *const rp = programId->get(host.mem);
    rp->program = programHeader;

    return 0;
}

EXPORT(int, sceGxmShaderPatcherReleaseFragmentProgram, SceGxmShaderPatcher *shaderPatcher, Ptr<SceGxmFragmentProgram> fragmentProgram) {
    assert(shaderPatcher != nullptr);
    assert(fragmentProgram);

    SceGxmFragmentProgram *const fp = fragmentProgram.get(host.mem);
    --fp->reference_count;
    if (fp->reference_count == 0) {
        for (FragmentProgramCache::const_iterator it = shaderPatcher->fragment_program_cache.begin(); it != shaderPatcher->fragment_program_cache.end(); ++it) {
            if (it->second == fragmentProgram) {
                shaderPatcher->fragment_program_cache.erase(it);
                break;
            }
        }
        free(host.mem, fragmentProgram);
    }

    return 0;
}

EXPORT(int, sceGxmShaderPatcherReleaseVertexProgram, SceGxmShaderPatcher *shaderPatcher, Ptr<SceGxmVertexProgram> vertexProgram) {
    assert(shaderPatcher != nullptr);
    assert(vertexProgram);

    SceGxmVertexProgram *const vp = vertexProgram.get(host.mem);
    --vp->reference_count;
    if (vp->reference_count == 0) {
        free(host.mem, vertexProgram);
    }

    return 0;
}

EXPORT(int, sceGxmShaderPatcherSetAuxiliarySurface) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherSetUserData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherUnregisterProgram, SceGxmShaderPatcher *shaderPatcher, emu::SceGxmShaderPatcherId programId) {
    assert(shaderPatcher != nullptr);
    assert(programId);

    SceGxmRegisteredProgram *const rp = programId.get(host.mem);
    rp->program.reset();

    free(host.mem, programId);

    return 0;
}

EXPORT(int, sceGxmSyncObjectCreate, Ptr<SceGxmSyncObject> *syncObject) {
    assert(syncObject != nullptr);

    *syncObject = alloc<SceGxmSyncObject>(host.mem, __FUNCTION__);
    if (!*syncObject) {
        return SCE_GXM_ERROR_OUT_OF_MEMORY;
    }

    // Set all subjects to be done.
    syncObject->get(host.mem)->done = 0xFFFFFFFF;

    return 0;
}

EXPORT(int, sceGxmSyncObjectDestroy, Ptr<SceGxmSyncObject> syncObject) {
    assert(syncObject);

    free(host.mem, syncObject);

    return 0;
}

EXPORT(int, sceGxmTerminate) {
    const ThreadStatePtr thread = lock_and_find(host.gxm.display_queue_thread, host.kernel.threads, host.kernel.mutex);
    std::unique_lock<std::mutex> thread_lock(thread->mutex);

    thread->to_do = ThreadToDo::exit;
    stop(*thread->cpu);
    thread->something_to_do.notify_all();

    for (auto t : thread->waiting_threads) {
        const std::lock_guard<std::mutex> lock(t->mutex);
        assert(t->to_do == ThreadToDo::wait);
        t->to_do = ThreadToDo::run;
        t->something_to_do.notify_one();
    }

    thread->waiting_threads.clear();

    // need to unlock thread->mutex because thread destructor (delete_thread) will get called, and it locks that mutex
    thread_lock.unlock();

    // TODO: This causes a deadlock
    //const std::lock_guard<std::mutex> lock2(host.kernel.mutex);
    host.kernel.running_threads.erase(host.gxm.display_queue_thread);
    host.kernel.waiting_threads.erase(host.gxm.display_queue_thread);
    host.kernel.threads.erase(host.gxm.display_queue_thread);
    return 0;
}

EXPORT(Ptr<void>, sceGxmTextureGetData, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    return Ptr<void>(texture->data_addr << 2);
}

EXPORT(SceGxmTextureFormat, sceGxmTextureGetFormat, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    return gxm::get_format(texture);
}

EXPORT(int, sceGxmTextureGetGammaMode, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    return (texture->gamma_mode << 27);
}

EXPORT(unsigned int, sceGxmTextureGetHeight, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    return static_cast<unsigned int>(gxm::get_height(texture));
}

EXPORT(unsigned int, sceGxmTextureGetLodBias, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return 0;
    }
    return texture->lod_bias;
}

EXPORT(unsigned int, sceGxmTextureGetLodMin, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return 0;
    }
    return (texture->lod_min0 << 2) | texture->lod_min1;
}

EXPORT(int, sceGxmTextureGetMagFilter, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    return texture->mag_filter;
}

EXPORT(int, sceGxmTextureGetMinFilter, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return texture->mag_filter;
    }
    return texture->min_filter;
}

EXPORT(SceGxmTextureMipFilter, sceGxmTextureGetMipFilter, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
    }
    return texture->mip_filter ? SCE_GXM_TEXTURE_MIP_FILTER_ENABLED : SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
}

EXPORT(unsigned int, sceGxmTextureGetMipmapCount, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return 0;
    }
    return texture->mip_count + 1;
}

EXPORT(unsigned int, sceGxmTextureGetMipmapCountUnsafe, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    return texture->mip_count + 1;
}

EXPORT(int, sceGxmTextureGetNormalizeMode, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    return texture->normalize_mode << 31;
}

EXPORT(Ptr<void>, sceGxmTextureGetPalette, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    assert(gxm::is_paletted_format(gxm::get_format(texture)));
    return Ptr<void>(texture->palette_addr << 6);
}

EXPORT(int, sceGxmTextureGetStride, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTextureGetType, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    return (texture->type << 29);
}

EXPORT(int, sceGxmTextureGetUAddrMode, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    return texture->uaddr_mode;
}

EXPORT(int, sceGxmTextureGetUAddrModeSafe, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return SCE_GXM_TEXTURE_ADDR_CLAMP;
    }
    return texture->uaddr_mode;
}

EXPORT(int, sceGxmTextureGetVAddrMode, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    return texture->vaddr_mode;
}

EXPORT(int, sceGxmTextureGetVAddrModeSafe, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return SCE_GXM_TEXTURE_ADDR_CLAMP;
    }
    return texture->vaddr_mode;
}

EXPORT(unsigned int, sceGxmTextureGetWidth, const emu::SceGxmTexture *texture) {
    assert(texture != nullptr);
    return static_cast<unsigned int>(gxm::get_width(texture));
}

EXPORT(int, sceGxmTextureInitCube) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTextureInitCubeArbitrary) {
    return UNIMPLEMENTED();
}

static int init_texture_base(const char *export_name, emu::SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, unsigned int width, unsigned int height, unsigned int mipCount,
    const SceGxmTextureType &texture_type) {
    if (width > 4096 || height > 4096 || mipCount > 13) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    } else if (!data) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_ALIGNMENT);
    } else if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    // Add supported formats here

    switch (texFormat) {
    case SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR:
    case SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB:
    case SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_ABGR:
    case SCE_GXM_TEXTURE_FORMAT_U1U5U5U5_ABGR:
    case SCE_GXM_TEXTURE_FORMAT_U5U6U5_BGR:
    case SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB:
    case SCE_GXM_TEXTURE_FORMAT_U8U8U8_BGR:
    case SCE_GXM_TEXTURE_FORMAT_U8_R111:
    case SCE_GXM_TEXTURE_FORMAT_U8_111R:
    case SCE_GXM_TEXTURE_FORMAT_U8_1RRR:
        break;

    default:
        if (gxm::is_paletted_format(texFormat)) {
            switch (texFormat) {
            case SCE_GXM_TEXTURE_FORMAT_P8_ABGR:
            case SCE_GXM_TEXTURE_FORMAT_P8_1BGR:
            case SCE_GXM_TEXTURE_FORMAT_P4_ABGR:
                break;
            default:
                LOG_WARN("Initialized texture with untested paletted texture format: {}", log_hex(texFormat));
            }
        } else
            LOG_ERROR("Initialized texture with unsupported texture format: {}", log_hex(texFormat));
    }

    texture->mip_count = std::min<std::uint32_t>(0, mipCount - 1);
    texture->format0 = (texFormat & 0x80000000) >> 31;
    texture->uaddr_mode = texture->vaddr_mode = SCE_GXM_TEXTURE_ADDR_CLAMP;
    texture->lod_bias = 31;

    if (texture_type == SCE_GXM_TEXTURE_SWIZZLED) {
        // Find highest set bit of width and height. It's also the 2^? for width and height
        static auto highest_set_bit = [](const int num) -> std::uint32_t {
            for (std::uint32_t i = 12; i >= 0; i--) {
                if (num & (1 << i)) {
                    return i;
                }
            }

            return 0;
        };

        texture->height = highest_set_bit(height);
        texture->width = highest_set_bit(width);
    } else {
        texture->height = height - 1;
        texture->width = width - 1;
    }

    texture->base_format = (texFormat & 0x1F000000) >> 24;
    texture->type = texture_type >> 29;
    texture->data_addr = data.address() >> 2;
    texture->swizzle_format = (texFormat & 0x7000) >> 12;
    texture->normalize_mode = 1;
    texture->min_filter = SCE_GXM_TEXTURE_FILTER_POINT;
    texture->mag_filter = SCE_GXM_TEXTURE_FILTER_POINT;

    return 0;
}

EXPORT(int, sceGxmTextureInitLinear, emu::SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, unsigned int width, unsigned int height, unsigned int mipCount) {
    const int result = init_texture_base(export_name, texture, data, texFormat, width, height, mipCount, SCE_GXM_TEXTURE_LINEAR);

    if (result == 0)
        return 0;

    return result;
}

EXPORT(int, sceGxmTextureInitLinearStrided) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTextureInitSwizzled, emu::SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, unsigned int width, unsigned int height, unsigned int mipCount) {
    const int result = init_texture_base(export_name, texture, data, texFormat, width, height, mipCount, SCE_GXM_TEXTURE_SWIZZLED);

    if (result == 0)
        return 0;

    return result;
}

EXPORT(int, sceGxmTextureInitSwizzledArbitrary) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTextureInitTiled, emu::SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, unsigned int width, unsigned int height, unsigned int mipCount) {
    const int result = init_texture_base(export_name, texture, data, texFormat, width, height, mipCount, SCE_GXM_TEXTURE_TILED);

    if (result == 0)
        return 0;

    return result;
}

EXPORT(int, sceGxmTextureSetData, emu::SceGxmTexture *texture, Ptr<const void> data) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    texture->data_addr = data.address() >> 2;
    return 0;
}

EXPORT(int, sceGxmTextureSetFormat, emu::SceGxmTexture *texture, SceGxmTextureFormat texFormat) {
    STUBBED("FAST");
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    texture->base_format = (texFormat & 0x1F000000) >> 24;
    texture->swizzle_format = (texFormat & 0x7000) >> 12;
    return SCE_KERNEL_OK;
}

EXPORT(int, sceGxmTextureSetGammaMode, emu::SceGxmTexture *texture, SceGxmTextureGammaMode gammaMode) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    texture->gamma_mode = (uint32_t)gammaMode;
    return 0;
}

EXPORT(int, sceGxmTextureSetHeight, emu::SceGxmTexture *texture, unsigned int height) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    } else if (height > 4096) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

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

EXPORT(int, sceGxmTextureSetLodBias, emu::SceGxmTexture *texture, unsigned int bias) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTextureSetLodMin, emu::SceGxmTexture *texture, unsigned int lodMin) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTextureSetMagFilter, emu::SceGxmTexture *texture, SceGxmTextureFilter magFilter) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    texture->mag_filter = (uint32_t)magFilter;
    return 0;
}

EXPORT(int, sceGxmTextureSetMinFilter, emu::SceGxmTexture *texture, SceGxmTextureFilter minFilter) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }

    texture->min_filter = (uint32_t)minFilter;
    return 0;
}

EXPORT(int, sceGxmTextureSetMipFilter, emu::SceGxmTexture *texture, SceGxmTextureMipFilter mipFilter) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }

    texture->mip_filter = (uint32_t)mipFilter;
    return 0;
}

EXPORT(int, sceGxmTextureSetMipmapCount, emu::SceGxmTexture *texture, uint32_t mipCount) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }

    texture->mip_count = (uint32_t)mipCount;
    return 0;
}

/**
 * TODO: uncomment code once https://github.com/vitasdk/vita-headers/commit/2995273a998f355a498cc4e9c563974fe5938ac6 is available
 */
EXPORT(int, sceGxmTextureSetNormalizeMode, emu::SceGxmTexture *texture /*, SceGxmTextureNormalizeMode normalizeMode*/) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    //texture->normalize_mode = (uint32_t)normalizeMode;
    return 0;
}

EXPORT(int, sceGxmTextureSetPalette, emu::SceGxmTexture *texture, Ptr<void> paletteData) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    } else if ((uint8_t)paletteData.address() & 0x3F) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_ALIGNMENT);
    }

    texture->palette_addr = ((unsigned int)paletteData.address() >> 6);
    return 0;
}

EXPORT(int, sceGxmTextureSetStride, emu::SceGxmTexture *texture, uint32_t byteStride) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((texture->type << 29) != SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTextureSetUAddrMode, emu::SceGxmTexture *texture, SceGxmTextureAddrMode mode) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    if ((texture->type << 29) == SCE_GXM_TEXTURE_CUBE || (texture->type << 29) == SCE_GXM_TEXTURE_CUBE_ARBITRARY) {
        if (mode != SCE_GXM_TEXTURE_ADDR_CLAMP) {
            return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
        }
    } else {
        if (mode <= SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER && mode >= SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER) {
            if ((texture->type << 29) != SCE_GXM_TEXTURE_SWIZZLED) {
                return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
            }
        }
        if (mode == SCE_GXM_TEXTURE_ADDR_MIRROR && ((texture->type << 29) != SCE_GXM_TEXTURE_SWIZZLED)) {
            return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
        }
    }
    texture->uaddr_mode = mode;
    return 0;
}

EXPORT(int, sceGxmTextureSetUAddrModeSafe, emu::SceGxmTexture *texture, SceGxmTextureAddrMode mode) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    if ((texture->type << 29) != SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        if (mode <= SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER) {
            if (((texture->type << 29) != SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) && ((texture->type << 29) != SCE_GXM_TEXTURE_SWIZZLED)) {
                return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
            }
        } else if ((mode == SCE_GXM_TEXTURE_ADDR_MIRROR) || ((texture->type << 29) == SCE_GXM_TEXTURE_SWIZZLED)) {
            return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
        } else {
            return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
        }
        texture->uaddr_mode = mode;
        return 0;
    }
    if (mode != SCE_GXM_TEXTURE_ADDR_CLAMP) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }
    return 0;
}

EXPORT(int, sceGxmTextureSetVAddrMode, emu::SceGxmTexture *texture, SceGxmTextureAddrMode mode) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    if ((texture->type << 29) == SCE_GXM_TEXTURE_CUBE || (texture->type << 29) == SCE_GXM_TEXTURE_CUBE_ARBITRARY) {
        if (mode != SCE_GXM_TEXTURE_ADDR_CLAMP) {
            return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
        }
    } else {
        if (mode <= SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER && mode >= SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER) {
            if ((texture->type << 29) != SCE_GXM_TEXTURE_SWIZZLED) {
                return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
            }
        }
        if (mode == SCE_GXM_TEXTURE_ADDR_MIRROR && ((texture->type << 29) != SCE_GXM_TEXTURE_SWIZZLED)) {
            return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
        }
    }
    texture->vaddr_mode = mode;
    return 0;
}

EXPORT(int, sceGxmTextureSetVAddrModeSafe, emu::SceGxmTexture *texture, SceGxmTextureAddrMode mode) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    if ((texture->type << 29) != SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        if (mode <= SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER) {
            if (((texture->type << 29) != SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) && ((texture->type << 29) != SCE_GXM_TEXTURE_SWIZZLED)) {
                return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
            }
        } else if ((mode == SCE_GXM_TEXTURE_ADDR_MIRROR) || ((texture->type << 29) == SCE_GXM_TEXTURE_SWIZZLED)) {
            return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
        } else {
            return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
        }
        texture->vaddr_mode = mode;
        return 0;
    }
    if (mode != SCE_GXM_TEXTURE_ADDR_CLAMP) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }
    return 0;
}

EXPORT(int, sceGxmTextureSetWidth, emu::SceGxmTexture *texture, unsigned int width) {
    if (texture == nullptr) {
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
    }

    // TODO: Add support for swizzled textures
    LOG_WARN("Unimplemented texture format detected in sceGxmTextureSetWidth call.");

    return 0;
}

EXPORT(int, sceGxmTextureValidate, const emu::SceGxmTexture *texture) {
    if (texture == nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTransferCopy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTransferDownscale) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTransferFill) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTransferFinish) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmUnmapFragmentUsseMemory, void *base) {
    assert(base != nullptr);

    return 0;
}

EXPORT(int, sceGxmUnmapMemory, void *base) {
    assert(base != nullptr);

    return 0;
}

EXPORT(int, sceGxmUnmapVertexUsseMemory, void *base) {
    assert(base != nullptr);

    return 0;
}

EXPORT(int, sceGxmVertexFence) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<const SceGxmProgram>, sceGxmVertexProgramGetProgram, Ptr<const SceGxmVertexProgram> vertex_program) {
    return vertex_program.get(host.mem)->program;
}

EXPORT(int, sceGxmWaitEvent) {
    return UNIMPLEMENTED();
}

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
BRIDGE_IMPL(sceGxmGetRenderTargetMemSizes)
BRIDGE_IMPL(sceGxmInitialize)
BRIDGE_IMPL(sceGxmIsDebugVersion)
BRIDGE_IMPL(sceGxmMapFragmentUsseMemory)
BRIDGE_IMPL(sceGxmMapMemory)
BRIDGE_IMPL(sceGxmMapVertexUsseMemory)
BRIDGE_IMPL(sceGxmMidSceneFlush)
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
