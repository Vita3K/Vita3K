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

// SceAVConfig
BRIDGE_DECL(sceAVConfigChangeReg)
BRIDGE_DECL(sceAVConfigClearAutoSuspend2)
BRIDGE_DECL(sceAVConfigDisplayOn)
BRIDGE_DECL(sceAVConfigGetAcStatus)
BRIDGE_DECL(sceAVConfigGetBtVol)
BRIDGE_DECL(sceAVConfigGetConnectedAudioDevice)
BRIDGE_DECL(sceAVConfigGetDisplayMaxBrightness)
BRIDGE_DECL(sceAVConfigGetMasterVol)
BRIDGE_DECL(sceAVConfigGetShutterVol)
BRIDGE_DECL(sceAVConfigGetSystemVol)
BRIDGE_DECL(sceAVConfigGetVolCtrlEnable)
BRIDGE_DECL(sceAVConfigHdmiCecCmdOneTouchPlay)
BRIDGE_DECL(sceAVConfigHdmiCecDisable)
BRIDGE_DECL(sceAVConfigHdmiCecEnable)
BRIDGE_DECL(sceAVConfigHdmiClearCecInfo)
BRIDGE_DECL(sceAVConfigHdmiGetCecInfo)
BRIDGE_DECL(sceAVConfigHdmiGetMonitorInfo)
BRIDGE_DECL(sceAVConfigHdmiGetOutScalingRatio)
BRIDGE_DECL(sceAVConfigHdmiSetOutScalingRatio)
BRIDGE_DECL(sceAVConfigHdmiSetResolution)
BRIDGE_DECL(sceAVConfigHdmiSetRgbRangeMode)
BRIDGE_DECL(sceAVConfigMuteOn)
BRIDGE_DECL(sceAVConfigRegisterCallback)
BRIDGE_DECL(sceAVConfigSendVolKey)
BRIDGE_DECL(sceAVConfigSetAutoDisplayDimming)
BRIDGE_DECL(sceAVConfigSetAutoSuspend)
BRIDGE_DECL(sceAVConfigSetAutoSuspend2)
BRIDGE_DECL(sceAVConfigSetDisplayBrightness)
BRIDGE_DECL(sceAVConfigSetDisplayColorSpaceMode)
BRIDGE_DECL(sceAVConfigSetMasterVol)
BRIDGE_DECL(sceAVConfigSetSystemVol)
BRIDGE_DECL(sceAVConfigUnRegisterCallback)
BRIDGE_DECL(sceAVConfigWriteMasterVol)
BRIDGE_DECL(sceAVConfigWriteRegSystemVol)
