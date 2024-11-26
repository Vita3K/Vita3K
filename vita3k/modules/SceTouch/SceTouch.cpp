// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <module/module.h>

#include <touch/functions.h>
#include <touch/state.h>
#include <touch/touch.h>
#include <util/tracy.h>

TRACY_MODULE_NAME(SceTouch);

template <>
std::string to_debug_str<SceTouchSamplingState>(const MemState &mem, SceTouchSamplingState type) {
    switch (type) {
    case SCE_TOUCH_SAMPLING_STATE_STOP:
        return "SCE_TOUCH_SAMPLING_STATE_STOP";
    case SCE_TOUCH_SAMPLING_STATE_START:
        return "SCE_TOUCH_SAMPLING_STATE_START";
    }
    return std::to_string(type);
}

EXPORT(int, sceTouchActivateRegion) {
    TRACY_FUNC(sceTouchActivateRegion);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchClearRegion) {
    TRACY_FUNC(sceTouchClearRegion);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchDisableTouchForce, SceUInt32 port) {
    TRACY_FUNC(sceTouchDisableTouchForce, port);
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    touch_set_force_mode(port, false);
    return SCE_TOUCH_OK;
}

EXPORT(int, sceTouchDisableTouchForceExt) {
    TRACY_FUNC(sceTouchDisableTouchForceExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchEnableIdleTimerCancelSetting) {
    TRACY_FUNC(sceTouchEnableIdleTimerCancelSetting);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchEnableTouchForce, SceUInt32 port) {
    TRACY_FUNC(sceTouchEnableTouchForce, port);
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    touch_set_force_mode(port, true);
    return SCE_TOUCH_OK;
}

EXPORT(int, sceTouchEnableTouchForceExt) {
    TRACY_FUNC(sceTouchEnableTouchForceExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetDeviceInfo) {
    TRACY_FUNC(sceTouchGetDeviceInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetPanelInfo, SceUInt32 port, SceTouchPanelInfo *pPanelInfo) {
    TRACY_FUNC(sceTouchGetPanelInfo, port, pPanelInfo);
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pPanelInfo == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

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

    if (port == SCE_TOUCH_PORT_BACK) {
        pPanelInfo->minAaY = 108;
        pPanelInfo->maxAaY = 889;
    }

    return SCE_TOUCH_OK;
}

EXPORT(int, sceTouchGetPixelDensity, float *p1, float *p2) {
    TRACY_FUNC(sceTouchGetPixelDensity, p1, p2);
    if (p1 == nullptr || p2 == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

    // This function can return one of two set of values depending on the output of GetHardwareInfo NID 0xCBD6D8BC
    // for now this value is hardcoded until the function is implemented
    int hardware_info = 0x30;

    if (hardware_info == 0x30 || hardware_info == 0x31) {
        *p1 = 22.0f;
        *p2 = 22.0f;
    } else {
        *p1 = 17.54f;
        *p2 = 17.54f;
    }

    return SCE_TOUCH_OK;
}

EXPORT(int, sceTouchGetPixelDensity2, float *p1, float *p2, float *p3, float *p4) {
    TRACY_FUNC(sceTouchGetPixelDensity2, p1, p2, p3, p4);
    if (p1 == nullptr || p2 == nullptr || p3 == nullptr || p4 == nullptr) {
        return SCE_TOUCH_ERROR_INVALID_ARG;
    }

    // This function can return one of two set of values depending on the output of GetHardwareInfo NID 0xCBD6D8BC
    // for now this value is hardcoded until the function is implemented
    int hardware_info = 0x30;

    if (hardware_info == 0x30 || hardware_info == 0x31) {
        *p1 = 22.0f;
        *p2 = 22.0f;
        *p3 = 22.0f;
        *p4 = 22.0f;
    } else {
        *p1 = 17.54f;
        *p2 = 17.54f;
        *p3 = 17.54f;
        *p4 = 17.54f;
    }

    return SCE_TOUCH_OK;
}

EXPORT(int, sceTouchGetProcessInfo) {
    TRACY_FUNC(sceTouchGetProcessInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchGetSamplingState, SceUInt32 port, SceTouchSamplingState *pState) {
    TRACY_FUNC(sceTouchGetSamplingState, port, pState);
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pState == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    *pState = emuenv.touch.touch_mode[port];
    return SCE_TOUCH_OK;
}

EXPORT(int, sceTouchGetSamplingStateExt) {
    TRACY_FUNC(sceTouchGetSamplingStateExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchPeek, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    TRACY_FUNC(sceTouchPeek, port, pData, nBufs);
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pData == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (nBufs > MAX_TOUCH_BUFFER_SAVED) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

    return touch_get(thread_id, emuenv, port, pData, nBufs, true);
}

EXPORT(int, sceTouchPeek2, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    TRACY_FUNC(sceTouchPeek2, port, pData, nBufs);
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pData == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (nBufs > MAX_TOUCH_BUFFER_SAVED) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

    return touch_get(thread_id, emuenv, port, pData, nBufs, true);
}

EXPORT(int, sceTouchPeekRegion, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs, int region) {
    TRACY_FUNC(sceTouchPeekRegion, port, pData, nBufs, region);
    if (region > 0xFF) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    STUBBED("Ignore region");
    return CALL_EXPORT(sceTouchPeek, port, pData, nBufs);
}

EXPORT(int, sceTouchPeekRegionExt) {
    TRACY_FUNC(sceTouchPeekRegionExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchRead, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    TRACY_FUNC(sceTouchRead, port, pData, nBufs);
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pData == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (nBufs > MAX_TOUCH_BUFFER_SAVED) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

    return touch_get(thread_id, emuenv, port, pData, nBufs, false);
}

EXPORT(int, sceTouchRead2, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    TRACY_FUNC(sceTouchRead2, port, pData, nBufs);
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (pData == nullptr) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (nBufs > MAX_TOUCH_BUFFER_SAVED) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }

    return touch_get(thread_id, emuenv, port, pData, nBufs, false);
}

EXPORT(int, sceTouchReadRegion) {
    TRACY_FUNC(sceTouchReadRegion);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchReadRegionExt) {
    TRACY_FUNC(sceTouchReadRegionExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchSetProcessPrivilege) {
    TRACY_FUNC(sceTouchSetProcessPrivilege);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchSetRegion) {
    TRACY_FUNC(sceTouchSetRegion);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchSetRegionAttr) {
    TRACY_FUNC(sceTouchSetRegionAttr);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTouchSetSamplingState, SceUInt32 port, SceTouchSamplingState state) {
    TRACY_FUNC(sceTouchSetSamplingState, port, state);
    if (port >= SCE_TOUCH_PORT_MAX_NUM) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    if (state != SCE_TOUCH_SAMPLING_STATE_STOP && state != SCE_TOUCH_SAMPLING_STATE_START) {
        return RET_ERROR(SCE_TOUCH_ERROR_INVALID_ARG);
    }
    emuenv.touch.touch_mode[port] = state;
    return SCE_TOUCH_OK;
}

EXPORT(int, sceTouchSetSamplingStateExt) {
    TRACY_FUNC(sceTouchSetSamplingStateExt);
    return UNIMPLEMENTED();
}
