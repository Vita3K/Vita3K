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

#include <F00DKeyEncryptorFactory.h>
#include <PsvPfsParserConfig.h>
#include <Utils.h>
#include <rif2zrif.h>

#include <app/functions.h>
#include <boost/algorithm/string/trim.hpp>
#include <crypto/aes.h>
#include <io/device.h>

#include <host/functions.h>
#include <host/pkg.h>
#include <host/sce_types.h>
#include <host/sfo.h>
#include <host/state.h>

#include <util/bytes.h>
#include <util/log.h>
#include <util/string_utils.h>

// Credits to mmozeiko https://github.com/mmozeiko/pkg2zip

static void ctr_add(uint8_t *counter, uint64_t n) {
    for (int i = 15; i >= 0; i--) {
        n = n + counter[i];
        counter[i] = (uint8_t)n;
        n >>= 8;
    }
}

static void aes128_ctr_xor(aes_context *ctx, const uint8_t *iv, uint64_t block, uint8_t *input, size_t size) {
    uint8_t tmp[16];
    uint8_t counter[16];
    for (uint32_t i = 0; i < 16; i++) {
        counter[i] = iv[i];
    }
    ctr_add(counter, block);

    while (size >= 16) {
        aes_crypt_ecb(ctx, AES_ENCRYPT, counter, tmp);
        for (uint32_t i = 0; i < 16; i++) {
            *input++ ^= tmp[i];
        }
        ctr_add(counter, 1);
        size -= 16;
    }

    if (size != 0) {
        aes_crypt_ecb(ctx, AES_ENCRYPT, counter, tmp);
        for (size_t i = 0; i < size; i++) {
            *input++ ^= tmp[i];
        }
    }
}

bool decrypt_install_nonpdrm(HostState &host, std::string &drmlicpath, const std::string &title_path) {
    std::string title_id_src = title_path;
    std::string title_id_dst = title_path + "_dec";
    fs::ifstream binfile(string_utils::utf_to_wide(drmlicpath), std::ios::in | std::ios::binary | std::ios::ate);
    std::string zRIF = rif2zrif(binfile);
    F00DEncryptorTypes f00d_enc_type = F00DEncryptorTypes::native;
    std::string f00d_arg = std::string();

    if ((execute(zRIF, title_id_src, title_id_dst, f00d_enc_type, f00d_arg) < 0) && (title_path.find("theme") == std::string::npos))
        return false;

    if (host.app_category.find("gp") == std::string::npos)
        copy_license(host, drmlicpath);

    fs::remove_all(fs::path(title_id_src));
    fs::rename(fs::path(title_id_dst), fs::path(title_id_src));

    KeyStore SCE_KEYS;
    register_keys(SCE_KEYS, 1);
    std::vector<uint8_t> temp_klicensee = get_temp_klicensee(zRIF);

    for (const auto &file : fs::recursive_directory_iterator(title_id_src)) {
        if ((file.path().extension() == ".suprx") || (file.path().extension() == ".self") || (file.path().filename() == "eboot.bin")) {
            self2elf(file.path().string(), file.path().string() + "elf", SCE_KEYS, temp_klicensee.data());
            fs::rename(file.path().string() + "elf", file.path().string());
            make_fself(file.path().string(), file.path().string() + "fself");
            fs::rename(file.path().string() + "fself", file.path().string());
            LOG_INFO("Decrypted {} with klicensee {}", file.path().string(), byte_array_to_string(temp_klicensee.data(), 16));
        }
    }

    return true;
}

bool install_pkg(const std::string &pkg, HostState &host, std::string &p_zRIF, const std::function<void(float)> &progress_callback) {
    std::wstring pkg_path = string_utils::utf_to_wide(pkg);
    fs::ifstream infile(pkg_path, std::ios::binary);
    PkgHeader pkg_header;
    PkgExtHeader ext_header;
    infile.read(reinterpret_cast<char *>(&pkg_header), sizeof(PkgHeader));
    infile.seekg(sizeof(PkgHeader));
    infile.read(reinterpret_cast<char *>(&ext_header), sizeof(PkgExtHeader));

    progress_callback(0);

    if (byte_swap(pkg_header.magic) != 0x7F504b47 && byte_swap(ext_header.magic) != 0x7F657874) {
        LOG_ERROR("Not a valid pkg file!");
        return false;
    }

    if (fs::file_size(pkg_path) < byte_swap(pkg_header.total_size)) {
        LOG_ERROR("The pkg file is too small");
        return false;
    }

    if (fs::file_size(pkg_path) < byte_swap(pkg_header.data_offset) + byte_swap(pkg_header.file_count) * 32) {
        LOG_ERROR("The pkg file is too small");
        return false;
    }

    uint32_t info_offset = byte_swap(pkg_header.info_offset);
    uint32_t content_type = 0;
    uint32_t sfo_offset = 0;
    uint32_t sfo_size = 0;
    uint32_t items_offset = 0;
    uint32_t items_size = 0;

    for (uint32_t i = 0; i < byte_swap(pkg_header.info_count); i++) {
        uint32_t block[4];
        infile.seekg(info_offset);
        infile.read((char *)block, sizeof(block));

        auto type = byte_swap(block[0]);
        auto size = byte_swap(block[1]);

        switch (type) {
        case 2:
            content_type = byte_swap(block[2]);
            break;
        case 13:
            items_offset = byte_swap(block[2]);
            items_size = byte_swap(block[3]);
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
    aes_context aes_ctx;
    uint8_t main_key[16];

    switch (key_type) {
    case 2:
        aes_setkey_enc(&aes_ctx, pkg_vita_2, 128);
        aes_crypt_ecb(&aes_ctx, AES_ENCRYPT, pkg_header.pkg_data_iv, main_key);
        break;
    case 3:
        aes_setkey_enc(&aes_ctx, pkg_vita_3, 128);
        aes_crypt_ecb(&aes_ctx, AES_ENCRYPT, pkg_header.pkg_data_iv, main_key);
        break;
    case 4:
        aes_setkey_enc(&aes_ctx, pkg_vita_4, 128);
        aes_crypt_ecb(&aes_ctx, AES_ENCRYPT, pkg_header.pkg_data_iv, main_key);
        break;
    default:
        LOG_ERROR("Unknown encryption key");
        return false;
        break;
    }

    aes_setkey_enc(&aes_ctx, main_key, 128);

    std::vector<uint8_t> sfo_buffer(sfo_size);
    SfoFile sfo_file;
    infile.seekg(sfo_offset);
    infile.read((char *)&sfo_buffer[0], sfo_size);
    sfo::load(sfo_file, sfo_buffer);
    sfo::get_param_info(host, sfo_buffer);

    if (type == PkgType::PKG_TYPE_VITA_DLC)
        host.app_content_id = host.app_content_id.substr(20);

    if (type == PkgType::PKG_TYPE_VITA_APP && strcmp(host.app_category.c_str(), "gp") == 0) {
        type = PkgType::PKG_TYPE_VITA_PATCH;
    }

    auto path{ fs::path(host.pref_path) / "ux0" };

    switch (type) {
    case PkgType::PKG_TYPE_VITA_APP:
        path /= fs::path("app") / host.app_title_id;
        if (fs::exists(path))
            fs::remove_all(path);
        host.app_title += " (App)";
        break;
    case PkgType::PKG_TYPE_VITA_DLC:
        path /= fs::path("addcont") / host.app_title_id / host.app_content_id;
        host.app_title += " (DLC)";
        break;
    case PkgType::PKG_TYPE_VITA_PATCH:
        path /= fs::path("patch") / host.app_title_id;
        host.app_title += " (Update)";
        break;
    case PkgType::PKG_TYPE_VITA_THEME:
        path /= fs::path("theme") / host.app_content_id;
        host.app_category = "theme";
        host.app_title += " (Theme)";
        break;
    }

    for (uint32_t i = 0; i < byte_swap(pkg_header.file_count); i++) {
        PkgEntry entry;
        uint64_t file_offset = items_offset + i * 32;
        infile.seekg(byte_swap(pkg_header.data_offset) + file_offset, std::ios_base::beg);
        infile.read(reinterpret_cast<char *>(&entry), sizeof(PkgEntry));
        aes128_ctr_xor(&aes_ctx, pkg_header.pkg_data_iv, file_offset / 16, reinterpret_cast<unsigned char *>(&entry), sizeof(PkgEntry));

        if (fs::file_size(pkg_path) < byte_swap(pkg_header.data_offset) + byte_swap(entry.name_offset) + byte_swap(entry.name_size) || fs::file_size(pkg_path) < byte_swap(pkg_header.data_offset) + byte_swap(entry.data_offset) + byte_swap(entry.data_size)) {
            LOG_ERROR("The pkg file size is too small, possibly corrupted");
            return false;
        }
        const auto file_count = (float)byte_swap(pkg_header.file_count);
        progress_callback(i / file_count * 100.f * 0.6f);
        std::vector<unsigned char> name(byte_swap(entry.name_size));
        infile.seekg(byte_swap(pkg_header.data_offset) + byte_swap(entry.name_offset));
        infile.read((char *)&name[0], byte_swap(entry.name_size));
        aes128_ctr_xor(&aes_ctx, pkg_header.pkg_data_iv, byte_swap(entry.name_offset) / 16, &name[0], byte_swap(entry.name_size));

        auto string_name = std::string(name.begin(), name.end());
        LOG_INFO(string_name);

        if ((byte_swap(entry.type) & 0xFF) == 4 || (byte_swap(entry.type) & 0xFF) == 18) { // Directory
            fs::create_directories(path.string() + "/" + string_name);
        } else { // File
            std::ofstream outfile(path.string() + "/" + string_name, std::ios::binary);

            auto offset = byte_swap(entry.data_offset);
            auto data_size = byte_swap(entry.data_size);
            while (data_size != 0) {
                unsigned char buffer[0x10000];
                auto size = data_size < sizeof(buffer) ? data_size : sizeof(buffer);
                infile.seekg(byte_swap(pkg_header.data_offset) + offset);
                infile.read((char *)buffer, size);

                aes128_ctr_xor(&aes_ctx, pkg_header.pkg_data_iv, offset / 16, buffer, size);

                outfile.write((char *)buffer, size);
                offset += size;
                data_size -= size;
            }
            outfile.close();
        }
    }
    infile.close();

    std::string title_id_src = path.string();
    std::string title_id_dst = path.string() + "_dec";
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
            return false;
        }
        fs::remove_all(fs::path(title_id_src));
        fs::rename(fs::path(title_id_dst), fs::path(title_id_src));

        for (const auto &file : fs::recursive_directory_iterator(title_id_src)) {
            if (file.path().extension() == ".suprx" || file.path().extension() == ".self" || file.path().filename() == "eboot.bin") {
                self2elf(file.path().string(), file.path().string() + "elf", SCE_KEYS, temp_klicensee.data());
                fs::rename(file.path().string() + "elf", file.path().string());
                make_fself(file.path().string(), file.path().string() + "fself");
                fs::rename(file.path().string() + "fself", file.path().string());
                LOG_INFO("Decrypted {} with klicensee {}", file.path().string(), byte_array_to_string(temp_klicensee.data(), 16));
            }
        }
        break;
    case PkgType::PKG_TYPE_VITA_DLC:

        if (execute(zRIF, title_id_src, title_id_dst, f00d_enc_type, f00d_arg) < 0) {
            return false;
        } else {
            fs::remove_all(fs::path(title_id_src));
            fs::rename(fs::path(title_id_dst), fs::path(title_id_src));
            return true;
        }
        break;

    case PkgType::PKG_TYPE_VITA_THEME:

        // Theme don't have keystone file, need skip error
        execute(zRIF, title_id_src, title_id_dst, f00d_enc_type, f00d_arg);
        fs::remove_all(fs::path(title_id_src));
        fs::rename(fs::path(title_id_dst), fs::path(title_id_src));
        return true;
        break;
    }

    if (!copy_path(host, title_id_src))
        return false;

    create_license(host, zRIF);
    progress_callback(100);
    return true;
}
