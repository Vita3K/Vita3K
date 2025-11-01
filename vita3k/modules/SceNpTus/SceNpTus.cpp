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

enum SceNpCommunityServerErrorCode : uint32_t {
    SCE_NP_COMMUNITY_SERVER_ERROR_BAD_REQUEST = 0x80550801,
    SCE_NP_COMMUNITY_SERVER_ERROR_INVALID_TICKET = 0x80550802,
    SCE_NP_COMMUNITY_SERVER_ERROR_INVALID_SIGNATURE = 0x80550803,
    SCE_NP_COMMUNITY_SERVER_ERROR_INVALID_NPID = 0x80550805,
    SCE_NP_COMMUNITY_SERVER_ERROR_FORBIDDEN = 0x80550806,
    SCE_NP_COMMUNITY_SERVER_ERROR_INTERNAL_SERVER_ERROR = 0x80550807,
    SCE_NP_COMMUNITY_SERVER_ERROR_VERSION_NOT_SUPPORTED = 0x80550808,
    SCE_NP_COMMUNITY_SERVER_ERROR_SERVICE_UNAVAILABLE = 0x80550809,
    SCE_NP_COMMUNITY_SERVER_ERROR_PLAYER_BANNED = 0x8055080a,
    SCE_NP_COMMUNITY_SERVER_ERROR_CENSORED = 0x8055080b,
    SCE_NP_COMMUNITY_SERVER_ERROR_RANKING_RECORD_FORBIDDEN = 0x8055080c,
    SCE_NP_COMMUNITY_SERVER_ERROR_USER_PROFILE_NOT_FOUND = 0x8055080d,
    SCE_NP_COMMUNITY_SERVER_ERROR_UPLOADER_DATA_NOT_FOUND = 0x8055080e,
    SCE_NP_COMMUNITY_SERVER_ERROR_QUOTA_MASTER_NOT_FOUND = 0x8055080f,
    SCE_NP_COMMUNITY_SERVER_ERROR_RANKING_TITLE_NOT_FOUND = 0x80550810,
    SCE_NP_COMMUNITY_SERVER_ERROR_BLACKLISTED_USER_ID = 0x80550811,
    SCE_NP_COMMUNITY_SERVER_ERROR_GAME_RANKING_NOT_FOUND = 0x80550812,
    SCE_NP_COMMUNITY_SERVER_ERROR_RANKING_STORE_NOT_FOUND = 0x80550814,
    SCE_NP_COMMUNITY_SERVER_ERROR_NOT_BEST_SCORE = 0x80550815,
    SCE_NP_COMMUNITY_SERVER_ERROR_LATEST_UPDATE_NOT_FOUND = 0x80550816,
    SCE_NP_COMMUNITY_SERVER_ERROR_RANKING_BOARD_MASTER_NOT_FOUND = 0x80550817,
    SCE_NP_COMMUNITY_SERVER_ERROR_RANKING_GAME_DATA_MASTER_NOT_FOUND = 0x80550818,
    SCE_NP_COMMUNITY_SERVER_ERROR_INVALID_ANTICHEAT_DATA = 0x80550819,
    SCE_NP_COMMUNITY_SERVER_ERROR_TOO_LARGE_DATA = 0x8055081a,
    SCE_NP_COMMUNITY_SERVER_ERROR_NO_SUCH_USER_NPID = 0x8055081b,
    SCE_NP_COMMUNITY_SERVER_ERROR_INVALID_ENVIRONMENT = 0x8055081d,
    SCE_NP_COMMUNITY_SERVER_ERROR_INVALID_ONLINE_NAME_CHARACTER = 0x8055081f,
    SCE_NP_COMMUNITY_SERVER_ERROR_INVALID_ONLINE_NAME_LENGTH = 0x80550820,
    SCE_NP_COMMUNITY_SERVER_ERROR_INVALID_ABOUT_ME_CHARACTER = 0x80550821,
    SCE_NP_COMMUNITY_SERVER_ERROR_INVALID_ABOUT_ME_LENGTH = 0x80550822,
    SCE_NP_COMMUNITY_SERVER_ERROR_INVALID_SCORE = 0x80550823,
    SCE_NP_COMMUNITY_SERVER_ERROR_OVER_THE_RANKING_LIMIT = 0x80550824,
    SCE_NP_COMMUNITY_SERVER_ERROR_FAIL_TO_CREATE_SIGNATURE = 0x80550826,
    SCE_NP_COMMUNITY_SERVER_ERROR_RANKING_MASTER_INFO_NOT_FOUND = 0x80550827,
    SCE_NP_COMMUNITY_SERVER_ERROR_OVER_THE_GAME_DATA_LIMIT = 0x80550828,
    SCE_NP_COMMUNITY_SERVER_ERROR_SELF_DATA_NOT_FOUND = 0x8055082a,
    SCE_NP_COMMUNITY_SERVER_ERROR_USER_NOT_ASSIGNED = 0x8055082b,
    SCE_NP_COMMUNITY_SERVER_ERROR_GAME_DATA_ALREADY_EXISTS = 0x8055082c,
    SCE_NP_COMMUNITY_SERVER_ERROR_MATCHING_BEFORE_SERVICE = 0x805508a0,
    SCE_NP_COMMUNITY_SERVER_ERROR_MATCHING_END_OF_SERVICE = 0x805508a1,
    SCE_NP_COMMUNITY_SERVER_ERROR_MATCHING_MAINTENANCE = 0x805508a2,
    SCE_NP_COMMUNITY_SERVER_ERROR_RANKING_BEFORE_SERVICE = 0x805508a3,
    SCE_NP_COMMUNITY_SERVER_ERROR_RANKING_END_OF_SERVICE = 0x805508a4,
    SCE_NP_COMMUNITY_SERVER_ERROR_RANKING_MAINTENANCE = 0x805508a5,
    SCE_NP_COMMUNITY_SERVER_ERROR_NO_SUCH_TITLE = 0x805508a6,
    SCE_NP_COMMUNITY_SERVER_ERROR_UNSPECIFIED = 0x805508ff
};

enum SceNpCommunityErrorCode : uint32_t {
    SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED = 0x80550701,
    SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED = 0x80550702,
    SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY = 0x80550703,
    SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT = 0x80550704,
    SCE_NP_COMMUNITY_ERROR_NO_LOGIN = 0x80550705,
    SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS = 0x80550706,
    SCE_NP_COMMUNITY_ERROR_ABORTED = 0x80550707,
    SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE = 0x80550708,
    SCE_NP_COMMUNITY_ERROR_BODY_TOO_LARGE = 0x80550709,
    SCE_NP_COMMUNITY_ERROR_HTTP_SERVER = 0x8055070a,
    SCE_NP_COMMUNITY_ERROR_INVALID_SIGNATURE = 0x8055070b,
    SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT = 0x8055070c,
    SCE_NP_COMMUNITY_ERROR_UNKNOWN_TYPE = 0x8055070d,
    SCE_NP_COMMUNITY_ERROR_INVALID_ID = 0x8055070e,
    SCE_NP_COMMUNITY_ERROR_INVALID_ONLINE_ID = 0x8055070f,
    SCE_NP_COMMUNITY_ERROR_CONNECTION_HANDLE_ALREADY_EXISTS = 0x80550710,
    SCE_NP_COMMUNITY_ERROR_INVALID_TYPE = 0x80550711,
    SCE_NP_COMMUNITY_ERROR_TRANSACTION_ALREADY_END = 0x80550712,
    SCE_NP_COMMUNITY_ERROR_INVALID_PARTITION = 0x80550713,
    SCE_NP_COMMUNITY_ERROR_INVALID_ALIGNMENT = 0x80550714,
    SCE_NP_COMMUNITY_ERROR_CLIENT_HANDLE_ALREADY_EXISTS = 0x80550715,
    SCE_NP_COMMUNITY_ERROR_NO_RESOURCE = 0x80550716
};

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

EXPORT(int, sceNpTssGetSmallStorage, SceInt32 requestId, char *pBuffer, SceSize bSize, SceSize *contentLength, void *optionPtr) {
    std::string buffer = "";
    strcpy(pBuffer, buffer.c_str());
    *contentLength = buffer.size();

    // TSS files come from PSN and are unavailable unless dumped
    LOG_WARN_ONCE("Stubbed {} import called. ({})", export_name, "Returning empty string w/ NO_RESOURCE for TSS Content");
    return SCE_NP_COMMUNITY_ERROR_NO_RESOURCE;
}

EXPORT(int, sceNpTssGetSmallStorageAsync, SceInt32 requestId, char *pBuffer, SceSize bSize, SceSize *contentLength, void *optionPtr) {
    std::string buffer = "";
    strcpy(pBuffer, buffer.c_str());
    *contentLength = buffer.size();

    // TSS files come from PSN and are unavailable unless dumped
    LOG_WARN_ONCE("Stubbed {} import called. ({})", export_name, "Returning empty string w/ NO_RESOURCE for TSS Content");
    return SCE_NP_COMMUNITY_ERROR_NO_RESOURCE;
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

EXPORT(int, sceNpTusCreateTitleCtx) {
    return UNIMPLEMENTED();
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
