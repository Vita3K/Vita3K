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

// SceBt
BRIDGE_DECL(sceBtAvrcpReadVolume)
BRIDGE_DECL(sceBtAvrcpSendButton)
BRIDGE_DECL(sceBtAvrcpSendVolume)
BRIDGE_DECL(sceBtAvrcpSetPlayStatus)
BRIDGE_DECL(sceBtAvrcpSetTitle)
BRIDGE_DECL(sceBtDeleteRegisteredInfo)
BRIDGE_DECL(sceBtFreqAudio)
BRIDGE_DECL(sceBtGetConfiguration)
BRIDGE_DECL(sceBtGetConnectingInfo)
BRIDGE_DECL(sceBtGetDeviceName)
BRIDGE_DECL(sceBtGetInfoForTest)
BRIDGE_DECL(sceBtGetLastError)
BRIDGE_DECL(sceBtGetRegisteredInfo)
BRIDGE_DECL(sceBtGetStatusForTest)
BRIDGE_DECL(sceBtGetVidPid)
BRIDGE_DECL(sceBtHfpGetCurrentPhoneNumber)
BRIDGE_DECL(sceBtHfpRequest)
BRIDGE_DECL(sceBtHidGetReportDescriptor)
BRIDGE_DECL(sceBtHidTransfer)
BRIDGE_DECL(sceBtPairingOOB)
BRIDGE_DECL(sceBtPushBip)
BRIDGE_DECL(sceBtPushOpp)
BRIDGE_DECL(sceBtReadEvent)
BRIDGE_DECL(sceBtRecvAudio)
BRIDGE_DECL(sceBtRecvBip)
BRIDGE_DECL(sceBtRecvOpp)
BRIDGE_DECL(sceBtRecvSpp)
BRIDGE_DECL(sceBtRegisterCallback)
BRIDGE_DECL(sceBtReplyPinCode)
BRIDGE_DECL(sceBtReplyUserConfirmation)
BRIDGE_DECL(sceBtSendAudio)
BRIDGE_DECL(sceBtSendL2capEchoRequestForTest)
BRIDGE_DECL(sceBtSendSpp)
BRIDGE_DECL(sceBtSetConfiguration)
BRIDGE_DECL(sceBtSetContentProtection)
BRIDGE_DECL(sceBtSetInquiryResultForTest)
BRIDGE_DECL(sceBtSetInquiryScan)
BRIDGE_DECL(sceBtSetL2capEchoResponseBufferForTest)
BRIDGE_DECL(sceBtSetStatusForTest)
BRIDGE_DECL(sceBtStartAudio)
BRIDGE_DECL(sceBtStartConnect)
BRIDGE_DECL(sceBtStartDisconnect)
BRIDGE_DECL(sceBtStartInquiry)
BRIDGE_DECL(sceBtStopAudio)
BRIDGE_DECL(sceBtStopInquiry)
BRIDGE_DECL(sceBtUnregisterCallback)
