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

#include <SceDisplay/exports.h>

#include <host/version.h>

#include <SDL_timer.h>
#include <SDL_video.h>
#include <psp2/display.h>

#include <sstream>

static bool SAVE_SURFACE_IMAGES = false;

namespace emu {
    struct SceDisplayFrameBuf {
        uint32_t size = 0;
        Ptr<const void> base;
        uint32_t pitch = 0;
        uint32_t pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
        uint32_t width = 0;
        uint32_t height = 0;
    };
}

using namespace emu;

EXPORT(int, sceDisplayGetFrameBuf) {
    return unimplemented("sceDisplayGetFrameBuf");
}

EXPORT(int, sceDisplayGetFrameBufInternal) {
    return unimplemented("sceDisplayGetFrameBufInternal");
}

EXPORT(int, sceDisplayGetMaximumFrameBufResolution) {
    return unimplemented("sceDisplayGetMaximumFrameBufResolution");
}

EXPORT(int, sceDisplayGetResolutionInfoInternal) {
    return unimplemented("sceDisplayGetResolutionInfoInternal");
}

EXPORT(int, sceDisplaySetFrameBuf, const emu::SceDisplayFrameBuf *pParam, SceDisplaySetBufSync sync) {
    typedef std::unique_ptr<SDL_Surface, void (*)(SDL_Surface *)> SurfacePtr;

    const MemState &mem = host.mem;
    assert(pParam != nullptr); // Todo: pParam can be NULL, in that case black screen is shown
    if (pParam->size != sizeof(emu::SceDisplayFrameBuf)){
        return error("sceDisplaySetFrameBuf", SCE_DISPLAY_ERROR_INVALID_VALUE);
    }
    if (!pParam->base){
        return error("sceDisplaySetFrameBuf", SCE_DISPLAY_ERROR_INVALID_ADDR);
    }
    if (pParam->pitch < pParam->width){
        return error("sceDisplaySetFrameBuf", SCE_DISPLAY_ERROR_INVALID_PITCH);
    }
    if (pParam->pixelformat != SCE_DISPLAY_PIXELFORMAT_A8B8G8R8){
        return error("sceDisplaySetFrameBuf", SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT);
    }
    if (sync != SCE_DISPLAY_SETBUF_NEXTFRAME && sync != SCE_DISPLAY_SETBUF_IMMEDIATE){
        return error("sceDisplaySetFrameBuf", SCE_DISPLAY_ERROR_INVALID_UPDATETIMING);
    }

    SDL_Window *const prev_gl_window = SDL_GL_GetCurrentWindow();
    const SDL_GLContext prev_gl_context = SDL_GL_GetCurrentContext();

    // TODO SLOW
    void *const pixels = pParam->base.cast<void>().get(mem);
    const SurfacePtr framebuffer_surface(SDL_CreateRGBSurfaceFrom(pixels, pParam->width, pParam->height, 32, pParam->pitch * 4, 0xff << 0, 0xff << 8, 0xff << 16, 0), SDL_FreeSurface);
    if (SAVE_SURFACE_IMAGES) {
        SDL_SaveBMP(framebuffer_surface.get(), (host.pref_path + "framebuffer.bmp").c_str());
    }
    SDL_Surface *const window_surface = SDL_GetWindowSurface(host.window.get());
    SDL_UpperBlit(framebuffer_surface.get(), nullptr, window_surface, nullptr);
    if (SAVE_SURFACE_IMAGES) {
        SDL_LockSurface(window_surface);
        SDL_SaveBMP(window_surface, (host.pref_path + "window.bmp").c_str());
        SDL_UnlockSurface(window_surface);
    }
    SDL_UpdateWindowSurface(host.window.get());

    SDL_GL_MakeCurrent(prev_gl_window, prev_gl_context);

    ++host.frame_count;
    const uint32_t t2 = SDL_GetTicks();
    const uint32_t ms = t2 - host.t1;
    if (ms >= 1000) {
        const uint32_t fps = (host.frame_count * 1000) / ms;
        const uint32_t ms_per_frame = ms / host.frame_count;
        std::ostringstream title;
        title << window_title << " - " << ms_per_frame << " ms/frame (" << fps << " frames/sec)";
        SDL_SetWindowTitle(host.window.get(), title.str().c_str());
        host.t1 = t2;
        host.frame_count = 0;
    }

    MicroProfileFlip(nullptr);

    return SCE_DISPLAY_ERROR_OK;
}

EXPORT(int, sceDisplaySetFrameBufForCompat) {
    return unimplemented("sceDisplaySetFrameBufForCompat");
}

EXPORT(int, sceDisplaySetFrameBufInternal) {
    return unimplemented("sceDisplaySetFrameBufInternal");
}

BRIDGE_IMPL(sceDisplayGetFrameBuf)
BRIDGE_IMPL(sceDisplayGetFrameBufInternal)
BRIDGE_IMPL(sceDisplayGetMaximumFrameBufResolution)
BRIDGE_IMPL(sceDisplayGetResolutionInfoInternal)
BRIDGE_IMPL(sceDisplaySetFrameBuf)
BRIDGE_IMPL(sceDisplaySetFrameBufForCompat)
BRIDGE_IMPL(sceDisplaySetFrameBufInternal)
