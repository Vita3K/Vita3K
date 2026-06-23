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
 * @file vci.cpp
 * @brief GcToolKit Vita Cartridge Image (`.vci`) handling
 *
 * Reference implementation: https://github.com/oestriot/VCI-TOOLS
 * (gc2nonpdrm.c + lib/gcauthmgr.c + lib/npdrm.c). Pipeline:
 *   VCI header -> cart_secret = SHA256(CMD56 keys) -> parse SceMBR ->
 *   extract gro0 (exFAT) -> read on-card RIF -> derive klicensee ->
 *   write a NoNpDrm work.bin -> reuse decrypt_install_nonpdrm() for the PFS.
 */

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <config/state.h>
#include <emuenv/state.h>
#include <io/functions.h>

#include <packages/exfat.h>
#include <packages/functions.h>
#include <packages/license.h>
#include <packages/pkg.h>
#include <packages/sfo.h>
#include <packages/vci.h>

#include <util/bytes.h>
#include <util/fs.h>
#include <util/log.h>

#include <cstring>
#include <vector>

// cart_secret = SHA256(packet20_key || packet18_key)  (0x40 -> 0x20)
static void get_cart_secret(const GcCmd56Keys &keys, uint8_t out[0x20]) {
    unsigned int len = 0;
    EVP_Digest(&keys, sizeof(keys), out, &len, EVP_sha256(), nullptr);
}

static bool decrypt_klicensee(const uint8_t cart_secret[0x20], const SceNpDrmLicense &license, uint8_t klicensee[0x10]) {
    VciBindData bind{};
    memcpy(bind.cart_secret, cart_secret, sizeof(bind.cart_secret));
    memcpy(bind.license, &license, sizeof(bind.license));

    uint8_t bind_hmac[0x20] = {};
    unsigned int hlen = 0;
    if (!HMAC(EVP_sha256(), BIND_KEY, sizeof(BIND_KEY),
            reinterpret_cast<const uint8_t *>(&bind), sizeof(bind), bind_hmac, &hlen)
        || hlen != sizeof(bind_hmac))
        return false;

    const uint8_t *aes_key = bind_hmac;
    const uint8_t *aes_iv = bind_hmac + 0x10;

    uint8_t buf[0x20];
    memcpy(buf, &license.key2, sizeof(buf));

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return false;
    int outl = 0;
    bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, aes_key, aes_iv) == 1;
    if (ok) {
        EVP_CIPHER_CTX_set_padding(ctx, 0);
        ok = EVP_DecryptUpdate(ctx, buf, &outl, buf, sizeof(buf)) == 1;
    }
    EVP_CIPHER_CTX_free(ctx);
    if (!ok || outl != sizeof(buf))
        return false;

    memcpy(klicensee, buf, 0x10); // klicensee is the first 16 bytes
    return true;
}

// Locate the gro0 (exFAT) partition in the raw card image (byte offsets).
static bool find_gro0(const SceMbr &mbr, uint64_t &offset, uint64_t &size) {
    if (memcmp(mbr.magic, SCE_MBR_MAGIC, sizeof(mbr.magic)) != 0) {
        LOG_ERROR("VCI image does not contain a valid SceMBR");
        return false;
    }
    for (const auto &p : mbr.partitions) {
        if (p.offset == 0 && p.size == 0)
            break; // end of table
        if (p.code == ScePartitionCode_GRO0 && p.type == ScePartitionType_EXFAT) {
            offset = static_cast<uint64_t>(p.offset) * SECTOR_SIZE;
            size = static_cast<uint64_t>(p.size) * SECTOR_SIZE;
            return true;
        }
    }
    LOG_ERROR("VCI: no gro0/exFAT partition found in partition table");
    return false;
}

// Build a NoNpDrm work.bin from the on-card RIF + derived klicensee.
static void write_nonpdrm_license(const SceNpDrmLicense &old_license, const uint8_t klicensee[0x10], const fs::path &work_bin_path) {
    SceNpDrmLicense lic{};
    lic.aid = 0x0123456789ABCDEFLL;
    lic.version = byte_swap<uint16_t>(1);
    lic.version_flag = byte_swap<uint16_t>(1);
    lic.type = byte_swap<uint16_t>(1);
    const uint16_t lf_host = byte_swap<uint16_t>(old_license.flags);
    lic.flags = byte_swap<uint16_t>(static_cast<uint16_t>(lf_host & ~0x400u));
    lic.flags2 = old_license.flags2;
    const uint32_t sku = byte_swap<uint32_t>(static_cast<uint32_t>(old_license.sku_flag));
    lic.sku_flag = static_cast<int32_t>(byte_swap<uint32_t>((sku == 1 || sku == 3) ? 3u : 0u));
    memcpy(lic.content_id, old_license.content_id, sizeof(lic.content_id));
    memcpy(lic.key, klicensee, sizeof(lic.key));

    fs::create_directories(work_bin_path.parent_path());
    fs::ofstream out(work_bin_path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(&lic), sizeof(lic));
}

bool install_vci(const fs::path &vci_path, EmuEnvState &emuenv, const std::function<void(float)> &progress_callback) {
    const auto progress = [&](float v) { if (progress_callback) progress_callback(v); };

    // 1) Open the .vci image
    fs::ifstream img(vci_path, std::ios::binary);
    if (!img) {
        LOG_CRITICAL("Failed to open VCI file: {}", fs_utils::path_to_utf8(vci_path));
        return false;
    }
    progress(0);
    LOG_INFO("Installing VCI: {}", fs_utils::path_to_utf8(vci_path));

    // 2) Header: validate magic/version and derive cart_secret from the CMD56 keys
    VciHeader hdr{};
    img.read(reinterpret_cast<char *>(&hdr), sizeof(hdr));
    if (memcmp(hdr.magic, VCI_MAGIC, sizeof(hdr.magic)) != 0 || hdr.major_version != VCI_MAJOR_VER) {
        LOG_ERROR("Not a valid VCI file (bad magic/version)");
        return false;
    }
    LOG_INFO("VCI header: version {}.{}, device size 0x{:X} bytes", hdr.major_version, hdr.minor_version, hdr.device_size);
    uint8_t cart_secret[0x20];
    get_cart_secret(hdr.keys, cart_secret);
    LOG_INFO("Derived cart_secret from CMD56 keys");

    // 3) SceMBR: locate the gro0 (exFAT) partition in the raw card image
    SceMbr mbr{};
    img.read(reinterpret_cast<char *>(&mbr), sizeof(mbr));
    uint64_t gro_off = 0, gro_size = 0;
    if (!find_gro0(mbr, gro_off, gro_size))
        return false;
    LOG_INFO("Found gro0 (exFAT) partition: card offset 0x{:X}, size 0x{:X} bytes ({} MiB)", gro_off, gro_size, gro_size >> 20);

    // 4) Carve gro0 out into a standalone exFAT image (long step: 0..30%)
    const auto staging = emuenv.cache_path / "VCI_TMP";
    fs::remove_all(staging);
    fs::create_directories(staging);
    const auto gro_img = staging / "gro0.img"; // substr(0,3) -> "gro" output dir
    {
        fs::ofstream o(gro_img, std::ios::binary);
        img.seekg(VCI_HEADER_SIZE + gro_off, std::ios::beg);
        std::vector<char> buf(1 << 20);
        uint64_t copied = 0;
        while (copied < gro_size) {
            const auto n = static_cast<std::streamsize>(std::min<uint64_t>(gro_size - copied, buf.size()));
            img.read(buf.data(), n);
            o.write(buf.data(), n);
            copied += n;
            progress(copied / static_cast<float>(gro_size) * 30.f);
        }
    }
    LOG_INFO("Carved gro0 image: {} ({} bytes)", fs_utils::path_to_utf8(gro_img), gro_size);

    // 5) Extract the exFAT tree (gives /app/<tid> + /license/app/<tid>/*.rif)
    exfat::extract_exfat(staging, "gro0.img", staging);
    const auto gro_root = staging / "gro"; // exfat::extract_exfat output root
    if (fs::is_directory(gro_root)) {
        std::string entries;
        for (const auto &e : fs::directory_iterator(gro_root))
            entries += (entries.empty() ? "" : ", ") + e.path().filename().string() + (e.is_directory() ? "/" : "");
        LOG_INFO("Extracted gro0 root entries: [{}]", entries);
    } else {
        LOG_ERROR("VCI: exFAT extraction produced no output at {}", fs_utils::path_to_utf8(gro_root));
    }
    progress(40);

    // 6) Resolve the title id from the extracted /app folder
    std::string title_id;
    const auto app_root = gro_root / "app";
    if (fs::is_directory(app_root)) {
        for (const auto &e : fs::directory_iterator(app_root)) {
            if (e.is_directory()) {
                title_id = e.path().filename().string();
                break;
            }
        }
    } else {
        LOG_ERROR("VCI: expected app folder not found at {}", fs_utils::path_to_utf8(app_root));
    }
    if (title_id.empty()) {
        LOG_ERROR("VCI: failed to determine title id from gro0:/app");
        return false;
    }
    LOG_INFO("VCI title id: {}", title_id);
    const auto title_src = app_root / title_id;

    // 7) Load param.sfo to fill app_info (mirrors install_pkg)
    const auto param_sfo = title_src / "sce_sys" / "param.sfo";
    if (!fs::exists(param_sfo)) {
        LOG_ERROR("VCI: param.sfo not found at {}", fs_utils::path_to_utf8(param_sfo));
        return false;
    }
    vfs::FileBuffer param;
    fs_utils::read_data(param_sfo, param);
    sfo::get_param_info(emuenv.app_info, param, emuenv.cfg.sys_lang);
    LOG_INFO("Found {} [{}], category: {}, content id: {}", emuenv.app_info.app_title, emuenv.app_info.app_title_id, emuenv.app_info.app_category, emuenv.app_info.app_content_id);

    // 8) Choose the ux0 destination (a cart image is always a full app)
    auto path{ emuenv.vita_fs_path / "ux0" };
    path /= fs::path("app") / emuenv.app_info.app_title_id;
    if (fs::exists(path))
        fs::remove_all(path);
    emuenv.app_info.app_title += " (App)";

    // 9) Read the on-card RIF: gro0:/license/app/<title_id>/<*.rif>
    SceNpDrmLicense card_license{};
    bool have_license = false;
    const auto lic_dir = gro_root / "license" / "app" / title_id;
    if (fs::is_directory(lic_dir)) {
        for (const auto &e : fs::directory_iterator(lic_dir)) {
            if (e.is_regular_file()) {
                fs::ifstream lf(e.path(), std::ios::binary);
                lf.read(reinterpret_cast<char *>(&card_license), sizeof(card_license));
                have_license = lf.gcount() == sizeof(card_license);
                if (have_license)
                    LOG_INFO("Found license file: {}", fs_utils::path_to_utf8(e.path()));
                break;
            }
        }
    }
    if (!have_license) {
        LOG_ERROR("VCI: failed to read on-card RIF for {} (looked in {})", title_id, fs_utils::path_to_utf8(lic_dir));
        return false;
    }

    // 10) Derive klicensee and synthesize a NoNpDrm work.bin in the staged app
    uint8_t klicensee[0x10];
    if (!decrypt_klicensee(cart_secret, card_license, klicensee)) {
        LOG_ERROR("VCI: failed to derive klicensee");
        return false;
    }
    write_nonpdrm_license(card_license, klicensee, title_src / "sce_sys" / "package" / "work.bin");
    progress(50);

    // 11) Copy the staged app into ux0 (copy, not rename, so it works across volumes)
    fs::create_directories(path);
    if (!fs_utils::copy_directory_contents(title_src, path)) {
        LOG_ERROR("VCI: failed to copy app into {}", fs_utils::path_to_utf8(path));
        fs::remove_all(staging);
        return false;
    }
    progress(60);

    // 12) PFS stage: decrypt in place + install the license (shared NoNpDrm tail)
    const fs::path drmlicpath = path / "sce_sys" / "package" / "work.bin";
    const auto decrypt_progress = [&](float frac) { progress(60.f + frac * 40.f); };
    if (!decrypt_install_nonpdrm(emuenv, drmlicpath, path, decrypt_progress)) {
        LOG_ERROR("VCI: failed to decrypt {} [{}]", emuenv.app_info.app_title, emuenv.app_info.app_title_id);
        fs::remove_all(path);
        fs::remove_all(staging);
        return false;
    }

    fs::remove_all(staging);
    progress(100);
    LOG_INFO("{} [{}] installed successfully!", emuenv.app_info.app_title, emuenv.app_info.app_title_id);
    return true;
}
