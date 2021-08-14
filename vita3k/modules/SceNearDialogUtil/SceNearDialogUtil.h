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

BRIDGE_DECL(sceNearDialogAbort)
BRIDGE_DECL(sceNearDialogGetResult)
BRIDGE_DECL(sceNearDialogGetStatus)
BRIDGE_DECL(sceNearDialogInit)
BRIDGE_DECL(sceNearDialogTerm)
BRIDGE_DECL(sceNearUtilityCloseDiscoveredGiftImage)
BRIDGE_DECL(sceNearUtilityCloseReceivedGiftData)
BRIDGE_DECL(sceNearUtilityConvertDiscoveredGiftParam)
BRIDGE_DECL(sceNearUtilityDeleteDiscoveredGift)
BRIDGE_DECL(sceNearUtilityDeleteGift)
BRIDGE_DECL(sceNearUtilityFinalize)
BRIDGE_DECL(sceNearUtilityGetDiscoveredGiftInfo)
BRIDGE_DECL(sceNearUtilityGetDiscoveredGiftSender)
BRIDGE_DECL(sceNearUtilityGetDiscoveredGiftStatus)
BRIDGE_DECL(sceNearUtilityGetDiscoveredGifts)
BRIDGE_DECL(sceNearUtilityGetGift)
BRIDGE_DECL(sceNearUtilityGetGiftStatus)
BRIDGE_DECL(sceNearUtilityGetLastNeighborFoundDateTime)
BRIDGE_DECL(sceNearUtilityGetMyStatus)
BRIDGE_DECL(sceNearUtilityGetNeighbors)
BRIDGE_DECL(sceNearUtilityGetNewNeighbors)
BRIDGE_DECL(sceNearUtilityGetRecentNeighbors)
BRIDGE_DECL(sceNearUtilityIgnoreDiscoveredGift)
BRIDGE_DECL(sceNearUtilityInitialize)
BRIDGE_DECL(sceNearUtilityLaunchNearAppForDownload)
BRIDGE_DECL(sceNearUtilityLaunchNearAppForUpdate)
BRIDGE_DECL(sceNearUtilityOpenDiscoveredGiftImage)
BRIDGE_DECL(sceNearUtilityOpenReceivedGiftData)
BRIDGE_DECL(sceNearUtilityReadDiscoveredGiftImage)
BRIDGE_DECL(sceNearUtilityReadReceivedGiftData)
BRIDGE_DECL(sceNearUtilityRefresh)
BRIDGE_DECL(sceNearUtilitySetGift)
BRIDGE_DECL(sceNearUtilitySetGift2)
