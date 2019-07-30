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

#pragma once

#include <module/module.h>

BRIDGE_DECL(_sceFiberAttachContextAndRun)
BRIDGE_DECL(_sceFiberAttachContextAndSwitch)
BRIDGE_DECL(_sceFiberInitializeImpl)
BRIDGE_DECL(_sceFiberInitializeWithInternalOptionImpl)
BRIDGE_DECL(sceFiberFinalize)
BRIDGE_DECL(sceFiberGetInfo)
BRIDGE_DECL(sceFiberGetSelf)
BRIDGE_DECL(sceFiberOptParamInitialize)
BRIDGE_DECL(sceFiberPopUserMarkerWithHud)
BRIDGE_DECL(sceFiberPushUserMarkerWithHud)
BRIDGE_DECL(sceFiberRenameSelf)
BRIDGE_DECL(sceFiberReturnToThread)
BRIDGE_DECL(sceFiberRun)
BRIDGE_DECL(sceFiberStartContextSizeCheck)
BRIDGE_DECL(sceFiberStopContextSizeCheck)
BRIDGE_DECL(sceFiberSwitch)
