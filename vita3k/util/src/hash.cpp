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

#include <util/hash.h>

#include <fmt/format.h>
#include <openssl/evp.h>

#include <stdexcept>

static constexpr char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const std::string &input) {
    const auto *data = reinterpret_cast<const unsigned char *>(input.data());
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);

    for (size_t i = 0; i < input.size(); i += 3) {
        unsigned int value = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < input.size())
            value |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < input.size())
            value |= data[i + 2];

        out.push_back(b64_table[(value >> 18) & 0x3F]);
        out.push_back(b64_table[(value >> 12) & 0x3F]);
        out.push_back(i + 1 < input.size() ? b64_table[(value >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < input.size() ? b64_table[value & 0x3F] : '=');
    }

    return out;
}

std::string derive_password(const std::string_view &password) {
    constexpr std::string_view salt = "No matter where you store, everyone has access somewhere.";
    constexpr int digest_len = 32;
    unsigned char digest[digest_len];

    if (!PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
            reinterpret_cast<const unsigned char *>(salt.data()), static_cast<int>(salt.size()),
            200000, EVP_sha3_256(), digest_len, digest))
        throw std::runtime_error("PBKDF2 failed");

    std::string result;
    result.reserve(digest_len * 2);
    for (const auto byte : digest)
        result += fmt::format("{:02X}", byte);
    return result;
}

Sha256Hash sha256(const void *data, size_t size) {
    Sha256Hash hash;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data, size);
    unsigned int len;
    EVP_DigestFinal(ctx, hash.data(), &len);
    EVP_MD_CTX_free(ctx);

    return hash;
}

void hex_buf(const std::uint8_t *hash, char *dst, const std::size_t source_size) {
    const char hex[17] = "0123456789abcdef";
    size_t j = 0;
    for (size_t i = 0; i < source_size; ++i) {
        const uint8_t byte = hash[i];
        const char hi = hex[byte >> 4];
        const char lo = hex[byte & 0xf];
        dst[j++] = hi;
        dst[j++] = lo;
    }

    dst[j] = '\0';
}
