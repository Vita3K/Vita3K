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

#include "SceCtrl.h"

#include <ctrl/functions.h>

#include <psp2/kernel/error.h>

#include <SDL_haptic.h>

EXPORT(int, sceCtrlClearRapidFire) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlDisconnect) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetAnalogStickCheckMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetAnalogStickCheckTarget) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetBatteryInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetButtonIntercept) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetControllerPortInfo, SceCtrlPortInfo *info) {
    remove_disconnected_controllers(host.ctrl);
    add_new_controllers(host.ctrl);
    info->port[0] = host.cfg.pstv_mode ? SCE_CTRL_TYPE_VIRT : SCE_CTRL_TYPE_PHY;
    for (int i = 0; i < 4; i++) {
        info->port[i + 1] = (host.cfg.pstv_mode && !host.ctrl.free_ports[i]) ? SCE_CTRL_TYPE_DS3 : SCE_CTRL_TYPE_UNPAIRED;
    }
    return 0;
}

EXPORT(int, sceCtrlGetProcessStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlGetSamplingMode, SceCtrlPadInputMode *mode) {
    if (mode == nullptr) {
        return RET_ERROR(SCE_CTRL_ERROR_INVALID_ARG);
    }
    *mode = host.ctrl.input_mode;
    return SCE_KERNEL_OK;
}

EXPORT(int, sceCtrlGetSamplingModeExt, SceCtrlPadInputMode *mode) {
    if (mode == nullptr) {
        return RET_ERROR(SCE_CTRL_ERROR_INVALID_ARG);
    }
    *mode = host.ctrl.input_mode_ext;
    return SCE_KERNEL_OK;
}

EXPORT(int, sceCtrlGetWirelessControllerInfo) {
    return UNIMPLEMENTED();
}

EXPORT(bool, sceCtrlIsMultiControllerSupported) {
    return host.cfg.pstv_mode;
}

EXPORT(int, sceCtrlPeekBufferNegative, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host.ctrl, port, pad_data, count, false, true);
}

EXPORT(int, sceCtrlPeekBufferNegative2, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host.ctrl, port, pad_data, count, true, true);
}

EXPORT(int, sceCtrlPeekBufferPositive, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host.ctrl, port, pad_data, count, false, false);
}

EXPORT(int, sceCtrlPeekBufferPositive2, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host.ctrl, port, pad_data, count, true, false);
}

EXPORT(int, sceCtrlPeekBufferPositiveExt, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host.ctrl, port, pad_data, count, false, false);
}

EXPORT(int, sceCtrlPeekBufferPositiveExt2, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host.ctrl, port, pad_data, count, true, false);
}

EXPORT(int, sceCtrlReadBufferNegative, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host.ctrl, port, pad_data, count, false, true);
}

EXPORT(int, sceCtrlReadBufferNegative2, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host.ctrl, port, pad_data, count, true, true);
}

EXPORT(int, sceCtrlReadBufferPositive, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host.ctrl, port, pad_data, count, false, false);
}

EXPORT(int, sceCtrlReadBufferPositive2, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host.ctrl, port, pad_data, count, true, false);
}

EXPORT(int, sceCtrlReadBufferPositiveExt, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host.ctrl, port, pad_data, count, false, false);
}

EXPORT(int, sceCtrlReadBufferPositiveExt2, int port, SceCtrlData *pad_data, int count) {
    if (port > 1 && !host.cfg.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
    }
    return peek_buffer(host.ctrl, port, pad_data, count, true, false);
}

EXPORT(int, sceCtrlRegisterBdRMCCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlResetLightBar) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetActuator, int port, const SceCtrlActuator *pState) {
    if (!host.cfg.pstv_mode) {
        return RET_ERROR(SCE_CTRL_ERROR_NOT_SUPPORTED);
    }

    remove_disconnected_controllers(host.ctrl);
    add_new_controllers(host.ctrl);

    for (const auto &controller : host.ctrl.controllers) {
        if (controller.second.port == port) {
            SDL_Haptic *handle = controller.second.haptic.get();
            if (pState->small == 0 && pState->large == 0) {
                SDL_HapticRumbleStop(handle);
            } else {
                // TODO: Look into a better implementation to distinguish both motors when available
                SDL_HapticRumblePlay(handle, ((pState->small * 1.0f) / 510.0f) + ((pState->large * 1.0f) / 510.0f), SDL_HAPTIC_INFINITY);
            }
            return 0;
        }
    }

    return RET_ERROR(SCE_CTRL_ERROR_NO_DEVICE);
}

EXPORT(int, sceCtrlSetAnalogStickCheckMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetAnalogStickCheckTarget) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetButtonIntercept) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetButtonRemappingInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetLightBar) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetRapidFire) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlSetSamplingMode, SceCtrlPadInputMode mode) {
    const auto old = host.ctrl.input_mode;
    if (mode < SCE_CTRL_MODE_DIGITAL || mode > SCE_CTRL_MODE_UNKNOWN) {
        return RET_ERROR(SCE_CTRL_ERROR_INVALID_ARG);
    }
    host.ctrl.input_mode = mode;
    return old;
}

EXPORT(int, sceCtrlSetSamplingModeExt, SceCtrlPadInputMode mode) {
    const auto old = host.ctrl.input_mode_ext;
    if (mode < SCE_CTRL_MODE_DIGITAL || mode > SCE_CTRL_MODE_UNKNOWN) {
        return RET_ERROR(SCE_CTRL_ERROR_INVALID_ARG);
    }
    host.ctrl.input_mode_ext = mode;
    return old;
}

EXPORT(int, sceCtrlSingleControllerMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCtrlUnregisterBdRMCCallback) {
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
