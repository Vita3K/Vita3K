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

#include "SceWlan.h"

EXPORT(int, sceWlanGetConfiguration) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceWlanGetWakeOnTargetProcess) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceWlanRegisterCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceWlanSetConfiguration) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceWlanUnregisterCallback) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceWlanGetConfiguration)
BRIDGE_IMPL(sceWlanGetWakeOnTargetProcess)
BRIDGE_IMPL(sceWlanRegisterCallback)
BRIDGE_IMPL(sceWlanSetConfiguration)
BRIDGE_IMPL(sceWlanUnregisterCallback)
