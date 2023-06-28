// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include "SceCtrl.h"

#include <ctrl/ctrl.h>
#include <ctrl/functions.h>
#include <kernel/types.h>
#include <util/log.h>
#include <util/tracy.h>

#include <algorithm>
#include <array>

TRACY_MODULE_NAME(SceCtrl);

template <>
std::string to_debug_str<SceCtrlPadInputMode>(const MemState &mem, SceCtrlPadInputMode mode) {
    switch (mode) {
    case SCE_CTRL_MODE_DIGITAL:
        return "SCE_CTRL_MODE_DIGITAL";
    case SCE_CTRL_MODE_ANALOG:
        return "SCE_CTRL_MODE_ANALOG";
    case SCE_CTRL_MODE_ANALOG_WIDE:
        return "SCE_CTRL_MODE_ANALOG_WIDE";
    }
    return std::to_string(mode);
}

EXPORT(int, sceCtrlClearRapidFire) {
    TRACY_FUNC(sceCtrlClearRapidFire);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlDisconnect) {
    TRACY_FUNC(sceCtrlDisconnect);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetAnalogStickCheckMode) {
    TRACY_FUNC(sceCtrlGetAnalogStickCheckMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetAnalogStickCheckTarget) {
    TRACY_FUNC(sceCtrlGetAnalogStickCheckTarget);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetBatteryInfo) {
    TRACY_FUNC(sceCtrlGetBatteryInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetButtonIntercept, int *intercept) {
    TRACY_FUNC(sceCtrlGetButtonIntercept);
    LOG_DEBUG("intercept: {}", *intercept);
    *intercept = 1;
    return 0;
}

EXPORT(int, sceCtrlGetControllerPortInfo, SceCtrlPortInfo *info) {
    TRACY_FUNC(sceCtrlGetControllerPortInfo, info);
    CtrlState &state = emuenv.ctrl;
    info->port[0] = emuenv.cfg.current_config.pstv_mode ? SCE_CTRL_TYPE_VIRT : SCE_CTRL_TYPE_PHY;
    for (int i = 0; i < SCE_CTRL_MAX_WIRELESS_NUM; i++) {
        info->port[i + 1] = (emuenv.cfg.current_config.pstv_mode && !emuenv.ctrl.free_ports[i]) ? get_type_of_controller(i) : SCE_CTRL_TYPE_UNPAIRED;
    }
    return 0;
}

EXPORT(int, sceCtrlGetProcessStatus) {
    TRACY_FUNC(sceCtrlGetProcessStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetSamplingMode, SceCtrlPadInputMode *mode) {
    TRACY_FUNC(sceCtrlGetSamplingMode, mode);
    if (mode == nullptr) {
        return RET_ERROR(SCE_CTRL_ERROR_INVALID_ARG);
    }
    *mode = emuenv.ctrl.input_mode;
    return SCE_KERNEL_OK;
}

EXPORT(int, sceCtrlGetSamplingModeExt, SceCtrlPadInputMode *mode) {
    TRACY_FUNC(sceCtrlGetSamplingModeExt, mode);
    if (mode == nullptr) {
        return RET_ERROR(SCE_CTRL_ERROR_INVALID_ARG);
    }
    *mode = emuenv.ctrl.input_mode_ext;
    return SCE_KERNEL_OK;
}

EXPORT(int, sceCtrlGetWirelessControllerInfo, SceCtrlWirelessControllerInfo *pInfo) {
    TRACY_FUNC(sceCtrlGetWirelessControllerInfo, pInfo);
    memset(pInfo->connected, SCE_CTRL_WIRELESS_INFO_NOT_CONNECTED, sizeof(pInfo->connected));
    if (emuenv.cfg.current_config.pstv_mode) {
        CtrlState &state = emuenv.ctrl;
        for (auto i = 0; i < state.controllers_num; i++)
            pInfo->connected[i] = SCE_CTRL_WIRELESS_INFO_CONNECTED;
    }

    return 0;
}

EXPORT(bool, sceCtrlIsMultiControllerSupported) {
    TRACY_FUNC(sceCtrlIsMultiControllerSupported);
    return emuenv.cfg.current_config.pstv_mode;
}

EXPORT(int, sceCtrlPeekBufferNegative, int port, SceCtrlData *pad_data, int count) {
    TRACY_FUNC(sceCtrlPeekBufferNegative, port, pad_data, count);
    return ctrl_get(thread_id, emuenv, port, reinterpret_cast<SceCtrlData2 *>(pad_data), count, true, true, false, false);
}

EXPORT(int, sceCtrlPeekBufferNegative2, int port, SceCtrlData2 *pad_data, int count) {
    TRACY_FUNC(sceCtrlPeekBufferNegative2, port, pad_data, count);
    return ctrl_get(thread_id, emuenv, port, pad_data, count, true, true, true, false);
}

EXPORT(int, sceCtrlPeekBufferPositive, int port, SceCtrlData *pad_data, int count) {
    TRACY_FUNC(sceCtrlPeekBufferPositive, port, pad_data, count);
    return ctrl_get(thread_id, emuenv, port, reinterpret_cast<SceCtrlData2 *>(pad_data), count, false, true, false, false);
}

EXPORT(int, sceCtrlPeekBufferPositive2, int port, SceCtrlData2 *pad_data, int count) {
    TRACY_FUNC(sceCtrlPeekBufferPositive2, port, pad_data, count);
    return ctrl_get(thread_id, emuenv, port, pad_data, count, false, true, true, false);
}

EXPORT(int, sceCtrlPeekBufferPositiveExt, int port, SceCtrlData *pad_data, int count) {
    TRACY_FUNC(sceCtrlPeekBufferPositiveExt, port, pad_data, count);
    return ctrl_get(thread_id, emuenv, port, reinterpret_cast<SceCtrlData2 *>(pad_data), count, false, true, false, true);
}

EXPORT(int, sceCtrlPeekBufferPositiveExt2, int port, SceCtrlData2 *pad_data, int count) {
    TRACY_FUNC(sceCtrlPeekBufferPositiveExt2, port, pad_data, count);
    return ctrl_get(thread_id, emuenv, port, pad_data, count, false, true, true, true);
}

EXPORT(int, sceCtrlReadBufferNegative, int port, SceCtrlData *pad_data, int count) {
    TRACY_FUNC(sceCtrlReadBufferNegative, port, pad_data, count);
    return ctrl_get(thread_id, emuenv, port, reinterpret_cast<SceCtrlData2 *>(pad_data), count, true, false, false, false);
}

EXPORT(int, sceCtrlReadBufferNegative2, int port, SceCtrlData2 *pad_data, int count) {
    TRACY_FUNC(sceCtrlReadBufferNegative2, port, pad_data, count);
    return ctrl_get(thread_id, emuenv, port, pad_data, count, true, false, true, false);
}

EXPORT(int, sceCtrlReadBufferPositive, int port, SceCtrlData *pad_data, int count) {
    TRACY_FUNC(sceCtrlReadBufferPositive, port, pad_data, count);
    return ctrl_get(thread_id, emuenv, port, reinterpret_cast<SceCtrlData2 *>(pad_data), count, false, false, false, false);
}

EXPORT(int, sceCtrlReadBufferPositive2, int port, SceCtrlData2 *pad_data, int count) {
    TRACY_FUNC(sceCtrlReadBufferPositive2, port, pad_data, count);
    return ctrl_get(thread_id, emuenv, port, pad_data, count, false, false, true, false);
}

EXPORT(int, sceCtrlReadBufferPositiveExt, int port, SceCtrlData *pad_data, int count) {
    TRACY_FUNC(sceCtrlReadBufferPositiveExt, port, pad_data, count);
    return ctrl_get(thread_id, emuenv, port, reinterpret_cast<SceCtrlData2 *>(pad_data), count, false, false, false, true);
}

EXPORT(int, sceCtrlReadBufferPositiveExt2, int port, SceCtrlData2 *pad_data, int count) {
    TRACY_FUNC(sceCtrlReadBufferPositiveExt2, port, pad_data, count);
    return ctrl_get(thread_id, emuenv, port, pad_data, count, false, false, true, true);
}

EXPORT(int, sceCtrlRegisterBdRMCCallback) {
    TRACY_FUNC(sceCtrlRegisterBdRMCCallback);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlResetLightBar) {
    TRACY_FUNC(sceCtrlResetLightBar);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetActuator, int port, const SceCtrlActuator *pState) {
    TRACY_FUNC(sceCtrlSetActuator, port, pState);
    if (!emuenv.cfg.current_config.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NOT_SUPPORTED);
    }

    CtrlState &state = emuenv.ctrl;
    for (const auto &controller : state.controllers) {
        if (controller.second.port == port) {
            SDL_GameControllerRumble(controller.second.controller.get(), pState->small * 655.35f, pState->large * 655.35f, SDL_HAPTIC_INFINITY);

            return 0;
        }
    }

    return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
}

EXPORT(int, sceCtrlSetAnalogStickCheckMode) {
    TRACY_FUNC(sceCtrlSetAnalogStickCheckMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetAnalogStickCheckTarget) {
    TRACY_FUNC(sceCtrlSetAnalogStickCheckTarget);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetButtonIntercept, int intercept) {
    TRACY_FUNC(sceCtrlSetButtonIntercept);
    LOG_DEBUG("intercept: {}", intercept);
    return 0;
}

EXPORT(int, sceCtrlSetButtonRemappingInfo) {
    TRACY_FUNC(sceCtrlSetButtonRemappingInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetLightBar) {
    TRACY_FUNC(sceCtrlSetLightBar);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetRapidFire) {
    TRACY_FUNC(sceCtrlSetRapidFire);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetSamplingMode, SceCtrlPadInputMode mode) {
    TRACY_FUNC(sceCtrlSetSamplingMode, mode);
    SceCtrlPadInputMode old = emuenv.ctrl.input_mode;
    if (mode < SCE_CTRL_MODE_DIGITAL || mode > SCE_CTRL_MODE_ANALOG_WIDE) {
        return RET_ERROR(SCE_CTRL_ERROR_INVALID_ARG);
    }
    emuenv.ctrl.input_mode = mode;
    return old;
}

EXPORT(int, sceCtrlSetSamplingModeExt, SceCtrlPadInputMode mode) {
    TRACY_FUNC(sceCtrlSetSamplingModeExt, mode);
    SceCtrlPadInputMode old = emuenv.ctrl.input_mode_ext;
    if (mode < SCE_CTRL_MODE_DIGITAL || mode > SCE_CTRL_MODE_ANALOG_WIDE) {
        return RET_ERROR(SCE_CTRL_ERROR_INVALID_ARG);
    }
    emuenv.ctrl.input_mode_ext = mode;
    return old;
}

EXPORT(int, sceCtrlSingleControllerMode) {
    TRACY_FUNC(sceCtrlSingleControllerMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlUnregisterBdRMCCallback) {
    TRACY_FUNC(sceCtrlUnregisterBdRMCCallback);
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceCtrlClearRapidFire)
BRIDGE_IMPL(sceCtrlDisconnect)
BRIDGE_IMPL(sceCtrlGetAnalogStickCheckMode)
BRIDGE_IMPL(sceCtrlGetAnalogStickCheckTarget)
BRIDGE_IMPL(sceCtrlGetBatteryInfo)
BRIDGE_IMPL(sceCtrlGetButtonIntercept)
BRIDGE_IMPL(sceCtrlGetControllerPortInfo)
BRIDGE_IMPL(sceCtrlGetProcessStatus)
BRIDGE_IMPL(sceCtrlGetSamplingMode)
BRIDGE_IMPL(sceCtrlGetSamplingModeExt)
BRIDGE_IMPL(sceCtrlGetWirelessControllerInfo)
BRIDGE_IMPL(sceCtrlIsMultiControllerSupported)
BRIDGE_IMPL(sceCtrlPeekBufferNegative)
BRIDGE_IMPL(sceCtrlPeekBufferNegative2)
BRIDGE_IMPL(sceCtrlPeekBufferPositive)
BRIDGE_IMPL(sceCtrlPeekBufferPositive2)
BRIDGE_IMPL(sceCtrlPeekBufferPositiveExt)
BRIDGE_IMPL(sceCtrlPeekBufferPositiveExt2)
BRIDGE_IMPL(sceCtrlReadBufferNegative)
BRIDGE_IMPL(sceCtrlReadBufferNegative2)
BRIDGE_IMPL(sceCtrlReadBufferPositive)
BRIDGE_IMPL(sceCtrlReadBufferPositive2)
BRIDGE_IMPL(sceCtrlReadBufferPositiveExt)
BRIDGE_IMPL(sceCtrlReadBufferPositiveExt2)
BRIDGE_IMPL(sceCtrlRegisterBdRMCCallback)
BRIDGE_IMPL(sceCtrlResetLightBar)
BRIDGE_IMPL(sceCtrlSetActuator)
BRIDGE_IMPL(sceCtrlSetAnalogStickCheckMode)
BRIDGE_IMPL(sceCtrlSetAnalogStickCheckTarget)
BRIDGE_IMPL(sceCtrlSetButtonIntercept)
BRIDGE_IMPL(sceCtrlSetButtonRemappingInfo)
BRIDGE_IMPL(sceCtrlSetLightBar)
BRIDGE_IMPL(sceCtrlSetRapidFire)
BRIDGE_IMPL(sceCtrlSetSamplingMode)
BRIDGE_IMPL(sceCtrlSetSamplingModeExt)
BRIDGE_IMPL(sceCtrlSingleControllerMode)
BRIDGE_IMPL(sceCtrlUnregisterBdRMCCallback)
