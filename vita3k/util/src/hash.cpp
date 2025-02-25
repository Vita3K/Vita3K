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

#include <util/hash.h>

#include <openssl/evp.h>

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
