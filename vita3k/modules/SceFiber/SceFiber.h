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

enum SceFiberErrorCode {
    SCE_FIBER_OK = 0x00000000, //!< Success
    SCE_FIBER_ERROR_NULL = 0x80590001, //!< Some parameters are NULL.
    SCE_FIBER_ERROR_ALIGNMENT = 0x80590002, //!< Some pointer-parameters are not aligned in their proper alignments.
    SCE_FIBER_ERROR_RANGE = 0x80590003, //!< A parameter exceeds its range in the specification.
    SCE_FIBER_ERROR_INVALID = 0x80590004, //!< A parameter has an invalid value.
    SCE_FIBER_ERROR_PERMISSION = 0x80590005, //!< The function was called from the entity which does not have the permission.
    SCE_FIBER_ERROR_STATE = 0x80590006, //!< The function was applied to a fiber in the state which the function does not support.
    SCE_FIBER_ERROR_BUSY = 0x80590007, //!< The module specified by the function is busy.
    SCE_FIBER_ERROR_AGAIN = 0x80590008, //!< The function could not complete because of the situation. Please try again later.
    SCE_FIBER_ERROR_FATAL = 0x80590009, //!< The fiber caused an unrecoverable error.
};

typedef void(SceFiberEntry)(SceUInt32 argOnInitialize, SceUInt32 argOnRun);

struct SceFiberOptParam {
    char reserved[128];
};

typedef struct SceFiber {
    Ptr<SceFiberEntry> entry;
    SceUInt32 argOnInitialize;
    Address addrContext;
    SceSize sizeContext;
    char name[32];
    CPUContext *cpu;
} SceFiber;

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
