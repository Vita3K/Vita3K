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
TRACY_MODULE_NAME(SceGxmInternalForVsh);

EXPORT(int, sceGxmVshInitialize, SceGxmInitializeParams *params) {
    TRACY_FUNC(sceGxmVshInitialize, params);

    STUBBED("sceGxmInitialize");
    // the flags for sceGxmVshInitialize are not the same compared to sceGxmInitialize
    *reinterpret_cast<uint32_t *>(params) = 0;
    return CALL_EXPORT(sceGxmInitialize, params);
}

EXPORT(int, sceGxmVshSyncObjectClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmVshSyncObjectCreate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmVshSyncObjectDestroy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceGxmVshSyncObjectOpen) {
    return UNIMPLEMENTED();
}
