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

#include "SceTouch.h"

#include <touch/touch.h>

EXPORT(int, sceTouchActivateRegion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchClearRegion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchDisableTouchForce) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchDisableTouchForceExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchEnableIdleTimerCancelSetting) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchEnableTouchForce) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchEnableTouchForceExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetDeviceInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetPanelInfo, SceUInt32 port, SceTouchPanelInfo *pPanelInfo) {
    switch (port) {
    case SCE_TOUCH_PORT_FRONT:
        // Active Area
        pPanelInfo->minAaX = 0;
        pPanelInfo->minAaY = 0;
        pPanelInfo->maxAaX = 1919;
        pPanelInfo->maxAaY = 1087;

        // Display
        pPanelInfo->minDispX = 0;
        pPanelInfo->minDispY = 0;
        pPanelInfo->maxDispX = 1919;
        pPanelInfo->maxDispY = 1087;

        // Force
        pPanelInfo->minForce = 1;
        pPanelInfo->maxForce = 128;

        return 0;
    case SCE_TOUCH_PORT_BACK:
        // Active Area
        pPanelInfo->minAaX = 0;
        pPanelInfo->minAaY = 108;
        pPanelInfo->maxAaX = 1919;
        pPanelInfo->maxAaY = 889;

        // Display
        pPanelInfo->minDispX = 0;
        pPanelInfo->minDispY = 0;
        pPanelInfo->maxDispX = 1919;
        pPanelInfo->maxDispY = 1087;

        // Force
        pPanelInfo->minForce = 1;
        pPanelInfo->maxForce = 128;

        return 0;
    default:
        return SCE_TOUCH_ERROR_INVALID_ARG;
    }
}

EXPORT(int, sceTouchGetPixelDensity) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetPixelDensity2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetProcessInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetSamplingState, SceUInt32 port, SceTouchSamplingState *pState) {
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pState == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    *pState = host.ctrl.touch_mode[port];
    return 0;
}

EXPORT(int, sceTouchGetSamplingStateExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchPeek, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pData == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

    return peek_touch(host.viewport_pos, host.viewport_size, host.drawable_size, host.gui, host.ctrl, port, pData, nBufs);
}

EXPORT(int, sceTouchPeek2, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pData == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

    return peek_touch(host.viewport_pos, host.viewport_size, host.drawable_size, host.gui, host.ctrl, port, pData, nBufs);
}

EXPORT(int, sceTouchPeekRegion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchPeekRegionExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchRead, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pData == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

    return peek_touch(host.viewport_pos, host.viewport_size, host.drawable_size, host.gui, host.ctrl, port, pData, nBufs);
}

EXPORT(int, sceTouchRead2, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pData == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

    return peek_touch(host.viewport_pos, host.viewport_size, host.drawable_size, host.gui, host.ctrl, port, pData, nBufs);
}

EXPORT(int, sceTouchReadRegion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchReadRegionExt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchSetProcessPrivilege) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchSetRegion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchSetRegionAttr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchSetSamplingState, SceUInt32 port, SceTouchSamplingState state) {
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (state != SCE_TOUCH_SAMPLING_STATE_STOP && state != SCE_TOUCH_SAMPLING_STATE_START) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    host.ctrl.touch_mode[port] = state;
    return 0;
}

EXPORT(int, sceTouchSetSamplingStateExt) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceTouchActivateRegion)
BRIDGE_IMPL(sceTouchClearRegion)
BRIDGE_IMPL(sceTouchDisableTouchForce)
BRIDGE_IMPL(sceTouchDisableTouchForceExt)
BRIDGE_IMPL(sceTouchEnableIdleTimerCancelSetting)
BRIDGE_IMPL(sceTouchEnableTouchForce)
BRIDGE_IMPL(sceTouchEnableTouchForceExt)
BRIDGE_IMPL(sceTouchGetDeviceInfo)
BRIDGE_IMPL(sceTouchGetPanelInfo)
BRIDGE_IMPL(sceTouchGetPixelDensity)
BRIDGE_IMPL(sceTouchGetPixelDensity2)
BRIDGE_IMPL(sceTouchGetProcessInfo)
BRIDGE_IMPL(sceTouchGetSamplingState)
BRIDGE_IMPL(sceTouchGetSamplingStateExt)
BRIDGE_IMPL(sceTouchPeek)
BRIDGE_IMPL(sceTouchPeek2)
BRIDGE_IMPL(sceTouchPeekRegion)
BRIDGE_IMPL(sceTouchPeekRegionExt)
BRIDGE_IMPL(sceTouchRead)
BRIDGE_IMPL(sceTouchRead2)
BRIDGE_IMPL(sceTouchReadRegion)
BRIDGE_IMPL(sceTouchReadRegionExt)
BRIDGE_IMPL(sceTouchSetProcessPrivilege)
BRIDGE_IMPL(sceTouchSetRegion)
BRIDGE_IMPL(sceTouchSetRegionAttr)
BRIDGE_IMPL(sceTouchSetSamplingState)
BRIDGE_IMPL(sceTouchSetSamplingStateExt)
