// Vita3K emulator project
// Copyright (C) 2020 Vita3K team
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
#include <gxm/types.h>
#include <immintrin.h>
#include <kernel/thread/thread_functions.h>
#include <mem/mempool.h>
#include <renderer/functions.h>
#include <renderer/types.h>
#include <util/bytes.h>
#include <util/lock_and_find.h>
#include <util/log.h>

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

    // Add supported formats here

    switch (tex_format) {
    case SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR:
    case SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_ARGB:
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
    case SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR:
    case SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR:
    case SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR:
    case SCE_GXM_TEXTURE_FORMAT_PVRT2BPP_ABGR:
    case SCE_GXM_TEXTURE_FORMAT_PVRT2BPP_1BGR:
    case SCE_GXM_TEXTURE_FORMAT_PVRT4BPP_ABGR:
    case SCE_GXM_TEXTURE_FORMAT_PVRT4BPP_1BGR:
    case SCE_GXM_TEXTURE_FORMAT_PVRTII2BPP_ABGR:
    case SCE_GXM_TEXTURE_FORMAT_PVRTII2BPP_1BGR:
    case SCE_GXM_TEXTURE_FORMAT_PVRTII4BPP_ABGR:
    case SCE_GXM_TEXTURE_FORMAT_PVRTII4BPP_1BGR:
        break;

    default:
        if (gxm::is_paletted_format(tex_format)) {
            switch (tex_format) {
            case SCE_GXM_TEXTURE_FORMAT_P8_ABGR:
            case SCE_GXM_TEXTURE_FORMAT_P8_1BGR:
            case SCE_GXM_TEXTURE_FORMAT_P4_ABGR:
                break;
            default:
                LOG_WARN("Initialized texture with untested paletted texture format: {}", log_hex(tex_format));
            }
        } else if (gxm::is_yuv_format(tex_format)) {
            switch (tex_format) {
            case SCE_GXM_TEXTURE_FORMAT_YUV420P2_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YUV420P2_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YUV420P3_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P3_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YUV420P3_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P3_CSC1:
                break;
            default:
                LOG_WARN("Initialized texture with untested YUV texture format: {}", log_hex(tex_format));
            }
        } else
            LOG_ERROR("Initialized texture with unsupported texture format: {}", log_hex(tex_format));
    }

    texture->mip_count = std::min<std::uint32_t>(0, mipCount - 1);
    texture->format0 = (tex_format & 0x80000000) >> 31;
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

        texture->uaddr_mode = texture->vaddr_mode = SCE_GXM_TEXTURE_ADDR_MIRROR;
        texture->height = highest_set_bit(height);
        texture->width = highest_set_bit(width);
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

    return 0;
}

EXPORT(int, _sceGxmBeginScene) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGxmMidSceneFlush) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGxmSetVertexTexture) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGxmTextureSetHeight) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGxmTextureSetWidth) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmAddRazorGpuCaptureBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmBeginCommandList, SceGxmContext *deferredContext) {
    if (!deferredContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmBeginScene, SceGxmContext *context, uint32_t flags, const SceGxmRenderTarget *renderTarget, const SceGxmValidRegion *validRegion, SceGxmSyncObject *vertexSyncObject, Ptr<SceGxmSyncObject> fragmentSyncObject, const SceGxmColorSurface *colorSurface, const SceGxmDepthStencilSurface *depthStencil) {
    if (!context) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (flags & 0xFFFFFFF0) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!renderTarget || (vertexSyncObject != nullptr)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (host.gxm.is_in_scene) {
        return RET_ERROR(SCE_GXM_ERROR_WITHIN_SCENE);
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

EXPORT(int, sceGxmBeginSceneEx, SceGxmContext *immediateContext, uint32_t flags, const SceGxmRenderTarget *renderTarget, const SceGxmValidRegion *validRegion, SceGxmSyncObject *vertexSyncObject, SceGxmSyncObject *fragmentSyncObject, const SceGxmColorSurface *colorSurface, const SceGxmDepthStencilSurface *loadDepthStencilSurface, const SceGxmDepthStencilSurface *storeDepthStencilSurface) {
    if (!immediateContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (flags & 0xFFFFFFF0) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!renderTarget || (vertexSyncObject != nullptr)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (host.gxm.is_in_scene) {
        return RET_ERROR(SCE_GXM_ERROR_WITHIN_SCENE);
    }

    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmColorSurfaceGetClip, const SceGxmColorSurface *surface, uint32_t *xMin, uint32_t *yMin, uint32_t *xMax, uint32_t *yMax) {
    assert(surface);
    UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceGxmColorSurfaceGetData, const SceGxmColorSurface *surface) {
    assert(surface);
    return surface->data;
}

EXPORT(SceGxmColorSurfaceDitherMode, sceGxmColorSurfaceGetDitherMode, const SceGxmColorSurface *surface) {
    assert(surface);
    STUBBED("SCE_GXM_COLOR_SURFACE_DITHER_DISABLED");
    return SceGxmColorSurfaceDitherMode::SCE_GXM_COLOR_SURFACE_DITHER_DISABLED;
}

EXPORT(SceGxmColorFormat, sceGxmColorSurfaceGetFormat, const SceGxmColorSurface *surface) {
    assert(surface);
    return surface->colorFormat;
}

EXPORT(SceGxmColorSurfaceGammaMode, sceGxmColorSurfaceGetGammaMode, const SceGxmColorSurface *surface) {
    assert(surface);
    STUBBED("SCE_GXM_COLOR_SURFACE_GAMMA_NONE");
    return SceGxmColorSurfaceGammaMode::SCE_GXM_COLOR_SURFACE_GAMMA_NONE;
}

EXPORT(SceGxmColorSurfaceScaleMode, sceGxmColorSurfaceGetScaleMode, const SceGxmColorSurface *surface) {
    assert(surface);
    return surface->downscale ? SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE : SCE_GXM_COLOR_SURFACE_SCALE_NONE;
}

EXPORT(uint32_t, sceGxmColorSurfaceGetStrideInPixels, const SceGxmColorSurface *surface) {
    assert(surface);
    return surface->strideInPixels;
}

EXPORT(SceGxmColorSurfaceType, sceGxmColorSurfaceGetType, const SceGxmColorSurface *surface) {
    assert(surface);
    return surface->surfaceType;
}

EXPORT(int, sceGxmColorSurfaceInit, SceGxmColorSurface *surface, SceGxmColorFormat colorFormat, SceGxmColorSurfaceType surfaceType, SceGxmColorSurfaceScaleMode scaleMode, SceGxmOutputRegisterSize outputRegisterSize, uint32_t width, uint32_t height, uint32_t strideInPixels, Ptr<void> data) {
    if (!surface || !data)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (width > 4096 || height > 4096)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    if (strideInPixels & 1)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_ALIGNMENT);

    if ((strideInPixels < width) || ((data.address() & 3) != 0))
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    memset(surface, 0, sizeof(*surface));
    surface->disabled = 0;
    surface->downscale = scaleMode == SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE;
    surface->width = width;
    surface->height = height;
    surface->strideInPixels = strideInPixels;
    surface->data = data;
    surface->colorFormat = colorFormat;
    surface->surfaceType = surfaceType;
    surface->outputRegisterSize = outputRegisterSize;

    // Create background object, for here don't return an error
    if (init_texture_base(export_name, &surface->backgroundTex, Ptr<void>(0), SCE_GXM_TEXTURE_FORMAT_A8R8G8B8, surface->width, surface->height, 0, SCE_GXM_TEXTURE_LINEAR) != SCE_KERNEL_OK) {
        LOG_WARN("Unable to initialize background object control texture!");
    }

    return 0;
}

EXPORT(int, sceGxmColorSurfaceInitDisabled, SceGxmColorSurface *surface) {
    if (!surface)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    surface->disabled = 1;
    return 0;
}

EXPORT(bool, sceGxmColorSurfaceIsEnabled, const SceGxmColorSurface *surface) {
    assert(surface);
    return !surface->disabled;
}

EXPORT(void, sceGxmColorSurfaceSetClip, SceGxmColorSurface *surface, uint32_t xMin, uint32_t yMin, uint32_t xMax, uint32_t yMax) {
    assert(surface);
    UNIMPLEMENTED();
}

EXPORT(int, sceGxmColorSurfaceSetData, SceGxmColorSurface *surface, Ptr<void> data) {
    if (!surface) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (data.address() & 3) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmColorSurfaceSetDitherMode, SceGxmColorSurface *surface, SceGxmColorSurfaceDitherMode ditherMode) {
    if (!surface) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmColorSurfaceSetFormat, SceGxmColorSurface *surface, SceGxmColorFormat format) {
    if (!surface) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmColorSurfaceSetGammaMode, SceGxmColorSurface *surface, SceGxmColorSurfaceGammaMode gammaMode) {
    if (!surface) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmColorSurfaceSetScaleMode, SceGxmColorSurface *surface, SceGxmColorSurfaceScaleMode scaleMode) {
    assert(surface);
    UNIMPLEMENTED();
}

EXPORT(int, sceGxmCreateContext, const SceGxmContextParams *params, Ptr<SceGxmContext> *context) {
    if (!params || !context)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

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

EXPORT(int, sceGxmCreateDeferredContext, SceGxmDeferredContextParams *params, Ptr<SceGxmContext> *deferredContext) {
    if (!params) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmCreateRenderTarget, const SceGxmRenderTargetParams *params, Ptr<SceGxmRenderTarget> *renderTarget) {
    if (!params) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (params->flags & 0xFFFF00EC) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!renderTarget) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

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

EXPORT(float, sceGxmDepthStencilSurfaceGetBackgroundDepth, const SceGxmDepthStencilSurface *surface) {
    assert(surface);
    return surface->backgroundDepth;
}

EXPORT(bool, sceGxmDepthStencilSurfaceGetBackgroundMask, const SceGxmDepthStencilSurface *surface) {
    assert(surface);
    return surface->control.get(host.mem)->backgroundMask;
}

EXPORT(uint8_t, sceGxmDepthStencilSurfaceGetBackgroundStencil, const SceGxmDepthStencilSurface *surface) {
    assert(surface);
    return surface->control.get(host.mem)->backgroundStencil;
}

EXPORT(SceGxmDepthStencilForceLoadMode, sceGxmDepthStencilSurfaceGetForceLoadMode, const SceGxmDepthStencilSurface *surface) {
    assert(surface);
    // TODO: Implement on the renderer side
    // return static_cast<SceGxmDepthStencilForceLoadMode>(surface->zlsControl & SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED);
    STUBBED("SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_DISABLED");
    return SceGxmDepthStencilForceLoadMode::SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_DISABLED;
}

EXPORT(SceGxmDepthStencilForceStoreMode, sceGxmDepthStencilSurfaceGetForceStoreMode, const SceGxmDepthStencilSurface *surface) {
    assert(surface);
    // TODO: Implement on the renderer side
    // return static_cast<SceGxmDepthStencilForceStoreMode>(surface->zlsControl & SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED);
    STUBBED("SCE_GXM_DEPTH_STENCIL_FORCE_STORE_DISABLED");
    return SceGxmDepthStencilForceStoreMode::SCE_GXM_DEPTH_STENCIL_FORCE_STORE_DISABLED;
}

EXPORT(int, sceGxmDepthStencilSurfaceGetFormat, const SceGxmDepthStencilSurface *surface) {
    assert(surface);
    return UNIMPLEMENTED();
}

EXPORT(uint32_t, sceGxmDepthStencilSurfaceGetStrideInSamples, const SceGxmDepthStencilSurface *surface) {
    assert(surface);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDepthStencilSurfaceInit, SceGxmDepthStencilSurface *surface, SceGxmDepthStencilFormat depthStencilFormat, SceGxmDepthStencilSurfaceType surfaceType, uint32_t strideInSamples, Ptr<void> depthData, Ptr<void> stencilData) {
    if (!surface) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((strideInSamples == 0) || ((strideInSamples % SCE_GXM_TILE_SIZEX) != 0)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    SceGxmDepthStencilSurface tmp_surface;
    tmp_surface.depthData = depthData;
    tmp_surface.stencilData = stencilData;

    tmp_surface.control = alloc<SceGxmDepthStencilControl>(host.mem, "gxm depth stencil control");
    SceGxmDepthStencilControl control;
    control.disabled = false;
    control.format = depthStencilFormat;
    memcpy(tmp_surface.control.get(host.mem), &control, sizeof(SceGxmDepthStencilControl));
    memcpy(surface, &tmp_surface, sizeof(SceGxmDepthStencilSurface));
    return 0;
}

EXPORT(int, sceGxmDepthStencilSurfaceInitDisabled, SceGxmDepthStencilSurface *surface) {
    if (!surface) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    SceGxmDepthStencilSurface tmp_surface;

    tmp_surface.control = alloc<SceGxmDepthStencilControl>(host.mem, "gxm depth stencil control");
    SceGxmDepthStencilControl control;
    control.disabled = true;
    memcpy(tmp_surface.control.get(host.mem), &control, sizeof(SceGxmDepthStencilControl));
    memcpy(surface, &tmp_surface, sizeof(SceGxmDepthStencilSurface));
    return 0;
}

EXPORT(bool, sceGxmDepthStencilSurfaceIsEnabled, const SceGxmDepthStencilSurface *surface) {
    assert(surface);
    return !surface->control.get(host.mem)->disabled;
}

EXPORT(void, sceGxmDepthStencilSurfaceSetBackgroundDepth, SceGxmDepthStencilSurface *surface, float depth) {
    assert(surface);
    surface->backgroundDepth = depth;
}

EXPORT(void, sceGxmDepthStencilSurfaceSetBackgroundMask, SceGxmDepthStencilSurface *surface, bool mask) {
    assert(surface);
    surface->control.get(host.mem)->backgroundMask = mask;
}

EXPORT(void, sceGxmDepthStencilSurfaceSetBackgroundStencil, SceGxmDepthStencilSurface *surface, uint8_t stencil) {
    assert(surface);
    surface->control.get(host.mem)->backgroundStencil = stencil;
}

EXPORT(void, sceGxmDepthStencilSurfaceSetForceLoadMode, SceGxmDepthStencilSurface *surface, SceGxmDepthStencilForceLoadMode forceLoad) {
    assert(surface);
    // TODO: Implement on the renderer side
    // surface->zlsControl = (forceLoad & SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED) | (surface->zlsControl & ~SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED);
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmDepthStencilSurfaceSetForceStoreMode, SceGxmDepthStencilSurface *surface, SceGxmDepthStencilForceStoreMode forceStore) {
    assert(surface);
    // TODO: Implement on the renderer side
    // surface->zlsControl = (forceStore & SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED) | (surface->zlsControl & ~SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED);
    UNIMPLEMENTED();
}

EXPORT(int, sceGxmDestroyContext, Ptr<SceGxmContext> context) {
    if (!context)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    free(host.mem, context);

    return 0;
}

EXPORT(int, sceGxmDestroyDeferredContext, SceGxmContext *deferredContext) {
    if (!deferredContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmDestroyRenderTarget, Ptr<SceGxmRenderTarget> renderTarget) {
    MemState &mem = host.mem;

    if (!renderTarget)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    renderer::destroy_render_target(*host.renderer, renderTarget.get(mem)->renderer);

    free(mem, renderTarget);

    return 0;
}

EXPORT(int, sceGxmDisplayQueueAddEntry, Ptr<SceGxmSyncObject> oldBuffer, Ptr<SceGxmSyncObject> newBuffer, Ptr<const void> callbackData) {
    if (!oldBuffer || !newBuffer)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    DisplayCallback display_callback;

    const Address address = alloc(host.mem, host.gxm.params.displayQueueCallbackDataSize, __FUNCTION__);
    const Ptr<void> ptr(address);
    memcpy(ptr.get(host.mem), callbackData.get(host.mem), host.gxm.params.displayQueueCallbackDataSize);

    // Block future rendering by setting value2 of sync object
    SceGxmSyncObject *oldBufferSync = oldBuffer.get(host.mem);
    SceGxmSyncObject *newBufferSync = newBuffer.get(host.mem);

    renderer::subject_in_progress(oldBufferSync, renderer::SyncObjectSubject::DisplayQueue);
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

static int gxmDrawElementGeneral(HostState &host, const char *export_name, SceGxmContext *context, SceGxmPrimitiveType primType, SceGxmIndexFormat indexType, const void *indexData, uint32_t indexCount, uint32_t instanceCount) {
    if (!context || !indexData)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

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

    renderer::set_uniform_buffers(*host.renderer, context->renderer.get(), vertex_program_gxp, context->state.vertex_uniform_buffers, gxm_vertex_program.renderer_data->uniform_buffer_sizes, host.mem);
    renderer::set_uniform_buffers(*host.renderer, context->renderer.get(), fragment_program_gxp, context->state.fragment_uniform_buffers, gxm_fragment_program.renderer_data->uniform_buffer_sizes, host.mem);

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
            const std::uint8_t *const data = context->state.stream_data[stream_index].cast<const std::uint8_t>().get(host.mem);

            renderer::set_vertex_stream(*host.renderer, context->renderer.get(), &context->state, stream_index,
                data_length, data);
        }
    }

    // Fragment texture is copied so no need to set it here.
    // Add draw command
    renderer::draw(*host.renderer, context->renderer.get(), &context->state, primType, indexType, indexData, indexCount, instanceCount);

    return 0;
}

EXPORT(int, sceGxmDraw, SceGxmContext *context, SceGxmPrimitiveType primType, SceGxmIndexFormat indexType, const void *indexData, uint32_t indexCount) {
    return gxmDrawElementGeneral(host, export_name, context, primType, indexType, indexData, indexCount, 1);
}

EXPORT(int, sceGxmDrawInstanced, SceGxmContext *context, SceGxmPrimitiveType primType, SceGxmIndexFormat indexType, const void *indexData, uint32_t indexCount, uint32_t indexWrap) {
    if (indexCount % indexWrap != 0) {
        LOG_WARN("Extra vertexes are requested to be drawn (ignored)");
    }

    return gxmDrawElementGeneral(host, export_name, context, primType, indexType, indexData, indexWrap, indexCount / indexWrap);
}

EXPORT(int, sceGxmDrawPrecomputed, SceGxmContext *context, SceGxmPrecomputedDraw *draw) {
    if (!context) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (!host.gxm.is_in_scene) {
        return RET_ERROR(SCE_GXM_ERROR_NOT_WITHIN_SCENE);
    }

    if (!draw) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    SceGxmPrecomputedVertexState *vertex_state = context->state.precomputed_vertex_state.cast<SceGxmPrecomputedVertexState>().get(host.mem);
    SceGxmPrecomputedFragmentState *fragment_state = context->state.precomputed_fragment_state.cast<SceGxmPrecomputedFragmentState>().get(host.mem);

    assert(vertex_state);
    assert(fragment_state);

    // not sure if precomputed uses current program... maybe it does?
    // anyway states have to be made on a program to program basis so this should be safe
    const SceGxmFragmentProgram *fragment_program = fragment_state->program.get(host.mem);
    const SceGxmVertexProgram *vertex_program = vertex_state->program.get(host.mem);

    if (!vertex_program || !fragment_program) {
        return RET_ERROR(SCE_GXM_ERROR_NULL_PROGRAM);
    }

    const SceGxmFragmentProgram &gxm_fragment_program = *context->record.fragment_program.get(host.mem);
    const SceGxmVertexProgram &gxm_vertex_program = *context->record.vertex_program.get(host.mem);

    // Set uniforms
    const SceGxmProgram &vertex_program_gxp = *vertex_program->program.get(host.mem);
    const SceGxmProgram &fragment_program_gxp = *fragment_program->program.get(host.mem);

    renderer::set_uniform_buffers(*host.renderer, context->renderer.get(), vertex_program_gxp, *vertex_state->uniform_buffers.get(host.mem), gxm_vertex_program.renderer_data->uniform_buffer_sizes, host.mem);
    renderer::set_uniform_buffers(*host.renderer, context->renderer.get(), fragment_program_gxp, *fragment_state->uniform_buffers.get(host.mem), gxm_fragment_program.renderer_data->uniform_buffer_sizes, host.mem);

    // Update vertex data. We should stores a copy of the data to pass it to GPU later, since another scene
    // may start to overwrite stuff when this scene is being processed in our queue (in case of OpenGL).
    size_t max_index = 0;
    if (draw->index_format == SCE_GXM_INDEX_FORMAT_U16) {
        const uint16_t *const data = draw->index_data.cast<const uint16_t>().get(host.mem);
        max_index = *std::max_element(&data[0], &data[draw->vertex_count]);
    } else {
        const uint32_t *const data = draw->index_data.cast<const uint32_t>().get(host.mem);
        max_index = *std::max_element(&data[0], &data[draw->vertex_count]);
    }

    const auto frag_paramters = gxp::program_parameters(fragment_program_gxp);
    auto &textures = *fragment_state->textures.get(host.mem);
    for (uint32_t i = 0; i < fragment_program_gxp.parameter_count; ++i) {
        const auto parameter = frag_paramters[i];
        if (parameter.category == SCE_GXM_PARAMETER_CATEGORY_SAMPLER) {
            const auto index = parameter.resource_index;
            renderer::set_fragment_texture(*host.renderer, context->renderer.get(), &context->state, index, textures[index]);
        }
    }

    size_t max_data_length[SCE_GXM_MAX_VERTEX_STREAMS] = {};
    std::uint32_t stream_used = 0;
    for (const SceGxmVertexAttribute &attribute : vertex_program->attributes) {
        const SceGxmAttributeFormat attribute_format = static_cast<SceGxmAttributeFormat>(attribute.format);
        const size_t attribute_size = gxm::attribute_format_size(attribute_format) * attribute.componentCount;
        const SceGxmVertexStream &stream = vertex_program->streams[attribute.streamIndex];
        const size_t data_length = attribute.offset + (max_index * stream.stride) + attribute_size;
        max_data_length[attribute.streamIndex] = std::max<size_t>(max_data_length[attribute.streamIndex], data_length);
        stream_used |= (1 << attribute.streamIndex);
    }

    auto &stream_data = *draw->stream_data.get(host.mem);
    // Copy and queue upload
    for (size_t stream_index = 0; stream_index < SCE_GXM_MAX_VERTEX_STREAMS; ++stream_index) {
        // Upload it
        if (stream_used & (1 << static_cast<std::uint16_t>(stream_index))) {
            const size_t data_length = max_data_length[stream_index];
            const std::uint8_t *const data = stream_data[stream_index].cast<const std::uint8_t>().get(host.mem);

            renderer::set_vertex_stream(*host.renderer, context->renderer.get(), &context->state, stream_index,
                data_length, data);
        }
    }

    // Fragment texture is copied so no need to set it here.
    // Add draw command
    renderer::draw(*host.renderer, context->renderer.get(), &context->state, draw->type, draw->index_format, draw->index_data.get(host.mem), draw->vertex_count, 1);

    return 0;
}

EXPORT(int, sceGxmEndCommandList) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmEndScene, SceGxmContext *context, const SceGxmNotification *vertexNotification, const SceGxmNotification *fragmentNotification) {
    const MemState &mem = host.mem;

    if (!context) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

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

    if (vertexNotification) {
        volatile uint32_t *vertex_address = vertexNotification->address.get(host.mem);
        *vertex_address = vertexNotification->value;
    }

    if (fragmentNotification) {
        volatile uint32_t *fragment_address = fragmentNotification->address.get(host.mem);
        *fragment_address = fragmentNotification->value;
    }

    // Submit our command list
    renderer::submit_command_list(*host.renderer, context->renderer.get(), &context->state,
        context->renderer->command_list);

    renderer::reset_command_list(context->renderer->command_list);

    host.gxm.is_in_scene = false;
    host.renderer->scene_processed_since_last_frame++;

    return 0;
}

EXPORT(int, sceGxmExecuteCommandList) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmFinish, SceGxmContext *context) {
    assert(context);

    if (!context)
        return;
    // Wait on this context's rendering finish code. There is one for sync object and one specifically
    // for SceGxmFinish.
    renderer::finish(*host.renderer, *context->renderer);
}

EXPORT(SceGxmPassType, sceGxmFragmentProgramGetPassType, const SceGxmFragmentProgram *fragmentProgram) {
    assert(fragmentProgram);
    STUBBED("SCE_GXM_PASS_TYPE_OPAQUE");
    return SceGxmPassType::SCE_GXM_PASS_TYPE_OPAQUE;
}

EXPORT(Ptr<const SceGxmProgram>, sceGxmFragmentProgramGetProgram, const SceGxmFragmentProgram *fragmentProgram) {
    assert(fragmentProgram);
    return fragmentProgram->program;
}

EXPORT(bool, sceGxmFragmentProgramIsEnabled, const SceGxmFragmentProgram *fragmentProgram) {
    assert(fragmentProgram);
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmGetContextType, const SceGxmContext *context, SceGxmContextType *type) {
    if (!context || !type) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    STUBBED("SCE_GXM_CONTEXT_TYPE_IMMEDIATE");
    *type = SCE_GXM_CONTEXT_TYPE_IMMEDIATE;
    return 0;
}

EXPORT(int, sceGxmGetDeferredContextFragmentBuffer, const SceGxmContext *deferredContext, Ptr<void> *mem) {
    if (!deferredContext || !mem) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmGetDeferredContextVdmBuffer, const SceGxmContext *deferredContext, Ptr<void> *mem) {
    if (!deferredContext || !mem) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmGetDeferredContextVertexBuffer, const SceGxmContext *deferredContext, Ptr<void> *mem) {
    if (!deferredContext || !mem) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(Ptr<uint32_t>, sceGxmGetNotificationRegion) {
    return host.gxm.notification_region;
}

EXPORT(int, sceGxmGetParameterBufferThreshold, uint32_t *parameterBufferSize) {
    if (!parameterBufferSize) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(uint32_t, sceGxmGetPrecomputedDrawSize, const SceGxmVertexProgram *vertexProgram) {
    assert(vertexProgram);
    return SCE_GXM_PRECOMPUTED_DRAW_EXTRA_SIZE;
}

EXPORT(uint32_t, sceGxmGetPrecomputedFragmentStateSize, const SceGxmFragmentProgram *fragmentProgram) {
    assert(fragmentProgram);
    return SCE_GXM_PRECOMPUTED_FRAGMENT_STATE_EXTRA_SIZE;
}

EXPORT(uint32_t, sceGxmGetPrecomputedVertexStateSize, const SceGxmVertexProgram *vertexProgram) {
    assert(vertexProgram);
    return SCE_GXM_PRECOMPUTED_VERTEX_STATE_EXTRA_SIZE;
}

EXPORT(int, sceGxmGetRenderTargetMemSize, const SceGxmRenderTargetParams *params, uint32_t *hostMemSize) {
    if (!params) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    *hostMemSize = uint32_t(MB(2));
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

        SceGxmSyncObject *oldBuffer = Ptr<SceGxmSyncObject>(display_callback->old_buffer).get(*params.mem);
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
        renderer::subject_done(oldBuffer, renderer::SyncObjectSubject::DisplayQueue);
        renderer::subject_done(newBuffer, renderer::SyncObjectSubject::DisplayQueue);
    }

    return 0;
}

EXPORT(int, sceGxmInitialize, const SceGxmInitializeParams *params) {
    if (!params) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (((params->flags != 0x00000000U) && (params->flags != 0x00010000U) && (params->flags != 0x00020000U)) || (params->parameterBufferSize & 0x3FFFF)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if ((params->displayQueueMaxPendingCount * params->displayQueueCallbackDataSize) > 0x200) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    host.gxm.params = *params;
    host.gxm.display_queue.maxPendingCount_ = params->displayQueueMaxPendingCount;

    const ThreadStatePtr main_thread = util::find(thread_id, host.kernel.threads);

    const auto stack_size = SCE_KERNEL_STACK_SIZE_USER_DEFAULT; // TODO: Verify this is the correct stack size

    auto inject = create_cpu_dep_inject(host);
    host.gxm.display_queue_thread = create_thread(Ptr<void>(read_pc(*main_thread->cpu)), host.kernel, host.mem, "SceGxmDisplayQueue", SCE_KERNEL_HIGHEST_PRIORITY_USER, stack_size, inject, nullptr);

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

EXPORT(int, sceGxmMapFragmentUsseMemory, Ptr<void> base, uint32_t size, uint32_t *offset) {
    STUBBED("always return success");

    if (!base || !offset) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    // TODO What should this be?
    *offset = base.address();

    return 0;
}

EXPORT(int, sceGxmMapMemory, Ptr<void> base, uint32_t size, uint32_t attribs) {
    STUBBED("always return success");

    if (!base) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    return 0;
}

EXPORT(int, sceGxmMapVertexUsseMemory, Ptr<void> base, uint32_t size, uint32_t *offset) {
    STUBBED("always return success");

    if (!base || !offset) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    // TODO What should this be?
    *offset = base.address();

    return 0;
}

EXPORT(int, sceGxmMidSceneFlush, SceGxmContext *immediateContext, uint32_t flags, SceGxmSyncObject *vertexSyncObject, const SceGxmNotification *vertexNotification) {
    if (!immediateContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (flags & 0xFFFFFFFE) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (vertexSyncObject != nullptr) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmNotificationWait, const SceGxmNotification *notification) {
    if (!notification) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPadHeartbeat, const SceGxmColorSurface *displaySurface, SceGxmSyncObject *displaySyncObject) {
    if (!displaySurface || !displaySyncObject)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

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

EXPORT(int, sceGxmPrecomputedDrawInit, SceGxmPrecomputedDraw *state, Ptr<const SceGxmVertexProgram> program, Ptr<void> extra_data) {
    if (!state || !program || !extra_data) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    SceGxmPrecomputedDraw new_draw;

    new_draw.program = program;

    MempoolObject allocator(extra_data, SCE_GXM_PRECOMPUTED_DRAW_EXTRA_SIZE);
    new_draw.stream_data = allocator.alloc<StreamDatas>();

    uint16_t max_stream_index = 0;
    const auto &gxm_vertex_program = *program.get(host.mem);
    for (const SceGxmVertexAttribute &attribute : gxm_vertex_program.attributes) {
        max_stream_index = std::max(attribute.streamIndex, max_stream_index);
    }

    new_draw.stream_count = max_stream_index + 1;

    *state = new_draw;

    return 0;
}

EXPORT(int, sceGxmPrecomputedDrawSetAllVertexStreams, SceGxmPrecomputedDraw *state, const Ptr<const void> *stream_data) {
    if (!state) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    auto &state_stream_data = *state->stream_data.get(host.mem);
    for (int i = 0; i < state->stream_count; ++i) {
        state_stream_data[i] = stream_data[i];
    }

    return 0;
}

EXPORT(int, sceGxmPrecomputedDrawSetParams, SceGxmPrecomputedDraw *state, SceGxmPrimitiveType type, SceGxmIndexFormat index_format, Ptr<const void> index_data, uint32_t vertex_count) {
    if (!state || !index_data) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    state->type = type;
    state->index_format = index_format;
    state->index_data = index_data;
    state->vertex_count = vertex_count;

    return 0;
}

EXPORT(int, sceGxmPrecomputedDrawSetParamsInstanced, SceGxmPrecomputedDraw *precomputedDraw, SceGxmPrimitiveType primType, SceGxmIndexFormat indexType, Ptr<const void> indexData, uint32_t indexCount, uint32_t indexWrap) {
    if (!precomputedDraw || !indexData) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedDrawSetVertexStream, SceGxmPrecomputedDraw *state, uint32_t streamIndex, Ptr<const void> streamData) {
    if (!state) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (streamIndex > (SCE_GXM_MAX_VERTEX_STREAMS - 1)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!streamData) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    auto &stream_data = *state->stream_data.get(host.mem);
    stream_data[streamIndex] = streamData;

    return 0;
}

EXPORT(Ptr<const void>, sceGxmPrecomputedFragmentStateGetDefaultUniformBuffer, const SceGxmPrecomputedFragmentState *state) {
    UniformBuffers &uniform_buffers = *state->uniform_buffers.get(host.mem);
    return uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX];
}

EXPORT(int, sceGxmPrecomputedFragmentStateInit, SceGxmPrecomputedFragmentState *state, Ptr<const SceGxmFragmentProgram> program, Ptr<void> extra_data) {
    if (!state || !program) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    SceGxmPrecomputedFragmentState new_state;
    new_state.program = program;

    MempoolObject allocator(extra_data, SCE_GXM_PRECOMPUTED_FRAGMENT_STATE_EXTRA_SIZE);
    new_state.textures = allocator.alloc<TextureDatas>();
    new_state.uniform_buffers = allocator.alloc<UniformBuffers>();

    const auto &fragment_program_gxp = *program.get(host.mem)->program.get(host.mem);
    const auto frag_paramters = gxp::program_parameters(fragment_program_gxp);
    int32_t max_texture_index = 0;
    for (uint32_t i = 0; i < fragment_program_gxp.parameter_count; ++i) {
        const auto parameter = frag_paramters[i];
        if (parameter.category == SCE_GXM_PARAMETER_CATEGORY_SAMPLER) {
            max_texture_index = std::max(max_texture_index, parameter.resource_index);
        }
    }
    new_state.texture_count = static_cast<uint16_t>(max_texture_index + 1);

    *state = new_state;

    return 0;
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetAllAuxiliarySurfaces) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetAllTextures, SceGxmPrecomputedFragmentState *state, Ptr<const SceGxmTexture> textures) {
    if (!state || !textures) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    auto &state_textures = *state->textures.get(host.mem);
    for (int i = 0; i < state->texture_count; ++i) {
        state_textures[i] = textures.get(host.mem)[i];
    }

    return 0;
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetAllUniformBuffers, SceGxmPrecomputedFragmentState *precomputedState, Ptr<const void> const *bufferDataArray) {
    if (!precomputedState || !precomputedState->uniform_buffers || !bufferDataArray)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    UniformBuffers *uniform_buffers = precomputedState->uniform_buffers.get(host.mem);
    if (!uniform_buffers)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    for (auto b = 0; bufferDataArray[b]; b++)
        (*uniform_buffers)[b] = bufferDataArray[b];

    return 0;
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetDefaultUniformBuffer, SceGxmPrecomputedFragmentState *state, Ptr<const void> buffer) {
    if (!state || !buffer) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    UniformBuffers &uniform_buffers = *state->uniform_buffers.get(host.mem);
    uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX] = buffer;

    return 0;
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetTexture, SceGxmPrecomputedFragmentState *state, uint32_t index, const SceGxmTexture *texture) {
    if (!state) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (index > (SCE_GXM_MAX_TEXTURE_UNITS - 1)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    auto &state_textures = *state->textures.get(host.mem);
    state_textures[index] = *texture;

    return 0;
}

EXPORT(int, sceGxmPrecomputedFragmentStateSetUniformBuffer, SceGxmPrecomputedFragmentState *precomputedState, uint32_t bufferIndex, Ptr<const void> bufferData) {
    if (!precomputedState) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (bufferIndex > (SCE_GXM_MAX_UNIFORM_BUFFERS - 1)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!bufferData) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(Ptr<const void>, sceGxmPrecomputedVertexStateGetDefaultUniformBuffer, SceGxmPrecomputedVertexState *state) {
    UniformBuffers &uniform_buffers = *state->uniform_buffers.get(host.mem);
    return uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX];
}

EXPORT(int, sceGxmPrecomputedVertexStateInit, SceGxmPrecomputedVertexState *state, Ptr<const SceGxmVertexProgram> program, Ptr<void> extra_data) {
    if (!state || !program || !extra_data) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    MempoolObject allocator(extra_data, SCE_GXM_PRECOMPUTED_VERTEX_STATE_EXTRA_SIZE);
    SceGxmPrecomputedVertexState new_state;
    new_state.program = program;
    new_state.uniform_buffers = allocator.alloc<UniformBuffers>();
    *state = new_state;

    return 0;
}

EXPORT(int, sceGxmPrecomputedVertexStateSetAllTextures, SceGxmPrecomputedVertexState *precomputedState, const SceGxmTexture *textureArray) {
    if (!precomputedState || !textureArray) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedVertexStateSetAllUniformBuffers, SceGxmPrecomputedVertexState *precomputedState, Ptr<const void> const *bufferDataArray) {
    if (!precomputedState || !precomputedState->uniform_buffers || !bufferDataArray)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    UniformBuffers *uniform_buffers = precomputedState->uniform_buffers.get(host.mem);
    if (!uniform_buffers)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    for (auto b = 0; bufferDataArray[b]; b++)
        (*uniform_buffers)[b] = bufferDataArray[b];

    return 0;
}

EXPORT(int, sceGxmPrecomputedVertexStateSetDefaultUniformBuffer, SceGxmPrecomputedVertexState *state, Ptr<const void> buffer) {
    if (!state || !buffer) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    UniformBuffers &uniform_buffers = *state->uniform_buffers.get(host.mem);
    uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX] = buffer;

    return 0;
}

EXPORT(int, sceGxmPrecomputedVertexStateSetTexture, SceGxmPrecomputedVertexState *precomputedState, uint32_t textureIndex, const SceGxmTexture *texture) {
    if (!precomputedState || !texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmPrecomputedVertexStateSetUniformBuffer, SceGxmPrecomputedVertexState *precomputedState, uint32_t bufferIndex, Ptr<const void> bufferData) {
    if (!precomputedState) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (bufferIndex > (SCE_GXM_MAX_UNIFORM_BUFFERS - 1)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    if (!bufferData) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    UniformBuffers &uniform_buffers = *precomputedState->uniform_buffers.get(host.mem);
    uniform_buffers[bufferIndex] = bufferData;

    return 0;
}

EXPORT(int, sceGxmProgramCheck, const SceGxmProgram *program) {
    if (!program)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    if (memcmp(&program->magic, "GXP", 4) != 0)
        return RET_ERROR(SCE_GXM_ERROR_NULL_PROGRAM);

    return 0;
}

EXPORT(Ptr<SceGxmProgramParameter>, sceGxmProgramFindParameterByName, const SceGxmProgram *program, const char *name) {
    const MemState &mem = host.mem;
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
    const MemState &mem = host.mem;

    if (semantic == SCE_GXM_PARAMETER_SEMANTIC_NONE) {
        return Ptr<SceGxmProgramParameter>();
    }

    assert(program);
    if (!program)
        return Ptr<SceGxmProgramParameter>();

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

EXPORT(Ptr<SceGxmProgramParameter>, _sceGxmProgramFindParameterBySemantic, const SceGxmProgram *program, SceGxmParameterSemantic semantic, uint32_t index) {
    return export_sceGxmProgramFindParameterBySemantic(host, thread_id, export_name, program, semantic, index);
}

EXPORT(uint32_t, sceGxmProgramGetDefaultUniformBufferSize, const SceGxmProgram *program) {
    return program->default_uniform_buffer_count * 4;
}

EXPORT(uint32_t, sceGxmProgramGetFragmentProgramInputs, Ptr<const SceGxmProgram> program_) {
    const auto program = program_.get(host.mem);

    return static_cast<uint32_t>(gxp::get_fragment_inputs(*program));
}

EXPORT(int, sceGxmProgramGetOutputRegisterFormat, const SceGxmProgram *program, SceGxmParameterType *type, uint32_t *componentCount) {
    if (!program || !type || !componentCount)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (!program->is_fragment())
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    return UNIMPLEMENTED();
}

EXPORT(Ptr<SceGxmProgramParameter>, sceGxmProgramGetParameter, const SceGxmProgram *program, uint32_t index) {
    const SceGxmProgramParameter *const parameters = reinterpret_cast<const SceGxmProgramParameter *>(reinterpret_cast<const uint8_t *>(&program->parameters_offset) + program->parameters_offset);

    const SceGxmProgramParameter *const parameter = &parameters[index];
    const uint8_t *const parameter_bytes = reinterpret_cast<const uint8_t *>(parameter);

    const Address parameter_address = static_cast<Address>(parameter_bytes - &host.mem.memory[0]);
    return Ptr<SceGxmProgramParameter>(parameter_address);
}

EXPORT(uint32_t, sceGxmProgramGetParameterCount, const SceGxmProgram *program) {
    assert(program);
    return program->parameter_count;
}

EXPORT(uint32_t, sceGxmProgramGetSize, const SceGxmProgram *program) {
    assert(program);
    return program->size;
}

EXPORT(SceGxmProgramType, sceGxmProgramGetType, const SceGxmProgram *program) {
    assert(program);
    return program->get_type();
}

EXPORT(uint32_t, sceGxmProgramGetVertexProgramOutputs, Ptr<const SceGxmProgram> program_) {
    const auto program = program_.get(host.mem);

    return static_cast<uint32_t>(gxp::get_vertex_outputs(*program));
}

EXPORT(bool, sceGxmProgramIsDepthReplaceUsed, const SceGxmProgram *program) {
    assert(program);
    return program->is_depth_replace_used();
}

EXPORT(bool, sceGxmProgramIsDiscardUsed, const SceGxmProgram *program) {
    assert(program);
    return program->is_discard_used();
}

EXPORT(bool, sceGxmProgramIsEquivalent, const SceGxmProgram *programA, const SceGxmProgram *programB) {
    if (!programA || !programB) {
        LOG_ERROR("SCE_GXM_ERROR_INVALID_POINTER");
        return false;
    }

    return (programA->size == programB->size) && (memcmp(programA, programB, programA->size) == 0);
}

EXPORT(bool, sceGxmProgramIsFragColorUsed, const SceGxmProgram *program) {
    assert(program);
    return program->is_frag_color_used();
}

EXPORT(bool, sceGxmProgramIsNativeColorUsed, const SceGxmProgram *program) {
    assert(program);
    return program->is_native_color();
}

EXPORT(bool, sceGxmProgramIsSpriteCoordUsed, const SceGxmProgram *program) {
    assert(program);
    return program->is_sprite_coord_used();
}

EXPORT(uint32_t, sceGxmProgramParameterGetArraySize, const SceGxmProgramParameter *parameter) {
    assert(parameter);
    return parameter->array_size;
}

EXPORT(int, sceGxmProgramParameterGetCategory, const SceGxmProgramParameter *parameter) {
    assert(parameter);
    return parameter->category;
}

EXPORT(uint32_t, sceGxmProgramParameterGetComponentCount, const SceGxmProgramParameter *parameter) {
    assert(parameter);
    return parameter->component_count;
}

EXPORT(uint32_t, sceGxmProgramParameterGetContainerIndex, const SceGxmProgramParameter *parameter) {
    assert(parameter);
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

EXPORT(uint32_t, sceGxmProgramParameterGetResourceIndex, const SceGxmProgramParameter *parameter) {
    assert(parameter);
    return parameter->resource_index;
}

EXPORT(int, sceGxmProgramParameterGetSemantic, const SceGxmProgramParameter *parameter) {
    assert(parameter);

    if (parameter->category != SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE)
        return SCE_GXM_PARAMETER_SEMANTIC_NONE;

    return parameter->semantic;
}

EXPORT(int, _sceGxmProgramParameterGetSemantic, const SceGxmProgramParameter *parameter) {
    return export_sceGxmProgramParameterGetSemantic(host, thread_id, export_name, parameter);
}

EXPORT(uint32_t, sceGxmProgramParameterGetSemanticIndex, const SceGxmProgramParameter *parameter) {
    return parameter->semantic_index & 0xf;
}

EXPORT(int, sceGxmProgramParameterGetType, const SceGxmProgramParameter *parameter) {
    return parameter->type;
}

EXPORT(bool, sceGxmProgramParameterIsRegFormat, const SceGxmProgram *program, const SceGxmProgramParameter *parameter) {
    if (program->is_fragment()) {
        return false;
    }

    if (parameter->category != SceGxmParameterCategory::SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) {
        return false;
    }

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

EXPORT(int, sceGxmRenderTargetGetDriverMemBlock, const SceGxmRenderTarget *renderTarget, SceUID *driverMemBlock) {
    if (!renderTarget || !driverMemBlock) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmReserveFragmentDefaultUniformBuffer, SceGxmContext *context, Ptr<void> *uniformBuffer) {
    if (!context || !uniformBuffer)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

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
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    *uniformBuffer = context->state.params.vertexRingBufferMem.cast<uint8_t>() + static_cast<int32_t>(context->state.vertex_ring_buffer_used);

    context->state.vertex_last_reserve_status = SceGxmLastReserveStatus::Reserved;
    context->state.vertex_uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX] = *uniformBuffer;

    return 0;
}

EXPORT(int, sceGxmSetAuxiliarySurface) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetBackDepthBias, SceGxmContext *context, int32_t factor, int32_t units) {
    renderer::set_depth_bias(*host.renderer, context->renderer.get(), &context->state, false, factor, units);
}

EXPORT(void, sceGxmSetBackDepthFunc, SceGxmContext *context, SceGxmDepthFunc depthFunc) {
    renderer::set_depth_func(*host.renderer, context->renderer.get(), &context->state, false, depthFunc);
}

EXPORT(void, sceGxmSetBackDepthWriteEnable, SceGxmContext *context, SceGxmDepthWriteMode enable) {
    renderer::set_depth_write_enable_mode(*host.renderer, context->renderer.get(), &context->state, false, enable);
}

EXPORT(void, sceGxmSetBackFragmentProgramEnable, SceGxmContext *context, SceGxmFragmentProgramMode enable) {
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetBackLineFillLastPixelEnable, SceGxmContext *context, SceGxmLineFillLastPixelMode enable) {
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetBackPointLineWidth, SceGxmContext *context, uint32_t width) {
    renderer::set_point_line_width(*host.renderer, context->renderer.get(), &context->state, false, width);
}

EXPORT(void, sceGxmSetBackPolygonMode, SceGxmContext *context, SceGxmPolygonMode mode) {
    renderer::set_polygon_mode(*host.renderer, context->renderer.get(), &context->state, false, mode);
}

EXPORT(void, sceGxmSetBackStencilFunc, SceGxmContext *context, SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail, SceGxmStencilOp depthPass, uint8_t compareMask, uint8_t writeMask) {
    renderer::set_stencil_func(*host.renderer, context->renderer.get(), &context->state, false, func, stencilFail, depthFail, depthPass, compareMask, writeMask);
}

EXPORT(void, sceGxmSetBackStencilRef, SceGxmContext *context, uint8_t sref) {
    renderer::set_stencil_ref(*host.renderer, context->renderer.get(), &context->state, false, sref);
}

EXPORT(void, sceGxmSetBackVisibilityTestEnable, SceGxmContext *context, SceGxmVisibilityTestMode enable) {
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetBackVisibilityTestIndex, SceGxmContext *context, uint32_t index) {
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetBackVisibilityTestOp, SceGxmContext *context, SceGxmVisibilityTestOp op) {
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetCullMode, SceGxmContext *context, SceGxmCullMode mode) {
    renderer::set_cull_mode(*host.renderer, context->renderer.get(), &context->state, mode);
}

EXPORT(void, sceGxmSetDefaultRegionClipAndViewport, SceGxmContext *context, uint32_t xMax, uint32_t yMax) {
    uint32_t xMin = 0, yMin = 0;

    renderer::set_region_clip(*host.renderer, context->renderer.get(), &context->state, SCE_GXM_REGION_CLIP_OUTSIDE, xMin, xMax, yMin, yMax);

    renderer::set_viewport(*host.renderer, context->renderer.get(), &context->state,
        0.5f * static_cast<float>(1 + xMax + xMin), 0.5f * static_cast<float>(1 + yMax + yMin),
        0.5f, 0.5f * static_cast<float>(1 + xMax - xMin), -0.5f * static_cast<float>(1 + yMax - yMin), 0.5f);
}

EXPORT(int, sceGxmSetDeferredContextFragmentBuffer, SceGxmContext *deferredContext, void *mem, uint32_t size) {
    if (!deferredContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetDeferredContextVdmBuffer, SceGxmContext *deferredContext, void *mem, uint32_t size) {
    if (!deferredContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetDeferredContextVertexBuffer, SceGxmContext *deferredContext, void *mem, uint32_t size) {
    if (!deferredContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetFragmentDefaultUniformBuffer, SceGxmContext *context, Ptr<const void> bufferData) {
    if (!context || !bufferData) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    context->state.fragment_uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX] = bufferData;
    return 0;
}

EXPORT(void, sceGxmSetFragmentProgram, SceGxmContext *context, Ptr<const SceGxmFragmentProgram> fragmentProgram) {
    assert(context);
    assert(fragmentProgram);

    if (!context || !fragmentProgram)
        return;

    context->record.fragment_program = fragmentProgram;
    renderer::set_program(*host.renderer, context->renderer.get(), &context->state, fragmentProgram, true);
}

EXPORT(int, sceGxmSetFragmentTexture, SceGxmContext *context, uint32_t textureIndex, const SceGxmTexture *texture) {
    if (!context || !texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (textureIndex > (SCE_GXM_MAX_TEXTURE_UNITS - 1)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    renderer::set_fragment_texture(*host.renderer, context->renderer.get(), &context->state, textureIndex, *texture);

    return 0;
}

EXPORT(int, sceGxmSetFragmentUniformBuffer, SceGxmContext *context, uint32_t bufferIndex, Ptr<const void> bufferData) {
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
    renderer::set_depth_bias(*host.renderer, context->renderer.get(), &context->state, true, factor, units);
}

EXPORT(void, sceGxmSetFrontDepthFunc, SceGxmContext *context, SceGxmDepthFunc depthFunc) {
    renderer::set_depth_func(*host.renderer, context->renderer.get(), &context->state, true, depthFunc);
}

EXPORT(void, sceGxmSetFrontDepthWriteEnable, SceGxmContext *context, SceGxmDepthWriteMode enable) {
    renderer::set_depth_write_enable_mode(*host.renderer, context->renderer.get(), &context->state, true, enable);
}

EXPORT(void, sceGxmSetFrontFragmentProgramEnable, SceGxmContext *context, SceGxmFragmentProgramMode enable) {
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetFrontLineFillLastPixelEnable, SceGxmContext *context, SceGxmLineFillLastPixelMode enable) {
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetFrontPointLineWidth, SceGxmContext *context, uint32_t width) {
    renderer::set_point_line_width(*host.renderer, context->renderer.get(), &context->state, true, width);
}

EXPORT(void, sceGxmSetFrontPolygonMode, SceGxmContext *context, SceGxmPolygonMode mode) {
    renderer::set_polygon_mode(*host.renderer, context->renderer.get(), &context->state, true, mode);
}

EXPORT(void, sceGxmSetFrontStencilFunc, SceGxmContext *context, SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail, SceGxmStencilOp depthPass, uint8_t compareMask, uint8_t writeMask) {
    renderer::set_stencil_func(*host.renderer, context->renderer.get(), &context->state, true, func, stencilFail, depthFail, depthPass, compareMask, writeMask);
}

EXPORT(void, sceGxmSetFrontStencilRef, SceGxmContext *context, uint8_t sref) {
    renderer::set_stencil_ref(*host.renderer, context->renderer.get(), &context->state, true, sref);
}

EXPORT(void, sceGxmSetFrontVisibilityTestEnable, SceGxmContext *context, SceGxmVisibilityTestMode enable) {
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetFrontVisibilityTestIndex, SceGxmContext *context, uint32_t index) {
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetFrontVisibilityTestOp, SceGxmContext *context, SceGxmVisibilityTestOp op) {
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetPrecomputedFragmentState, SceGxmContext *context, Ptr<SceGxmPrecomputedFragmentState> state) {
    if (!state) {
        context->state.precomputed_fragment_state.reset();
        return;
    }

    context->state.precomputed_fragment_state = state;
}

EXPORT(void, sceGxmSetPrecomputedVertexState, SceGxmContext *context, Ptr<SceGxmPrecomputedVertexState> state) {
    if (!state) {
        context->state.precomputed_vertex_state.reset();
        return;
    }

    context->state.precomputed_vertex_state = state;
}

EXPORT(void, sceGxmSetRegionClip, SceGxmContext *context, SceGxmRegionClipMode mode, uint32_t xMin, uint32_t yMin, uint32_t xMax, uint32_t yMax) {
    renderer::set_region_clip(*host.renderer, context->renderer.get(), &context->state, mode, xMin, xMax, yMin, yMax);
}

EXPORT(void, sceGxmSetTwoSidedEnable, SceGxmContext *context, SceGxmTwoSidedMode mode) {
    renderer::set_two_sided_enable(*host.renderer, context->renderer.get(), &context->state, mode);
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
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetValidationEnable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetVertexDefaultUniformBuffer, SceGxmContext *context, Ptr<const void> bufferData) {
    if (!context || !bufferData) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    context->state.vertex_uniform_buffers[SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX] = bufferData;
    return 0;
}

EXPORT(void, sceGxmSetVertexProgram, SceGxmContext *context, Ptr<const SceGxmVertexProgram> vertexProgram) {
    assert(context);
    assert(vertexProgram);

    if (!context || !vertexProgram)
        return;

    context->record.vertex_program = vertexProgram;
    renderer::set_program(*host.renderer, context->renderer.get(), &context->state, vertexProgram, false);
}

EXPORT(int, sceGxmSetVertexStream, SceGxmContext *context, uint32_t streamIndex, Ptr<const void> streamData) {
    if (!context || !streamData)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (streamIndex > (SCE_GXM_MAX_VERTEX_STREAMS - 1))
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    context->state.stream_data[streamIndex] = streamData;
    return 0;
}

EXPORT(int, sceGxmSetVertexTexture, SceGxmContext *context, uint32_t textureIndex, const SceGxmTexture *texture) {
    if (!context || !texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if (textureIndex > (SCE_GXM_MAX_TEXTURE_UNITS - 1)) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetVertexUniformBuffer, SceGxmContext *context, uint32_t bufferIndex, Ptr<const void> bufferData) {
    if (!context || !bufferData)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (bufferIndex > (SCE_GXM_MAX_UNIFORM_BUFFERS - 1))
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);

    context->state.vertex_uniform_buffers[bufferIndex] = bufferData;
    return 0;
}

EXPORT(void, sceGxmSetViewport, SceGxmContext *context, float xOffset, float xScale, float yOffset, float yScale, float zOffset, float zScale) {
    // Set viewport to enable, enable more offset and scale to set
    renderer::set_viewport(*host.renderer, context->renderer.get(), &context->state, xOffset, yOffset, zOffset, xScale, yScale, zScale);
}

EXPORT(void, sceGxmSetViewportEnable, SceGxmContext *context, SceGxmViewportMode enable) {
    // Set viewport to enable/disable, no additional offset and scale to set.
    renderer::set_viewport_enable(*host.renderer, context->renderer.get(), &context->state, enable);
}

EXPORT(int, sceGxmSetVisibilityBuffer, SceGxmContext *immediateContext, void *bufferBase, uint32_t stridePerCore) {
    if (!immediateContext) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetWBufferEnable) {
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetWClampEnable) {
    UNIMPLEMENTED();
}

EXPORT(void, sceGxmSetWClampValue, SceGxmContext *context, float clampValue) {
    UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetWarningEnabled) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetYuvProfile) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherAddRefFragmentProgram, SceGxmShaderPatcher *shaderPatcher, SceGxmFragmentProgram *fragmentProgram) {
    if (!shaderPatcher || !fragmentProgram)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    ++fragmentProgram->reference_count;

    return 0;
}

EXPORT(int, sceGxmShaderPatcherAddRefVertexProgram, SceGxmShaderPatcher *shaderPatcher, SceGxmVertexProgram *vertexProgram) {
    if (!shaderPatcher || !vertexProgram)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    ++vertexProgram->reference_count;

    return 0;
}

EXPORT(int, sceGxmShaderPatcherCreate, const SceGxmShaderPatcherParams *params, Ptr<SceGxmShaderPatcher> *shaderPatcher) {
    if (!params || !shaderPatcher)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    *shaderPatcher = alloc<SceGxmShaderPatcher>(host.mem, __FUNCTION__);
    assert(*shaderPatcher);
    if (!*shaderPatcher) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    return 0;
}

EXPORT(int, sceGxmShaderPatcherCreateFragmentProgram, SceGxmShaderPatcher *shaderPatcher, const SceGxmRegisteredProgram *programId, SceGxmOutputRegisterFormat outputFormat, SceGxmMultisampleMode multisampleMode, const SceGxmBlendInfo *blendInfo, Ptr<const SceGxmProgram> vertexProgram, Ptr<SceGxmFragmentProgram> *fragmentProgram) {
    MemState &mem = host.mem;

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

    *fragmentProgram = alloc<SceGxmFragmentProgram>(mem, __FUNCTION__);
    assert(*fragmentProgram);
    if (!*fragmentProgram) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    SceGxmFragmentProgram *const fp = fragmentProgram->get(mem);
    fp->is_maskupdate = false;
    fp->program = programId->program;

    if (!renderer::create(fp->renderer_data, *host.renderer, *programId->program.get(mem), blendInfo, host.renderer->gxp_ptr_map, host.base_path.c_str(), host.io.title_id.c_str())) {
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }

    shaderPatcher->fragment_program_cache.emplace(key, *fragmentProgram);

    return 0;
}

EXPORT(int, sceGxmShaderPatcherCreateMaskUpdateFragmentProgram, SceGxmShaderPatcher *shaderPatcher, Ptr<SceGxmFragmentProgram> *fragmentProgram) {
    MemState &mem = host.mem;

    if (!shaderPatcher || !fragmentProgram)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    *fragmentProgram = alloc<SceGxmFragmentProgram>(mem, __FUNCTION__);
    assert(*fragmentProgram);
    if (!*fragmentProgram) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    SceGxmFragmentProgram *const fp = fragmentProgram->get(mem);
    fp->is_maskupdate = true;
    fp->program = alloc(mem, size_mask_gxp, __FUNCTION__);
    memcpy(const_cast<SceGxmProgram *>(fp->program.get(mem)), mask_gxp, size_mask_gxp);

    if (!renderer::create(fp->renderer_data, *host.renderer, *fp->program.get(mem), nullptr, host.renderer->gxp_ptr_map, host.base_path.c_str(), host.io.title_id.c_str())) {
        return RET_ERROR(SCE_GXM_ERROR_DRIVER);
    }

    return 0;
}

EXPORT(int, sceGxmShaderPatcherCreateVertexProgram, SceGxmShaderPatcher *shaderPatcher, const SceGxmRegisteredProgram *programId, const SceGxmVertexAttribute *attributes, uint32_t attributeCount, const SceGxmVertexStream *streams, uint32_t streamCount, Ptr<SceGxmVertexProgram> *vertexProgram) {
    MemState &mem = host.mem;

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

    shaderPatcher->vertex_program_cache.emplace(key, *vertexProgram);

    return 0;
}

EXPORT(int, sceGxmShaderPatcherDestroy, Ptr<SceGxmShaderPatcher> shaderPatcher) {
    if (!shaderPatcher)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    free(host.mem, shaderPatcher);

    return 0;
}

EXPORT(int, sceGxmShaderPatcherForceUnregisterProgram, SceGxmShaderPatcher *shaderPatcher, SceGxmShaderPatcherId programId) {
    if (!shaderPatcher || !programId) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(uint32_t, sceGxmShaderPatcherGetBufferMemAllocated, const SceGxmShaderPatcher *shaderPatcher) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherGetFragmentProgramRefCount, const SceGxmShaderPatcher *shaderPatcher, const SceGxmFragmentProgram *fragmentProgram, uint32_t *refCount) {
    if (!shaderPatcher || !fragmentProgram || !refCount) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    *refCount = fragmentProgram->reference_count;
    return 0;
}

EXPORT(uint32_t, sceGxmShaderPatcherGetFragmentUsseMemAllocated, const SceGxmShaderPatcher *shaderPatcher) {
    return UNIMPLEMENTED();
}

EXPORT(uint32_t, sceGxmShaderPatcherGetHostMemAllocated, const SceGxmShaderPatcher *shaderPatcher) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<const SceGxmProgram>, sceGxmShaderPatcherGetProgramFromId, SceGxmShaderPatcherId programId) {
    if (!programId) {
        return Ptr<const SceGxmProgram>();
    }

    return programId.get(host.mem)->program;
}

EXPORT(int, sceGxmShaderPatcherGetUserData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherGetVertexProgramRefCount, const SceGxmShaderPatcher *shaderPatcher, const SceGxmVertexProgram *vertexProgram, uint32_t *refCount) {
    if (!shaderPatcher || !vertexProgram || !refCount) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    *refCount = vertexProgram->reference_count;
    return 0;
}

EXPORT(uint32_t, sceGxmShaderPatcherGetVertexUsseMemAllocated, const SceGxmShaderPatcher *shaderPatcher) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherRegisterProgram, SceGxmShaderPatcher *shaderPatcher, Ptr<const SceGxmProgram> programHeader, SceGxmShaderPatcherId *programId) {
    if (!shaderPatcher || !programHeader || !programId)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

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
    if (!shaderPatcher || !fragmentProgram)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

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
    if (!shaderPatcher || !vertexProgram)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    SceGxmVertexProgram *const vp = vertexProgram.get(host.mem);
    --vp->reference_count;
    if (vp->reference_count == 0) {
        for (VertexProgramCache::const_iterator it = shaderPatcher->vertex_program_cache.begin(); it != shaderPatcher->vertex_program_cache.end(); ++it) {
            if (it->second == vertexProgram) {
                shaderPatcher->vertex_program_cache.erase(it);
                break;
            }
        }
        free(host.mem, vertexProgram);
    }

    return 0;
}

EXPORT(int, sceGxmShaderPatcherSetAuxiliarySurface) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherSetUserData, SceGxmShaderPatcher *shaderPatcher, void *userData) {
    if (!shaderPatcher) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmShaderPatcherUnregisterProgram, SceGxmShaderPatcher *shaderPatcher, SceGxmShaderPatcherId programId) {
    if (!shaderPatcher || !programId)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    SceGxmRegisteredProgram *const rp = programId.get(host.mem);
    rp->program.reset();

    free(host.mem, programId);

    return 0;
}

EXPORT(int, sceGxmSyncObjectCreate, Ptr<SceGxmSyncObject> *syncObject) {
    if (!syncObject)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    *syncObject = alloc<SceGxmSyncObject>(host.mem, __FUNCTION__);
    if (!*syncObject) {
        return RET_ERROR(SCE_GXM_ERROR_OUT_OF_MEMORY);
    }

    // Set all subjects to be done.
    syncObject->get(host.mem)->done = 0xFFFFFFFF;

    return 0;
}

EXPORT(int, sceGxmSyncObjectDestroy, Ptr<SceGxmSyncObject> syncObject) {
    if (!syncObject)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

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

EXPORT(Ptr<void>, sceGxmTextureGetData, const SceGxmTexture *texture) {
    assert(texture);
    return Ptr<void>(texture->data_addr << 2);
}

EXPORT(SceGxmTextureFormat, sceGxmTextureGetFormat, const SceGxmTexture *texture) {
    assert(texture);
    return gxm::get_format(texture);
}

EXPORT(int, sceGxmTextureGetGammaMode, const SceGxmTexture *texture) {
    assert(texture);
    return (texture->gamma_mode << 27);
}

EXPORT(uint32_t, sceGxmTextureGetHeight, const SceGxmTexture *texture) {
    assert(texture);
    return static_cast<uint32_t>(gxm::get_height(texture));
}

EXPORT(uint32_t, sceGxmTextureGetLodBias, const SceGxmTexture *texture) {
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return 0;
    }

    return texture->lod_bias;
}

EXPORT(uint32_t, sceGxmTextureGetLodMin, const SceGxmTexture *texture) {
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return 0;
    }

    return texture->lod_min0 | (texture->lod_min1 << 2);
}

EXPORT(int, sceGxmTextureGetMagFilter, const SceGxmTexture *texture) {
    assert(texture);
    return texture->mag_filter;
}

EXPORT(int, sceGxmTextureGetMinFilter, const SceGxmTexture *texture) {
    assert(texture);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return texture->mag_filter;
    }
    return texture->min_filter;
}

EXPORT(SceGxmTextureMipFilter, sceGxmTextureGetMipFilter, const SceGxmTexture *texture) {
    assert(texture);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
    }
    return texture->mip_filter ? SCE_GXM_TEXTURE_MIP_FILTER_ENABLED : SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
}

EXPORT(uint32_t, sceGxmTextureGetMipmapCount, const SceGxmTexture *texture) {
    assert(texture);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return 0;
    }
    return (texture->mip_count + 1) & 0xf;
}

EXPORT(uint32_t, sceGxmTextureGetMipmapCountUnsafe, const SceGxmTexture *texture) {
    assert(texture);
    return (texture->mip_count + 1) & 0xf;
}

EXPORT(int, sceGxmTextureGetNormalizeMode, const SceGxmTexture *texture) {
    assert(texture);
    return texture->normalize_mode << 31;
}

EXPORT(Ptr<void>, sceGxmTextureGetPalette, const SceGxmTexture *texture) {
    assert(texture);
    assert(gxm::is_paletted_format(gxm::get_format(texture)));
    return Ptr<void>(texture->palette_addr << 6);
}

EXPORT(uint32_t, sceGxmTextureGetStride, const SceGxmTexture *texture) {
    assert(texture);
    if (texture->texture_type() != SCE_GXM_TEXTURE_LINEAR_STRIDED)
        return 0;

    return uint32_t(gxm::get_stride_in_bytes(texture));
}

EXPORT(int, sceGxmTextureGetType, const SceGxmTexture *texture) {
    assert(texture);
    return (texture->type << 29);
}

EXPORT(int, sceGxmTextureGetUAddrMode, const SceGxmTexture *texture) {
    assert(texture);
    return texture->uaddr_mode;
}

EXPORT(int, sceGxmTextureGetUAddrModeSafe, const SceGxmTexture *texture) {
    assert(texture);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return SCE_GXM_TEXTURE_ADDR_CLAMP;
    }
    return texture->uaddr_mode;
}

EXPORT(int, sceGxmTextureGetVAddrMode, const SceGxmTexture *texture) {
    assert(texture);
    return texture->vaddr_mode;
}

EXPORT(int, sceGxmTextureGetVAddrModeSafe, const SceGxmTexture *texture) {
    assert(texture);
    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return SCE_GXM_TEXTURE_ADDR_CLAMP;
    }
    return texture->vaddr_mode;
}

EXPORT(uint32_t, sceGxmTextureGetWidth, const SceGxmTexture *texture) {
    assert(texture);
    return static_cast<uint32_t>(gxm::get_width(texture));
}

EXPORT(int, sceGxmTextureInitCube, SceGxmTexture *texture, const void *data, SceGxmTextureFormat texFormat, uint32_t width, uint32_t height, uint32_t mipCount) {
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTextureInitCubeArbitrary, SceGxmTexture *texture, const void *data, SceGxmTextureFormat texFormat, uint32_t width, uint32_t height, uint32_t mipCount) {
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmTextureInitLinear, SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, uint32_t width, uint32_t height, uint32_t mipCount) {
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const int result = init_texture_base(export_name, texture, data, texFormat, width, height, mipCount, SCE_GXM_TEXTURE_LINEAR);

    return result;
}

EXPORT(int, sceGxmTextureInitLinearStrided, SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, uint32_t width, uint32_t height, uint32_t byteStride) {
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
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const int result = init_texture_base(export_name, texture, data, texFormat, width, height, mipCount, SCE_GXM_TEXTURE_SWIZZLED);

    return result;
}

EXPORT(int, sceGxmTextureInitSwizzledArbitrary, SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, uint32_t width, uint32_t height, uint32_t mipCount) {
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const auto result = init_texture_base(export_name, texture, data, texFormat, width, height, mipCount, SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY);

    return result;
}

EXPORT(int, sceGxmTextureInitTiled, SceGxmTexture *texture, Ptr<const void> data, SceGxmTextureFormat texFormat, uint32_t width, uint32_t height, uint32_t mipCount) {
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    const int result = init_texture_base(export_name, texture, data, texFormat, width, height, mipCount, SCE_GXM_TEXTURE_TILED);

    return result;
}

EXPORT(int, sceGxmTextureSetData, SceGxmTexture *texture, Ptr<const void> data) {
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    texture->data_addr = data.address() >> 2;
    return 0;
}

EXPORT(int, sceGxmTextureSetFormat, SceGxmTexture *texture, SceGxmTextureFormat texFormat) {
    STUBBED("FAST");
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    texture->base_format = (texFormat & 0x1F000000) >> 24;
    texture->swizzle_format = (texFormat & 0x7000) >> 12;
    return SCE_KERNEL_OK;
}

EXPORT(int, sceGxmTextureSetGammaMode, SceGxmTexture *texture, SceGxmTextureGammaMode gammaMode) {
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }
    texture->gamma_mode = (static_cast<std::uint32_t>(gammaMode) >> 27);
    return 0;
}

EXPORT(int, sceGxmTextureSetHeight, SceGxmTexture *texture, uint32_t height) {
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

EXPORT(int, sceGxmTextureSetLodBias, SceGxmTexture *texture, uint32_t bias) {
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
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    texture->mag_filter = (uint32_t)magFilter;
    return 0;
}

EXPORT(int, sceGxmTextureSetMinFilter, SceGxmTexture *texture, SceGxmTextureFilter minFilter) {
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
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }

    texture->mip_filter = (uint32_t)mipFilter;
    return 0;
}

EXPORT(int, sceGxmTextureSetMipmapCount, SceGxmTexture *texture, uint32_t mipCount) {
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    if ((texture->type << 29) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);
    }

    if (mipCount > 13) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_VALUE);
    }

    texture->mip_count = (uint32_t)mipCount;
    return 0;
}

EXPORT(int, sceGxmTextureSetNormalizeMode, SceGxmTexture *texture, SceGxmTextureNormalizeMode normalizeMode) {
    if (!texture) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    texture->normalize_mode = (static_cast<std::uint32_t>(normalizeMode) >> 31);
    return 0;
}

EXPORT(int, sceGxmTextureSetPalette, SceGxmTexture *texture, Ptr<const void> paletteData) {
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    if (paletteData.address() & 0x3F)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_ALIGNMENT);

    texture->palette_addr = (paletteData.address() >> 6);
    return 0;
}

EXPORT(int, sceGxmTextureSetStride, SceGxmTexture *texture, uint32_t byteStride) {
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
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (!verify_texture_mode(texture, mode))
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);

    texture->uaddr_mode = mode;
    return 0;
}

EXPORT(int, sceGxmTextureSetUAddrModeSafe, SceGxmTexture *texture, SceGxmTextureAddrMode mode) {
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (!verify_texture_mode(texture, mode))
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);

    texture->uaddr_mode = mode;
    return 0;
}

EXPORT(int, sceGxmTextureSetVAddrMode, SceGxmTexture *texture, SceGxmTextureAddrMode mode) {
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (!verify_texture_mode(texture, mode))
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);

    texture->vaddr_mode = mode;
    return 0;
}

EXPORT(int, sceGxmTextureSetVAddrModeSafe, SceGxmTexture *texture, SceGxmTextureAddrMode mode) {
    if (!texture)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    if (!verify_texture_mode(texture, mode))
        return RET_ERROR(SCE_GXM_ERROR_UNSUPPORTED);

    texture->vaddr_mode = mode;
    return 0;
}

EXPORT(int, sceGxmTextureSetWidth, SceGxmTexture *texture, uint32_t width) {
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
    }

    // TODO: Add support for swizzled textures
    LOG_WARN("Unimplemented texture format detected in sceGxmTextureSetWidth call.");

    return 0;
}

EXPORT(int, sceGxmTextureValidate, const SceGxmTexture *texture) {
    if (!texture) {
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
    if (!base) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    return 0;
}

EXPORT(int, sceGxmUnmapMemory, void *base) {
    if (!base) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    return 0;
}

EXPORT(int, sceGxmUnmapVertexUsseMemory, void *base) {
    if (!base) {
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);
    }

    return 0;
}

EXPORT(int, sceGxmVertexFence) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<const SceGxmProgram>, sceGxmVertexProgramGetProgram, const SceGxmVertexProgram *vertexProgram) {
    return vertexProgram->program;
}

EXPORT(int, sceGxmWaitEvent) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_sceGxmBeginScene)
BRIDGE_IMPL(_sceGxmMidSceneFlush)
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
