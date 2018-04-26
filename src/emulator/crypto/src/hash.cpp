// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <crypto/hash.h>

extern "C" {
#include <sha256.h>
}

Sha256Hash sha256(const void *data, size_t size) {
    Sha256Hash hash;
    SHA256_CTX sha_ctx = {};
    sha256_init(&sha_ctx);
    sha256_update(&sha_ctx, static_cast<const uint8_t *>(data), size);
    sha256_final(&sha_ctx, hash.data());

    return hash;
}
