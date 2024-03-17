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
 * @file license.cpp
 * @brief Sony `NpDrm` license and `work.bin` handling
 */

#include <emuenv/state.h>

#include <util/bytes.h>
#include <util/log.h>

#include <miniz.h>
#include <zrif2rif.h>

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

static bool open_license(const fs::path &license_path, SceNpDrmLicense &license_buf) {
    memset(&license_buf, 0, sizeof(SceNpDrmLicense));
    fs::ifstream license(license_path, std::ios::in | std::ios::binary);
    if (license.is_open()) {
        license.read((char *)&license_buf, sizeof(SceNpDrmLicense));
        license.close();
        return true;
    }

    return false;
}

bool copy_license(EmuEnvState &emuenv, const fs::path &license_path) {
    SceNpDrmLicense license_buf;
    if (open_license(license_path, license_buf)) {
        emuenv.license_content_id = license_buf.content_id;
        emuenv.license_title_id = emuenv.license_content_id.substr(7, 9);
        const auto dst_path{ emuenv.pref_path / "ux0/license" / emuenv.license_title_id };
        fs::create_directories(dst_path);

        const auto license_dst_path{ dst_path / fmt::format("{}.rif", emuenv.license_content_id) };
        if (license_path != license_dst_path) {
            fs::copy_file(license_path, license_dst_path, fs::copy_options::overwrite_existing);
            if (fs::exists(license_dst_path)) {
                LOG_INFO("Success copy license file to: {}", license_dst_path);
                return true;
            } else
                LOG_ERROR("Fail copy license file to: {}", license_dst_path);
        } else
            LOG_ERROR("Source and destination license is same at: {}", license_path);
    } else
        LOG_ERROR("License file is corrupted at: {}", license_path);

    return false;
}

int32_t get_license_sku_flag(EmuEnvState &emuenv, const std::string &content_id) {
    int32_t sku_flag;
    const auto title_id = content_id.substr(7, 9);
    const auto license_path{ emuenv.pref_path / "ux0/license" / title_id / fmt::format("{}.rif", content_id) };
    SceNpDrmLicense license_buf;
    if (open_license(license_path, license_buf)) {
        sku_flag = byte_swap(license_buf.sku_flag);
    } else {
        const auto RETAIL_APP_PATH{ emuenv.pref_path / "ux0/app" / title_id / "sce_sys/retail/livearea" };
        if (fs::exists(RETAIL_APP_PATH))
            sku_flag = 1;
        else
            sku_flag = 0;
        if (fs::exists(license_path)) {
            fs::remove(license_path);
            LOG_WARN("License file is corrupted at: {}, using default value.", license_path);
        }
    }

    return sku_flag;
}

bool create_license(EmuEnvState &emuenv, const std::string &zRIF) {
    fs::create_directories(emuenv.cache_path);

    // Create temp license file
    const auto temp_license_path = emuenv.cache_path / "temp_licence.rif";
    fs::ofstream temp_file(temp_license_path, std::ios::out | std::ios::binary);
    zrif2rif(zRIF, temp_file);
    auto res = copy_license(emuenv, temp_license_path);
    fs::remove(temp_license_path);
    return res;
}
