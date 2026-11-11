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
 * @file pkg.cpp
 * @brief PlayStation Vita software package (`.pkg`) handling
 */

#include <openssl/evp.h>

#include <io/functions.h>

#include <config/state.h>
#include <emuenv/state.h>

#include <packages/functions.h>
#include <packages/license.h>
#include <packages/pkg.h>
#include <packages/sce_types.h>
#include <packages/sfo.h>
#include <pfsmgr/functions.h>

#include <util/bytes.h>
#include <util/log.h>

// Credits to mmozeiko https://github.com/mmozeiko/pkg2zip

static void ctr_init(uint8_t *counter, uint8_t *iv, uint64_t n) {
    for (int i = 15; i >= 0; i--) {
        n = n + iv[i];
        counter[i] = static_cast<uint8_t>(n);
        n >>= 8;
    }
}

bool install_nonpdrm_license(EmuEnvState &emuenv, const fs::path &drmlicpath, const fs::path &title_path) {
    SceNpDrmLicense license{};
    std::ifstream input(drmlicpath, std::ios::binary);
    if (!input.read(reinterpret_cast<char *>(&license), sizeof(license)) || input.peek() != std::char_traits<char>::eof()) {
        LOG_ERROR("Cannot read valid license '{}'", drmlicpath);
        return false;
    }
    input.close();
    const auto end = std::find(std::begin(license.content_id), std::end(license.content_id), '\0');
    const std::string content_id(std::begin(license.content_id), end);
    const auto parent = title_path.parent_path();
    const bool theme = parent.filename() == "theme";
    const bool addcont = parent.parent_path().filename() == "addcont";
    if (content_id.size() != 36 || content_id[6] != '-' || content_id[16] != '_' || content_id[19] != '-'
        || (theme && title_path.filename() != content_id)
        || (addcont && (parent.filename() != content_id.substr(7, 9) || title_path.filename() != content_id.substr(20)))
        || (!theme && !addcont && title_path.filename() != content_id.substr(7, 9))) {
        LOG_ERROR("License '{}' does not match installed content '{}'", drmlicpath, title_path);
        return false;
    }
    const auto mode = theme ? pfsmgr::Mode::ThemeData : addcont ? pfsmgr::Mode::AcidDirectory
                                                                : pfsmgr::Mode::GameData;
    try {
        const auto files_db = title_path / "sce_pfs" / "files.db";
        const auto unicv_db = title_path / "sce_pfs" / "unicv.db";
        const bool has_files = fs::exists(fs::symlink_status(files_db));
        const bool has_unicv = fs::exists(fs::symlink_status(unicv_db));
        if (has_files != has_unicv) {
            LOG_ERROR("Cannot authenticate installed PFS '{}': incomplete PFS databases", title_path);
            return false;
        }
        if (has_files) {
            pfsmgr::AesKey key{};
            std::copy(std::begin(license.key), std::end(license.key), key.begin());
            pfsmgr::Layer layer(title_path, key, pfsmgr::make_key_material(), mode);
        }
    } catch (const std::runtime_error &error) {
        LOG_ERROR("Cannot authenticate installed PFS '{}': {}", title_path, error.what());
        return false;
    }
    return copy_license(emuenv, drmlicpath);
}

bool install_pkg(const fs::path &pkg_path, EmuEnvState &emuenv, std::string &p_zRIF, const std::function<void(float)> &progress_callback) {
    FILE *infile = FOPEN(pkg_path.c_str(), "rb");
    if (!infile) {
        LOG_CRITICAL("Failed to load pkg file in path: {}", fs_utils::path_to_utf8(pkg_path));
        return false;
    }

    fseek(infile, 0, SEEK_END);
    const uint64_t pkg_size = ftell(infile);

    PkgHeader pkg_header;
    PkgExtHeader ext_header;
    fseek(infile, 0, SEEK_SET);
    fread(reinterpret_cast<void *>(&pkg_header), sizeof(PkgHeader), 1, infile);
    fseek(infile, sizeof(PkgHeader), SEEK_SET);
    fread(reinterpret_cast<char *>(&ext_header), sizeof(PkgExtHeader), 1, infile);

    if (progress_callback)
        progress_callback(0);

    if (byte_swap(pkg_header.magic) != 0x7F504b47 && byte_swap(ext_header.magic) != 0x7F657874) {
        LOG_ERROR("Not a valid pkg file!");
        return false;
    }

    if (pkg_size < byte_swap(pkg_header.total_size)) {
        LOG_ERROR("The pkg file is too small");
        return false;
    }

    if (pkg_size < byte_swap(pkg_header.data_offset) + byte_swap(pkg_header.file_count) * 32) {
        LOG_ERROR("The pkg file is too small");
        return false;
    }

    uint32_t info_offset = byte_swap(pkg_header.info_offset);
    uint32_t content_type = 0;
    uint32_t sfo_offset = 0;
    uint32_t sfo_size = 0;
    uint32_t items_offset = 0;

    for (uint32_t i = 0; i < byte_swap(pkg_header.info_count); i++) {
        uint32_t block[4];
        fseek(infile, info_offset, SEEK_SET);
        fread(block, sizeof(block), 1, infile);

        auto type = byte_swap(block[0]);
        auto size = byte_swap(block[1]);

        switch (type) {
        case 2:
            content_type = byte_swap(block[2]);
            break;
        case 13:
            items_offset = byte_swap(block[2]);
            break;
        case 14:
            sfo_offset = byte_swap(block[2]);
            sfo_size = byte_swap(block[3]);
            break;
        default:
            break;
        }

        info_offset += 2 * sizeof(uint32_t) + size;
    }

    PkgType type;

    switch (content_type) {
    case 0x15:
        type = PkgType::PKG_TYPE_VITA_APP;
        break;
    case 0x16:
        type = PkgType::PKG_TYPE_VITA_DLC;
        break;
    case 0x1F:
        type = PkgType::PKG_TYPE_VITA_THEME;
        break;
    default:
        LOG_ERROR("Unsupported content type: {}", content_type);
        return false;
        break;
    }

    auto key_type = byte_swap(ext_header.data_type2) & 7;

    uint8_t main_key[16];
    const uint8_t *pkg_vita_key = nullptr;
    switch (key_type) {
    case 2:
        pkg_vita_key = pkg_vita_2;
        break;
    case 3:
        pkg_vita_key = pkg_vita_3;
        break;
    case 4:
        pkg_vita_key = pkg_vita_4;
        break;
    default:
        LOG_ERROR("Unknown encryption key");
        return false;
        break;
    }

    EVP_CIPHER_CTX *cipher_ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER *cipher_CTR = EVP_CIPHER_fetch(nullptr, "AES-128-CTR", nullptr);
    EVP_CIPHER *cipher_ECB = EVP_CIPHER_fetch(nullptr, "AES-128-ECB", nullptr);
    int dec_len = 0;

    auto evp_cleanup = [&]() {
        EVP_CIPHER_CTX_free(cipher_ctx);
        EVP_CIPHER_free(cipher_CTR);
        EVP_CIPHER_free(cipher_ECB);
    };

    // get the main key
    EVP_EncryptInit_ex(cipher_ctx, cipher_ECB, nullptr, pkg_vita_key, nullptr);
    EVP_CIPHER_CTX_set_padding(cipher_ctx, 0);
    EVP_EncryptUpdate(cipher_ctx, main_key, &dec_len, pkg_header.pkg_data_iv, 0x10);
    EVP_EncryptFinal_ex(cipher_ctx, main_key + dec_len, &dec_len);

    std::vector<uint8_t> sfo_buffer(sfo_size);
    if (fseek(infile, sfo_offset, SEEK_SET) != 0 || fread(sfo_buffer.data(), sfo_buffer.size(), 1, infile) != 1) {
        LOG_ERROR("Failed to read param.sfo from {}", fs_utils::path_to_utf8(pkg_path));
        fclose(infile);
        evp_cleanup();
        return false;
    }

    if (!sfo::get_param_info(emuenv.app_info, sfo_buffer, emuenv.cfg.sys_lang)) {
        LOG_ERROR("The param.sfo in {} is invalid, not installing it", fs_utils::path_to_utf8(pkg_path));
        evp_cleanup();
        fclose(infile);
        return false;
    }

    const bool is_theme = type == PkgType::PKG_TYPE_VITA_THEME;
    const std::string &dest_id = is_theme ? emuenv.app_info.app_content_id : emuenv.app_info.app_title_id;
    if (dest_id.empty() || (type == PkgType::PKG_TYPE_VITA_DLC && emuenv.app_info.app_content_id.size() <= 20)) {
        LOG_ERROR("The param.sfo in {} has no usable title or content ID, not installing it", fs_utils::path_to_utf8(pkg_path));
        evp_cleanup();
        fclose(infile);
        return false;
    }

    if (type == PkgType::PKG_TYPE_VITA_DLC) {
        emuenv.app_info.app_content_id.erase(0, 20);
    }

    if (type == PkgType::PKG_TYPE_VITA_APP && emuenv.app_info.app_category.starts_with("gp")) {
        type = PkgType::PKG_TYPE_VITA_PATCH;
    }

    auto path{ emuenv.vita_fs_path / "ux0" };

    switch (type) {
    case PkgType::PKG_TYPE_VITA_APP:
        path /= fs::path("app") / emuenv.app_info.app_title_id;
        if (fs::exists(path))
            fs::remove_all(path);
        emuenv.app_info.app_title += " (App)";
        break;
    case PkgType::PKG_TYPE_VITA_DLC:
        path /= fs::path("addcont") / emuenv.app_info.app_title_id / emuenv.app_info.app_content_id;
        emuenv.app_info.app_title += " (DLC)";
        break;
    case PkgType::PKG_TYPE_VITA_PATCH:
        path /= fs::path("patch") / emuenv.app_info.app_title_id;
        emuenv.app_info.app_title += " (Update)";
        break;
    case PkgType::PKG_TYPE_VITA_THEME:
        path /= fs::path("theme") / emuenv.app_info.app_content_id;
        emuenv.app_info.app_category = "theme";
        emuenv.app_info.app_title += " (Theme)";
        break;
    }

    auto decrypt_aes_ctr = [&](uint32_t offset, unsigned char *data, size_t size) {
        uint8_t counter[0x10];
        ctr_init(counter, pkg_header.pkg_data_iv, offset);
        EVP_DecryptInit_ex(cipher_ctx, cipher_CTR, nullptr, main_key, counter);
        EVP_CIPHER_CTX_set_padding(cipher_ctx, 0);
        EVP_DecryptUpdate(cipher_ctx, data, &dec_len, data, size);
        EVP_DecryptFinal_ex(cipher_ctx, data + dec_len, &dec_len);
    };

    std::vector<uint8_t> buffer(0x10000);
    const auto file_count = byte_swap(pkg_header.file_count);
    for (uint32_t i = 0; i < file_count; i++) {
        PkgEntry entry;
        uint64_t file_offset = items_offset + i * 32;
        fseek(infile, byte_swap(pkg_header.data_offset) + file_offset, SEEK_SET);
        fread(&entry, sizeof(PkgEntry), 1, infile);

        decrypt_aes_ctr(file_offset / 16, reinterpret_cast<unsigned char *>(&entry), sizeof(PkgEntry));

        if (pkg_size < byte_swap(pkg_header.data_offset) + byte_swap(entry.name_offset) + byte_swap(entry.name_size) || pkg_size < byte_swap(pkg_header.data_offset) + byte_swap(entry.data_offset) + byte_swap(entry.data_size)) {
            LOG_ERROR("The pkg file size is too small, possibly corrupted");
            evp_cleanup();
            return false;
        }
        std::vector<unsigned char> name(byte_swap(entry.name_size));
        fseek(infile, byte_swap(pkg_header.data_offset) + byte_swap(entry.name_offset), SEEK_SET);
        fread(name.data(), byte_swap(entry.name_size), 1, infile);

        decrypt_aes_ctr(byte_swap(entry.name_offset) / 16, name.data(), byte_swap(entry.name_size));

        auto string_name = std::string(name.begin(), name.end());
        LOG_INFO(string_name);

        if ((byte_swap(entry.type) & 0xFF) == 4 || (byte_swap(entry.type) & 0xFF) == 18) { // Directory
            fs::create_directories(path / string_name);
        } else { // File
            std::ofstream outfile(path / string_name, std::ios::binary);

            auto offset = byte_swap(entry.data_offset);
            auto data_size = byte_swap(entry.data_size);

            uint8_t counter[0x10];
            ctr_init(counter, pkg_header.pkg_data_iv, offset / 16);
            EVP_DecryptInit_ex(cipher_ctx, cipher_CTR, nullptr, main_key, counter);
            EVP_CIPHER_CTX_set_padding(cipher_ctx, 0);

            fseek(infile, byte_swap(pkg_header.data_offset) + offset, SEEK_SET);
            while (data_size != 0) {
                size_t size = data_size < buffer.size() ? data_size : buffer.size();
                fread(buffer.data(), size, 1, infile);

                EVP_DecryptUpdate(cipher_ctx, buffer.data(), &dec_len, buffer.data(), size);

                outfile.write(reinterpret_cast<char *>(buffer.data()), dec_len);
                data_size -= size;
            }

            EVP_DecryptFinal_ex(cipher_ctx, buffer.data(), &dec_len);
            outfile.write(reinterpret_cast<char *>(buffer.data()), dec_len);
            outfile.close();
        }
        if (progress_callback)
            progress_callback(static_cast<float>(i + 1) / file_count * 99.f);
    }
    fclose(infile);

    evp_cleanup();
    std::string zRIF = p_zRIF;
    if (!create_license(emuenv, zRIF))
        LOG_WARN("Package extracted, but its license could not be created for {}", emuenv.app_info.app_title_id);

    if (type == PkgType::PKG_TYPE_VITA_APP || type == PkgType::PKG_TYPE_VITA_PATCH) {
        if (!create_appmeta(emuenv.io, emuenv.vita_fs_path, emuenv.app_info.app_title_id,
                type == PkgType::PKG_TYPE_VITA_PATCH ? AppmetaInstallType::Patch : AppmetaInstallType::Application,
                AppmetaFormat::Protected, AppmetaCache::Populate)) {
            LOG_ERROR("Package extracted, but Appmeta or its shell cache for {} could not be completed; see preceding log messages", emuenv.app_info.app_title_id);
            return false;
        }
    }
    if (progress_callback)
        progress_callback(100);
    return true;
}

std::string find_pkg_zrif(const fs::path &pkg_path, const fs::path &vita_fs_path) {
    FILE *infile = FOPEN(pkg_path.c_str(), "rb");
    if (!infile)
        return {};

    PkgHeader pkg_header{};
    fread(&pkg_header, sizeof(PkgHeader), 1, infile);
    fclose(infile);

    const std::string content_id(pkg_header.content_id);
    if (content_id.size() < 16)
        return {};

    const std::string title_id = content_id.substr(7, 9);
    const auto rif_path = vita_fs_path / "ux0/license" / title_id / (content_id + ".rif");

    if (!fs::exists(rif_path))
        return {};

    LOG_INFO("Found license file: {}", rif_path);
    std::ifstream binfile(rif_path, std::ios::in | std::ios::binary | std::ios::ate);
    if (!binfile)
        return {};

    return rif2zrif(binfile);
}
