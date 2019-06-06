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

BRIDGE_DECL(sceBbmcClearEvent)
BRIDGE_DECL(sceBbmcDLGetSwdlError)
BRIDGE_DECL(sceBbmcDLQAbort)
BRIDGE_DECL(sceBbmcDLQBackupEFSNV)
BRIDGE_DECL(sceBbmcDLQConfirmChangePartition)
BRIDGE_DECL(sceBbmcDLQDownloadSafeImage)
BRIDGE_DECL(sceBbmcDLQDownloadSlotImage)
BRIDGE_DECL(sceBbmcDLQGetCurrentFwDone)
BRIDGE_DECL(sceBbmcDLQGetCurrentFwStart)
BRIDGE_DECL(sceBbmcDLQGetFwListDone)
BRIDGE_DECL(sceBbmcDLQGetFwListStart)
BRIDGE_DECL(sceBbmcDLQRestoreEFSNV)
BRIDGE_DECL(sceBbmcDLUpdaterCheckComVersion)
BRIDGE_DECL(sceBbmcDLUpdaterContinue)
BRIDGE_DECL(sceBbmcDLUpdaterFin)
BRIDGE_DECL(sceBbmcDLUpdaterGetArchiveVersion)
BRIDGE_DECL(sceBbmcDLUpdaterGetLatestArchiveVersion)
BRIDGE_DECL(sceBbmcDLUpdaterGetPkgId)
BRIDGE_DECL(sceBbmcDLUpdaterGetSequence)
BRIDGE_DECL(sceBbmcDLUpdaterStart)
BRIDGE_DECL(sceBbmcDebugOutString)
BRIDGE_DECL(sceBbmcDebugSelectOutMedia)
BRIDGE_DECL(sceBbmcDepersonalizeCK)
BRIDGE_DECL(sceBbmcEnvelopeCmd)
BRIDGE_DECL(sceBbmcFileRefreshResponse)
BRIDGE_DECL(sceBbmcGetCKStatus)
BRIDGE_DECL(sceBbmcGetComFirmVersion)
BRIDGE_DECL(sceBbmcGetDriverInfo)
BRIDGE_DECL(sceBbmcGetFailureInfo)
BRIDGE_DECL(sceBbmcGetFileRefreshDetail)
BRIDGE_DECL(sceBbmcGetIdStorageFrom3GModule)
BRIDGE_DECL(sceBbmcGetNetworkInfo)
BRIDGE_DECL(sceBbmcGetPinStatus)
BRIDGE_DECL(sceBbmcGetProactiveCmd)
BRIDGE_DECL(sceBbmcGpsCellGetLocationInfoDone)
BRIDGE_DECL(sceBbmcGpsCellGetLocationInfoStart)
BRIDGE_DECL(sceBbmcIsAbleToRunUnderCurrentTemp)
BRIDGE_DECL(sceBbmcIsAbleToRunUnderCurrentVoltage)
BRIDGE_DECL(sceBbmcIsPowerConfigForbiddenMode)
BRIDGE_DECL(sceBbmcNetGetRejectCause)
BRIDGE_DECL(sceBbmcNetworkAttach)
BRIDGE_DECL(sceBbmcNetworkScanAbort)
BRIDGE_DECL(sceBbmcNetworkScanDone)
BRIDGE_DECL(sceBbmcNetworkScanStart)
BRIDGE_DECL(sceBbmcPacketGetCurrentBearerTech)
BRIDGE_DECL(sceBbmcPacketGetLastCallEndReason)
BRIDGE_DECL(sceBbmcPacketIfClearControlFlag)
BRIDGE_DECL(sceBbmcPacketIfGetControlFlag)
BRIDGE_DECL(sceBbmcPacketIfSetControlFlag)
BRIDGE_DECL(sceBbmcPacketReleasePDPContext)
BRIDGE_DECL(sceBbmcReadSimContentDone)
BRIDGE_DECL(sceBbmcReadSimContentStart)
BRIDGE_DECL(sceBbmcReserveOperationMode)
BRIDGE_DECL(sceBbmcResumeSubscriberCallback)
BRIDGE_DECL(sceBbmcSMSGetListDone)
BRIDGE_DECL(sceBbmcSMSGetListStart)
BRIDGE_DECL(sceBbmcSMSReadAck)
BRIDGE_DECL(sceBbmcSMSReadDone)
BRIDGE_DECL(sceBbmcSMSReadStart)
BRIDGE_DECL(sceBbmcSMSSend)
BRIDGE_DECL(sceBbmcSetLPMMode)
BRIDGE_DECL(sceBbmcSetupGetValueDone)
BRIDGE_DECL(sceBbmcSetupGetValueStart)
BRIDGE_DECL(sceBbmcSetupSetValue)
BRIDGE_DECL(sceBbmcSimGetPLMNModeBit)
BRIDGE_DECL(sceBbmcSubscribeFeatureId)
BRIDGE_DECL(sceBbmcTerminalResponseCmd)
BRIDGE_DECL(sceBbmcTurnOff)
BRIDGE_DECL(sceBbmcTurnOn)
BRIDGE_DECL(sceBbmcUnblockCK)
BRIDGE_DECL(sceBbmcUnblockPin)
BRIDGE_DECL(sceBbmcUnsubscribeFeature)
BRIDGE_DECL(sceBbmcUsbExtIfRead)
BRIDGE_DECL(sceBbmcUsbExtIfStart)
BRIDGE_DECL(sceBbmcUsbExtIfStop)
BRIDGE_DECL(sceBbmcUsbExtIfWrite)
BRIDGE_DECL(sceBbmcVerifyPin)
