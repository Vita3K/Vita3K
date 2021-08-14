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

#include <cstdint>
#include <module/module.h>

using SceNpTrophyHandle = std::int32_t;
using SceNpTrophyID = std::int32_t;
using SceNpTrophyGroupId = std::int32_t;

#define SCE_NP_TROPHY_ERROR_UNKNOWN 0x80551600
#define SCE_NP_TROPHY_ERROR_NOT_INITIALIZED 0x80551601
#define SCE_NP_TROPHY_ERROR_ALREADY_INITIALIZED 0x80551602
#define SCE_NP_TROPHY_ERROR_NO_MEMORY 0x80551603
#define SCE_NP_TROPHY_ERROR_INVALID_ARGUMENT 0x80551604
#define SCE_NP_TROPHY_ERROR_INSUFFICIENT_BUFFER 0x80551605
#define SCE_NP_TROPHY_ERROR_EXCEEDS_MAX 0x80551606
#define SCE_NP_TROPHY_ERROR_ABORT 0x80551607
#define SCE_NP_TROPHY_ERROR_INVALID_HANDLE 0x80551608
#define SCE_NP_TROPHY_ERROR_INVALID_CONTEXT 0x80551609
#define SCE_NP_TROPHY_ERROR_INVALID_NPCOMMID 0x8055160a
#define SCE_NP_TROPHY_ERROR_INVALID_NPCOMMSIGN 0x8055160b
#define SCE_NP_TROPHY_ERROR_NPCOMMSIGN_VERIFICATION_FAILURE 0x8055160c
#define SCE_NP_TROPHY_ERROR_INVALID_GROUP_ID 0x8055160d
#define SCE_NP_TROPHY_ERROR_INVALID_TROPHY_ID 0x8055160e
#define SCE_NP_TROPHY_ERROR_TROPHY_ALREADY_UNLOCKED 0x8055160f
#define SCE_NP_TROPHY_ERROR_PLATINUM_CANNOT_UNLOCK 0x80551610
#define SCE_NP_TROPHY_ERROR_ACCOUNTID_NOT_MATCH 0x80551611
#define SCE_NP_TROPHY_ERROR_SETUP_REQUIRED 0x80551612
#define SCE_NP_TROPHY_ERROR_ALREADY_SETUP 0x80551613
#define SCE_NP_TROPHY_ERROR_BROKEN_DATA 0x80551614
#define SCE_NP_TROPHY_ERROR_INSUFFICIENT_EM_SPACE 0x80551615
#define SCE_NP_TROPHY_ERROR_CONTEXT_ALREADY_EXISTS 0x80551616
#define SCE_NP_TROPHY_ERROR_TRP_FILE_VERIFICATION_FAILURE 0x80551617
#define SCE_NP_TROPHY_ERROR_ICON_FILE_NOT_FOUND 0x80551618
#define SCE_NP_TROPHY_ERROR_TRP_FILE_NOT_FOUND 0x80551619
#define SCE_NP_TROPHY_ERROR_INVALID_TRP_FILE_FORMAT 0x8055161a
#define SCE_NP_TROPHY_ERROR_UNSUPPORTED_TRP_FILE 0x8055161b
#define SCE_NP_TROPHY_ERROR_INVALID_TROPHY_CONF_FORMAT 0x8055161c
#define SCE_NP_TROPHY_ERROR_UNSUPPORTED_TROPHY_CONF 0x8055161d
#define SCE_NP_TROPHY_ERROR_TROPHY_NOT_UNLOCKED 0x8055161e
#define SCE_NP_TROPHY_ERROR_UNLOCK_DENIED 0x8055161f
#define SCE_NP_TROPHY_ERROR_INSUFFICIENT_MC_SPACE 0x80551620
#define SCE_NP_TROPHY_ERROR_DEBUG_FAILURE 0x80551621

#define SCE_NP_TROPHY_GAME_TITLE_MAX_SIZE 128
#define SCE_NP_TROPHY_GAME_DESCR_MAX_SIZE 1024
#define SCE_NP_TROPHY_NAME_MAX_SIZE 128
#define SCE_NP_TROPHY_DESCR_MAX_SIZE 1024

struct SceNpTrophyGameDetails {
    SceSize size;
    SceUInt32 numGroups;
    SceUInt32 numTrophies;
    SceUInt32 numPlatinum;
    SceUInt32 numGold;
    SceUInt32 numSilver;
    SceUInt32 numBronze;
    SceChar8 title[SCE_NP_TROPHY_GAME_TITLE_MAX_SIZE];
    SceChar8 description[SCE_NP_TROPHY_GAME_DESCR_MAX_SIZE];
};

struct SceNpTrophyGameData {
    SceSize size;
    SceUInt32 unlockedTrophies;
    SceUInt32 unlockedPlatinum;
    SceUInt32 unlockedGold;
    SceUInt32 unlockedSilver;
    SceUInt32 unlockedBronze;
    SceUInt32 progressPercentage;
};

struct SceNpTrophyDetails {
    SceSize size;
    SceNpTrophyID trophyId;
    np::trophy::SceNpTrophyGrade trophyGrade;
    SceNpTrophyGroupId groupId;
    SceBool hidden;
    SceChar8 name[SCE_NP_TROPHY_NAME_MAX_SIZE];
    SceChar8 description[SCE_NP_TROPHY_DESCR_MAX_SIZE];
};

struct SceNpTrophyData {
    SceSize size;
    SceNpTrophyID trophyId;
    SceBool unlocked;
    SceUInt8 reserved[4];
    SceRtcTick timestamp;
};

BRIDGE_DECL(sceNpTrophyAbortHandle)
BRIDGE_DECL(sceNpTrophyCreateContext)
BRIDGE_DECL(sceNpTrophyCreateHandle)
BRIDGE_DECL(sceNpTrophyDestroyContext)
BRIDGE_DECL(sceNpTrophyDestroyHandle)
BRIDGE_DECL(sceNpTrophyGetGameIcon)
BRIDGE_DECL(sceNpTrophyGetGameInfo)
BRIDGE_DECL(sceNpTrophyGetGroupIcon)
BRIDGE_DECL(sceNpTrophyGetGroupInfo)
BRIDGE_DECL(sceNpTrophyGetTrophyIcon)
BRIDGE_DECL(sceNpTrophyGetTrophyInfo)
BRIDGE_DECL(sceNpTrophyGetTrophyUnlockState)
BRIDGE_DECL(sceNpTrophyInit)
BRIDGE_DECL(sceNpTrophyTerm)
BRIDGE_DECL(sceNpTrophyUnlockTrophy)
