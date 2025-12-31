// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <module/module.h>

#include <np/common.h>

EXPORT(int, sceNpTssGetData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTssGetDataAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTssGetDataNoLimit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTssGetDataNoLimitAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTssGetSmallStorage) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTssGetSmallStorageAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTssGetStorage) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTssGetStorageAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusAbortRequest) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusAddAndGetVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusAddAndGetVariableAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusAddAndGetVariableVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusAddAndGetVariableVUserAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusChangeModeForOtherSaveDataOwners) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusCreateRequest) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusCreateTitleCtx, const np::CommunicationID *communicationId,
    const np::SceNpCommunicationPassphrase *passphrase, const np::SceNpId *selfNpId) {
    LOG_INFO("sceNpTusCreateTitleCtx");
    if (communicationId)
        LOG_DEBUG("sceNpTusCreateTitleCtx(communicationId: {}, num: {})",
            communicationId->data, communicationId->num);
    if (passphrase)
        LOG_DEBUG("sceNpTusCreateTitleCtx, passphrase: {}", passphrase->data);

    if (selfNpId)
        LOG_DEBUG("sceNpTusCreateTitleCtx, selfNpId: handle: {}, isIdValid: {}", selfNpId->handle.data, selfNpId->isIdValid);

    return 1;
}

EXPORT(int, sceNpTusDeleteMultiSlotData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusDeleteMultiSlotDataAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusDeleteMultiSlotDataVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusDeleteMultiSlotDataVUserAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusDeleteMultiSlotVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusDeleteMultiSlotVariableAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusDeleteMultiSlotVariableVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusDeleteMultiSlotVariableVUserAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusDeleteRequest) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusDeleteTitleCtx) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetDataAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetDataVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetDataVUserAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetFriendsDataStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetFriendsDataStatusAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetFriendsVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetFriendsVariableAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiSlotDataStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiSlotDataStatusAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiSlotDataStatusVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiSlotDataStatusVUserAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiSlotVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiSlotVariableAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiSlotVariableVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiSlotVariableVUserAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiUserDataStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiUserDataStatusAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiUserDataStatusVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiUserDataStatusVUserAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiUserVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiUserVariableAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiUserVariableVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiUserVariableVUserAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusPollAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusSetData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusSetDataAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusSetDataVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusSetDataVUserAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusSetMultiSlotVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusSetMultiSlotVariableAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusSetMultiSlotVariableVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusSetMultiSlotVariableVUserAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusSetTimeout) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusTryAndSetVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusTryAndSetVariableAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusTryAndSetVariableVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusTryAndSetVariableVUserAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusWaitAsync) {
    return UNIMPLEMENTED();
}
