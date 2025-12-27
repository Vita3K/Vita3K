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

#pragma once

#include <mem/ptr.h>
#include <util/types.h>

#include <cstdint>
#include <cstring>

namespace np {

#define SCE_NP_ONLINEID_MIN_LENGTH 3
#define SCE_NP_ONLINEID_MAX_LENGTH 16

struct SceNpOnlineId {
    char data[SCE_NP_ONLINEID_MAX_LENGTH];
    char term;
    char dummy[3];
};
static_assert(sizeof(SceNpOnlineId) == 0x14, "SceNpOnlineId is an invalid size");

struct SceNpIdOptParam {
    SceUChar8 unknown[4];
    char platformType[4];
};
static_assert(sizeof(SceNpIdOptParam) == 0x8, "SceNpIdOptParam is an invalid size");

struct SceNpId {
    SceNpOnlineId handle;
    SceNpIdOptParam opt;
    SceInt8 isIdValid;
    SceUChar8 reserved[7];
};
static_assert(sizeof(SceNpId) == 0x24, "SceNpId is an invalid size");

struct CommunicationID {
    char data[9];
    char term;
    std::uint8_t num;
    char dummy;
};

#define SCE_NP_COMMUNICATION_PASSPHRASE_SIZE (128)
struct SceNpCommunicationPassphrase {
    SceUChar8 data[SCE_NP_COMMUNICATION_PASSPHRASE_SIZE];
};

inline bool operator==(const CommunicationID &lhs, const CommunicationID &rhs) {
    return (strncmp(lhs.data, rhs.data, 9) == 0) && (lhs.num == rhs.num);
}

struct CommunicationConfig {
    Ptr<CommunicationID> comm_id;
    Ptr<void> unk0;
    Ptr<void> unk1;
};

constexpr auto SCE_NP_TROPHY_INVALID_TROPHY_ID = -1;

enum class NpTrophyError {
    TROPHY_ERROR_NONE = 0,
    TROPHY_CONTEXT_EXIST = 1,
    TROPHY_CONTEXT_FILE_NON_EXIST = 2,
    TROPHY_ID_INVALID = 3,
    TROPHY_ALREADY_UNLOCKED = 4,
    TROPHY_PLATINUM_IS_UNBREAKABLE = 5, // Platinum is unbreakable
};

} // namespace np
