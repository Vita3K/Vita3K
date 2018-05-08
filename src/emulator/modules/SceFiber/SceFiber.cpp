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

#include "SceFiber.h"

EXPORT(int, _sceFiberAttachContextAndRun) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiberAttachContextAndSwitch) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiberInitializeImpl) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiberInitializeWithInternalOptionImpl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberFinalize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberGetInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberGetSelf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberOptParamInitialize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberPopUserMarkerWithHud) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberPushUserMarkerWithHud) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberRenameSelf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberReturnToThread) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberRun) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberStartContextSizeCheck) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberStopContextSizeCheck) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberSwitch) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_sceFiberAttachContextAndRun)
BRIDGE_IMPL(_sceFiberAttachContextAndSwitch)
BRIDGE_IMPL(_sceFiberInitializeImpl)
BRIDGE_IMPL(_sceFiberInitializeWithInternalOptionImpl)
BRIDGE_IMPL(sceFiberFinalize)
BRIDGE_IMPL(sceFiberGetInfo)
BRIDGE_IMPL(sceFiberGetSelf)
BRIDGE_IMPL(sceFiberOptParamInitialize)
BRIDGE_IMPL(sceFiberPopUserMarkerWithHud)
BRIDGE_IMPL(sceFiberPushUserMarkerWithHud)
BRIDGE_IMPL(sceFiberRenameSelf)
BRIDGE_IMPL(sceFiberReturnToThread)
BRIDGE_IMPL(sceFiberRun)
BRIDGE_IMPL(sceFiberStartContextSizeCheck)
BRIDGE_IMPL(sceFiberStopContextSizeCheck)
BRIDGE_IMPL(sceFiberSwitch)
