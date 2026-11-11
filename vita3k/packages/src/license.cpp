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

/**
 * @file license.cpp
 * @brief Sony `NpDrm` license and `work.bin` handling
 */

#include <emuenv/state.h>

#include <packages/license.h>

#include <util/bytes.h>
#include <util/log.h>

#include <zRIF/keyflate.h>

#include <b64/cdecode.h>
#include <b64/cencode.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

static bool decode_zrif(const std::string &zRIF, std::array<uint8_t, 512> &license) {
    if (zRIF.empty() || zRIF.size() > 2048)
        return false;

    std::array<char, 2048> buf{};
    base64_decodestate state;
    base64_init_decodestate(&state);
    const int decoded_len = base64_decode_block(zRIF.c_str(), static_cast<int>(zRIF.size()), buf.data(), &state);
    if (decoded_len <= 0)
        return false;

    return inflateKey(reinterpret_cast<const uint8_t *>(buf.data()), decoded_len, license.data(), license.size()) == license.size();
}

bool validate_zrif(const std::string &zRIF) {
    std::array<uint8_t, 512> license{};
    return decode_zrif(zRIF, license);
}

std::string rif2zrif(std::ifstream &binfile) {
    std::array<uint8_t, 512> license{};
    binfile.seekg(0, std::ios::beg);
    if (!binfile.read(reinterpret_cast<char *>(license.data()), license.size()) || binfile.peek() != std::char_traits<char>::eof()) {
        LOG_ERROR("Cannot convert an unreadable or invalid-size RIF license");
        return {};
    }
    std::array<uint8_t, 2048> compressed{};
    const int length = deflateKey(license.data(), license.size(), compressed.data(), compressed.size());
    if (length <= 0 || static_cast<size_t>(length) > compressed.size() - 2) {
        LOG_ERROR("Cannot compress RIF license");
        return {};
    }
    const int padded = (length + 2) / 3 * 3;
    std::vector<char> encoded(static_cast<size_t>(padded) * 2 + 16);
    base64_encodestate state;
    base64_init_encodestate(&state);
    int size = base64_encode_block(reinterpret_cast<const char *>(compressed.data()), padded, encoded.data(), &state);
    size += base64_encode_blockend(encoded.data() + size, &state);
    return std::string(encoded.data(), size);
}

static bool open_license(const fs::path &license_path, SceNpDrmLicense &license_buf) {
    memset(&license_buf, 0, sizeof(SceNpDrmLicense));
    std::ifstream license(license_path, std::ios::in | std::ios::binary);
    if (license.is_open()) {
        return static_cast<bool>(license.read(reinterpret_cast<char *>(&license_buf), sizeof(SceNpDrmLicense)));
    }

    return false;
}

bool copy_license(EmuEnvState &emuenv, const fs::path &license_path) {
    SceNpDrmLicense license_buf;
    if (open_license(license_path, license_buf)) {
        const auto end = std::find(std::begin(license_buf.content_id), std::end(license_buf.content_id), '\0');
        emuenv.license_content_id.assign(std::begin(license_buf.content_id), end);
        if (emuenv.license_content_id.size() != 36 || emuenv.license_content_id[6] != '-' || emuenv.license_content_id[16] != '_' || emuenv.license_content_id[19] != '-'
            || !std::all_of(emuenv.license_content_id.begin(), emuenv.license_content_id.end(), [](unsigned char c) {
                   return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
               })) {
            LOG_ERROR("License has an invalid content ID: {}", license_path);
            return false;
        }
        emuenv.license_title_id = emuenv.license_content_id.substr(7, 9);
        const auto dst_path{ emuenv.vita_fs_path / "ux0/license" / emuenv.license_title_id };
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
            return true;
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
    const auto license_path{ emuenv.vita_fs_path / "ux0/license" / title_id / fmt::format("{}.rif", content_id) };
    if (!open_license(license_path, license_buf)) {
        if (fs::exists(license_path))
            fs::remove(license_path);

        LOG_WARN("License file is corrupted or missing at: {}, using default value.", license_path);
        const auto RETAIL_APP_PATH{ emuenv.vita_fs_path / "ux0/app" / title_id / "sce_sys/retail/livearea" };
        if (fs::exists(RETAIL_APP_PATH))
            license_buf.sku_flag = 1;
        else
            license_buf.sku_flag = 0;
    } else
        license_buf.sku_flag = byte_swap(license_buf.sku_flag); // Convert to little endian
}

bool create_license(EmuEnvState &emuenv, const std::string &zRIF) {
    std::array<uint8_t, 512> license{};
    if (!decode_zrif(zRIF, license)) {
        LOG_ERROR("Invalid zRIF key provided");
        return false;
    }

    fs::create_directories(emuenv.cache_path);

    // Create a temp license file
    const auto temp_license_path = emuenv.cache_path / "temp_licence.rif";
    std::ofstream temp_file(temp_license_path, std::ios::out | std::ios::binary);
    if (!temp_file.is_open()) {
        LOG_ERROR("Failed to create temp license file at: {}", temp_license_path);
        return false;
    }

    if (!temp_file.write(reinterpret_cast<const char *>(license.data()), license.size())) {
        LOG_ERROR("Failed to write temporary license file at: {}", temp_license_path);
        temp_file.close();
        fs::remove(temp_license_path);
        return false;
    }
    temp_file.close();
    if (!temp_file) {
        LOG_ERROR("Failed to finish temporary license file at: {}", temp_license_path);
        fs::remove(temp_license_path);
        return false;
    }
    auto res = copy_license(emuenv, temp_license_path);
    fs::remove(temp_license_path);
    return res;
}
