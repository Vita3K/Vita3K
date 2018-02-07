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

#include <SceNpScore/exports.h>

EXPORT(int, sceNpScoreAbortRequest) {
    return unimplemented("sceNpScoreAbortRequest");
}

EXPORT(int, sceNpScoreCensorComment) {
    return unimplemented("sceNpScoreCensorComment");
}

EXPORT(int, sceNpScoreCensorCommentAsync) {
    return unimplemented("sceNpScoreCensorCommentAsync");
}

EXPORT(int, sceNpScoreChangeModeForOtherSaveDataOwners) {
    return unimplemented("sceNpScoreChangeModeForOtherSaveDataOwners");
}

EXPORT(int, sceNpScoreCreateRequest) {
    return unimplemented("sceNpScoreCreateRequest");
}

EXPORT(int, sceNpScoreCreateTitleCtx) {
    return unimplemented("sceNpScoreCreateTitleCtx");
}

EXPORT(int, sceNpScoreDeleteRequest) {
    return unimplemented("sceNpScoreDeleteRequest");
}

EXPORT(int, sceNpScoreDeleteTitleCtx) {
    return unimplemented("sceNpScoreDeleteTitleCtx");
}

EXPORT(int, sceNpScoreGetBoardInfo) {
    return unimplemented("sceNpScoreGetBoardInfo");
}

EXPORT(int, sceNpScoreGetBoardInfoAsync) {
    return unimplemented("sceNpScoreGetBoardInfoAsync");
}

EXPORT(int, sceNpScoreGetFriendsRanking) {
    return unimplemented("sceNpScoreGetFriendsRanking");
}

EXPORT(int, sceNpScoreGetFriendsRankingAsync) {
    return unimplemented("sceNpScoreGetFriendsRankingAsync");
}

EXPORT(int, sceNpScoreGetGameData) {
    return unimplemented("sceNpScoreGetGameData");
}

EXPORT(int, sceNpScoreGetGameDataAsync) {
    return unimplemented("sceNpScoreGetGameDataAsync");
}

EXPORT(int, sceNpScoreGetRankingByNpId) {
    return unimplemented("sceNpScoreGetRankingByNpId");
}

EXPORT(int, sceNpScoreGetRankingByNpIdAsync) {
    return unimplemented("sceNpScoreGetRankingByNpIdAsync");
}

EXPORT(int, sceNpScoreGetRankingByNpIdPcId) {
    return unimplemented("sceNpScoreGetRankingByNpIdPcId");
}

EXPORT(int, sceNpScoreGetRankingByNpIdPcIdAsync) {
    return unimplemented("sceNpScoreGetRankingByNpIdPcIdAsync");
}

EXPORT(int, sceNpScoreGetRankingByRange) {
    return unimplemented("sceNpScoreGetRankingByRange");
}

EXPORT(int, sceNpScoreGetRankingByRangeAsync) {
    return unimplemented("sceNpScoreGetRankingByRangeAsync");
}

EXPORT(int, sceNpScoreInit) {
    return unimplemented("sceNpScoreInit");
}

EXPORT(int, sceNpScorePollAsync) {
    return unimplemented("sceNpScorePollAsync");
}

EXPORT(int, sceNpScoreRecordGameData) {
    return unimplemented("sceNpScoreRecordGameData");
}

EXPORT(int, sceNpScoreRecordGameDataAsync) {
    return unimplemented("sceNpScoreRecordGameDataAsync");
}

EXPORT(int, sceNpScoreRecordScore) {
    return unimplemented("sceNpScoreRecordScore");
}

EXPORT(int, sceNpScoreRecordScoreAsync) {
    return unimplemented("sceNpScoreRecordScoreAsync");
}

EXPORT(int, sceNpScoreSanitizeComment) {
    return unimplemented("sceNpScoreSanitizeComment");
}

EXPORT(int, sceNpScoreSanitizeCommentAsync) {
    return unimplemented("sceNpScoreSanitizeCommentAsync");
}

EXPORT(int, sceNpScoreSetPlayerCharacterId) {
    return unimplemented("sceNpScoreSetPlayerCharacterId");
}

EXPORT(int, sceNpScoreSetTimeout) {
    return unimplemented("sceNpScoreSetTimeout");
}

EXPORT(int, sceNpScoreTerm) {
    return unimplemented("sceNpScoreTerm");
}

EXPORT(int, sceNpScoreWaitAsync) {
    return unimplemented("sceNpScoreWaitAsync");
}

BRIDGE_IMPL(sceNpScoreAbortRequest)
BRIDGE_IMPL(sceNpScoreCensorComment)
BRIDGE_IMPL(sceNpScoreCensorCommentAsync)
BRIDGE_IMPL(sceNpScoreChangeModeForOtherSaveDataOwners)
BRIDGE_IMPL(sceNpScoreCreateRequest)
BRIDGE_IMPL(sceNpScoreCreateTitleCtx)
BRIDGE_IMPL(sceNpScoreDeleteRequest)
BRIDGE_IMPL(sceNpScoreDeleteTitleCtx)
BRIDGE_IMPL(sceNpScoreGetBoardInfo)
BRIDGE_IMPL(sceNpScoreGetBoardInfoAsync)
BRIDGE_IMPL(sceNpScoreGetFriendsRanking)
BRIDGE_IMPL(sceNpScoreGetFriendsRankingAsync)
BRIDGE_IMPL(sceNpScoreGetGameData)
BRIDGE_IMPL(sceNpScoreGetGameDataAsync)
BRIDGE_IMPL(sceNpScoreGetRankingByNpId)
BRIDGE_IMPL(sceNpScoreGetRankingByNpIdAsync)
BRIDGE_IMPL(sceNpScoreGetRankingByNpIdPcId)
BRIDGE_IMPL(sceNpScoreGetRankingByNpIdPcIdAsync)
BRIDGE_IMPL(sceNpScoreGetRankingByRange)
BRIDGE_IMPL(sceNpScoreGetRankingByRangeAsync)
BRIDGE_IMPL(sceNpScoreInit)
BRIDGE_IMPL(sceNpScorePollAsync)
BRIDGE_IMPL(sceNpScoreRecordGameData)
BRIDGE_IMPL(sceNpScoreRecordGameDataAsync)
BRIDGE_IMPL(sceNpScoreRecordScore)
BRIDGE_IMPL(sceNpScoreRecordScoreAsync)
BRIDGE_IMPL(sceNpScoreSanitizeComment)
BRIDGE_IMPL(sceNpScoreSanitizeCommentAsync)
BRIDGE_IMPL(sceNpScoreSetPlayerCharacterId)
BRIDGE_IMPL(sceNpScoreSetTimeout)
BRIDGE_IMPL(sceNpScoreTerm)
BRIDGE_IMPL(sceNpScoreWaitAsync)
