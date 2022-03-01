// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include "SceSblPmMgr.h"

EXPORT(int, sceSblPmMgrAuthEtoI) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblPmMgrGetCurrentMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblPmMgrGetProductModeForUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblPmMgrGetProductModeFromNVS) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblPmMgrSetProductModeOffForUser) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceSblPmMgrAuthEtoI)
BRIDGE_IMPL(sceSblPmMgrGetCurrentMode)
BRIDGE_IMPL(sceSblPmMgrGetProductModeForUser)
BRIDGE_IMPL(sceSblPmMgrGetProductModeFromNVS)
BRIDGE_IMPL(sceSblPmMgrSetProductModeOffForUser)
