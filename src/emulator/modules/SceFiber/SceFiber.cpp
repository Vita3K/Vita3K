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
    return unimplemented("_sceFiberAttachContextAndRun");
}

EXPORT(int, _sceFiberAttachContextAndSwitch) {
    return unimplemented("_sceFiberAttachContextAndSwitch");
}

EXPORT(int, _sceFiberInitializeImpl) {
    return unimplemented("_sceFiberInitializeImpl");
}

EXPORT(int, _sceFiberInitializeWithInternalOptionImpl) {
    return unimplemented("_sceFiberInitializeWithInternalOptionImpl");
}

EXPORT(int, sceFiberFinalize) {
    return unimplemented("sceFiberFinalize");
}

EXPORT(int, sceFiberGetInfo) {
    return unimplemented("sceFiberGetInfo");
}

EXPORT(int, sceFiberGetSelf) {
    return unimplemented("sceFiberGetSelf");
}

EXPORT(int, sceFiberOptParamInitialize) {
    return unimplemented("sceFiberOptParamInitialize");
}

EXPORT(int, sceFiberPopUserMarkerWithHud) {
    return unimplemented("sceFiberPopUserMarkerWithHud");
}

EXPORT(int, sceFiberPushUserMarkerWithHud) {
    return unimplemented("sceFiberPushUserMarkerWithHud");
}

EXPORT(int, sceFiberRenameSelf) {
    return unimplemented("sceFiberRenameSelf");
}

EXPORT(int, sceFiberReturnToThread) {
    return unimplemented("sceFiberReturnToThread");
}

EXPORT(int, sceFiberRun) {
    return unimplemented("sceFiberRun");
}

EXPORT(int, sceFiberStartContextSizeCheck) {
    return unimplemented("sceFiberStartContextSizeCheck");
}

EXPORT(int, sceFiberStopContextSizeCheck) {
    return unimplemented("sceFiberStopContextSizeCheck");
}

EXPORT(int, sceFiberSwitch) {
    return unimplemented("sceFiberSwitch");
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
