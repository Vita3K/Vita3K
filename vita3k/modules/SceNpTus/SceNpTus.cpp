// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <kernel/state.h>

#include <np/common.h>
#include <np/state.h>

// Handles are simulated. TSS reads fail explicitly without PSN storage or a network worker.
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

bool is_valid_title_context(const NpTusState &tus, SceInt32 titleCtxId) {
    return tus.title_contexts.contains(titleCtxId);
}

// The caller must hold tus.access_mutex until it has finished using the request.
NpTusRequest *get_request(NpTusState &tus, SceInt32 requestId) {
    const auto request = tus.requests.find(requestId);
    if (request == tus.requests.end())
        return nullptr;

    return is_valid_title_context(tus, request->second.title_context_id) ? &request->second : nullptr;
}

int get_tss_data(NpTusState &tus, const char *export_name, SceInt32 requestId, SceInt32 slotId,
    np::SceNpTssDataStatus *dataStatus, SceSize dataStatusSize, bool async) {
    std::lock_guard lock(tus.access_mutex);
    if (!tus.inited)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED);

    auto *request = get_request(tus, requestId);
    if (!request)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_INVALID_ID);
    if (!dataStatus)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT);
    if (dataStatusSize != sizeof(*dataStatus) || slotId < 0 || slotId > 15)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT);
    if (request->result)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_TRANSACTION_ALREADY_END);

    *dataStatus = {};
    request->result = static_cast<SceInt32>(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
    LOG_WARN_ONCE("Stubbed {} import called. TSS content is unavailable without PSN storage.", export_name);
    return async ? 0 : RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

int get_tss_small_storage(NpTusState &tus, const char *export_name, SceInt32 requestId, void *data,
    SceSize maxSize, SceSize *contentLength, const void *option, bool async) {
    std::lock_guard lock(tus.access_mutex);
    if (!tus.inited)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED);

    auto *request = get_request(tus, requestId);
    if (!request)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_INVALID_ID);
    if (!data || !contentLength)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT);
    if (option || maxSize > 64 * 1024)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT);
    if (request->result)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_TRANSACTION_ALREADY_END);

    *contentLength = 0;
    request->result = static_cast<SceInt32>(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
    LOG_WARN_ONCE("Stubbed {} import called. TSS content is unavailable without PSN storage.", export_name);
    return async ? 0 : RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

int get_async_result(NpTusState &tus, const char *export_name, SceInt32 requestId, SceInt32 *result) {
    std::lock_guard lock(tus.access_mutex);
    if (!tus.inited)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED);

    const auto *request = get_request(tus, requestId);
    if (!request)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_INVALID_ID);
    if (!result || !request->result)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT);

    *result = *request->result;
    return 0;
}

EXPORT(int, sceNpTssGetData, SceInt32 requestId, SceInt32 slotId, np::SceNpTssDataStatus *dataStatus,
    SceSize dataStatusSize, void *data, SceSize recvSize, const void *option) {
    return get_tss_data(emuenv.np.tus_state, export_name, requestId, slotId, dataStatus, dataStatusSize, false);
}

EXPORT(int, sceNpTssGetDataAsync, SceInt32 requestId, SceInt32 slotId, np::SceNpTssDataStatus *dataStatus,
    SceSize dataStatusSize, void *data, SceSize recvSize, const void *option) {
    return get_tss_data(emuenv.np.tus_state, export_name, requestId, slotId, dataStatus, dataStatusSize, true);
}

EXPORT(int, sceNpTssGetDataNoLimit) {
    STUBBED("TSS content is unavailable, returning NO_RESOURCE");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTssGetDataNoLimitAsync) {
    STUBBED("TSS content is unavailable, returning NO_RESOURCE without starting a request");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTssGetSmallStorage, SceInt32 requestId, void *data, SceSize maxSize, SceSize *contentLength, const void *option) {
    return get_tss_small_storage(emuenv.np.tus_state, export_name, requestId, data, maxSize, contentLength, option, false);
}

EXPORT(int, sceNpTssGetSmallStorageAsync, SceInt32 requestId, void *data, SceSize maxSize, SceSize *contentLength, const void *option) {
    return get_tss_small_storage(emuenv.np.tus_state, export_name, requestId, data, maxSize, contentLength, option, true);
}

EXPORT(int, sceNpTssGetStorage) {
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTssGetStorageAsync) {
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusAbortRequest, SceInt32 requestId) {
    auto &tus = emuenv.np.tus_state;
    std::lock_guard lock(tus.access_mutex);
    if (!tus.inited)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED);

    auto *request = get_request(tus, requestId);
    if (!request)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_INVALID_ID);

    if (!request->result)
        request->result = static_cast<SceInt32>(SCE_NP_COMMUNITY_ERROR_ABORTED);
    return 0;
}

EXPORT(int, sceNpTusAddAndGetVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusAddAndGetVariableAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusAddAndGetVariableVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusAddAndGetVariableVUserAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusChangeModeForOtherSaveDataOwners) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusCreateRequest, SceInt32 titleCtxId) {
    auto &tus = emuenv.np.tus_state;
    std::lock_guard lock(tus.access_mutex);
    if (!tus.inited)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED);
    if (!is_valid_title_context(tus, titleCtxId))
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_INVALID_ID);

    if (tus.requests.size() >= SCE_NP_TUS_MAX_CTX_NUM)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS);

    const SceUID requestId = emuenv.kernel.get_next_uid();
    if (requestId <= 0)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);

    if (!tus.requests.try_emplace(requestId, NpTusRequest{ titleCtxId, std::nullopt }).second)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);

    STUBBED("Simulated request handle, no network request is started");
    return requestId;
}

EXPORT(int, sceNpTusCreateTitleCtx, const np::CommunicationID *communicationId,
    const np::SceNpCommunicationPassphrase *passphrase, const np::SceNpId *selfNpId) {
    auto &tus = emuenv.np.tus_state;
    std::lock_guard lock(tus.access_mutex);
    if (!tus.inited)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED);

    if (tus.title_contexts.size() >= SCE_NP_TUS_MAX_CTX_NUM)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS);

    const SceUID titleCtxId = emuenv.kernel.get_next_uid();
    if (titleCtxId <= 0)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);

    if (!tus.title_contexts.try_emplace(titleCtxId, true).second)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);

    STUBBED("Simulated title context, communication credentials and user defaults are not resolved");
    return titleCtxId;
}

EXPORT(int, sceNpTusDeleteMultiSlotData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusDeleteMultiSlotDataAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusDeleteMultiSlotDataVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusDeleteMultiSlotDataVUserAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusDeleteMultiSlotVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusDeleteMultiSlotVariableAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusDeleteMultiSlotVariableVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusDeleteMultiSlotVariableVUserAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusDeleteRequest, SceInt32 requestId) {
    auto &tus = emuenv.np.tus_state;
    std::lock_guard lock(tus.access_mutex);
    if (!tus.inited)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED);

    auto *request = get_request(tus, requestId);
    if (!request)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_INVALID_ID);

    tus.requests.erase(requestId);
    LOG_DEBUG("sceNpTusDeleteRequest: requestId: {}", requestId);
    return 0;
}

EXPORT(int, sceNpTusDeleteTitleCtx, SceInt32 titleCtxId) {
    auto &tus = emuenv.np.tus_state;
    std::lock_guard lock(tus.access_mutex);
    if (!tus.inited)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED);

    if (!is_valid_title_context(tus, titleCtxId))
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_INVALID_ID);

    std::erase_if(tus.requests, [titleCtxId](const auto &request) {
        return request.second.title_context_id == titleCtxId;
    });
    tus.title_contexts.erase(titleCtxId);
    LOG_DEBUG("sceNpTusDeleteTitleCtx: titleCtxId: {}", titleCtxId);
    return 0;
}

EXPORT(int, sceNpTusGetData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetDataAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusGetDataVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetDataVUserAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusGetFriendsDataStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetFriendsDataStatusAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusGetFriendsVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetFriendsVariableAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusGetMultiSlotDataStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiSlotDataStatusAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusGetMultiSlotDataStatusVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiSlotDataStatusVUserAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusGetMultiSlotVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiSlotVariableAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusGetMultiSlotVariableVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiSlotVariableVUserAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusGetMultiUserDataStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiUserDataStatusAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusGetMultiUserDataStatusVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiUserDataStatusVUserAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusGetMultiUserVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiUserVariableAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusGetMultiUserVariableVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusGetMultiUserVariableVUserAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusInit, SceInt32 threadPriority, SceInt32 cpuAffinityMask, const void *option) {
    auto &tus = emuenv.np.tus_state;
    std::lock_guard lock(tus.access_mutex);
    if (tus.inited)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED);

    if (option)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT);

    tus.title_contexts.clear();
    tus.requests.clear();
    tus.inited = true;
    return STUBBED("Simulated TUS initialization, worker priority and affinity are ignored");
}

EXPORT(int, sceNpTusPollAsync, SceInt32 requestId, SceInt32 *result) {
    return get_async_result(emuenv.np.tus_state, export_name, requestId, result);
}

EXPORT(int, sceNpTusSetData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusSetDataAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusSetDataVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusSetDataVUserAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusSetMultiSlotVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusSetMultiSlotVariableAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusSetMultiSlotVariableVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusSetMultiSlotVariableVUserAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusSetTimeout) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusTerm) {
    auto &tus = emuenv.np.tus_state;
    std::lock_guard lock(tus.access_mutex);
    if (!tus.inited)
        return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED);

    tus.title_contexts.clear();
    tus.requests.clear();
    tus.inited = false;
    return 0;
}

EXPORT(int, sceNpTusTryAndSetVariable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusTryAndSetVariableAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusTryAndSetVariableVUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTusTryAndSetVariableVUserAsync) {
    STUBBED("TUS operation is unavailable, no asynchronous request is started");
    return RET_ERROR(SCE_NP_COMMUNITY_ERROR_NO_RESOURCE);
}

EXPORT(int, sceNpTusWaitAsync, SceInt32 requestId, SceInt32 *result) {
    return get_async_result(emuenv.np.tus_state, export_name, requestId, result);
}
