// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "savedata_transaction.h"
#include <pfsmgr/functions.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <util/log.h>
#include <util/types.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace pfsmgr {

constexpr size_t header_size = 0x160;
constexpr size_t page_size = 0x400;
constexpr size_t order = 10;
constexpr size_t key_size = 72;
constexpr size_t record_size = 16;
constexpr size_t records_offset = 16 + (order - 1) * key_size;
constexpr size_t hashes_offset = records_offset + order * record_size;
constexpr uint32_t no_parent = std::numeric_limits<uint32_t>::max();
constexpr uintmax_t max_database_size = 64 * 1024 * 1024;
constexpr uintmax_t max_file_size = 64 * 1024 * 1024;

static uint64_t savedata_disk_size(uint64_t size) {
    constexpr uint64_t block_size = 0x8000;
    uint64_t blocks = (size + block_size - 1) / block_size;
    const uint64_t data_size = blocks * block_size;
    uint64_t nodes = 0;
    while (blocks != 0) {
        blocks = (blocks + 22) / 23;
        nodes += blocks;
        if (blocks == 1)
            break;
    }
    const uint64_t icv_size = nodes == 0 ? block_size : ((nodes + 1) * page_size + block_size - 1) / block_size * block_size;
    return data_size + icv_size;
}

static bool savedata_name_equal(const std::string &left, const std::string &right);

static bool supported_savedata_file_attributes(uint16_t attributes) {
    return attributes == 0 || attributes == 1 || attributes == 4 || attributes == 6 || attributes == 7
        || attributes == 0x4000 || attributes == 0x4001 || attributes == 0x4004 || attributes == 0x4006 || attributes == 0x4007;
}

static bool valid_component(const std::string &name) {
    if (name.empty() || name == "." || name == ".." || name.find_first_of("\\/:") != std::string::npos)
        return false;
#ifdef _WIN32
    if (name.back() == '.' || name.back() == ' ' || name.find_first_of("<>\"|?*") != std::string::npos
        || std::any_of(name.begin(), name.end(), [](unsigned char ch) { return ch < 32; }))
        return false;
    auto base = name.substr(0, name.find('.'));
    std::transform(base.begin(), base.end(), base.begin(), [](unsigned char ch) {
        return static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch);
    });
    if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL"
        || (base.size() == 4 && (base.starts_with("COM") || base.starts_with("LPT")) && base[3] >= '1' && base[3] <= '9'))
        return false;
#endif
    return true;
}

std::filesystem::path Layer::savedata_host_path(const std::filesystem::path &relative_path) const {
    if (mode_ != Mode::Mode5 || relative_path.has_root_path())
        throw std::runtime_error("PFS: invalid savedata path");
    auto source = root_;
    for (const auto &part : relative_path) {
        const auto part_utf8 = part.u8string();
        const std::string name(part_utf8.begin(), part_utf8.end());
        if (!valid_component(name))
            throw std::runtime_error("PFS: invalid savedata path component");
        std::filesystem::path match;
        for (const auto &candidate : std::filesystem::directory_iterator(source)) {
            const auto candidate_utf8 = candidate.path().filename().u8string();
            const std::string candidate_name(candidate_utf8.begin(), candidate_utf8.end());
            if (!savedata_name_equal(candidate_name, name))
                continue;
            if (!match.empty())
                throw std::runtime_error("PFS: ambiguous savedata path");
            match = candidate.path();
        }
        if (match.empty() || std::filesystem::is_symlink(std::filesystem::symlink_status(match)))
            throw std::runtime_error("PFS: invalid savedata path");
        source = std::move(match);
    }
    return source;
}

/* 0x812764: DMAC5 keyseed type 0x00000 key */
static constexpr uint8_t s_dmac5KeyType0[32] = {
    0xe1u,
    0x22u,
    0x13u,
    0xb4u,
    0x80u,
    0x16u,
    0xb0u,
    0xe9u,
    0x9au,
    0xb8u,
    0x1fu,
    0x8eu,
    0xc0u,
    0x2au,
    0xd4u,
    0xa2u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
};

/* the hmac keys of the file and icv key derivations, and the CBC IV the *counter derivation encrypts its hmac with(names unknown) */
static constexpr SceUInt8 g_pfsFileKeyHmacKey[0x14] = {
    0xe4,
    0x62,
    0x25,
    0x8b,
    0x1f,
    0x31,
    0x21,
    0x56,
    0x07,
    0x45,
    0xdb,
    0x62,
    0xb1,
    0x43,
    0x67,
    0x23,
    0xd2,
    0xbf,
    0x80,
    0xfe,
};

static constexpr SceUInt8 g_pfsIcvKeyHmacKey[0x14] = {
    0xaf,
    0xe6,
    0x56,
    0xbb,
    0x3c,
    0x17,
    0x25,
    0x6a,
    0x3c,
    0x80,
    0x9f,
    0x6e,
    0x9b,
    0xf1,
    0x9f,
    0xdd,
    0x5a,
    0x38,
    0x85,
    0x43,
};

/* a 16-byte AES key (name unknown) */
typedef struct ScePfsKey {
    uint32_t words[4];
} ScePfsKey;

static constexpr ScePfsKey g_pfsCounterKeyIv = {
    0xc30cd274,
    0x13c28198,
    0x100b77ee,
    0xa7bee410,
};

static uint16_t read16(const uint8_t *p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

static uint32_t read32(const uint8_t *p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
        | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

static uint64_t read64(const uint8_t *p) {
    return static_cast<uint64_t>(read32(p)) | (static_cast<uint64_t>(read32(p + 4)) << 32);
}

static Hash hmac_sha1(const Hash &key, const uint8_t *data, size_t size) {
    Hash digest{};
    unsigned int length = 0;
    if (!HMAC(EVP_sha1(), key.data(), static_cast<int>(key.size()), data, size, digest.data(), &length)
        || length != digest.size())
        throw std::runtime_error("PFS: SHA-1 HMAC failed");
    return digest;
}

static Hash sha1(const uint8_t *data, size_t size) {
    Hash digest{};
    unsigned int length = 0;
    if (EVP_Digest(data, size, digest.data(), &length, EVP_sha1(), nullptr) != 1 || length != digest.size())
        throw std::runtime_error("PFS: SHA-1 failed");
    return digest;
}

static Hash derive_savedata_hash(const Hash &key_hash, uint32_t first, uint32_t second) {
    std::array<uint8_t, 8> values{};
    for (size_t i = 0; i < 4; ++i) {
        values[i] = static_cast<uint8_t>(first >> (8 * i));
        values[4 + i] = static_cast<uint8_t>(second >> (8 * i));
    }
    const auto value_hash = sha1(values.data(), values.size());
    std::array<uint8_t, 40> chain{};
    std::copy(key_hash.begin(), key_hash.end(), chain.begin());
    std::copy(value_hash.begin(), value_hash.end(), chain.begin() + key_hash.size());
    return sha1(chain.data(), chain.size());
}

Hash derive_savedata_secret(const AesKey &key, uint32_t index) {
    return derive_savedata_hash(sha1(key.data(), key.size()), 10, index);
}

SavedataFileKeys derive_savedata_file_keys(const AesKey &key, uint32_t index) {
    const auto key_hash = sha1(key.data(), key.size());
    const auto data_hash = derive_savedata_hash(key_hash, index, 1);
    const auto tweak_hash = derive_savedata_hash(key_hash, index, 2);
    SavedataFileKeys result{};
    std::copy_n(data_hash.begin(), result.data.size(), result.data.begin());
    std::copy_n(tweak_hash.begin(), result.tweak.size(), result.tweak.begin());
    return result;
}

template <size_t N>
static std::vector<uint8_t> aes_crypt(const EVP_CIPHER *cipher, const std::array<uint8_t, N> &key, const uint8_t *iv,
    const uint8_t *data, size_t size, bool encrypt) {
    if (size > static_cast<size_t>(std::numeric_limits<int>::max()) || size % 16 != 0)
        throw std::runtime_error("PFS: invalid AES input size");
    const std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx || EVP_CipherInit_ex(ctx.get(), cipher, nullptr, key.data(), iv, encrypt ? 1 : 0) != 1
        || EVP_CIPHER_CTX_set_padding(ctx.get(), 0) != 1)
        throw std::runtime_error("PFS: cannot initialize AES");
    std::vector<uint8_t> output(size + 16);
    int written = 0;
    int final = 0;
    if (EVP_CipherUpdate(ctx.get(), output.data(), &written, data, static_cast<int>(size)) != 1
        || EVP_CipherFinal_ex(ctx.get(), output.data() + written, &final) != 1
        || static_cast<size_t>(written + final) != size)
        throw std::runtime_error("PFS: AES operation failed");
    output.resize(size);
    return output;
}

#define SEALED_KEY_MAGIC "pfsSKKey"
#define SEALED_KEY_ENC_MAGIC "pfsSKKey__EncKey"
#define SEALED_KEY_SEC_MAGIC "pfsSKKey__Secret"

typedef struct ScePortabilityData {
    SceSize msg_size; /* at most sizeof(msg) */
    SceUInt8 msg[0x20];
} ScePortabilityData;

typedef struct SceSealedKey {
    char magic[8];
    SceUInt8 version;
    SceUInt8 versionHigh;
    SceUInt8 reserved[6];
    SceUInt8 iv[0x10];
    SceUInt8 key[0x10];
    SceUInt8 hmac[0x20];
} SceSealedKey;

typedef struct SceAesCompactContext {
    uint32_t size;
    uint32_t keyWords;
    uint32_t blockWords;
    uint32_t rounds;
    uint32_t roundKeys[120];
} SceAesCompactContext;

typedef struct SceSealedKeyCipherContext {
    SceAesCompactContext aes;
    SceUInt8 iv[0x20];
} SceSealedKeyCipherContext;

static constexpr SceUInt8 g_sealedKeySecIv1[0x10] = {
    0xb3, 0x1e, 0x7e, 0x0e, 0xbf, 0x82, 0xa3, 0xb1,
    0x73, 0x9d, 0x16, 0x43, 0x62, 0x98, 0x27, 0x09
};

static constexpr ScePortabilityData g_sealedKeySecData0 = {
    .msg_size = 0x20,
    .msg = {
        0x13,
        0x3e,
        0xf7,
        0x73,
        0xbb,
        0x61,
        0xee,
        0xc3,
        0x9b,
        0x02,
        0xe1,
        0x46,
        0x35,
        0x2a,
        0xff,
        0xdf,
        0x40,
        0x3d,
        0x4b,
        0xf5,
        0x85,
        0xcd,
        0x1e,
        0xed,
        0x38,
        0x50,
        0x6f,
        0x17,
        0xb1,
        0x94,
        0xf7,
        0x69,
    },
};

static constexpr SceUInt8 g_sealedKeyEncIv0[0x10] = {
    0x57, 0xd4, 0xab, 0x60, 0x5b, 0x5d, 0x35, 0x24,
    0x8a, 0xf4, 0xa9, 0xc2, 0xe2, 0xfd, 0x86, 0x0e
};

static constexpr ScePortabilityData g_sealedKeyEncData1 = {
    .msg_size = 0x20,
    .msg = {
        0x12,
        0xe9,
        0x45,
        0x3e,
        0xcb,
        0x8f,
        0x21,
        0x54,
        0x33,
        0xa1,
        0x44,
        0x13,
        0x30,
        0x2c,
        0xea,
        0x1d,
        0x4f,
        0xf0,
        0x5d,
        0xed,
        0xe4,
        0x30,
        0xed,
        0x2f,
        0xf3,
        0x0f,
        0x6d,
        0xb7,
        0xf6,
        0xef,
        0xe7,
        0x53,
    },
};

static constexpr SceUInt8 g_sealedKeySecIv0[0x10] = {
    0x93,
    0x98,
    0x0f,
    0x23,
    0x66,
    0x2a,
    0x69,
    0xaa,
    0x0a,
    0x49,
    0x3a,
    0xd1,
    0xbd,
    0x37,
    0xf4,
    0x03,
};

static constexpr ScePortabilityData g_sealedKeyEncData0 = {
    .msg_size = 0x20,
    .msg = {
        0x8b,
        0xe4,
        0x2b,
        0x24,
        0x94,
        0xf9,
        0xf9,
        0x71,
        0x94,
        0x52,
        0x57,
        0x3b,
        0xbe,
        0x0a,
        0x36,
        0x0d,
        0x3e,
        0x8e,
        0x13,
        0x79,
        0x0d,
        0x2f,
        0xf9,
        0x5c,
        0xc6,
        0x70,
        0xca,
        0xb9,
        0xa2,
        0x8e,
        0x9d,
        0x42,
    },
};

static constexpr SceUInt8 g_sealedKeyEncIv1[0x10] = {
    0x4e, 0x30, 0x5a, 0x5a, 0x58, 0x75, 0x31, 0xcf,
    0x2f, 0x5e, 0x94, 0x47, 0x7b, 0xba, 0x61, 0x60
};

static constexpr ScePortabilityData g_sealedKeySecData1 = {
    .msg_size = 0x20,
    .msg = {
        0xe9, 0x79, 0xde, 0xb7, 0x26, 0x49, 0x43, 0x20,
        0x87, 0x72, 0xcc, 0xa7, 0x42, 0x2d, 0x9e, 0xcc,
        0xed, 0xed, 0x98, 0x02, 0x56, 0x69, 0x65, 0xec,
        0xec, 0x33, 0xa8, 0x5c, 0xf2, 0x7f, 0xe7, 0x7f }
};

static bool decrypt_portability_key(const std::array<uint8_t, 32> &key, const SceUInt8 *iv,
    const ScePortabilityData &data, const char *magic, AesKey &result, const char *&error) {
    if (data.msg_size != 0x20) {
        error = "PFS: invalid sealedkey portability data";
        LOG_ERROR("{}", error);
        return false;
    }
    const auto plaintext = aes_crypt(EVP_aes_256_cbc(), key, iv, data.msg, data.msg_size, false);
    if (CRYPTO_memcmp(plaintext.data(), magic, 0x10) != 0) {
        error = "PFS: sealedkey portability data authentication failed";
        LOG_ERROR("{}", error);
        return false;
    }
    std::copy_n(plaintext.begin() + 0x10, result.size(), result.begin());
    return true;
}

static constexpr AesKey ebootparam_key_iv{
    0xfc, 0x9b, 0xa4, 0x1a, 0x36, 0xd5, 0x9b, 0x6f,
    0x2a, 0x48, 0x0d, 0x34, 0xe1, 0x78, 0x66, 0x0c
};

static constexpr std::array<uint8_t, 32> ebootparam_encrypted_key_debug{
    0x3c, 0x32, 0xc3, 0xf8, 0x65, 0x8e, 0x32, 0xfe,
    0x4b, 0xca, 0xc2, 0x69, 0x07, 0x07, 0x6d, 0xc6,
    0xa8, 0xf5, 0x3a, 0x6f, 0x9d, 0x57, 0xcb, 0xa5,
    0x31, 0x2c, 0x35, 0x55, 0xa7, 0xe2, 0xa5, 0xda
};

static constexpr std::array<uint8_t, 32> ebootparam_encrypted_key_retail{
    0x3c, 0x67, 0xad, 0x8f, 0xca, 0x88, 0xb8, 0x85,
    0x8e, 0xc0, 0x21, 0xdd, 0x7e, 0x33, 0x1a, 0xf4,
    0x0a, 0x13, 0x2a, 0x13, 0x73, 0x3e, 0x86, 0xe7,
    0xe6, 0x87, 0x70, 0xd8, 0xef, 0xbe, 0x4f, 0x09
};

/* key  7 (0x80ba80) debug */
static constexpr std::array<uint8_t, 32> debug_portability_key7{
    0xc8u, 0xbfu, 0xadu, 0xc3u, 0x85u, 0x6bu, 0x9cu, 0xcbu,
    0x34u, 0x7du, 0x07u, 0x42u, 0x8bu, 0xb0u, 0x1fu, 0xc1u,
    0x50u, 0xd2u, 0xddu, 0xd8u, 0xe7u, 0x99u, 0xdau, 0x55u,
    0x53u, 0x98u, 0xd3u, 0x51u, 0x27u, 0x81u, 0xc8u, 0xa3u
};

/* key  7 (0x80ba80) release */
static constexpr std::array<uint8_t, 32> retail_portability_key7{
    0xfcu, 0xdfu, 0x26u, 0x1eu, 0x1du, 0x91u, 0xd3u, 0xdfu,
    0x79u, 0x0bu, 0x1bu, 0x89u, 0x89u, 0xe0u, 0x46u, 0xebu,
    0x5bu, 0xe0u, 0xebu, 0x77u, 0x2eu, 0xafu, 0x4bu, 0xe3u,
    0xf6u, 0xf5u, 0x8cu, 0x02u, 0xc4u, 0x29u, 0x6cu, 0x52u
};

std::vector<uint8_t> decrypt_portability_data(const PortabilityKey &key, const AesKey &iv, std::span<const uint8_t> data) {
    if (data.size() > 32 || data.size() % 16 != 0)
        throw std::runtime_error("PFS: invalid portability data size");
    if (data.empty())
        return {};
    return aes_crypt(EVP_aes_256_cbc(), key, iv.data(), data.data(), data.size(), false);
}

std::vector<uint8_t> getDecryptedPortabilityData(const AesKey &iv, std::span<const uint8_t> data, bool debug) {
    return decrypt_portability_data(debug ? debug_portability_key7 : retail_portability_key7, iv, data);
}

PortabilityKey getEbootparamKey() {
    struct Plaintext {
        std::vector<uint8_t> data;
        ~Plaintext() { OPENSSL_cleanse(data.data(), data.size()); }
    };
    const Plaintext debug{ getDecryptedPortabilityData(ebootparam_key_iv, ebootparam_encrypted_key_debug, true) };
    const Plaintext retail{ getDecryptedPortabilityData(ebootparam_key_iv, ebootparam_encrypted_key_retail, false) };
    constexpr std::array<uint8_t, 3> marker{ 'E', 'B', 'P' };
    if (debug.data.size() != PortabilityKey{}.size() || retail.data.size() != PortabilityKey{}.size()
        || CRYPTO_memcmp(debug.data.data(), marker.data(), marker.size()) != 0
        || CRYPTO_memcmp(retail.data.data(), marker.data(), marker.size()) != 0
        || CRYPTO_memcmp(debug.data.data(), retail.data.data(), debug.data.size()) != 0)
        throw std::runtime_error("PFS: invalid ebootparam portability data");
    PortabilityKey key{};
    std::copy(debug.data.begin(), debug.data.end(), key.begin());
    return key;
}

static std::pair<AesKey, AesKey> getSealedKeySecret() {
    constexpr std::array<uint8_t, 32> retail_portability_key{
        0xdeu, 0xadu, 0x01u, 0x08u, 0x5fu, 0x00u, 0xd5u, 0x13u, 0xc3u, 0x5du, 0xb4u, 0xc5u, 0x1eu, 0xe2u, 0xecu, 0x27u,
        0x50u, 0xe5u, 0xa0u, 0xcbu, 0xefu, 0x81u, 0x47u, 0xb4u, 0x02u, 0x08u, 0x05u, 0x06u, 0x8au, 0x2bu, 0xf3u, 0x4bu
    };
    constexpr std::array<uint8_t, 32> debug_portability_key{
        0x15u, 0x5fu, 0x7du, 0xcau, 0xefu, 0x58u, 0xd2u, 0xe5u, 0xcbu, 0x92u, 0x28u, 0x6fu, 0xf5u, 0x7au, 0x4du, 0xa1u,
        0xfbu, 0x56u, 0x23u, 0xd8u, 0x3bu, 0x62u, 0xabu, 0xe4u, 0x38u, 0xb6u, 0xb9u, 0x2du, 0x04u, 0xcfu, 0x6du, 0xc4u
    };
    AesKey enc{}, secret{}, enc_backup{}, secret_backup{};
    const char *error = nullptr;
    if (!decrypt_portability_key(retail_portability_key, g_sealedKeyEncIv0, g_sealedKeyEncData0, SEALED_KEY_ENC_MAGIC, enc, error)
        || !decrypt_portability_key(retail_portability_key, g_sealedKeySecIv0, g_sealedKeySecData0, SEALED_KEY_SEC_MAGIC, secret, error)
        || !decrypt_portability_key(debug_portability_key, g_sealedKeyEncIv1, g_sealedKeyEncData1, SEALED_KEY_ENC_MAGIC, enc_backup, error)
        || !decrypt_portability_key(debug_portability_key, g_sealedKeySecIv1, g_sealedKeySecData1, SEALED_KEY_SEC_MAGIC, secret_backup, error)
        || CRYPTO_memcmp(enc.data(), enc_backup.data(), enc.size()) != 0
        || CRYPTO_memcmp(secret.data(), secret_backup.data(), secret.size()) != 0)
        throw std::runtime_error(error ? error : "PFS: sealedkey portability copies do not match");
    return { enc, secret };
}

GeneratedSavedataKey generate_savedata_key() {
    GeneratedSavedataKey result{};
    std::memcpy(result.sealedkey.data(), SEALED_KEY_MAGIC, sizeof(SEALED_KEY_MAGIC) - 1);
    result.sealedkey[8] = 2;
    if (RAND_priv_bytes(result.key.data(), static_cast<int>(result.key.size())) != 1
        || RAND_bytes(result.sealedkey.data() + 0x10, 0x10) != 1)
        throw std::runtime_error("PFS: cannot generate savedata key or IV");
    const auto [enc_key, secret] = getSealedKeySecret();
    const auto encrypted = aes_crypt(EVP_aes_128_cbc(), enc_key, result.sealedkey.data() + 0x10,
        result.key.data(), result.key.size(), true);
    std::copy(encrypted.begin(), encrypted.end(), result.sealedkey.begin() + 0x20);
    unsigned int length = 0;
    if (!HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()), result.sealedkey.data(), 0x30,
            result.sealedkey.data() + 0x30, &length)
        || length != 0x20)
        throw std::runtime_error("PFS: sealedkey HMAC failed");
    return result;
}

AesKey unseal_savedata_key(std::span<const uint8_t> sealedkey) {
    if (sealedkey.size() != 0x50 || std::memcmp(sealedkey.data(), SEALED_KEY_MAGIC, sizeof(SEALED_KEY_MAGIC) - 1) != 0)
        throw std::runtime_error("PFS: invalid sealedkey header or size");
    if (read16(sealedkey.data() + 8) != 2)
        throw std::runtime_error("PFS: unsupported sealedkey version");
    const auto [enc_key, secret] = getSealedKeySecret();
    std::array<uint8_t, 32> digest{};
    unsigned int length = 0;
    if (!HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()), sealedkey.data(), 0x30, digest.data(), &length)
        || length != digest.size())
        throw std::runtime_error("PFS: sealedkey HMAC failed");
    if (CRYPTO_memcmp(digest.data(), sealedkey.data() + 0x30, digest.size()) != 0)
        throw std::runtime_error("PFS: sealedkey HMAC mismatch");
    const auto plaintext = aes_crypt(EVP_aes_128_cbc(), enc_key,
        sealedkey.data() + 0x10, sealedkey.data() + 0x20, AesKey{}.size(), false);
    AesKey result{};
    std::copy(plaintext.begin(), plaintext.end(), result.begin());
    return result;
}

static bool equal_hash(const Hash &a, const uint8_t *b) {
    return CRYPTO_memcmp(a.data(), b, a.size()) == 0;
}

static std::vector<uint8_t> read_regular_file(const std::filesystem::path &path, uintmax_t limit) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error || !std::filesystem::is_regular_file(status))
        throw std::runtime_error("PFS: source is not a regular file");
    const auto length = std::filesystem::file_size(path, error);
    if (error || length > limit)
        throw std::runtime_error("PFS: source size exceeds the supported limit");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("PFS: cannot open source");
    std::vector<uint8_t> data(static_cast<size_t>(length));
    if (!input.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size())))
        throw std::runtime_error("PFS: cannot read source");
    return data;
}

static AesKey read_savedata_bootstrap_key(const std::filesystem::path &root) {
    if (!std::filesystem::is_directory(std::filesystem::symlink_status(root))
        || !std::filesystem::is_directory(std::filesystem::symlink_status(root / "sce_sys")))
        throw std::runtime_error("PFS: invalid sealedkey directory");
    const auto data = read_regular_file(root / "sce_sys" / "sealedkey", 0x8000);
    if (data.size() < 0x50)
        throw std::runtime_error("PFS: truncated sealedkey");
    return unseal_savedata_key(std::span(data).first(0x50));
}

void recover_savedata_transaction(const std::filesystem::path &root, const std::optional<AesKey> &key) {
    const auto savedata_key = key ? *key : read_savedata_bootstrap_key(root);
    if (!detail::recover_savedata_transaction(root, derive_savedata_secret(savedata_key, 0)))
        throw std::runtime_error("PFS: savedata transaction recovery failed");
}

struct PendingPage {
    uint32_t index;
    uint32_t parent;
    Hash expected;
};

KeyMaterial make_key_material(const Hash &icv_hmac, const Hash &file_hmac, const AesKey &counter_iv) {
    KeyMaterial material{};
    std::copy_n(s_dmac5KeyType0, material.dmac5_type0.size(), material.dmac5_type0.begin());
    material.icv_hmac = icv_hmac;
    material.file_hmac = file_hmac;
    material.counter_iv = counter_iv;
    return material;
}

KeyMaterial make_key_material() {
    Hash icv{};
    Hash file{};
    AesKey iv{};
    std::copy_n(g_pfsIcvKeyHmacKey, icv.size(), icv.begin());
    std::copy_n(g_pfsFileKeyHmacKey, file.size(), file.begin());
    for (size_t i = 0; i < iv.size(); ++i)
        iv[i] = static_cast<uint8_t>(g_pfsCounterKeyIv.words[i / 4] >> (8 * (i % 4)));
    return make_key_material(icv, file, iv);
}

AesKey prepare_np_key(const AesKey &license_key, const KeyMaterial &material) {
    const auto key = aes_crypt(EVP_aes_128_ecb(), material.dmac5_type0, nullptr,
        license_key.data(), license_key.size(), false);
    AesKey result{};
    std::copy(key.begin(), key.end(), result.begin());
    return result;
}

Hash derive_np_secret(const AesKey &np_key, const KeyMaterial &material, uint32_t key_param, uint32_t index) {
    std::array<uint8_t, 8> values{};
    const auto write32 = [](uint8_t *out, uint32_t value) {
        for (size_t i = 0; i < 4; ++i)
            out[i] = static_cast<uint8_t>(value >> (8 * i));
    };
    size_t length = 4;
    if (key_param != 0) {
        write32(values.data(), key_param);
        write32(values.data() + 4, index);
        length = 8;
    } else {
        write32(values.data(), index);
    }
    const auto digest = hmac_sha1(material.icv_hmac, values.data(), length);
    const auto first = aes_crypt(EVP_aes_128_cbc(), np_key, material.counter_iv.data(), digest.data(), 16, true);
    const auto tail = aes_crypt(EVP_aes_128_ecb(), np_key, nullptr, first.data(), first.size(), true);
    Hash result{};
    std::copy(first.begin(), first.end(), result.begin());
    for (size_t i = 0; i < 4; ++i)
        result[16 + i] = digest[16 + i] ^ tail[i];
    return result;
}

static Hash savedata_db_node_hash(const uint8_t *node, const Hash &key) {
    const auto count = read32(node + 8) + read32(node + 4);
    Hash hash{};
    for (size_t i = 0; i < count; ++i) {
        std::array<uint8_t, Hash{}.size() * 2> combined{};
        std::copy(hash.begin(), hash.end(), combined.begin());
        std::copy_n(node + hashes_offset + i * hash.size(), hash.size(), combined.begin() + hash.size());
        hash = hmac_sha1(key, combined.data(), combined.size());
    }
    return hash;
}

static std::vector<FileEntry> parse_file_table(std::span<const uint8_t> data, const Hash &key) {
    if (data.size() < page_size * 2 || data.size() > max_database_size || data.size() % page_size != 0)
        throw std::runtime_error("PFS: invalid files.db size");
    const auto version = read32(data.data() + 8);
    const auto image_spec = read16(data.data() + 12);
    if (std::memcmp(data.data(), "SCENGPFS", 8) != 0
        || !((version == 5 && image_spec == 1) || (version == 3 && (image_spec == 1 || image_spec == 2 || image_spec == 4)))
        || read16(data.data() + 14) != 0
        || read32(data.data() + 16) != page_size || read32(data.data() + 20) != order)
        throw std::runtime_error("PFS: unsupported files.db format");
    if (version == 3 && image_spec == 2 && read32(data.data() + 0x60) != 0)
        throw std::runtime_error("PFS: savedata files.db was left dirty");

    std::array<uint8_t, header_size> header{};
    std::copy_n(data.data(), header.size(), header.data());
    std::fill(header.begin() + 0x4c, header.end(), 0);
    if (!equal_hash(hmac_sha1(key, header.data(), header.size()), data.data() + 0x4c))
        throw std::runtime_error("PFS: files.db header authentication failed");

    PendingPage root{ read32(data.data() + 0x18), no_parent, {} };
    std::copy_n(data.data() + 0x38, root.expected.size(), root.expected.begin());
    std::vector<PendingPage> pending{ root };
    std::set<uint32_t> visited;
    std::set<std::pair<uint32_t, std::string>> names;
    std::vector<FileEntry> entries;

    while (!pending.empty()) {
        const auto current = pending.back();
        pending.pop_back();
        const auto page_count = data.size() / page_size - 1;
        if (current.index >= page_count || !visited.insert(current.index).second)
            throw std::runtime_error("PFS: invalid or cyclic files.db tree");
        const auto *node = data.data() + page_size * (static_cast<size_t>(current.index) + 1);
        const auto internal = read32(node + 4);
        const auto count = read32(node + 8);
        if (internal > 1 || count > order - 1 || (internal && count == 0))
            throw std::runtime_error("PFS: invalid files.db node");
        Hash node_hash{};
        if (version == 5) {
            node_hash = hmac_sha1(key, node + 4, page_size - 4);
        } else {
            node_hash = savedata_db_node_hash(node, key);
        }
        if (read32(node) != current.parent || !equal_hash(node_hash, current.expected.data()))
            throw std::runtime_error("PFS: files.db tree authentication failed");

        if (internal) {
            for (size_t i = 0; i <= count; ++i) {
                PendingPage child{ read32(node + records_offset + i * record_size), current.index, {} };
                std::copy_n(node + hashes_offset + i * child.expected.size(), child.expected.size(), child.expected.begin());
                pending.push_back(child);
            }
            continue;
        }

        for (size_t i = 0; i < count; ++i) {
            const auto *entry_key = node + 16 + i * key_size;
            const auto *name = reinterpret_cast<const char *>(entry_key + 4);
            const auto *end = static_cast<const char *>(std::memchr(name, '\0', 64));
            if (!end || end == name)
                throw std::runtime_error("PFS: invalid files.db name");
            const auto *record = node + records_offset + i * record_size;
            FileEntry entry{ read32(entry_key), std::string(name, end), read32(record), read16(record + 4), read64(record + 8), {} };
            if (!valid_component(entry.name))
                throw std::runtime_error("PFS: invalid files.db path component");
            if (!names.emplace(entry.parent, entry.name).second)
                throw std::runtime_error("PFS: duplicate files.db entry");
            std::copy_n(node + hashes_offset + i * entry.hash.size(), entry.hash.size(), entry.hash.begin());
            entries.push_back(std::move(entry));
        }
    }
    return entries;
}

std::vector<FileEntry> read_file_table(const std::filesystem::path &path, const Hash &key) {
    return parse_file_table(read_regular_file(path, max_database_size), key);
}

std::vector<uint8_t> update_savedata_file_table(std::span<const uint8_t> database, const FileEntry &expected,
    uint64_t size, const Hash &file_hash, const Hash &database_key) {
    const auto entries = parse_file_table(database, database_key);
    if (read32(database.data() + 8) != 3 || read16(database.data() + 12) != 2 || read32(database.data() + 0x1c) != 0
        || expected.size > max_file_size || size > max_file_size
        || expected.index == 0 || expected.index == no_parent || !supported_savedata_file_attributes(expected.attributes))
        throw std::runtime_error("PFS: unsupported savedata table update");
    const auto matches = std::count_if(entries.begin(), entries.end(), [&](const FileEntry &entry) {
        return entry.index == expected.index;
    });
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const FileEntry &entry) {
        return entry.index == expected.index && entry.parent == expected.parent && entry.name == expected.name
            && entry.attributes == expected.attributes && entry.size == expected.size && entry.hash == expected.hash;
    });
    if (matches != 1 || found == entries.end())
        throw std::runtime_error("PFS: missing, ambiguous or stale savedata entry");

    std::vector<uint8_t> result(database.begin(), database.end());
    const auto put64 = [&](size_t offset, uint64_t value) {
        for (size_t i = 0; i < 8; ++i)
            result[offset + i] = static_cast<uint8_t>(value >> (8 * i));
    };
    const uint64_t old_disk_size = savedata_disk_size(expected.size);
    const uint64_t new_disk_size = savedata_disk_size(size);
    const uint64_t usage = read64(result.data() + 0x30);
    if (usage < old_disk_size || usage - old_disk_size > std::numeric_limits<uint64_t>::max() - new_disk_size)
        throw std::runtime_error("PFS: invalid savedata disk usage");
    put64(0x30, usage - old_disk_size + new_disk_size);

    std::vector<uint32_t> pending{ read32(result.data() + 0x18) };
    uint32_t target = no_parent;
    while (!pending.empty()) {
        const auto page = pending.back();
        pending.pop_back();
        auto *node = result.data() + page_size * (static_cast<size_t>(page) + 1);
        const auto count = read32(node + 8);
        if (read32(node + 4)) {
            for (size_t i = 0; i <= count; ++i)
                pending.push_back(read32(node + records_offset + i * record_size));
        } else {
            for (size_t i = 0; i < count; ++i) {
                if (read32(node + records_offset + i * record_size) != expected.index)
                    continue;
                put64(page_size * (static_cast<size_t>(page) + 1) + records_offset + i * record_size + 8, size);
                std::copy(file_hash.begin(), file_hash.end(), node + hashes_offset + i * file_hash.size());
                target = page;
            }
        }
    }
    if (target == no_parent)
        throw std::runtime_error("PFS: savedata entry disappeared");
    for (;;) {
        auto *node = result.data() + page_size * (static_cast<size_t>(target) + 1);
        const auto node_hash = savedata_db_node_hash(node, database_key);
        const auto parent = read32(node);
        if (parent == no_parent) {
            std::copy(node_hash.begin(), node_hash.end(), result.begin() + 0x38);
            break;
        }
        auto *parent_node = result.data() + page_size * (static_cast<size_t>(parent) + 1);
        const auto count = read32(parent_node + 8);
        size_t slot = 0;
        while (slot <= count && read32(parent_node + records_offset + slot * record_size) != target)
            ++slot;
        if (slot > count)
            throw std::runtime_error("PFS: invalid savedata parent link");
        std::copy(node_hash.begin(), node_hash.end(), parent_node + hashes_offset + slot * node_hash.size());
        target = parent;
    }
    std::array<uint8_t, header_size> header{};
    std::copy_n(result.data(), header.size(), header.data());
    std::fill(header.begin() + 0x4c, header.end(), 0);
    const auto header_hash = hmac_sha1(database_key, header.data(), header.size());
    std::copy(header_hash.begin(), header_hash.end(), result.begin() + 0x4c);
    return result;
}

static bool simple_savedata_name(const std::string &name) {
    return std::all_of(name.begin(), name.end(), [](unsigned char ch) { return ch >= 0x20 && ch < 0x7f && !(ch >= 'a' && ch <= 'z'); });
}

static std::vector<uint8_t> rebuild_savedata_file_table(std::span<const uint8_t> database, std::vector<FileEntry> entries,
    uint64_t usage, const Hash &database_key) {
    struct OriginalRecord {
        std::array<uint8_t, key_size> key;
        std::array<uint8_t, record_size> record;
    };
    std::map<uint32_t, OriginalRecord> original_records;
    std::vector<uint32_t> pending{ read32(database.data() + 0x18) };
    while (!pending.empty()) {
        const auto page = pending.back();
        pending.pop_back();
        const auto *node = database.data() + page_size * (static_cast<size_t>(page) + 1);
        const auto count = read32(node + 8);
        if (read32(node + 4)) {
            for (size_t i = 0; i <= count; ++i)
                pending.push_back(read32(node + records_offset + i * record_size));
        } else {
            for (size_t i = 0; i < count; ++i) {
                OriginalRecord original{};
                std::copy_n(node + 16 + i * key_size, original.key.size(), original.key.begin());
                std::copy_n(node + records_offset + i * record_size, original.record.size(), original.record.begin());
                original_records.emplace(read32(original.record.data()), original);
            }
        }
    }
    std::sort(entries.begin(), entries.end(), [](const FileEntry &left, const FileEntry &right) {
        return left.parent != right.parent ? left.parent < right.parent : left.name < right.name;
    });

    std::vector<uint8_t> result(database.begin(), database.begin() + page_size);
    const auto put32 = [&](size_t offset, uint32_t value) {
        for (size_t i = 0; i < 4; ++i)
            result[offset + i] = static_cast<uint8_t>(value >> (8 * i));
    };
    const auto put64 = [&](size_t offset, uint64_t value) {
        for (size_t i = 0; i < 8; ++i)
            result[offset + i] = static_cast<uint8_t>(value >> (8 * i));
    };
    const auto allocate = [&]() {
        if (result.size() > max_database_size - page_size)
            throw std::runtime_error("PFS: savedata database is too large");
        const auto page = static_cast<uint32_t>(result.size() / page_size - 1);
        result.resize(result.size() + page_size);
        const size_t base = (static_cast<size_t>(page) + 1) * page_size;
        put32(base, no_parent);
        for (size_t i = 0; i < order; ++i)
            put32(base + records_offset + i * record_size, no_parent);
        return page;
    };
    const auto put_key = [&](size_t offset, const FileEntry &item) {
        put32(offset, item.parent);
        std::copy(item.name.begin(), item.name.end(), result.begin() + offset + 4);
    };
    struct BuiltNode {
        uint32_t page;
        size_t first;
        Hash hash;
    };
    std::vector<BuiltNode> level;
    const size_t leaf_count = std::max<size_t>(1, (entries.size() + order - 2) / (order - 1));
    size_t first = 0;
    for (size_t group = 0; group < leaf_count; ++group) {
        const size_t count = entries.size() / leaf_count + (group < entries.size() % leaf_count ? 1 : 0);
        const auto page = allocate();
        const size_t base = (static_cast<size_t>(page) + 1) * page_size;
        put32(base + 8, static_cast<uint32_t>(count));
        for (size_t i = 0; i < count; ++i) {
            const auto &item = entries[first + i];
            const auto original = original_records.find(item.index);
            if (original != original_records.end()) {
                std::copy(original->second.key.begin(), original->second.key.end(), result.begin() + base + 16 + i * key_size);
                std::copy(original->second.record.begin(), original->second.record.end(), result.begin() + base + records_offset + i * record_size);
            } else {
                put_key(base + 16 + i * key_size, item);
                put32(base + records_offset + i * record_size, item.index);
                result[base + records_offset + i * record_size + 4] = static_cast<uint8_t>(item.attributes);
                result[base + records_offset + i * record_size + 5] = static_cast<uint8_t>(item.attributes >> 8);
                put64(base + records_offset + i * record_size + 8, item.size);
            }
            std::copy(item.hash.begin(), item.hash.end(), result.begin() + base + hashes_offset + i * item.hash.size());
        }
        if (!level.empty()) {
            const size_t previous = (static_cast<size_t>(level.back().page) + 1) * page_size;
            put32(previous + records_offset + (order - 1) * record_size, page);
        }
        level.push_back({ page, first, savedata_db_node_hash(result.data() + base, database_key) });
        first += count;
    }
    while (level.size() > 1) {
        const size_t groups = (level.size() + order - 1) / order;
        std::vector<BuiltNode> next;
        size_t start = 0;
        for (size_t group = 0; group < groups; ++group) {
            const size_t children = level.size() / groups + (group < level.size() % groups ? 1 : 0);
            const auto page = allocate();
            const size_t base = (static_cast<size_t>(page) + 1) * page_size;
            put32(base + 4, 1);
            put32(base + 8, static_cast<uint32_t>(children - 1));
            for (size_t i = 0; i < children; ++i) {
                const auto &child = level[start + i];
                put32((static_cast<size_t>(child.page) + 1) * page_size, page);
                put32(base + records_offset + i * record_size, child.page);
                std::copy(child.hash.begin(), child.hash.end(), result.begin() + base + hashes_offset + i * child.hash.size());
                if (i != 0)
                    put_key(base + 16 + (i - 1) * key_size, entries[child.first]);
            }
            next.push_back({ page, level[start].first, savedata_db_node_hash(result.data() + base, database_key) });
            start += children;
        }
        level = std::move(next);
    }
    put32(0x18, level.front().page);
    put64(0x20, std::numeric_limits<uint64_t>::max());
    put64(0x28, result.size() - page_size);
    put64(0x30, usage);
    std::copy(level.front().hash.begin(), level.front().hash.end(), result.begin() + 0x38);
    std::array<uint8_t, header_size> header{};
    std::copy_n(result.data(), header.size(), header.data());
    std::fill(header.begin() + 0x4c, header.end(), 0);
    const auto header_hash = hmac_sha1(database_key, header.data(), header.size());
    std::copy(header_hash.begin(), header_hash.end(), result.begin() + 0x4c);
    return result;
}

static std::vector<uint8_t> insert_savedata_entry_table(std::span<const uint8_t> database, const FileEntry &entry,
    const Hash &database_key, bool directory) {
    auto entries = parse_file_table(database, database_key);
    if (read32(database.data() + 8) != 3 || read16(database.data() + 12) != 2 || read32(database.data() + 0x1c) != 0
        || entry.index == 0 || entry.index == no_parent || entry.size > max_file_size
        || (directory ? entry.attributes != 0x8006 || entry.size != 0x8000 : !supported_savedata_file_attributes(entry.attributes))
        || !valid_component(entry.name) || entry.name.size() >= 64)
        throw std::runtime_error("PFS: unsupported savedata insertion");
    if (!simple_savedata_name(entry.name) || std::any_of(entries.begin(), entries.end(), [&](const FileEntry &existing) {
            return !simple_savedata_name(existing.name) || existing.index == entry.index
                || (existing.parent == entry.parent && savedata_name_equal(existing.name, entry.name));
        }))
        throw std::runtime_error("PFS: duplicate or unsupported savedata entry");
    if (entry.parent != 0 && std::none_of(entries.begin(), entries.end(), [&](const FileEntry &parent) {
            return parent.index == entry.parent && (parent.attributes & 0x8000) != 0;
        }))
        throw std::runtime_error("PFS: missing savedata parent directory");
    std::set<uint32_t> indices;
    for (const auto &existing : entries) {
        if (existing.index == 0 || existing.index == no_parent || !indices.insert(existing.index).second)
            throw std::runtime_error("PFS: invalid savedata entry index");
    }
    const uint64_t disk_size = directory ? 0x10000 : savedata_disk_size(entry.size);
    const uint64_t usage = read64(database.data() + 0x30);
    if (usage > std::numeric_limits<uint64_t>::max() - disk_size)
        throw std::runtime_error("PFS: savedata disk usage overflow");
    entries.push_back(entry);
    return rebuild_savedata_file_table(database, std::move(entries), usage + disk_size, database_key);
}

std::vector<uint8_t> insert_savedata_file_table(std::span<const uint8_t> database, const FileEntry &entry, const Hash &database_key) {
    return insert_savedata_entry_table(database, entry, database_key, false);
}

static std::array<uint8_t, 24> make_savedata_directory_icv() {
    std::array<uint8_t, 24> icv{};
    std::memcpy(icv.data(), "SCEINULL", 8);
    icv[8] = 1;
    return icv;
}

PreparedSavedataDirectory prepare_savedata_directory(std::span<const uint8_t> database, uint32_t parent,
    uint32_t index, const std::string &name, const AesKey &savedata_key) {
    PreparedSavedataDirectory result{};
    result.icv = make_savedata_directory_icv();
    auto normalized_name = name;
    std::transform(normalized_name.begin(), normalized_name.end(), normalized_name.begin(), [](unsigned char ch) {
        return static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch);
    });
    const auto directory_key = derive_savedata_secret(savedata_key, index);
    result.entry = { parent, std::move(normalized_name), index, 0x8006, 0x8000,
        hmac_sha1(directory_key, result.icv.data(), result.icv.size()) };
    const auto database_key = derive_savedata_secret(savedata_key, 0);
    result.files_db = insert_savedata_entry_table(database, result.entry, database_key, true);
    (void)parse_file_table(result.files_db, database_key);
    return result;
}

static std::vector<uint8_t> remove_savedata_entry_table(std::span<const uint8_t> database, const FileEntry &expected,
    const Hash &database_key, bool directory) {
    auto entries = parse_file_table(database, database_key);
    if (read32(database.data() + 8) != 3 || read16(database.data() + 12) != 2 || read32(database.data() + 0x1c) != 0
        || expected.index == 0 || expected.index == no_parent || expected.size > max_file_size
        || (directory ? expected.attributes != 0x8006 || expected.size != 0x8000 : !supported_savedata_file_attributes(expected.attributes)))
        throw std::runtime_error("PFS: unsupported savedata removal");
    std::set<uint32_t> indices;
    for (const auto &existing : entries) {
        if (existing.index == 0 || existing.index == no_parent || !simple_savedata_name(existing.name) || !indices.insert(existing.index).second)
            throw std::runtime_error("PFS: invalid savedata entry index or name");
    }
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const FileEntry &entry) {
        return entry.index == expected.index && entry.parent == expected.parent && entry.name == expected.name
            && entry.attributes == expected.attributes && entry.size == expected.size && entry.hash == expected.hash;
    });
    if (found == entries.end())
        throw std::runtime_error("PFS: missing or stale savedata entry");
    if (directory && std::any_of(entries.begin(), entries.end(), [&](const FileEntry &entry) { return entry.parent == expected.index; }))
        throw std::runtime_error("PFS: savedata directory is not empty");
    const uint64_t disk_size = directory ? 0x10000 : savedata_disk_size(expected.size);
    const uint64_t usage = read64(database.data() + 0x30);
    if (usage < disk_size)
        throw std::runtime_error("PFS: invalid savedata disk usage");
    entries.erase(found);
    return rebuild_savedata_file_table(database, std::move(entries), usage - disk_size, database_key);
}

std::vector<uint8_t> remove_savedata_file_table(std::span<const uint8_t> database, const FileEntry &expected, const Hash &database_key) {
    return remove_savedata_entry_table(database, expected, database_key, false);
}

static std::vector<uint8_t> crypt_cbc_file(const VerifiedFile &file, const AesKey &np_key, const KeyMaterial &material, bool encrypt, uint64_t base_offset = 0, const Hash *legacy_tweak = nullptr);

static VerifiedFile read_file_checked(const std::filesystem::path &unicv_path, const std::filesystem::path &file_path,
    const FileEntry &entry, const Hash &file_key, const AesKey *np_key, const KeyMaterial *material) {
    if (entry.attributes != 0x0001 && entry.attributes != 0x0006
        && entry.attributes != 0x4001 && entry.attributes != 0x4006)
        throw std::runtime_error("PFS: unsupported file attributes");
    const auto unicv = read_regular_file(unicv_path, max_database_size);
    if (unicv.size() < page_size * 2 || unicv.size() % page_size != 0
        || std::memcmp(unicv.data(), "SCEIRODB", 8) != 0 || read32(unicv.data() + 8) != 2
        || read32(unicv.data() + 12) != page_size || read64(unicv.data() + 16) != std::numeric_limits<uint64_t>::max()
        || read64(unicv.data() + 24) != unicv.size() - page_size)
        throw std::runtime_error("PFS: unsupported or invalid unicv.db header");
    if (entry.index == 0 || entry.index >= unicv.size() / page_size)
        throw std::runtime_error("PFS: invalid unicv record index");

    const auto record_offset = static_cast<size_t>(entry.index) * page_size;
    const auto *record = unicv.data() + record_offset;
    if (std::memcmp(record, "SCEIFTBL", 8) != 0 || read32(record + 8) != 2
        || read32(record + 12) != page_size || read32(record + 16) != (page_size - 16) / Hash{}.size())
        throw std::runtime_error("PFS: unsupported or invalid unicv record");
    if (!equal_hash(hmac_sha1(file_key, record, 72), entry.hash.data()))
        throw std::runtime_error("PFS: unicv record authentication failed");

    const auto hashes_per_page = read32(record + 16);
    const auto count = read32(record + 20);
    const auto block_size = read32(record + 24);
    if (block_size == 0 || block_size > max_file_size || entry.size > max_file_size
        || count != entry.size / block_size + (entry.size % block_size != 0))
        throw std::runtime_error("PFS: invalid file block count or size");

    VerifiedFile result{ read_regular_file(file_path, max_file_size), {}, block_size };
    if (result.ciphertext.size() != entry.size)
        throw std::runtime_error("PFS: file size does not match files.db");
    std::copy_n(record + 0x34, result.seed.size(), result.seed.begin());

    const auto hashes_match = [&](const std::vector<uint8_t> &data) {
        for (uint32_t i = 0; i < count; ++i) {
            const auto page_offset = record_offset + page_size * (1 + static_cast<size_t>(i / hashes_per_page));
            if (page_offset > unicv.size() || unicv.size() - page_offset < page_size)
                throw std::runtime_error("PFS: missing unicv hash page");
            const auto *hash_page = unicv.data() + page_offset;
            const auto page_count = read32(hash_page + 8);
            if (read32(hash_page) != 0x3f8 || read32(hash_page + 4) != Hash{}.size()
                || page_count > hashes_per_page || i % hashes_per_page >= page_count)
                throw std::runtime_error("PFS: invalid unicv hash page");
            const uint8_t block_index[] = { static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8),
                static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 24) };
            const auto block_key = hmac_sha1(file_key, block_index, sizeof(block_index));
            const auto offset = static_cast<size_t>(i) * block_size;
            const auto length = std::min(static_cast<size_t>(block_size), data.size() - offset);
            const auto digest = hmac_sha1(block_key, data.data() + offset, length);
            if (!equal_hash(digest, hash_page + 16 + (i % hashes_per_page) * digest.size()))
                return false;
        }
        return true;
    };
    if (!hashes_match(result.ciphertext)) {
        if (!np_key || !material || (entry.attributes & 0x4000))
            throw std::runtime_error("PFS: file block authentication failed");
        // Previously decrypted installs can retain the original ciphertext hashes.
        auto encrypted = crypt_cbc_file(result, *np_key, *material, true);
        if (!hashes_match(encrypted))
            throw std::runtime_error("PFS: file block authentication failed");
        result.ciphertext = std::move(encrypted);
    }
    return result;
}

VerifiedFile read_verified_file(const std::filesystem::path &unicv_path, const std::filesystem::path &file_path,
    const FileEntry &entry, const Hash &file_key) {
    return read_file_checked(unicv_path, file_path, entry, file_key, nullptr, nullptr);
}

static std::vector<uint8_t> crypt_cbc_file(const VerifiedFile &file, const AesKey &np_key, const KeyMaterial &material, bool encrypt, uint64_t base_offset, const Hash *legacy_tweak) {
    if (file.block_size == 0 || file.block_size > max_file_size || file.ciphertext.size() > max_file_size)
        throw std::runtime_error("PFS: invalid CBC file size");
    const auto tweak = legacy_tweak ? *legacy_tweak : hmac_sha1(material.file_hmac, file.seed.data(), file.seed.size());
    std::vector<uint8_t> plaintext(file.ciphertext.size());
    for (size_t offset = 0; offset < file.ciphertext.size(); offset += file.block_size) {
        const auto length = std::min(static_cast<size_t>(file.block_size), file.ciphertext.size() - offset);
        const auto full = length & ~size_t{ 15 };
        AesKey iv{};
        std::copy_n(tweak.begin(), iv.size(), iv.begin());
        for (size_t i = 0; i < 8; ++i)
            iv[i] ^= static_cast<uint8_t>((base_offset + offset) >> (8 * i));
        if (full != 0) {
            const auto part = aes_crypt(EVP_aes_128_cbc(), np_key, iv.data(), file.ciphertext.data() + offset, full, encrypt);
            std::copy(part.begin(), part.end(), plaintext.begin() + offset);
            const auto *last_ciphertext = encrypt ? part.data() + full - iv.size() : file.ciphertext.data() + offset + full - iv.size();
            std::copy_n(last_ciphertext, iv.size(), iv.begin());
        }
        if (full != length) {
            const auto stream = aes_crypt(EVP_aes_128_ecb(), np_key, nullptr, iv.data(), iv.size(), true);
            for (size_t i = 0; i < length - full; ++i)
                plaintext[offset + full + i] = file.ciphertext[offset + full + i] ^ stream[i];
        }
    }
    return plaintext;
}

std::vector<uint8_t> decrypt_cbc_file(const VerifiedFile &file, const AesKey &np_key, const KeyMaterial &material) {
    return crypt_cbc_file(file, np_key, material, false);
}

static std::vector<size_t> savedata_tree_indices(size_t count) {
    if (count == 0 || count > 23)
        return {};
    const size_t total = 2 * count - 1;
    std::vector<size_t> indices(total + 1);
    size_t next = 1;
    for (size_t i = 1; i <= total / 2; ++i) {
        indices[2 * i] = indices[i];
        indices[2 * i + 1] = next++;
    }
    return indices;
}

static std::vector<Hash> savedata_hash_tree(std::span<const Hash> leaves, const Hash &secret) {
    const size_t count = leaves.size();
    if (count == 0)
        return {};
    if (count > 23)
        throw std::runtime_error("PFS: unsupported savedata node size");
    const size_t total = 2 * count - 1;
    const auto indices = savedata_tree_indices(count);
    std::vector<Hash> tree(total + 1);
    for (size_t i = total - count + 1; i <= total; ++i)
        tree[i] = leaves[indices[i]];
    for (size_t i = total / 2; i > 0; --i) {
        std::array<uint8_t, 40> children{};
        std::copy(tree[2 * i].begin(), tree[2 * i].end(), children.begin());
        std::copy(tree[2 * i + 1].begin(), tree[2 * i + 1].end(), children.begin() + Hash{}.size());
        tree[i] = hmac_sha1(secret, children.data(), children.size());
    }
    return tree;
}

EncodedSavedataFile encode_savedata_file(std::span<const uint8_t> plaintext, uint32_t index, uint16_t attributes, const AesKey &key) {
    constexpr size_t block_size = 0x8000;
    if (plaintext.size() > max_file_size || !supported_savedata_file_attributes(attributes))
        throw std::runtime_error("PFS: unsupported savedata file attributes or size");
    const size_t count = (plaintext.size() + block_size - 1) / block_size;
    EncodedSavedataFile result{};
    result.data.resize(count * block_size);
    std::copy(plaintext.begin(), plaintext.end(), result.data.begin());
    if (!(attributes & 0x4000)) {
        const auto keys = derive_savedata_file_keys(key, index);
        std::array<uint8_t, 32> xts_key{};
        std::copy(keys.data.begin(), keys.data.end(), xts_key.begin());
        std::copy(keys.tweak.begin(), keys.tweak.end(), xts_key.begin() + keys.data.size());
        for (size_t i = 0; i < count; ++i) {
            std::array<uint8_t, 16> tweak{};
            const uint64_t position = i * block_size;
            for (size_t j = 0; j < 8; ++j)
                tweak[j] = static_cast<uint8_t>(position >> (8 * j));
            const std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
            int written = 0;
            int final = 0;
            if (!ctx || EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_xts(), nullptr, xts_key.data(), tweak.data()) != 1
                || EVP_EncryptUpdate(ctx.get(), result.data.data() + position, &written, result.data.data() + position, block_size) != 1
                || EVP_EncryptFinal_ex(ctx.get(), result.data.data() + position + written, &final) != 1
                || written + final != block_size)
                throw std::runtime_error("PFS: savedata XTS encryption failed");
        }
    }
    const auto secret = derive_savedata_secret(key, index);
    result.icv.resize(page_size);
    std::memcpy(result.icv.data(), "SCEICVDB", 8);
    const auto put32 = [&](size_t offset, uint32_t value) {
        for (size_t i = 0; i < 4; ++i)
            result.icv[offset + i] = static_cast<uint8_t>(value >> (8 * i));
    };
    put32(8, 2);
    put32(12, block_size);
    put32(16, page_size);
    std::fill_n(result.icv.begin() + 24, 8, 0xff);
    put32(40, static_cast<uint32_t>(count));
    struct IcvChild {
        uint32_t page;
        Hash hash;
    };
    const auto add_node = [&](uint32_t level, std::span<const IcvChild> children) {
        const auto page = static_cast<uint32_t>(result.icv.size() / page_size - 1);
        result.icv.resize(result.icv.size() + page_size);
        const size_t base = (static_cast<size_t>(page) + 1) * page_size;
        put32(base, level);
        put32(base + 0x10, 0x394);
        put32(base + 0x14, Hash{}.size());
        std::vector<Hash> hashes;
        for (const auto &child : children)
            hashes.push_back(child.hash);
        const auto tree = savedata_hash_tree(hashes, secret);
        put32(base + 0x18, tree.empty() ? 0 : static_cast<uint32_t>(tree.size() - 1));
        for (size_t i = 1; i < tree.size(); ++i)
            std::copy(tree[i].begin(), tree[i].end(), result.icv.begin() + base + 0x20 + (i - 1) * Hash{}.size());
        for (size_t i = 0; i < children.size(); ++i)
            put32(base + 0x3a4 + i * 4, children[i].page);
        return IcvChild{ page, tree.empty() ? Hash{} : tree[1] };
    };
    std::vector<IcvChild> level;
    for (size_t first = 0; first < count; first += 23) {
        std::vector<IcvChild> blocks;
        for (size_t i = first; i < std::min(count, first + 23); ++i)
            blocks.push_back({ no_parent, hmac_sha1(secret, result.data.data() + i * block_size, block_size) });
        level.push_back(add_node(0, blocks));
    }
    if (level.empty())
        level.push_back(add_node(0, {}));
    for (uint32_t height = 1; level.size() > 1; ++height) {
        std::vector<IcvChild> next;
        for (size_t first = 0; first < level.size(); first += 23)
            next.push_back(add_node(height, std::span(level).subspan(first, std::min<size_t>(23, level.size() - first))));
        level = std::move(next);
    }
    put32(20, level.front().page);
    put32(32, static_cast<uint32_t>(result.icv.size() - page_size));
    std::copy(level.front().hash.begin(), level.front().hash.end(), result.icv.begin() + 44);
    result.entry_hash = hmac_sha1(secret, result.icv.data(), 0x40);
    return result;
}

InitialSavedata prepare_initial_savedata() {
    const auto generated = generate_savedata_key();
    InitialSavedata result{};
    result.directory_icv = make_savedata_directory_icv();
    result.sealedkey = encode_savedata_file(generated.sealedkey, 2, 0x4007, generated.key);

    std::vector<uint8_t> empty_database(page_size * 2);
    std::memcpy(empty_database.data(), "SCENGPFS", 8);
    const auto put32 = [&](size_t offset, uint32_t value) {
        for (size_t i = 0; i < 4; ++i)
            empty_database[offset + i] = static_cast<uint8_t>(value >> (8 * i));
    };
    put32(8, 3);
    put32(12, 2);
    put32(16, page_size);
    put32(20, order);
    put32(page_size, no_parent);

    const auto directory_key = derive_savedata_secret(generated.key, 1);
    const auto directory_hash = hmac_sha1(directory_key, result.directory_icv.data(), result.directory_icv.size());
    std::vector<FileEntry> entries{
        { 0, "SCE_SYS", 1, 0x8006, 0x8000, directory_hash },
        { 1, "SEALEDKEY", 2, 0x4007, generated.sealedkey.size(), result.sealedkey.entry_hash }
    };
    const auto database_key = derive_savedata_secret(generated.key, 0);
    result.files_db = rebuild_savedata_file_table(empty_database, std::move(entries),
        0x10000 + savedata_disk_size(generated.sealedkey.size()), database_key);
    (void)parse_file_table(result.files_db, database_key);
    return result;
}

std::vector<uint8_t> read_savedata_file(const std::filesystem::path &file_path, const std::filesystem::path &icv_path,
    const FileEntry &entry, const AesKey &key) {
    constexpr uint32_t block_size = 0x8000;
    if ((entry.attributes & 0x8000) != 0 || entry.size > max_file_size)
        throw std::runtime_error("PFS: unsupported savedata file attributes or size");
    const auto count = static_cast<uint32_t>((entry.size + block_size - 1) / block_size);
    const auto icv = read_regular_file(icv_path, max_database_size);
    const auto secret = derive_savedata_secret(key, entry.index);
    if (icv.size() < 2 * page_size || icv.size() % page_size != 0 || std::memcmp(icv.data(), "SCEICVDB", 8) != 0
        || read32(icv.data() + 8) != 2 || read32(icv.data() + 12) != block_size
        || read32(icv.data() + 16) != page_size || read32(icv.data() + 40) != count
        || !equal_hash(hmac_sha1(secret, icv.data(), 0x40), entry.hash.data()))
        throw std::runtime_error("PFS: invalid or unauthenticated savedata ICV header");
    const auto ciphertext = read_regular_file(file_path, max_file_size);
    if (ciphertext.size() != static_cast<size_t>(count) * block_size)
        throw std::runtime_error("PFS: invalid savedata physical file size");

    std::set<uint32_t> visited;
    size_t block = 0;
    const auto root_page = read32(icv.data() + 20);
    if (root_page >= icv.size() / page_size - 1)
        throw std::runtime_error("PFS: invalid savedata ICV root");
    const auto root_level = read32(icv.data() + (static_cast<size_t>(root_page) + 1) * page_size);
    if (root_level > 4)
        throw std::runtime_error("PFS: unsupported savedata ICV height");
    const auto visit = [&](auto &&self, uint32_t page, uint32_t level) -> Hash {
        if (page >= icv.size() / page_size - 1 || !visited.insert(page).second)
            throw std::runtime_error("PFS: invalid or cyclic savedata ICV tree");
        const auto *node = icv.data() + (static_cast<size_t>(page) + 1) * page_size;
        const auto length = read32(node + 0x18);
        if (read32(node) != level || read32(node + 0x10) != 0x394 || read32(node + 0x14) != Hash{}.size()
            || length > 45 || (length != 0 && length % 2 == 0) || (level != 0 && length == 0))
            throw std::runtime_error("PFS: invalid savedata ICV node");
        const size_t children = (length + 1) / 2;
        std::vector<Hash> leaves(children);
        for (size_t i = 0; i < children; ++i) {
            const auto child = read32(node + 0x3a4 + i * 4);
            if (level != 0) {
                leaves[i] = self(self, child, level - 1);
            } else {
                if (child != no_parent || block >= count)
                    throw std::runtime_error("PFS: invalid savedata ICV leaf");
                leaves[i] = hmac_sha1(secret, ciphertext.data() + block * block_size, block_size);
                ++block;
            }
        }
        const auto tree = savedata_hash_tree(leaves, secret);
        for (size_t i = 1; i < tree.size(); ++i) {
            if (!equal_hash(tree[i], node + 0x20 + (i - 1) * Hash{}.size()))
                throw std::runtime_error("PFS: savedata ICV node authentication failed");
        }
        return tree.empty() ? Hash{} : tree[1];
    };
    const auto root_hash = visit(visit, root_page, root_level);
    if (block != count)
        throw std::runtime_error("PFS: savedata ICV block count mismatch");
    if (!equal_hash(root_hash, icv.data() + 44))
        throw std::runtime_error("PFS: savedata block authentication failed");

    if (entry.attributes & 0x4000) {
        auto plaintext = ciphertext;
        plaintext.resize(static_cast<size_t>(entry.size));
        return plaintext;
    }

    const auto keys = derive_savedata_file_keys(key, entry.index);
    std::array<uint8_t, 32> xts_key{};
    std::copy(keys.data.begin(), keys.data.end(), xts_key.begin());
    std::copy(keys.tweak.begin(), keys.tweak.end(), xts_key.begin() + keys.data.size());
    std::vector<uint8_t> plaintext(ciphertext.size());
    for (uint32_t i = 0; i < count; ++i) {
        std::array<uint8_t, 16> tweak{};
        const uint64_t position = static_cast<uint64_t>(i) * block_size;
        for (size_t j = 0; j < 8; ++j)
            tweak[j] = static_cast<uint8_t>(position >> (8 * j));
        const std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
        int written = 0;
        int final = 0;
        if (!ctx || EVP_DecryptInit_ex(ctx.get(), EVP_aes_128_xts(), nullptr, xts_key.data(), tweak.data()) != 1
            || EVP_DecryptUpdate(ctx.get(), plaintext.data() + position, &written, ciphertext.data() + position, block_size) != 1
            || EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + position + written, &final) != 1
            || written + final != block_size)
            throw std::runtime_error("PFS: savedata XTS decryption failed");
    }
    plaintext.resize(static_cast<size_t>(entry.size));
    return plaintext;
}

struct File::Impl {
    std::ifstream input;
    std::optional<std::vector<uint8_t>> savedata;
    std::vector<uint8_t> unicv;
    FileEntry entry;
    Hash file_key;
    Hash seed;
    Hash legacy_tweak;
    bool legacy = false;
    AesKey np_key;
    KeyMaterial material;
    size_t record_offset;
    uint32_t hashes_per_page;
    uint32_t block_size;
};

File::File(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
File::~File() = default;

uint64_t File::size() const {
    return impl_->entry.size;
}

size_t File::read(uint64_t offset, void *data, size_t length) {
    if (offset >= size() || length == 0)
        return 0;
    auto &file = *impl_;
    const auto wanted = static_cast<size_t>(std::min<uint64_t>(length, size() - offset));
    if (file.savedata) {
        std::memcpy(data, file.savedata->data() + static_cast<size_t>(offset), wanted);
        return wanted;
    }
    auto *output = static_cast<uint8_t *>(data);
    size_t total = 0;
    while (total < wanted) {
        const uint64_t position = offset + total;
        const uint32_t index = static_cast<uint32_t>(position / file.block_size);
        const uint64_t block_offset = static_cast<uint64_t>(index) * file.block_size;
        const auto block_length = static_cast<size_t>(std::min<uint64_t>(file.block_size, size() - block_offset));
        const auto hash_offset = file.record_offset + page_size * (1 + static_cast<size_t>(index / file.hashes_per_page));
        if (hash_offset > file.unicv.size() || file.unicv.size() - hash_offset < page_size)
            throw std::runtime_error("PFS: missing unicv hash page");
        const auto *page = file.unicv.data() + hash_offset;
        if (read32(page) != 0x3f8 || read32(page + 4) != Hash{}.size()
            || read32(page + 8) > file.hashes_per_page || index % file.hashes_per_page >= read32(page + 8))
            throw std::runtime_error("PFS: invalid unicv hash page");
        VerifiedFile block{ std::vector<uint8_t>(block_length), file.seed, file.block_size };
        file.input.clear();
        file.input.seekg(static_cast<std::streamoff>(block_offset));
        if (!file.input.read(reinterpret_cast<char *>(block.ciphertext.data()), static_cast<std::streamsize>(block_length)))
            throw std::runtime_error("PFS: cannot read file block");
        const uint8_t block_index[] = { static_cast<uint8_t>(index), static_cast<uint8_t>(index >> 8),
            static_cast<uint8_t>(index >> 16), static_cast<uint8_t>(index >> 24) };
        const auto key = hmac_sha1(file.file_key, block_index, sizeof(block_index));
        const auto *expected = page + 16 + (index % file.hashes_per_page) * Hash{}.size();
        const auto digest = hmac_sha1(key, block.ciphertext.data(), block.ciphertext.size());
        std::vector<uint8_t> plaintext;
        if (equal_hash(digest, expected)) {
            plaintext = (file.entry.attributes & 0x4000) ? std::move(block.ciphertext)
                                                         : crypt_cbc_file(block, file.np_key, file.material, false, block_offset, file.legacy ? &file.legacy_tweak : nullptr);
        } else {
            if (file.entry.attributes & 0x4000)
                throw std::runtime_error("PFS: file block authentication failed");
            const auto encrypted = crypt_cbc_file(block, file.np_key, file.material, true, block_offset, file.legacy ? &file.legacy_tweak : nullptr);
            if (!equal_hash(hmac_sha1(key, encrypted.data(), encrypted.size()), expected))
                throw std::runtime_error("PFS: file block authentication failed");
            plaintext = std::move(block.ciphertext);
        }
        const size_t within = static_cast<size_t>(position - block_offset);
        const size_t count = std::min(wanted - total, block_length - within);
        std::memcpy(output + total, plaintext.data() + within, count);
        total += count;
    }
    return total;
}

Layer::Layer(const std::filesystem::path &root, const AesKey &license_key, const KeyMaterial &material, Mode mode)
    : root_(root)
    , mode_(mode)
    , material_(material)
    , np_key_(prepare_np_key(license_key, material))
    , key_param_(0) {
    if (!validate_read_mode(mode_))
        throw std::runtime_error("PFS: invalid layer mode");
    if (!std::filesystem::is_directory(std::filesystem::symlink_status(root_))
        || !std::filesystem::is_directory(std::filesystem::symlink_status(root_ / "sce_pfs")))
        throw std::runtime_error("PFS: invalid layer directory");
    const auto database = root_ / "sce_pfs" / "files.db";
    const auto header = read_regular_file(database, max_database_size);
    if (header.size() < page_size)
        throw std::runtime_error("PFS: missing files.db header");
    if (!validate_read_mode(mode_, read16(header.data() + 12)))
        throw std::runtime_error("PFS: invalid layer database mode");
    key_param_ = read32(header.data() + 0x1c);
    entries_ = read_file_table(database, derive_np_secret(np_key_, material_, key_param_, 0));
}

Layer::Layer(const std::filesystem::path &root, const AesKey &savedata_key)
    : root_(root)
    , mode_(Mode::Mode5)
    , material_{}
    , np_key_(savedata_key)
    , key_param_(0) {
    if (!std::filesystem::is_directory(std::filesystem::symlink_status(root_))
        || !std::filesystem::is_directory(std::filesystem::symlink_status(root_ / "sce_pfs"))
        || !std::filesystem::is_directory(std::filesystem::symlink_status(root_ / "sce_pfs" / "icv.db")))
        throw std::runtime_error("PFS: invalid savedata layer directory");
    if (std::filesystem::exists(std::filesystem::symlink_status(root_ / "sce_pfs" / ".vita3k-transaction")))
        throw std::runtime_error("PFS: savedata transaction requires recovery");
    const auto database = root_ / "sce_pfs" / "files.db";
    const auto header = read_regular_file(database, max_database_size);
    if (header.size() < page_size || read32(header.data() + 8) != 3 || read16(header.data() + 12) != 2
        || read32(header.data() + 0x1c) != 0)
        throw std::runtime_error("PFS: unsupported savedata database header");
    entries_ = read_file_table(database, derive_savedata_secret(savedata_key, 0));
}

Layer::Layer(const std::filesystem::path &root)
    : Layer(root, read_savedata_bootstrap_key(root)) {
    const auto sealedkey = read(std::filesystem::path("sce_sys") / "sealedkey");
    if (!sealedkey || CRYPTO_memcmp(unseal_savedata_key(*sealedkey).data(), np_key_.data(), np_key_.size()) != 0)
        throw std::runtime_error("PFS: sealedkey does not match the authenticated savedata");
}

uint32_t Layer::find_available_savedata_file_index() const {
    if (mode_ != Mode::Mode5
        || !std::filesystem::is_directory(std::filesystem::symlink_status(root_))
        || !std::filesystem::is_directory(std::filesystem::symlink_status(root_ / "sce_pfs"))
        || !std::filesystem::is_directory(std::filesystem::symlink_status(root_ / "sce_pfs" / "icv.db")))
        throw std::runtime_error("PFS: invalid savedata ICV directory");

    std::set<uint32_t> used;
    for (const auto &entry : entries_) {
        if (entry.index == 0 || entry.index == no_parent || !used.insert(entry.index).second)
            throw std::runtime_error("PFS: invalid savedata entry index");
    }
    for (const auto &item : std::filesystem::directory_iterator(root_ / "sce_pfs" / "icv.db")) {
        const auto name_utf8 = item.path().filename().u8string();
        const std::string name(name_utf8.begin(), name_utf8.end());
        if (name.size() != 12 || !savedata_name_equal(name.substr(8), ".icv"))
            continue;
        uint32_t index = 0;
        bool valid = true;
        for (size_t i = 0; i < 8; ++i) {
            const char ch = name[i];
            unsigned digit;
            if (ch >= '0' && ch <= '9')
                digit = ch - '0';
            else if (ch >= 'a' && ch <= 'f')
                digit = ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F')
                digit = ch - 'A' + 10;
            else {
                valid = false;
                break;
            }
            index = (index << 4) | digit;
        }
        if (valid)
            used.insert(index);
    }
    uint32_t available = 1;
    for (const auto index : used) {
        if (index < available)
            continue;
        if (index > available)
            break;
        if (++available == no_parent)
            throw std::runtime_error("PFS: no available savedata file index");
    }
    return available;
}

void Layer::create_savedata_directory(const std::filesystem::path &relative_path) const {
    if (mode_ != Mode::Mode5 || relative_path.empty() || relative_path.has_root_path())
        throw std::runtime_error("PFS: invalid savedata directory path");
    if (std::filesystem::exists(std::filesystem::symlink_status(root_ / "sce_pfs" / ".vita3k-transaction")))
        throw std::runtime_error("PFS: existing transaction requires recovery");
    if (find(relative_path))
        throw std::runtime_error("PFS: savedata entry already exists");
    const auto parent_path = relative_path.parent_path();
    uint32_t parent = 0;
    if (!parent_path.empty()) {
        const auto *entry = find(parent_path);
        if (!entry || !(entry->attributes & 0x8000))
            throw std::runtime_error("PFS: missing savedata parent directory");
        parent = entry->index;
    }
    const auto host_parent = savedata_host_path(parent_path);
    const auto filename_utf8 = relative_path.filename().u8string();
    const std::string name(filename_utf8.begin(), filename_utf8.end());
    for (const auto &candidate : std::filesystem::directory_iterator(host_parent)) {
        const auto candidate_utf8 = candidate.path().filename().u8string();
        const std::string candidate_name(candidate_utf8.begin(), candidate_utf8.end());
        if (savedata_name_equal(candidate_name, name))
            throw std::runtime_error("PFS: untracked savedata entry conflicts with directory creation");
    }
    const auto index = find_available_savedata_file_index();
    const auto database = read_regular_file(root_ / "sce_pfs" / "files.db", max_database_size);
    auto prepared = prepare_savedata_directory(database, parent, index, name, np_key_);
    std::ostringstream icv_name;
    icv_name << std::hex << std::setw(8) << std::setfill('0') << index << ".icv";
    std::array changes{
        detail::SavedataChange{ std::filesystem::path("sce_pfs") / "icv.db" / icv_name.str(),
            std::vector<uint8_t>(prepared.icv.begin(), prepared.icv.end()) },
        detail::SavedataChange{ std::filesystem::path("sce_pfs") / "files.db", std::move(prepared.files_db) }
    };
    const auto host_directory = host_parent / relative_path.filename();
    if (!detail::publish_savedata_directory_transaction(root_, derive_savedata_secret(np_key_, 0),
            host_directory.lexically_relative(root_), changes))
        throw std::runtime_error("PFS: savedata directory transaction failed");
}

void Layer::replace_savedata_file(const std::filesystem::path &relative_path, std::span<const uint8_t> plaintext) const {
    if (mode_ != Mode::Mode5 || relative_path.empty() || relative_path.has_root_path())
        throw std::runtime_error("PFS: invalid savedata replacement path");
    if (std::filesystem::exists(std::filesystem::symlink_status(root_ / "sce_pfs" / ".vita3k-transaction")))
        throw std::runtime_error("PFS: existing transaction requires recovery");
    const auto filename_utf8 = relative_path.filename().u8string();
    std::string name(filename_utf8.begin(), filename_utf8.end());
    if (!valid_component(name) || name.size() >= 64)
        throw std::runtime_error("PFS: invalid savedata file name");
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
        return static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch);
    });
    if (!simple_savedata_name(name))
        throw std::runtime_error("PFS: unsupported savedata file name");

    const auto *existing = find(relative_path);
    uint32_t parent = 0;
    const auto parent_path = relative_path.parent_path();
    if (!parent_path.empty()) {
        const auto *directory = find(parent_path);
        if (!directory || !(directory->attributes & 0x8000))
            throw std::runtime_error("PFS: missing savedata parent directory");
        parent = directory->index;
    }
    const auto host_parent = savedata_host_path(parent_path);
    std::filesystem::path host_file;
    if (existing) {
        if (existing->attributes & 0x8000)
            throw std::runtime_error("PFS: cannot replace a directory");
        host_file = savedata_host_path(relative_path);
        if (!read(relative_path))
            throw std::runtime_error("PFS: cannot authenticate existing savedata file");
    } else {
        for (const auto &candidate : std::filesystem::directory_iterator(host_parent)) {
            const auto candidate_utf8 = candidate.path().filename().u8string();
            const std::string candidate_name(candidate_utf8.begin(), candidate_utf8.end());
            if (savedata_name_equal(candidate_name, name))
                throw std::runtime_error("PFS: untracked savedata file conflicts with creation");
        }
        host_file = host_parent / relative_path.filename();
    }
    const uint32_t index = existing ? existing->index : find_available_savedata_file_index();
    const uint16_t attributes = existing ? existing->attributes : 4;
    auto encoded = encode_savedata_file(plaintext, index, attributes, np_key_);
    const auto database_path = root_ / "sce_pfs" / "files.db";
    const auto database = read_regular_file(database_path, max_database_size);
    const auto database_key = derive_savedata_secret(np_key_, 0);
    auto updated = existing
        ? update_savedata_file_table(database, *existing, plaintext.size(), encoded.entry_hash, database_key)
        : insert_savedata_file_table(database, FileEntry{ parent, name, index, attributes, plaintext.size(), encoded.entry_hash }, database_key);
    std::ostringstream icv_name;
    icv_name << std::hex << std::setw(8) << std::setfill('0') << index << ".icv";
    std::array changes{
        detail::SavedataChange{ host_file.lexically_relative(root_), std::move(encoded.data) },
        detail::SavedataChange{ std::filesystem::path("sce_pfs") / "icv.db" / icv_name.str(), std::move(encoded.icv) },
        detail::SavedataChange{ std::filesystem::path("sce_pfs") / "files.db", std::move(updated) }
    };
    if (!detail::publish_savedata_transaction(root_, database_key, changes))
        throw std::runtime_error("PFS: savedata file transaction failed");
}

void Layer::remove_savedata_file(const std::filesystem::path &relative_path) const {
    if (mode_ != Mode::Mode5 || relative_path.empty() || relative_path.has_root_path())
        throw std::runtime_error("PFS: invalid savedata removal path");
    if (std::filesystem::exists(std::filesystem::symlink_status(root_ / "sce_pfs" / ".vita3k-transaction")))
        throw std::runtime_error("PFS: existing transaction requires recovery");
    const auto *existing = find(relative_path);
    if (!existing)
        throw std::runtime_error("PFS: missing savedata entry");
    const auto host_file = savedata_host_path(relative_path);
    const bool directory = (existing->attributes & 0x8000) != 0;
    if (directory) {
        if (existing->parent == 0 && savedata_name_equal(existing->name, "sce_sys"))
            throw std::runtime_error("PFS: cannot remove savedata system directory");
    } else if (!read(relative_path)) {
        throw std::runtime_error("PFS: cannot authenticate existing savedata file");
    }
    const auto database_path = root_ / "sce_pfs" / "files.db";
    const auto database = read_regular_file(database_path, max_database_size);
    const auto database_key = derive_savedata_secret(np_key_, 0);
    auto updated = remove_savedata_entry_table(database, *existing, database_key, directory);
    std::ostringstream icv_name;
    icv_name << std::hex << std::setw(8) << std::setfill('0') << existing->index << ".icv";
    if (directory) {
        const auto icv = read_regular_file(root_ / "sce_pfs" / "icv.db" / icv_name.str(), 24);
        const auto key = derive_savedata_secret(np_key_, existing->index);
        const auto expected_icv = make_savedata_directory_icv();
        if (icv.size() != expected_icv.size() || !std::equal(icv.begin(), icv.end(), expected_icv.begin())
            || !equal_hash(hmac_sha1(key, icv.data(), icv.size()), existing->hash.data()))
            throw std::runtime_error("PFS: cannot authenticate savedata directory");
        std::array changes{
            detail::SavedataChange{ std::filesystem::path("sce_pfs") / "icv.db" / icv_name.str(), std::nullopt },
            detail::SavedataChange{ std::filesystem::path("sce_pfs") / "files.db", std::move(updated) }
        };
        if (!detail::publish_savedata_directory_removal(root_, database_key, host_file.lexically_relative(root_), changes))
            throw std::runtime_error("PFS: savedata directory removal transaction failed");
        return;
    }
    std::array changes{
        detail::SavedataChange{ host_file.lexically_relative(root_), std::nullopt },
        detail::SavedataChange{ std::filesystem::path("sce_pfs") / "icv.db" / icv_name.str(), std::nullopt },
        detail::SavedataChange{ std::filesystem::path("sce_pfs") / "files.db", std::move(updated) }
    };
    if (!detail::publish_savedata_transaction(root_, database_key, changes))
        throw std::runtime_error("PFS: savedata file removal transaction failed");
}

static bool savedata_name_equal(const std::string &left, const std::string &right) {
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](unsigned char a, unsigned char b) {
        const auto upper = [](unsigned char c) { return c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c; };
        return upper(a) == upper(b);
    });
}

const FileEntry *Layer::find(const std::filesystem::path &relative_path) const {
    if (relative_path.empty() || relative_path.has_root_path())
        throw std::runtime_error("PFS: invalid relative path");
    uint32_t parent = 0;
    const FileEntry *found = nullptr;
    for (const auto &part : relative_path) {
        const auto utf8 = part.generic_u8string();
        const std::string name(utf8.begin(), utf8.end());
        if (!valid_component(name))
            throw std::runtime_error("PFS: invalid path component");
        if (found && !(found->attributes & 0x8000))
            throw std::runtime_error("PFS: path component is not a directory");
        const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const FileEntry &entry) {
            return entry.parent == parent && (mode_ == Mode::Mode5 ? savedata_name_equal(entry.name, name) : entry.name == name);
        });
        if (it == entries_.end())
            return nullptr;
        found = &*it;
        parent = found->index;
    }
    return found;
}

std::vector<FileEntry> Layer::children(const std::filesystem::path &relative_path) const {
    uint32_t parent = 0;
    if (!relative_path.empty()) {
        const auto *directory = find(relative_path);
        if (!directory)
            return {};
        if (!(directory->attributes & 0x8000))
            throw std::runtime_error("PFS: entry is not a directory");
        parent = directory->index;
    }
    std::vector<FileEntry> result;
    for (const auto &entry : entries_) {
        if (entry.parent == parent)
            result.push_back(entry);
    }
    return result;
}

std::unique_ptr<File> Layer::open_file(const std::filesystem::path &relative_path) const {
    const auto *entry = find(relative_path);
    if (!entry)
        return nullptr;
    if (mode_ == Mode::Mode5) {
        if (entry->attributes & 0x8000)
            throw std::runtime_error("PFS: unsupported savedata entry attributes");
        const auto source = savedata_host_path(relative_path);
        std::ostringstream icv_name;
        icv_name << std::hex << std::setw(8) << std::setfill('0') << entry->index << ".icv";
        auto file = std::make_unique<File::Impl>();
        file->entry = *entry;
        file->savedata = read_savedata_file(source, root_ / "sce_pfs" / "icv.db" / icv_name.str(), *entry, np_key_);
        return std::unique_ptr<File>(new File(std::move(file)));
    }
    if (entry->attributes != 0x0001 && entry->attributes != 0x0006
        && entry->attributes != 0x4001 && entry->attributes != 0x4006)
        throw std::runtime_error("PFS: unsupported file attributes");
    auto file = std::make_unique<File::Impl>();
    file->entry = *entry;
    file->np_key = np_key_;
    file->material = material_;
    file->file_key = derive_np_secret(np_key_, material_, key_param_, entry->index);
    file->unicv = read_regular_file(root_ / "sce_pfs" / "unicv.db", max_database_size);
    const auto &unicv = file->unicv;
    const auto unicv_version = unicv.size() >= 12 ? read32(unicv.data() + 8) : 0;
    if (unicv.size() < page_size * 2 || unicv.size() % page_size != 0
        || std::memcmp(unicv.data(), "SCEIRODB", 8) != 0 || (unicv_version != 1 && unicv_version != 2)
        || read32(unicv.data() + 12) != page_size || read64(unicv.data() + 16) != std::numeric_limits<uint64_t>::max()
        || read64(unicv.data() + 24) != unicv.size() - page_size
        || entry->index == 0 || entry->index >= unicv.size() / page_size)
        throw std::runtime_error("PFS: unsupported or invalid unicv database");
    file->record_offset = static_cast<size_t>(entry->index) * page_size;
    const auto *record = unicv.data() + file->record_offset;
    if (std::memcmp(record, "SCEIFTBL", 8) != 0 || read32(record + 8) != unicv_version
        || read32(record + 12) != page_size || read32(record + 16) != (page_size - 16) / Hash{}.size()
        || !equal_hash(hmac_sha1(file->file_key, record, unicv_version == 1 ? 32 : 72), entry->hash.data()))
        throw std::runtime_error("PFS: invalid or unauthenticated unicv record");
    file->hashes_per_page = read32(record + 16);
    file->block_size = read32(record + 24);
    if (file->block_size == 0 || file->block_size > max_file_size
        || entry->size > static_cast<uint64_t>(std::numeric_limits<std::streamoff>::max())
        || read32(record + 20) != entry->size / file->block_size + (entry->size % file->block_size != 0))
        throw std::runtime_error("PFS: invalid file block count or size");
    std::copy_n(record + 0x34, file->seed.size(), file->seed.begin());
    if (unicv_version == 1) {
        std::array<uint8_t, 8> salts{};
        const auto write32 = [](uint8_t *out, uint32_t value) {
            for (size_t i = 0; i < 4; ++i)
                out[i] = static_cast<uint8_t>(value >> (8 * i));
        };
        size_t salt_length = 4;
        if (key_param_ != 0) {
            write32(salts.data(), key_param_);
            salt_length = 8;
        }
        write32(salts.data() + salt_length - 4, entry->index);
        file->legacy_tweak = hmac_sha1(material_.file_hmac, salts.data(), salt_length);
        file->legacy = true;
    }
    std::filesystem::path source = root_;
    for (const auto &part : relative_path) {
        source /= part;
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(source)))
            throw std::runtime_error("PFS: symlinks are not supported");
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(source, error)) || error
        || std::filesystem::file_size(source, error) != entry->size || error)
        throw std::runtime_error("PFS: invalid file or file size");
    file->input.open(source, std::ios::binary);
    if (!file->input)
        throw std::runtime_error("PFS: cannot open file");
    return std::unique_ptr<File>(new File(std::move(file)));
}

std::optional<std::vector<uint8_t>> Layer::read(const std::filesystem::path &relative_path) const {
    auto file = open_file(relative_path);
    if (!file)
        return std::nullopt;
    if (file->size() > max_file_size)
        throw std::runtime_error("PFS: file exceeds the in-memory read limit");
    std::vector<uint8_t> result(static_cast<size_t>(file->size()));
    if (file->read(0, result.data(), result.size()) != result.size())
        throw std::runtime_error("PFS: short file read");
    return result;
}

std::unique_ptr<Layer> open_layer(const std::filesystem::path &root,
    const std::filesystem::path &license_directory, const std::string &title_id,
    Mode mode, const std::string &required_content_id) {
    if (!validate_read_mode(mode))
        throw std::runtime_error("PFS: invalid layer mode");
    if (mode == Mode::AcidDirectory) {
        const auto directory = root.filename().u8string();
        const std::string entitlement(directory.begin(), directory.end());
        if (title_id.size() != 9 || entitlement.size() != 16
            || (!required_content_id.empty() && (required_content_id.size() != 36 || required_content_id[6] != '-' || required_content_id[16] != '_' || required_content_id[19] != '-' || required_content_id.compare(7, title_id.size(), title_id) != 0 || required_content_id.compare(20, std::string::npos, entitlement) != 0)))
            throw std::runtime_error("PFS: ACID content ID does not match the title and directory");
    }
    const auto database = root / "sce_pfs" / "files.db";
    const auto unicv = root / "sce_pfs" / "unicv.db";
    const bool has_database = std::filesystem::exists(std::filesystem::symlink_status(database));
    const bool has_unicv = std::filesystem::exists(std::filesystem::symlink_status(unicv));
    if (!has_database && !has_unicv)
        return nullptr;
    if (!has_database || !has_unicv)
        throw std::runtime_error("PFS: incomplete layer databases");
    if (!std::filesystem::is_directory(std::filesystem::symlink_status(license_directory)))
        throw std::runtime_error("PFS: license directory is missing or invalid");

    const auto material = make_key_material();
    std::string failure = "no matching license";
    for (const auto &candidate : std::filesystem::directory_iterator(license_directory)) {
        if (candidate.path().extension() != ".rif")
            continue;
        try {
            const auto license = read_regular_file(candidate.path(), 512);
            if (license.size() != 512) {
                failure = "invalid license size";
                LOG_ERROR("PFS: invalid license size in '{}'", candidate.path());
                continue;
            }
            const auto *content = reinterpret_cast<const char *>(license.data() + 0x10);
            const auto *end = std::find(content, content + 0x30, '\0');
            const std::string content_id(content, end);
            if (content_id.size() <= 7 + title_id.size() || content_id[6] != '-'
                || content_id.compare(7, title_id.size(), title_id) != 0 || content_id[7 + title_id.size()] != '_'
                || (mode == Mode::AcidDirectory && (content_id.size() != 36 || content_id[19] != '-' || content_id.compare(20, std::string::npos, root.filename().string()) != 0))
                || (!required_content_id.empty() && content_id != required_content_id))
                continue;
            AesKey key{};
            std::copy_n(license.data() + 0x50, key.size(), key.begin());
            return std::make_unique<Layer>(root, key, material, mode);
        } catch (const std::runtime_error &error) {
            failure = error.what();
        }
    }
    throw std::runtime_error("PFS: cannot authenticate layer with an installed license: " + failure);
}

} // namespace pfsmgr
