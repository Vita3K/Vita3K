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

// SceNearUtil
BRIDGE_DECL(sceNearCloseDiscoveredGiftImage)
BRIDGE_DECL(sceNearCloseReceivedGiftData)
BRIDGE_DECL(sceNearConvertDiscoveredGiftParam)
BRIDGE_DECL(sceNearDeleteDiscoveredGift)
BRIDGE_DECL(sceNearDeleteGift)
BRIDGE_DECL(sceNearFinalize)
BRIDGE_DECL(sceNearFinalizeAndLaunchNearApp)
BRIDGE_DECL(sceNearGetDiscoveredGiftInfo)
BRIDGE_DECL(sceNearGetDiscoveredGiftSender)
BRIDGE_DECL(sceNearGetDiscoveredGiftStatus)
BRIDGE_DECL(sceNearGetDiscoveredGifts)
BRIDGE_DECL(sceNearGetGift)
BRIDGE_DECL(sceNearGetGiftStatus)
BRIDGE_DECL(sceNearGetLastNeighborFoundDateTime)
BRIDGE_DECL(sceNearGetMyStatus)
BRIDGE_DECL(sceNearGetNeighbors)
BRIDGE_DECL(sceNearGetNewNeighbors)
BRIDGE_DECL(sceNearGetRecentNeighbors)
BRIDGE_DECL(sceNearIgnoreDiscoveredGift)
BRIDGE_DECL(sceNearInitialize)
BRIDGE_DECL(sceNearLaunchNearAppForDownload)
BRIDGE_DECL(sceNearLaunchNearAppForUpdate)
BRIDGE_DECL(sceNearOpenDiscoveredGiftImage)
BRIDGE_DECL(sceNearOpenReceivedGiftData)
BRIDGE_DECL(sceNearReadDiscoveredGiftImage)
BRIDGE_DECL(sceNearReadReceivedGiftData)
BRIDGE_DECL(sceNearRefresh)
BRIDGE_DECL(sceNearSetGift)
BRIDGE_DECL(sceNearSetGift2)
