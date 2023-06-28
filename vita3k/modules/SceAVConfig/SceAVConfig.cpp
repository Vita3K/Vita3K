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

#include "SceAVConfig.h"

#include <audio/state.h>

EXPORT(int, sceAVConfigChangeReg) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigClearAutoSuspend2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigDisplayOn) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigGetAcStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigGetBtVol) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigGetConnectedAudioDevice) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigGetDisplayMaxBrightness) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigGetMasterVol, int *vol) {
    LOG_DEBUG("Call");
    *vol = 30;
    return 0;
}

EXPORT(int, sceAVConfigGetShutterVol) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigGetSystemVol, int *vol) {
    STUBBED("");
    *vol = 100;
    return 0;
}

EXPORT(int, sceAVConfigGetVolCtrlEnable, int *vol) {
    STUBBED("");
    *vol = 100;
    return 0;
}

EXPORT(int, sceAVConfigHdmiCecCmdOneTouchPlay) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigHdmiCecDisable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigHdmiCecEnable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigHdmiClearCecInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigHdmiGetCecInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigHdmiGetMonitorInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigHdmiGetOutScalingRatio) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigHdmiSetOutScalingRatio) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigHdmiSetResolution) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigHdmiSetRgbRangeMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigMuteOn) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigRegisterCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigSendVolKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigSetAutoDisplayDimming) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigSetAutoSuspend) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigSetAutoSuspend2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigSetDisplayBrightness) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigSetDisplayColorSpaceMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigSetMasterVol, int vol) {
    LOG_DEBUG("vol: {}", vol);
    float volume = static_cast<float>(vol) / 30.0f; // Conversion de 0-30 en 0.0-1.0

    for (const auto &audio : emuenv.audio.out_ports) {
        audio.second->volume = volume;
        emuenv.audio.adapter->set_volume(*audio.second, volume);
    }

    return 0;
}

EXPORT(int, sceAVConfigSetSystemVol) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigUnRegisterCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAVConfigWriteMasterVol) {
    LOG_DEBUG("Call");
    return 10;
}

EXPORT(int, sceAVConfigWriteRegSystemVol) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceAVConfigChangeReg)
BRIDGE_IMPL(sceAVConfigClearAutoSuspend2)
BRIDGE_IMPL(sceAVConfigDisplayOn)
BRIDGE_IMPL(sceAVConfigGetAcStatus)
BRIDGE_IMPL(sceAVConfigGetBtVol)
BRIDGE_IMPL(sceAVConfigGetConnectedAudioDevice)
BRIDGE_IMPL(sceAVConfigGetDisplayMaxBrightness)
BRIDGE_IMPL(sceAVConfigGetMasterVol)
BRIDGE_IMPL(sceAVConfigGetShutterVol)
BRIDGE_IMPL(sceAVConfigGetSystemVol)
BRIDGE_IMPL(sceAVConfigGetVolCtrlEnable)
BRIDGE_IMPL(sceAVConfigHdmiCecCmdOneTouchPlay)
BRIDGE_IMPL(sceAVConfigHdmiCecDisable)
BRIDGE_IMPL(sceAVConfigHdmiCecEnable)
BRIDGE_IMPL(sceAVConfigHdmiClearCecInfo)
BRIDGE_IMPL(sceAVConfigHdmiGetCecInfo)
BRIDGE_IMPL(sceAVConfigHdmiGetMonitorInfo)
BRIDGE_IMPL(sceAVConfigHdmiGetOutScalingRatio)
BRIDGE_IMPL(sceAVConfigHdmiSetOutScalingRatio)
BRIDGE_IMPL(sceAVConfigHdmiSetResolution)
BRIDGE_IMPL(sceAVConfigHdmiSetRgbRangeMode)
BRIDGE_IMPL(sceAVConfigMuteOn)
BRIDGE_IMPL(sceAVConfigRegisterCallback)
BRIDGE_IMPL(sceAVConfigSendVolKey)
BRIDGE_IMPL(sceAVConfigSetAutoDisplayDimming)
BRIDGE_IMPL(sceAVConfigSetAutoSuspend)
BRIDGE_IMPL(sceAVConfigSetAutoSuspend2)
BRIDGE_IMPL(sceAVConfigSetDisplayBrightness)
BRIDGE_IMPL(sceAVConfigSetDisplayColorSpaceMode)
BRIDGE_IMPL(sceAVConfigSetMasterVol)
BRIDGE_IMPL(sceAVConfigSetSystemVol)
BRIDGE_IMPL(sceAVConfigUnRegisterCallback)
BRIDGE_IMPL(sceAVConfigWriteMasterVol)
BRIDGE_IMPL(sceAVConfigWriteRegSystemVol)
