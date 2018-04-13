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

#include "SceDisplay.h"

#include <host/functions.h>

#include <psp2/display.h>

EXPORT(int, _sceDisplayGetFrameBuf) {
    return unimplemented("_sceDisplayGetFrameBuf");
}

EXPORT(int, _sceDisplayGetFrameBufInternal) {
    return unimplemented("_sceDisplayGetFrameBufInternal");
}

EXPORT(int, _sceDisplayGetMaximumFrameBufResolution) {
    return unimplemented("_sceDisplayGetMaximumFrameBufResolution");
}

EXPORT(int, _sceDisplayGetResolutionInfoInternal) {
    return unimplemented("_sceDisplayGetResolutionInfoInternal");
}

EXPORT(int, _sceDisplaySetFrameBuf) {
    return unimplemented("_sceDisplaySetFrameBuf");
}

EXPORT(int, _sceDisplaySetFrameBufForCompat) {
    return unimplemented("_sceDisplaySetFrameBufForCompat");
}

EXPORT(int, _sceDisplaySetFrameBufInternal) {
    return unimplemented("_sceDisplaySetFrameBufInternal");
}

EXPORT(int, sceDisplayGetPrimaryHead) {
    return unimplemented("sceDisplayGetPrimaryHead");
}

EXPORT(int, sceDisplayGetRefreshRate) {
    return unimplemented("sceDisplayGetRefreshRate");
}

EXPORT(int, sceDisplayGetVcount) {
    return unimplemented("sceDisplayGetVcount");
}

EXPORT(int, sceDisplayGetVcountInternal) {
    return unimplemented("sceDisplayGetVcountInternal");
}

EXPORT(int, sceDisplayRegisterVblankStartCallback) {
    return unimplemented("sceDisplayRegisterVblankStartCallback");
}

EXPORT(int, sceDisplayUnregisterVblankStartCallback) {
    return unimplemented("sceDisplayUnregisterVblankStartCallback");
}

EXPORT(int, sceDisplayWaitSetFrameBuf) {
    return unimplemented("sceDisplayWaitSetFrameBuf");
}

EXPORT(int, sceDisplayWaitSetFrameBufCB) {
    return unimplemented("sceDisplayWaitSetFrameBufCB");
}

EXPORT(int, sceDisplayWaitSetFrameBufMulti) {
    return unimplemented("sceDisplayWaitSetFrameBufMulti");
}

EXPORT(int, sceDisplayWaitSetFrameBufMultiCB) {
    return unimplemented("sceDisplayWaitSetFrameBufMultiCB");
}

EXPORT(int, sceDisplayWaitVblankStart) {
    {
        std::unique_lock<std::mutex> lock(host.display.mutex);
        host.display.condvar.wait(lock);
        if (host.display.abort.load()) {
            lock.release();
        }
    }
    return SCE_DISPLAY_ERROR_OK;
}

EXPORT(int, sceDisplayWaitVblankStartCB) {
    return unimplemented("sceDisplayWaitVblankStartCB");
}

EXPORT(int, sceDisplayWaitVblankStartMulti) {
    return unimplemented("sceDisplayWaitVblankStartMulti");
}

EXPORT(int, sceDisplayWaitVblankStartMultiCB) {
    return unimplemented("sceDisplayWaitVblankStartMultiCB");
}

BRIDGE_IMPL(_sceDisplayGetFrameBuf)
BRIDGE_IMPL(_sceDisplayGetFrameBufInternal)
BRIDGE_IMPL(_sceDisplayGetMaximumFrameBufResolution)
BRIDGE_IMPL(_sceDisplayGetResolutionInfoInternal)
BRIDGE_IMPL(_sceDisplaySetFrameBuf)
BRIDGE_IMPL(_sceDisplaySetFrameBufForCompat)
BRIDGE_IMPL(_sceDisplaySetFrameBufInternal)
BRIDGE_IMPL(sceDisplayGetPrimaryHead)
BRIDGE_IMPL(sceDisplayGetRefreshRate)
BRIDGE_IMPL(sceDisplayGetVcount)
BRIDGE_IMPL(sceDisplayGetVcountInternal)
BRIDGE_IMPL(sceDisplayRegisterVblankStartCallback)
BRIDGE_IMPL(sceDisplayUnregisterVblankStartCallback)
BRIDGE_IMPL(sceDisplayWaitSetFrameBuf)
BRIDGE_IMPL(sceDisplayWaitSetFrameBufCB)
BRIDGE_IMPL(sceDisplayWaitSetFrameBufMulti)
BRIDGE_IMPL(sceDisplayWaitSetFrameBufMultiCB)
BRIDGE_IMPL(sceDisplayWaitVblankStart)
BRIDGE_IMPL(sceDisplayWaitVblankStartCB)
BRIDGE_IMPL(sceDisplayWaitVblankStartMulti)
BRIDGE_IMPL(sceDisplayWaitVblankStartMultiCB)
