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

// SceNpTus
BRIDGE_DECL(sceNpTssGetData)
BRIDGE_DECL(sceNpTssGetDataAsync)
BRIDGE_DECL(sceNpTssGetDataNoLimit)
BRIDGE_DECL(sceNpTssGetDataNoLimitAsync)
BRIDGE_DECL(sceNpTssGetSmallStorage)
BRIDGE_DECL(sceNpTssGetSmallStorageAsync)
BRIDGE_DECL(sceNpTssGetStorage)
BRIDGE_DECL(sceNpTssGetStorageAsync)
BRIDGE_DECL(sceNpTusAbortRequest)
BRIDGE_DECL(sceNpTusAddAndGetVariable)
BRIDGE_DECL(sceNpTusAddAndGetVariableAsync)
BRIDGE_DECL(sceNpTusAddAndGetVariableVUser)
BRIDGE_DECL(sceNpTusAddAndGetVariableVUserAsync)
BRIDGE_DECL(sceNpTusChangeModeForOtherSaveDataOwners)
BRIDGE_DECL(sceNpTusCreateRequest)
BRIDGE_DECL(sceNpTusCreateTitleCtx)
BRIDGE_DECL(sceNpTusDeleteMultiSlotData)
BRIDGE_DECL(sceNpTusDeleteMultiSlotDataAsync)
BRIDGE_DECL(sceNpTusDeleteMultiSlotDataVUser)
BRIDGE_DECL(sceNpTusDeleteMultiSlotDataVUserAsync)
BRIDGE_DECL(sceNpTusDeleteMultiSlotVariable)
BRIDGE_DECL(sceNpTusDeleteMultiSlotVariableAsync)
BRIDGE_DECL(sceNpTusDeleteMultiSlotVariableVUser)
BRIDGE_DECL(sceNpTusDeleteMultiSlotVariableVUserAsync)
BRIDGE_DECL(sceNpTusDeleteRequest)
BRIDGE_DECL(sceNpTusDeleteTitleCtx)
BRIDGE_DECL(sceNpTusGetData)
BRIDGE_DECL(sceNpTusGetDataAsync)
BRIDGE_DECL(sceNpTusGetDataVUser)
BRIDGE_DECL(sceNpTusGetDataVUserAsync)
BRIDGE_DECL(sceNpTusGetFriendsDataStatus)
BRIDGE_DECL(sceNpTusGetFriendsDataStatusAsync)
BRIDGE_DECL(sceNpTusGetFriendsVariable)
BRIDGE_DECL(sceNpTusGetFriendsVariableAsync)
BRIDGE_DECL(sceNpTusGetMultiSlotDataStatus)
BRIDGE_DECL(sceNpTusGetMultiSlotDataStatusAsync)
BRIDGE_DECL(sceNpTusGetMultiSlotDataStatusVUser)
BRIDGE_DECL(sceNpTusGetMultiSlotDataStatusVUserAsync)
BRIDGE_DECL(sceNpTusGetMultiSlotVariable)
BRIDGE_DECL(sceNpTusGetMultiSlotVariableAsync)
BRIDGE_DECL(sceNpTusGetMultiSlotVariableVUser)
BRIDGE_DECL(sceNpTusGetMultiSlotVariableVUserAsync)
BRIDGE_DECL(sceNpTusGetMultiUserDataStatus)
BRIDGE_DECL(sceNpTusGetMultiUserDataStatusAsync)
BRIDGE_DECL(sceNpTusGetMultiUserDataStatusVUser)
BRIDGE_DECL(sceNpTusGetMultiUserDataStatusVUserAsync)
BRIDGE_DECL(sceNpTusGetMultiUserVariable)
BRIDGE_DECL(sceNpTusGetMultiUserVariableAsync)
BRIDGE_DECL(sceNpTusGetMultiUserVariableVUser)
BRIDGE_DECL(sceNpTusGetMultiUserVariableVUserAsync)
BRIDGE_DECL(sceNpTusInit)
BRIDGE_DECL(sceNpTusPollAsync)
BRIDGE_DECL(sceNpTusSetData)
BRIDGE_DECL(sceNpTusSetDataAsync)
BRIDGE_DECL(sceNpTusSetDataVUser)
BRIDGE_DECL(sceNpTusSetDataVUserAsync)
BRIDGE_DECL(sceNpTusSetMultiSlotVariable)
BRIDGE_DECL(sceNpTusSetMultiSlotVariableAsync)
BRIDGE_DECL(sceNpTusSetMultiSlotVariableVUser)
BRIDGE_DECL(sceNpTusSetMultiSlotVariableVUserAsync)
BRIDGE_DECL(sceNpTusSetTimeout)
BRIDGE_DECL(sceNpTusTerm)
BRIDGE_DECL(sceNpTusTryAndSetVariable)
BRIDGE_DECL(sceNpTusTryAndSetVariableAsync)
BRIDGE_DECL(sceNpTusTryAndSetVariableVUser)
BRIDGE_DECL(sceNpTusTryAndSetVariableVUserAsync)
BRIDGE_DECL(sceNpTusWaitAsync)
