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

#include "SceFios2KernelForDriver.h"

EXPORT(int, ksceFiosKernelOverlayAdd) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceFiosKernelOverlayAddForProcess) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceFiosKernelOverlayGetInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceFiosKernelOverlayGetInfoForProcess) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceFiosKernelOverlayGetList) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceFiosKernelOverlayGetRecommendedScheduler) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceFiosKernelOverlayModify) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceFiosKernelOverlayModifyForProcess) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceFiosKernelOverlayRemove) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceFiosKernelOverlayRemoveForProcess) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceFiosKernelOverlayResolveSync) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceFiosKernelOverlayResolveWithRangeSync) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceFiosKernelOverlayThreadIsDisabled) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceFiosKernelOverlayThreadSetDisabled) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(ksceFiosKernelOverlayAdd)
BRIDGE_IMPL(ksceFiosKernelOverlayAddForProcess)
BRIDGE_IMPL(ksceFiosKernelOverlayGetInfo)
BRIDGE_IMPL(ksceFiosKernelOverlayGetInfoForProcess)
BRIDGE_IMPL(ksceFiosKernelOverlayGetList)
BRIDGE_IMPL(ksceFiosKernelOverlayGetRecommendedScheduler)
BRIDGE_IMPL(ksceFiosKernelOverlayModify)
BRIDGE_IMPL(ksceFiosKernelOverlayModifyForProcess)
BRIDGE_IMPL(ksceFiosKernelOverlayRemove)
BRIDGE_IMPL(ksceFiosKernelOverlayRemoveForProcess)
BRIDGE_IMPL(ksceFiosKernelOverlayResolveSync)
BRIDGE_IMPL(ksceFiosKernelOverlayResolveWithRangeSync)
BRIDGE_IMPL(ksceFiosKernelOverlayThreadIsDisabled)
BRIDGE_IMPL(ksceFiosKernelOverlayThreadSetDisabled)
