// Vita3K emulator project
// Copyright (C) 2020 Vita3K team
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

#include <PsvPfsParserConfig.h>
#include <app/functions.h>
#include <crypto/aes.h>
#include <host/functions.h>
#include <host/pkg.h>
#include <host/sce_types.h>
#include <host/sfo.h>
#include <host/state.h>
#include <io/device.h>
#include <util/bytes.h>
#include <util/log.h>
#include <zRIF/licdec.h>

#include <fstream>

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

bool install_pkg(const std::string &pkg, const std::string &pref_path, HostState &host) {
    std::ifstream infile(pkg, std::ios::binary);
    PkgHeader pkg_header;
    PkgExtHeader ext_header;
    infile.read(reinterpret_cast<char *>(&pkg_header), sizeof(PkgHeader));
    infile.seekg(sizeof(PkgHeader));
    infile.read(reinterpret_cast<char *>(&ext_header), sizeof(PkgExtHeader));

    if (byte_swap(pkg_header.magic) != 0x7F504b47 && byte_swap(ext_header.magic) != 0x7F657874) {
        LOG_ERROR("Not a valid pkg file!");
        return false;
    }

    if (fs::file_size(pkg) < byte_swap(pkg_header.total_size)) {
        LOG_ERROR("The pkg file is too small");
        return false;
    }

    if (fs::file_size(pkg) < byte_swap(pkg_header.data_offset) + byte_swap(pkg_header.file_count) * 32) {
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
    default:
        LOG_ERROR("Unsupported content type");
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
    std::string title_id;
    std::string category;
    std::string content_id;
    infile.seekg(sfo_offset);
    infile.read((char *)&sfo_buffer[0], sfo_size);
    sfo::load(sfo_file, sfo_buffer);
    sfo::get_data_by_key(title_id, sfo_file, "TITLE_ID");
    sfo::get_data_by_key(category, sfo_file, "CATEGORY");
    sfo::get_data_by_key(content_id, sfo_file, "CONTENT_ID");
    content_id = content_id.substr(20);

    if (type == PkgType::PKG_TYPE_VITA_APP && strcmp(category.c_str(), "gp") == 0) {
        type = PkgType::PKG_TYPE_VITA_PATCH;
    }

    fs::path root_path;

    switch (type) {
    case PkgType::PKG_TYPE_VITA_APP:
        root_path = device::construct_emulated_path(VitaIoDevice::ux0, "app/" + title_id, pref_path);
        break;
    case PkgType::PKG_TYPE_VITA_DLC:
        root_path = device::construct_emulated_path(VitaIoDevice::ux0, "addcont/" + title_id + content_id, pref_path);
        break;
    case PkgType::PKG_TYPE_VITA_PATCH:
        app::error_dialog("Sorry, but game updates/patches are not supported at this time.", nullptr);
        return false;
        //root_path = device::construct_emulated_path(VitaIoDevice::ux0, "patch/" + title_id, pref_path);
    }

    for (uint32_t i = 0; i < byte_swap(pkg_header.file_count); i++) {
        PkgEntry entry;
        uint64_t file_offset = items_offset + i * 32;
        infile.seekg(byte_swap(pkg_header.data_offset) + file_offset, std::ios_base::beg);
        infile.read(reinterpret_cast<char *>(&entry), sizeof(PkgEntry));
        aes128_ctr_xor(&aes_ctx, pkg_header.pkg_data_iv, file_offset / 16, reinterpret_cast<unsigned char *>(&entry), sizeof(PkgEntry));

        if (fs::file_size(pkg) < byte_swap(pkg_header.data_offset) + byte_swap(entry.name_offset) + byte_swap(entry.name_size) || fs::file_size(pkg) < byte_swap(pkg_header.data_offset) + byte_swap(entry.data_offset) + byte_swap(entry.data_size)) {
            LOG_ERROR("The pkg file size is too small, possibly corrupted");
            return false;
        }

        std::vector<unsigned char> name(byte_swap(entry.name_size));
        infile.seekg(byte_swap(pkg_header.data_offset) + byte_swap(entry.name_offset));
        infile.read((char *)&name[0], byte_swap(entry.name_size));
        aes128_ctr_xor(&aes_ctx, pkg_header.pkg_data_iv, byte_swap(entry.name_offset) / 16, &name[0], byte_swap(entry.name_size));

        auto string_name = std::string(name.begin(), name.end());
        LOG_INFO(string_name);

        if ((byte_swap(entry.type) & 0xFF) == 4 || (byte_swap(entry.type) & 0xFF) == 18) { // Directory
            fs::create_directories(root_path.string() + "/" + string_name);
        } else { // File
            std::ofstream outfile(root_path.string() + "/" + string_name, std::ios::binary);

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

    switch (type) {
    case PkgType::PKG_TYPE_VITA_APP:
        host.title_id_src = root_path.string();
        host.title_id_dst = root_path.string() + "_dec";
        host.f00d_enc_type = F00DEncryptorTypes::native;
        host.f00d_arg = std::string();

        if (execute(host) < 0) {
            return false;
        } else {
            fs::remove_all(fs::path(host.title_id_src));
            fs::rename(fs::path(host.title_id_dst), fs::path(host.title_id_src));

            KeyStore SCE_KEYS;
            register_keys(SCE_KEYS);
            unsigned char temp_klicensee[0x10] = { 0 };
            std::shared_ptr<SceNpDrmLicense> slic = decode_license_np(host.zRIF);
            memcpy(temp_klicensee, slic->key, 0x10);

            for (const auto &file : fs::directory_iterator(host.title_id_src + "/sce_module/")) {
                self2elf(file.path().string(), file.path().string() + "elf", SCE_KEYS, temp_klicensee);
                fs::rename(file.path().string() + "elf", file.path().string());
                make_fself(file.path().string(), file.path().string() + "fself");
                fs::rename(file.path().string() + "fself", file.path().string());
            }

            self2elf(host.title_id_src + "/eboot.bin", host.title_id_src + "/eboot.bin" + "elf", SCE_KEYS, temp_klicensee);
            fs::rename(host.title_id_src + "/eboot.bin" + "elf", host.title_id_src + "/eboot.bin");
            make_fself(host.title_id_src + "/eboot.bin", host.title_id_src + "/eboot.bin" + "fself");
            fs::rename(host.title_id_src + "/eboot.bin" + "fself", host.title_id_src + "/eboot.bin");
        }

        break;
    case PkgType::PKG_TYPE_VITA_DLC:
        host.title_id_src = root_path.string();
        host.title_id_dst = root_path.string() + "_dec";
        host.f00d_enc_type = F00DEncryptorTypes::native;
        host.f00d_arg = std::string();

        if (execute(host) < 0) {
            return false;
        } else {
            fs::remove_all(fs::path(host.title_id_src));
            fs::rename(fs::path(host.title_id_dst), fs::path(host.title_id_src));
            return true;
        }
        break;
    }

    host.title_id_src.clear();
    host.title_id_dst.clear();
    host.zRIF.clear();
    host.f00d_arg.clear();
    host.klicensee.clear();

    return true;
}