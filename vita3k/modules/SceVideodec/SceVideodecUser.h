// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

BRIDGE_DECL(sceAvcdecCreateDecoder)
BRIDGE_DECL(sceAvcdecCreateDecoderInternal)
BRIDGE_DECL(sceAvcdecCreateDecoderNongameapp)
BRIDGE_DECL(sceAvcdecCsc)
BRIDGE_DECL(sceAvcdecCscInternal)
BRIDGE_DECL(sceAvcdecDecode)
BRIDGE_DECL(sceAvcdecDecodeAuInternal)
BRIDGE_DECL(sceAvcdecDecodeAuNalAuInternal)
BRIDGE_DECL(sceAvcdecDecodeAuNalAuNongameapp)
BRIDGE_DECL(sceAvcdecDecodeAuNongameapp)
BRIDGE_DECL(sceAvcdecDecodeAvailableSize)
BRIDGE_DECL(sceAvcdecDecodeFlush)
BRIDGE_DECL(sceAvcdecDecodeGetPictureInternal)
BRIDGE_DECL(sceAvcdecDecodeGetPictureNongameapp)
BRIDGE_DECL(sceAvcdecDecodeGetPictureWithWorkPictureInternal)
BRIDGE_DECL(sceAvcdecDecodeNalAu)
BRIDGE_DECL(sceAvcdecDecodeNalAuWithWorkPicture)
BRIDGE_DECL(sceAvcdecDecodeSetTrickModeNongameapp)
BRIDGE_DECL(sceAvcdecDecodeSetUserDataSei1FieldMemSizeNongameapp)
BRIDGE_DECL(sceAvcdecDecodeStop)
BRIDGE_DECL(sceAvcdecDecodeStopWithWorkPicture)
BRIDGE_DECL(sceAvcdecDecodeWithWorkPicture)
BRIDGE_DECL(sceAvcdecDeleteDecoder)
BRIDGE_DECL(sceAvcdecGetSeiPictureTimingInternal)
BRIDGE_DECL(sceAvcdecGetSeiUserDataNongameapp)
BRIDGE_DECL(sceAvcdecQueryDecoderMemSize)
BRIDGE_DECL(sceAvcdecQueryDecoderMemSizeInternal)
BRIDGE_DECL(sceAvcdecQueryDecoderMemSizeNongameapp)
BRIDGE_DECL(sceAvcdecRegisterCallbackInternal)
BRIDGE_DECL(sceAvcdecRegisterCallbackNongameapp)
BRIDGE_DECL(sceAvcdecSetDecodeMode)
BRIDGE_DECL(sceAvcdecSetDecodeModeInternal)
BRIDGE_DECL(sceAvcdecSetInterlacedStreamMode)
BRIDGE_DECL(sceAvcdecSetLowDelayModeNongameapp)
BRIDGE_DECL(sceAvcdecSetRecoveryPointSEIMode)
BRIDGE_DECL(sceAvcdecUnregisterCallbackInternal)
BRIDGE_DECL(sceAvcdecUnregisterCallbackNongameapp)
BRIDGE_DECL(sceAvcdecUnregisterCallbackWithCbidInternal)
BRIDGE_DECL(sceAvcdecUnregisterCallbackWithCbidNongameapp)
BRIDGE_DECL(sceM4vdecCreateDecoder)
BRIDGE_DECL(sceM4vdecCreateDecoderInternal)
BRIDGE_DECL(sceM4vdecCsc)
BRIDGE_DECL(sceM4vdecDecode)
BRIDGE_DECL(sceM4vdecDecodeAvailableSize)
BRIDGE_DECL(sceM4vdecDecodeFlush)
BRIDGE_DECL(sceM4vdecDecodeStop)
BRIDGE_DECL(sceM4vdecDecodeStopWithWorkPicture)
BRIDGE_DECL(sceM4vdecDecodeWithWorkPicture)
BRIDGE_DECL(sceM4vdecDeleteDecoder)
BRIDGE_DECL(sceM4vdecQueryDecoderMemSize)
BRIDGE_DECL(sceM4vdecQueryDecoderMemSizeInternal)
BRIDGE_DECL(sceVideodecInitLibrary)
BRIDGE_DECL(sceVideodecInitLibraryInternal)
BRIDGE_DECL(sceVideodecInitLibraryNongameapp)
BRIDGE_DECL(sceVideodecInitLibraryWithUnmapMem)
BRIDGE_DECL(sceVideodecInitLibraryWithUnmapMemInternal)
BRIDGE_DECL(sceVideodecInitLibraryWithUnmapMemNongameapp)
BRIDGE_DECL(sceVideodecQueryInstanceNongameapp)
BRIDGE_DECL(sceVideodecQueryMemSize)
BRIDGE_DECL(sceVideodecQueryMemSizeInternal)
BRIDGE_DECL(sceVideodecQueryMemSizeNongameapp)
BRIDGE_DECL(sceVideodecSetConfig)
BRIDGE_DECL(sceVideodecSetConfigInternal)
BRIDGE_DECL(sceVideodecTermLibrary)
