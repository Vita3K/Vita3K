// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <packages/ebootparam.h>
#include <pfsmgr/functions.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <algorithm>
#include <cstddef>

namespace packages {

// Offsets correspond to SceHeader followed by SelfHeader in sce_types.h.
constexpr size_t self_header_size = 120;
constexpr size_t control_header_size = 16;
constexpr size_t payload_size = 256;

static uint64_t read_le(std::span<const uint8_t> data, size_t offset, size_t size) {
    uint64_t value = 0;
    for (size_t i = 0; i < size; ++i)
        value |= uint64_t(data[offset + i]) << (8 * i);
    return value;
}

static bool fits(uint64_t total, uint64_t offset, uint64_t size) {
    return offset <= total && size <= total - offset;
}

std::optional<Ebootparam> generate_ebootparam(std::span<const uint8_t> self, const EbootparamKey &key) {
    if (self.size() < self_header_size || read_le(self, 0, 4) != 0x00454353
        || read_le(self, 4, 4) != 3 || read_le(self, 10, 2) != 1)
        return std::nullopt;

    const uint64_t header_size = read_le(self, 16, 8);
    const uint64_t control_offset = read_le(self, 104, 8);
    const uint64_t control_size = read_le(self, 112, 8);
    if (header_size < self_header_size || header_size > self.size()
        || control_offset < self_header_size || !fits(header_size, control_offset, control_size))
        return std::nullopt;

    Ebootparam result{};
    bool found = false;
    const size_t end = static_cast<size_t>(control_offset + control_size);
    for (size_t pos = static_cast<size_t>(control_offset); pos < end;) {
        if (end - pos < control_header_size)
            return std::nullopt;
        const uint64_t type = read_le(self, pos, 4);
        const uint64_t size = read_le(self, pos + 4, 4);
        if (size < control_header_size || size > end - pos)
            return std::nullopt;
        if (type == 6) {
            if (found || size < control_header_size + payload_size)
                return std::nullopt;
            std::copy_n(self.begin() + pos + control_header_size, payload_size, result.begin() + 32);
            found = true;
        }
        pos += static_cast<size_t>(size);
    }
    if (!found)
        return std::nullopt;

    std::array<uint8_t, 32> digest{};
    unsigned int digest_size = 0;
    if (!HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), result.data(), result.size(), digest.data(), &digest_size)
        || digest_size != digest.size())
        return std::nullopt;
    constexpr char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < 16; ++i) {
        result[2 * i] = hex[digest[i] >> 4];
        result[2 * i + 1] = hex[digest[i] & 15];
    }
    return result;
}

std::optional<Ebootparam> generate_ebootparam(std::span<const uint8_t> self) {
    struct KeyGuard {
        EbootparamKey key = pfsmgr::getEbootparamKey();
        ~KeyGuard() { OPENSSL_cleanse(key.data(), key.size()); }
    } guard;
    return generate_ebootparam(self, guard.key);
}

} // namespace packages
