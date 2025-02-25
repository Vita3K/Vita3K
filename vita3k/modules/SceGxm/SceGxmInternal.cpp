// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <module/module.h>

#include "SceGxm.h"

#include <util/tracy.h>

TRACY_MODULE_NAME(SceGxmInternal);

EXPORT(int, sceGxmCheckMemoryInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmCreateRenderTargetInternal, const SceGxmRenderTargetParams *params, Ptr<SceGxmRenderTarget> *renderTarget) {
    TRACY_FUNC(sceGxmCreateRenderTargetInternal, params, renderTarget);
    return CALL_EXPORT(sceGxmCreateRenderTarget, params, renderTarget);
}

EXPORT(int, sceGxmGetDisplayQueueThreadIdInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmGetRenderTargetMemSizeInternal, const SceGxmRenderTargetParams *params, uint32_t *hostMemSize) {
    TRACY_FUNC(sceGxmGetRenderTargetMemSizeInternal, params, hostMemSize);
    return CALL_EXPORT(sceGxmGetRenderTargetMemSize, params, hostMemSize);
}

EXPORT(int, sceGxmGetTopContextInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmInitializedInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmIsInitializationInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmMapFragmentUsseMemoryInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmMapVertexUsseMemoryInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmRenderingContextIsWithinSceneInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetCallbackInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmSetInitializeParamInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmUnmapFragmentUsseMemoryInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmUnmapVertexUsseMemoryInternal) {
    return UNIMPLEMENTED();
}
