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
 * @file sce_types.h
 * @brief SCE metadata types embedded into SCE binaries
 */

#pragma once

#include <util/log.h>

// Credits to TeamMolecule for their original work on this https://github.com/TeamMolecule/sceutils

#define SCE_MAGIC 0x00454353
#define HEADER_LENGTH 0x1000

struct KeyEntry {
    uint64_t minver;
    uint64_t maxver;
    int keyrev;
    std::string key;
    std::string iv;
};

struct SceSegment {
    uint64_t offset;
    int32_t idx;
    uint64_t size;
    bool compressed;
    std::string key;
    std::string iv;
};

enum class SceType {
    SELF = 1,
    SRVK = 2,
    SPKG = 3,
    DEV = 0xC0
};

enum class SelfType {
    NONE = 0,
    KERNEL = 0x07,
    APP = 0x08,
    BOOT = 0x09,
    SECURE = 0x0B,
    USER = 0x0D
};

enum class KeyType {
    METADATA = 0,
    NPDRM = 1
};

enum class SelfPlatform {
    PS3 = 0,
    VITA = 0x40
};

enum class SpkgType {
    TYPE_0 = 0x0,
    OS0 = 0x1,
    TYPE_2 = 0x2,
    TYPE_3 = 0x3,
    PERMISSIONS_4 = 0x4,
    TYPE_5 = 0x5,
    TYPE_6 = 0x6,
    TYPE_7 = 0x7,
    SYSCON_8 = 0x8,
    BOOT = 0x9,
    VS0 = 0xA,
    CPFW = 0xB,
    MOTION_C = 0xC,
    BBMC_D = 0xD,
    TYPE_E = 0xE,
    MOTION_F = 0xF,
    TOUCH_10 = 0x10,
    TOUCH_11 = 0x11,
    SYSCON_12 = 0x12,
    SYSCON_13 = 0x13,
    SYSCON_14 = 0x14,
    TYPE_15 = 0x15,
    VS0_TAR_PATCH = 0x16,
    SA0 = 0x17,
    PD0 = 0x18,
    SYSCON_19 = 0x19,
    TYPE_1A = 0x1A,
    PSPEMU_LIST = 0x1B
};

enum class ControlType {
    CONTROL_FLAGS = 1,
    DIGEST_SHA1 = 2,
    NPDRM_PS3 = 3,
    DIGEST_SHA256 = 4,
    NPDRM_VITA = 5,
    UNK_SIG1 = 6,
    UNK_HASH1 = 7
};

enum class SecureBool {
    UNUSED = 0,
    NO = 1,
    YES = 2
};

enum class EncryptionType {
    NONE = 1,
    AES128CTR = 3
};

enum class HashType {
    NONE = 1,
    HMACSHA1 = 2,
    HMACSHA256 = 6
};

enum class CompressionType {
    NONE = 1,
    DEFLATE = 2
};

typedef std::unordered_map<
    KeyType, std::unordered_map<SceType, std::unordered_map<SelfType, std::vector<KeyEntry>>>>
    keystore;

class KeyStore {
private:
    keystore store;

public:
    void register_keys(KeyType keytype, SceType scetype, int keyrev, const std::string &key, const std::string &iv, uint64_t minver = 0, uint64_t maxver = 0xffffffffffffffff, SelfType selftype = SelfType::NONE) {
        store[keytype][scetype][selftype].push_back({ minver, maxver, keyrev, key, iv });
    }

    KeyEntry get(KeyType keytype, SceType scetype, uint64_t sysver = -1, int keyrev = -1, SelfType selftype = SelfType::NONE) {
        KeyEntry empty_keys = { 0, 0, 0, "", "" };

        auto key_type = store.find(keytype);
        if (key_type == store.end()) {
            LOG_ERROR("Cannot find any keys for this key type");
            return empty_keys;
        }
        auto sce_type = key_type->second.find(scetype);
        if (sce_type == key_type->second.end()) {
            LOG_ERROR("Cannot find any keys for this SCE type");
            return empty_keys;
        }
        auto self_type = sce_type->second.find(selftype);
        if (self_type == sce_type->second.end()) {
            LOG_ERROR("Cannot find any keys for this SELF type");
            return empty_keys;
        }

        auto &key_entries = *self_type;

        for (const auto &item : key_entries.second) {
            if ((sysver < 0 || (sysver >= item.minver && sysver <= item.maxver)) && (keyrev < 0 || keyrev == item.keyrev)) {
                return item;
            }
        }
        LOG_ERROR("Cannot find key/iv for this SCE file");
        return empty_keys;
    }
};

class SceHeader {
private:
    uint32_t magic;
    uint32_t version;
    uint8_t _platform;
    uint16_t _sce_type;
    uint64_t data_length;

public:
    static const int Size = 32;
    SceType sce_type;
    SelfPlatform platform;
    uint64_t header_length;
    uint32_t metadata_offset;
    uint8_t key_revision;

    explicit SceHeader(const char *data) {
        memcpy(&magic, &data[0], 4);
        memcpy(&version, &data[4], 4);
        memcpy(&_platform, &data[8], 1);
        memcpy(&key_revision, &data[9], 1);
        memcpy(&_sce_type, &data[10], 2);
        memcpy(&metadata_offset, &data[12], 4);
        memcpy(&header_length, &data[16], 8);
        memcpy(&data_length, &data[24], 8);

        if (magic != SCE_MAGIC) {
            LOG_ERROR("Invalid SCE magic");
        }
        if (version != 3) {
            LOG_ERROR("Invalid SCE version");
        }
        this->sce_type = SceType(_sce_type);
        this->platform = SelfPlatform(_platform);
    }
};

class SelfHeader {
private:
    uint64_t file_length;
    uint64_t field_8;
    uint64_t self_offset;
    uint64_t shdr_offset;
    uint64_t controlinfo_length;

public:
    static const int Size = 88;
    uint64_t appinfo_offset;
    uint64_t elf_offset;
    uint64_t phdr_offset;
    uint64_t segment_info_offset;
    uint64_t sceversion_offset;
    uint64_t controlinfo_offset;

    explicit SelfHeader(const char *data) {
        memcpy(&file_length, &data[0], 8);
        memcpy(&field_8, &data[8], 8);
        memcpy(&self_offset, &data[16], 8);
        memcpy(&appinfo_offset, &data[24], 8);
        memcpy(&elf_offset, &data[32], 8);
        memcpy(&phdr_offset, &data[40], 8);
        memcpy(&shdr_offset, &data[48], 8);
        memcpy(&segment_info_offset, &data[56], 8);
        memcpy(&sceversion_offset, &data[64], 8);
        memcpy(&controlinfo_offset, &data[72], 8);
        memcpy(&controlinfo_length, &data[80], 8);
    }
};

struct AppInfoHeader {
    uint64_t auth_id;
    uint32_t vendor_id;
    uint32_t _self_type;
    uint64_t field_18;

    static const int Size = 32;
    uint64_t sys_version;
    SelfType self_type;

    explicit AppInfoHeader(const char *data) {
        memcpy(&auth_id, &data[0], 8);
        memcpy(&vendor_id, &data[8], 4);
        memcpy(&_self_type, &data[12], 4);
        memcpy(&sys_version, &data[16], 8);
        memcpy(&field_18, &data[24], 8);

        this->self_type = SelfType(_self_type);
    }
};

class ElfHeader {
public:
    static const int Size = 52;
    unsigned char e_ident_1[8];
    unsigned char e_ident_2[8];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;

    explicit ElfHeader(const char *data) {
        memcpy(&e_ident_1, &data[0], 8);
        memcpy(&e_ident_2, &data[8], 8);
        memcpy(&e_type, &data[16], 2);
        memcpy(&e_machine, &data[18], 2);
        memcpy(&e_version, &data[20], 4);
        memcpy(&e_entry, &data[24], 4);
        memcpy(&e_phoff, &data[28], 4);
        memcpy(&e_shoff, &data[32], 4);
        memcpy(&e_flags, &data[36], 4);
        memcpy(&e_ehsize, &data[40], 2);
        memcpy(&e_phentsize, &data[42], 2);
        memcpy(&e_phnum, &data[44], 2);
        memcpy(&e_shentsize, &data[46], 2);
        memcpy(&e_shnum, &data[48], 2);
        memcpy(&e_shstrndx, &data[50], 2);
    }
};

class ElfPhdr {
public:
    static const int Size = 32;
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;

    explicit ElfPhdr(const char *data) {
        memcpy(&p_type, &data[0], 4);
        memcpy(&p_offset, &data[4], 4);
        memcpy(&p_vaddr, &data[8], 4);
        memcpy(&p_paddr, &data[12], 4);
        memcpy(&p_filesz, &data[16], 4);
        memcpy(&p_memsz, &data[20], 4);
        memcpy(&p_flags, &data[24], 4);
        memcpy(&p_align, &data[28], 4);
    }
};

class SegmentInfo {
private:
    uint32_t _compressed;
    uint32_t field_14;
    uint32_t _plaintext;
    uint32_t field_1C;

public:
    static const int Size = 32;
    uint64_t offset;
    uint64_t size;
    SecureBool compressed;
    SecureBool plaintext;

    explicit SegmentInfo(const char *data) {
        memcpy(&offset, &data[0], 8);
        memcpy(&size, &data[8], 8);
        memcpy(&_compressed, &data[16], 4);
        memcpy(&field_14, &data[20], 4);
        memcpy(&_plaintext, &data[24], 4);
        memcpy(&field_1C, &data[28], 4);

        this->compressed = SecureBool(_compressed);
        this->plaintext = SecureBool(_plaintext);
    }
};

class MetadataInfo {
private:
    uint64_t pad0;
    uint64_t pad1;
    uint64_t pad2;
    uint64_t pad3;

public:
    static const int Size = 64;
    unsigned char key[16];
    unsigned char iv[16];

    explicit MetadataInfo(const char *data) {
        memcpy(&key, &data[0], 16);
        memcpy(&iv, &data[32], 16);

        memcpy(&pad0, &data[16], 8);
        memcpy(&pad1, &data[24], 8);
        memcpy(&pad2, &data[48], 8);
        memcpy(&pad3, &data[56], 8);

        if (pad0 != 0 || pad1 != 0 || pad2 != 0 || pad3 != 0)
            LOG_ERROR("Invalid metadata info padding (decryption likely failed");
    }
};

class MetadataHeader {
private:
    uint64_t signature_input_length;
    uint32_t signature_type;
    uint32_t opt_header_size;
    uint32_t field_18;
    uint32_t field_1C;

public:
    static const int Size = 32;
    uint32_t section_count;
    uint32_t key_count;

    explicit MetadataHeader(const char *data) {
        memcpy(&signature_input_length, &data[0], 8);
        memcpy(&signature_type, &data[8], 4);
        memcpy(&section_count, &data[12], 4);
        memcpy(&key_count, &data[16], 4);
        memcpy(&opt_header_size, &data[20], 4);
        memcpy(&field_18, &data[24], 4);
        memcpy(&field_1C, &data[28], 4);
    }
};

class MetadataSection {
private:
    uint32_t type;
    uint32_t hashtype;
    int32_t hash_idx;
    uint32_t _encryption;
    uint32_t _compression;

public:
    static const int Size = 48;
    HashType hash;
    EncryptionType encryption;
    CompressionType compression;
    uint64_t offset;
    int32_t seg_idx;
    uint64_t size;
    int32_t key_idx;
    int32_t iv_idx;

    explicit MetadataSection(const char *data) {
        memcpy(&offset, &data[0], 8);
        memcpy(&size, &data[8], 8);
        memcpy(&type, &data[16], 4);
        memcpy(&seg_idx, &data[20], 4);
        memcpy(&hashtype, &data[24], 4);
        memcpy(&hash_idx, &data[28], 4);
        memcpy(&_encryption, &data[32], 4);
        memcpy(&key_idx, &data[36], 4);
        memcpy(&iv_idx, &data[40], 4);
        memcpy(&_compression, &data[44], 4);

        this->hash = HashType(hashtype);
        this->encryption = EncryptionType(_encryption);
        this->compression = CompressionType(_compression);
    }
};

class SrvkHeader {
private:
    uint32_t field_0;
    uint32_t field_4;
    uint32_t field_10;
    uint32_t field_14;
    uint32_t field_18;
    uint32_t field_1C;

public:
    static const int Size = 32;
    uint64_t sys_version;

    explicit SrvkHeader(const char *data) {
        memcpy(&field_0, &data[0], 4);
        memcpy(&field_4, &data[4], 4);
        memcpy(&sys_version, &data[8], 8);
        memcpy(&field_10, &data[16], 4);
        memcpy(&field_14, &data[20], 4);
        memcpy(&field_18, &data[24], 4);
        memcpy(&field_1C, &data[28], 4);
    }
};

class SpkgHeader {
private:
    uint32_t field_0;
    uint32_t _pkg_type;
    uint32_t flags;
    uint32_t field_C;
    uint64_t final_size;
    uint64_t decrypted_size;
    uint32_t field_28;
    uint32_t field_30;
    uint32_t field_34;
    uint32_t field_38;
    uint64_t field_3C;
    uint64_t field_40;
    uint64_t field_48;
    uint64_t offset;
    uint64_t size;
    uint64_t part_idx;
    uint64_t total_parts;
    uint64_t field_70;
    uint64_t field_78;

public:
    static const int Size = 128;
    SpkgType type;
    uint64_t update_version;

    explicit SpkgHeader(const char *data) {
        memcpy(&field_0, &data[0], 4);
        memcpy(&_pkg_type, &data[4], 4);
        memcpy(&flags, &data[8], 4);
        memcpy(&field_C, &data[12], 4);
        memcpy(&update_version, &data[16], 8);
        memcpy(&final_size, &data[24], 8);
        memcpy(&decrypted_size, &data[32], 8);
        memcpy(&field_28, &data[40], 4);
        memcpy(&field_30, &data[44], 4);
        memcpy(&field_34, &data[48], 4);
        memcpy(&field_38, &data[52], 4);
        memcpy(&field_3C, &data[56], 8);
        memcpy(&field_40, &data[64], 8);
        memcpy(&field_48, &data[72], 8);
        memcpy(&offset, &data[80], 8);
        memcpy(&size, &data[88], 8);
        memcpy(&part_idx, &data[96], 8);
        memcpy(&total_parts, &data[104], 8);
        memcpy(&field_70, &data[112], 8);
        memcpy(&field_78, &data[120], 8);

        this->type = SpkgType(_pkg_type);
    }
};

class SceVersionInfo {
public:
    static const int Size = 16;
    uint32_t subtype;
    uint32_t is_present;
    uint64_t size;

    explicit SceVersionInfo(const char *data) {
        memcpy(&subtype, &data[0], 4);
        memcpy(&is_present, &data[4], 4);
        memcpy(&size, &data[8], 8);
    }
};

class SceControlInfo {
private:
    uint32_t control_type;
    uint64_t more;

public:
    static const int Size = 16;
    ControlType type;
    uint32_t size;

    explicit SceControlInfo(const char *data) {
        memcpy(&control_type, &data[0], 4);
        memcpy(&size, &data[4], 4);
        memcpy(&more, &data[8], 8);

        this->type = ControlType(control_type);
    }
};

class SceControlInfoDigest256 {
private:
    unsigned char sce_hash[20];
    unsigned char file_hash[32];
    uint32_t filler1;
    uint32_t filler2;
    uint32_t sdk_version;

public:
    static const int Size = 64;

    explicit SceControlInfoDigest256(const char *data) {
        memcpy(&sce_hash, &data[0], 20);
        memcpy(&file_hash, &data[20], 32);

        memcpy(&filler1, &data[52], 4);
        memcpy(&filler2, &data[56], 4);
        memcpy(&sdk_version, &data[60], 4);
    }
};

class SceControlInfoDRM {
private:
    unsigned char content_id[0x30];
    unsigned char digest1[0x10];
    unsigned char hash1[0x20];
    unsigned char hash2[0x20];
    unsigned char sig1r[0x1C];
    unsigned char sig1s[0x1C];
    unsigned char sig2r[0x1C];
    unsigned char sig2s[0x1C];
    uint32_t magic;
    uint16_t sig_offset;
    uint16_t size;
    uint32_t field_C;

public:
    static const int Size = 0x100;
    uint32_t npdrm_type;

    explicit SceControlInfoDRM(const char *data) {
        memcpy(&content_id, &data[0x10], 0x30);
        memcpy(&digest1, &data[0x40], 0x10);
        memcpy(&hash1, &data[0x50], 0x20);
        memcpy(&hash2, &data[0x70], 0x20);
        memcpy(&sig1r, &data[0x90], 0x1C);
        memcpy(&sig1s, &data[0xAC], 0x1C);
        memcpy(&sig2r, &data[0xC8], 0x1C);
        memcpy(&sig2s, &data[0xE4], 0x1C);

        memcpy(&magic, &data[0], 4);
        memcpy(&sig_offset, &data[4], 2);
        memcpy(&size, &data[6], 2);
        memcpy(&npdrm_type, &data[8], 4);
        memcpy(&field_C, &data[12], 4);
    }
};

class SceRIF {
private:
    unsigned char content_id[0x30];
    unsigned char actidx[0x10];
    unsigned char dates[0x10];
    unsigned char filler[0x8];
    unsigned char sig1r[0x14];
    unsigned char sig1s[0xC];

    // These members are big endian
    uint16_t majver;
    uint16_t minver;
    uint16_t style;
    uint16_t riftype;
    uint64_t cid;

public:
    static const int Size = 0x98;
    unsigned char klicense[0x10];

    explicit SceRIF(const char *data) {
        memcpy(&majver, &data[0x0], 0x2);
        memcpy(&minver, &data[0x2], 0x2);
        memcpy(&style, &data[0x4], 0x2);
        memcpy(&riftype, &data[0x6], 0x2);
        memcpy(&cid, &data[0x8], 0x8);

        memcpy(&content_id, &data[0x10], 0x30);
        memcpy(&actidx, &data[0x40], 0x10);
        memcpy(&klicense, &data[0x50], 0x10);
        memcpy(&dates, &data[0x60], 0x10);
        memcpy(&filler, &data[0x70], 0x8);
        memcpy(&sig1r, &data[0x78], 0x14);
        memcpy(&sig1s, &data[0x8C], 0xC);
    }
};

void register_keys(KeyStore &SCE_KEYS, int type);
void extract_fat(const fs::path &partition_path, const std::string &partition, const fs::path &pref_path);
std::string decompress_segments(const std::vector<uint8_t> &decrypted_data, const uint64_t &size);
std::tuple<uint64_t, SelfType> get_key_type(std::ifstream &file, const SceHeader &sce_hdr);
std::vector<SceSegment> get_segments(const uint8_t *input, const SceHeader &sce_hdr, KeyStore &SCE_KEYS, uint64_t sysver = -1, SelfType self_type = static_cast<SelfType>(0), int keytype = 0, const uint8_t *klic = 0);
std::vector<uint8_t> decrypt_fself(const std::vector<uint8_t> &fself, const uint8_t *klic);
std::string resolve_ver_xml_url(const std::string &title_id);
