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

// SceNpScore
BRIDGE_DECL(sceNpScoreAbortRequest)
BRIDGE_DECL(sceNpScoreCensorComment)
BRIDGE_DECL(sceNpScoreCensorCommentAsync)
BRIDGE_DECL(sceNpScoreChangeModeForOtherSaveDataOwners)
BRIDGE_DECL(sceNpScoreCreateRequest)
BRIDGE_DECL(sceNpScoreCreateTitleCtx)
BRIDGE_DECL(sceNpScoreDeleteRequest)
BRIDGE_DECL(sceNpScoreDeleteTitleCtx)
BRIDGE_DECL(sceNpScoreGetBoardInfo)
BRIDGE_DECL(sceNpScoreGetBoardInfoAsync)
BRIDGE_DECL(sceNpScoreGetFriendsRanking)
BRIDGE_DECL(sceNpScoreGetFriendsRankingAsync)
BRIDGE_DECL(sceNpScoreGetGameData)
BRIDGE_DECL(sceNpScoreGetGameDataAsync)
BRIDGE_DECL(sceNpScoreGetRankingByNpId)
BRIDGE_DECL(sceNpScoreGetRankingByNpIdAsync)
BRIDGE_DECL(sceNpScoreGetRankingByNpIdPcId)
BRIDGE_DECL(sceNpScoreGetRankingByNpIdPcIdAsync)
BRIDGE_DECL(sceNpScoreGetRankingByRange)
BRIDGE_DECL(sceNpScoreGetRankingByRangeAsync)
BRIDGE_DECL(sceNpScoreInit)
BRIDGE_DECL(sceNpScorePollAsync)
BRIDGE_DECL(sceNpScoreRecordGameData)
BRIDGE_DECL(sceNpScoreRecordGameDataAsync)
BRIDGE_DECL(sceNpScoreRecordScore)
BRIDGE_DECL(sceNpScoreRecordScoreAsync)
BRIDGE_DECL(sceNpScoreSanitizeComment)
BRIDGE_DECL(sceNpScoreSanitizeCommentAsync)
BRIDGE_DECL(sceNpScoreSetPlayerCharacterId)
BRIDGE_DECL(sceNpScoreSetTimeout)
BRIDGE_DECL(sceNpScoreTerm)
BRIDGE_DECL(sceNpScoreWaitAsync)
