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
    return unimplemented(export_name);
}

EXPORT(int, _sceFiberAttachContextAndSwitch) {
    return unimplemented(export_name);
}

EXPORT(int, _sceFiberInitializeImpl) {
    return unimplemented(export_name);
}

EXPORT(int, _sceFiberInitializeWithInternalOptionImpl) {
    return unimplemented(export_name);
}

EXPORT(int, sceFiberFinalize) {
    return unimplemented(export_name);
}

EXPORT(int, sceFiberGetInfo) {
    return unimplemented(export_name);
}

EXPORT(int, sceFiberGetSelf) {
    return unimplemented(export_name);
}

EXPORT(int, sceFiberOptParamInitialize) {
    return unimplemented(export_name);
}

EXPORT(int, sceFiberPopUserMarkerWithHud) {
    return unimplemented(export_name);
}

EXPORT(int, sceFiberPushUserMarkerWithHud) {
    return unimplemented(export_name);
}

EXPORT(int, sceFiberRenameSelf) {
    return unimplemented(export_name);
}

EXPORT(int, sceFiberReturnToThread) {
    return unimplemented(export_name);
}

EXPORT(int, sceFiberRun) {
    return unimplemented(export_name);
}

EXPORT(int, sceFiberStartContextSizeCheck) {
    return unimplemented(export_name);
}

EXPORT(int, sceFiberStopContextSizeCheck) {
    return unimplemented(export_name);
}

EXPORT(int, sceFiberSwitch) {
    return unimplemented(export_name);
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
