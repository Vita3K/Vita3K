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
 * @file pkg.cpp
 * @brief PlayStation Vita software package (`.pkg`) handling
 */

#include <F00DKeyEncryptorFactory.h>
#include <PsvPfsParserConfig.h>
#include <Utils.h>
#include <openssl/evp.h>
#include <rif2zrif.h>

#include <io/functions.h>

#include <config/state.h>
#include <emuenv/state.h>

#include <packages/functions.h>
#include <packages/license.h>
#include <packages/pkg.h>
#include <packages/sce_types.h>
#include <packages/sfo.h>

#include <util/bytes.h>
#include <util/log.h>

// Credits to mmozeiko https://github.com/mmozeiko/pkg2zip

static void ctr_init(uint8_t *counter, uint8_t *iv, uint64_t n) {
    for (int i = 15; i >= 0; i--) {
        n = n + iv[i];
        counter[i] = (uint8_t)n;
        n >>= 8;
    }
}

static int execute(std::string &zrif, fs::path &title_src, fs::path &title_dst, F00DEncryptorTypes type, std::string &f00d_arg) {
    std::string title_src_str = title_src.string();
    std::string title_dst_str = title_dst.string();
    return execute(zrif, title_src_str, title_dst_str, type, f00d_arg);
}

bool decrypt_install_nonpdrm(EmuEnvState &emuenv, const fs::path &drmlicpath, const fs::path &title_path) {
    fs::path title_id_src = title_path;
    fs::path title_id_dst = fs_utils::path_concat(title_path, "_dec");
    fs::ifstream binfile(drmlicpath, std::ios::in | std::ios::binary | std::ios::ate);
    std::string zRIF = rif2zrif(binfile);
    F00DEncryptorTypes f00d_enc_type = F00DEncryptorTypes::native;
    std::string f00d_arg = std::string();

    if ((execute(zRIF, title_id_src, title_id_dst, f00d_enc_type, f00d_arg) < 0) && (title_path.string().find("theme") == std::string::npos))
        return false;

    if (emuenv.app_info.app_category.find("gp") == std::string::npos)
        copy_license(emuenv, drmlicpath);

    fs::remove_all(title_id_src);
    fs::rename(title_id_dst, title_id_src);

    return true;
}

bool install_pkg(const fs::path &pkg_path, EmuEnvState &emuenv, std::string &p_zRIF, const std::function<void(float)> &progress_callback) {
    FILE *infile = fs_utils::open_file_handle_from_path(pkg_path);
    if (!infile) {
        LOG_CRITICAL("Failed to load pkg file in path: {}", pkg_path.generic_path().string());
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
    SfoFile sfo_file;
    fseek(infile, sfo_offset, SEEK_SET);
    fread(sfo_buffer.data(), sfo_buffer.size(), 1, infile);
    sfo::load(sfo_file, sfo_buffer);
    sfo::get_param_info(emuenv.app_info, sfo_buffer, emuenv.cfg.sys_lang);

    if (type == PkgType::PKG_TYPE_VITA_DLC)
        emuenv.app_info.app_content_id = emuenv.app_info.app_content_id.substr(20);

    if (type == PkgType::PKG_TYPE_VITA_APP && strcmp(emuenv.app_info.app_category.c_str(), "gp") == 0) {
        type = PkgType::PKG_TYPE_VITA_PATCH;
    }

    auto path{ emuenv.pref_path / "ux0" };

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
    for (uint32_t i = 0; i < byte_swap(pkg_header.file_count); i++) {
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
        const auto file_count = (float)byte_swap(pkg_header.file_count);
        progress_callback(i / file_count * 100.f * 0.6f);
        std::vector<unsigned char> name(byte_swap(entry.name_size));
        fseek(infile, byte_swap(pkg_header.data_offset) + byte_swap(entry.name_offset), SEEK_SET);
        fread(name.data(), byte_swap(entry.name_size), 1, infile);

        decrypt_aes_ctr(byte_swap(entry.name_offset) / 16, name.data(), byte_swap(entry.name_size));

        auto string_name = std::string(name.begin(), name.end());
        LOG_INFO(string_name);

        if ((byte_swap(entry.type) & 0xFF) == 4 || (byte_swap(entry.type) & 0xFF) == 18) { // Directory
            fs::create_directories(path / string_name);
        } else { // File
            fs::ofstream outfile(path / string_name, std::ios::binary);

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
    }
    fclose(infile);

    evp_cleanup();
    fs::path title_id_src = path;
    fs::path title_id_dst = fs_utils::path_concat(path, "_dec");
    std::string zRIF = p_zRIF;
    F00DEncryptorTypes f00d_enc_type = F00DEncryptorTypes::native;
    std::string f00d_arg = std::string();

    KeyStore SCE_KEYS;
    register_keys(SCE_KEYS, 1);
    std::vector<uint8_t> temp_klicensee = get_temp_klicensee(zRIF);

    progress_callback(80);
    switch (type) {
    case PkgType::PKG_TYPE_VITA_APP:
    case PkgType::PKG_TYPE_VITA_PATCH:

        if (execute(zRIF, title_id_src, title_id_dst, f00d_enc_type, f00d_arg) < 0) {
            fs::remove_all(fs::path(title_id_src));
            fs::remove_all(fs::path(title_id_dst));
            return false;
        }
        fs::remove_all(title_id_src);
        fs::rename(title_id_dst, title_id_src);

        break;
    case PkgType::PKG_TYPE_VITA_DLC:

        if (execute(zRIF, title_id_src, title_id_dst, f00d_enc_type, f00d_arg) < 0) {
            fs::remove_all(fs::path(title_id_src));
            fs::remove_all(fs::path(title_id_dst));
            return false;
        } else {
            fs::remove_all(title_id_src);
            fs::rename(title_id_dst, title_id_src);
            return true;
        }
        break;

    case PkgType::PKG_TYPE_VITA_THEME:

        // Theme don't have keystone file, need skip error
        execute(zRIF, title_id_src, title_id_dst, f00d_enc_type, f00d_arg);
        fs::remove_all(title_id_src);
        fs::rename(title_id_dst, title_id_src);
        return true;
        break;
    }

    if (!copy_path(title_id_src, emuenv.pref_path, emuenv.app_info.app_title_id, emuenv.app_info.app_category))
        return false;

    create_license(emuenv, zRIF);

    progress_callback(100);
    return true;
}
