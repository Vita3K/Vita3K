// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

/**
 * @file sce_types.h
 * @brief SCE metadata types embedded into SCE binaries
 */

#pragma once

#include <emuenv/state.h>

#include <util/log.h>

#include <map>
#include <string>

struct SceNpDrmLicense {
    uint16_t version; // 0x00
    uint16_t version_flag; // 0x02
    uint16_t type; // 0x04
    uint16_t flags; // 0x06
    uint64_t aid; // 0x08
    char content_id[0x30]; // 0x10
    uint8_t key_table[0x10]; // 0x40
    uint8_t key[0x10]; // 0x50
    uint64_t start_time; // 0x60
    uint64_t expiration_time; // 0x68
    uint8_t ecdsa_signature[0x28]; // 0x70
    uint64_t flags2; // 0x98
    uint8_t key2[0x10]; // 0xA0
    uint8_t unk_B0[0x10]; // 0xB0
    uint8_t openpsid[0x10]; // 0xC0
    uint8_t unk_D0[0x10]; // 0xD0
    uint8_t cmd56_handshake[0x14]; // 0xE0
    uint32_t unk_F4; // 0xF4
    uint32_t unk_F8; // 0xF8
    int32_t sku_flag; // 0xFC
    uint8_t rsa_signature[0x100]; // 0x100
};

struct License {
    std::map<std::string, SceNpDrmLicense> rif;
};

bool copy_license(EmuEnvState &emuenv, const fs::path &license_path);
bool create_license(EmuEnvState &emuenv, const std::string &zRIF);
void get_license(EmuEnvState &emuenv, const std::string &title_id, const std::string &content_id);
