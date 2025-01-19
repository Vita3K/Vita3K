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

/**
 * @file license.cpp
 * @brief Sony `NpDrm` license and `work.bin` handling
 */

#include <emuenv/state.h>

#include <packages/license.h>

#include <util/bytes.h>
#include <util/log.h>

#include <zrif2rif.h>

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

void get_license(EmuEnvState &emuenv, const std::string &title_id, const std::string &content_id) {
    // Skip if it's not a retail game or already have a license
    if (!title_id.starts_with("PCS") || emuenv.license.rif.contains(title_id))
        return;

    // Get license buffer corresponding to the title id
    auto &license_buf = emuenv.license.rif[title_id];
    license_buf = {};

    // Open license file
    const auto license_path{ emuenv.pref_path / "ux0/license" / title_id / fmt::format("{}.rif", content_id) };
    if (!open_license(license_path, license_buf)) {
        if (fs::exists(license_path))
            fs::remove(license_path);

        LOG_WARN("License file is corrupted or missing at: {}, using default value.", license_path);
        const auto RETAIL_APP_PATH{ emuenv.pref_path / "ux0/app" / title_id / "sce_sys/retail/livearea" };
        if (fs::exists(RETAIL_APP_PATH))
            license_buf.sku_flag = 1;
        else
            license_buf.sku_flag = 0;
    } else
        license_buf.sku_flag = byte_swap(license_buf.sku_flag); // Convert to little endian
}

bool create_license(EmuEnvState &emuenv, const std::string &zRIF) {
    fs::create_directories(emuenv.cache_path);

    // Create a temp license file
    const auto temp_license_path = emuenv.cache_path / "temp_licence.rif";
    fs::ofstream temp_file(temp_license_path, std::ios::out | std::ios::binary);
    if (!temp_file.is_open()) {
        LOG_ERROR("Failed to create temp license file at: {}", temp_license_path);
        return false;
    }

    // Convert zRIF to RIF
    zrif2rif(zRIF, temp_file);
    auto res = copy_license(emuenv, temp_license_path);
    fs::remove(temp_license_path);
    return res;
}
