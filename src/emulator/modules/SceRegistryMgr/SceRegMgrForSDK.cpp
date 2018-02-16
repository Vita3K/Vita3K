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

#include "SceRegMgrForSDK.h"

EXPORT(int, sceRegMgrUtilityGetBin) {
    return unimplemented("sceRegMgrUtilityGetBin");
}

EXPORT(int, sceRegMgrUtilityGetInt) {
    return unimplemented("sceRegMgrUtilityGetInt");
}

EXPORT(int, sceRegMgrUtilityGetStr) {
    return unimplemented("sceRegMgrUtilityGetStr");
}

EXPORT(int, sceRegMgrUtilitySetBin) {
    return unimplemented("sceRegMgrUtilitySetBin");
}

EXPORT(int, sceRegMgrUtilitySetInt) {
    return unimplemented("sceRegMgrUtilitySetInt");
}

EXPORT(int, sceRegMgrUtilitySetStr) {
    return unimplemented("sceRegMgrUtilitySetStr");
}

BRIDGE_IMPL(sceRegMgrUtilityGetBin)
BRIDGE_IMPL(sceRegMgrUtilityGetInt)
BRIDGE_IMPL(sceRegMgrUtilityGetStr)
BRIDGE_IMPL(sceRegMgrUtilitySetBin)
BRIDGE_IMPL(sceRegMgrUtilitySetInt)
BRIDGE_IMPL(sceRegMgrUtilitySetStr)
