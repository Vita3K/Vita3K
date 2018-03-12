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

#include "pkg.h"

#include <miniz.h>
#include <base64.h>
#include <aes.h>

#include <util/string_convert.h>

#include <vector>
#include <string>

// https://wiki.henkaku.xyz/vita/Packages#AES_Keys
static const uint8_t pkg_ps3_key[] = { 0x2e, 0x7b, 0x71, 0xd7, 0xc9, 0xc9, 0xa1, 0x4e, 0xa3, 0x22, 0x1f, 0x18, 0x88, 0x28, 0xb8, 0xf8 };
static const uint8_t pkg_psp_key[] = { 0x07, 0xf2, 0xc6, 0x82, 0x90, 0xb5, 0x0d, 0x2c, 0x33, 0x81, 0x8d, 0x70, 0x9b, 0x60, 0xe6, 0x2b };
static const uint8_t pkg_vita_2[] = { 0xe3, 0x1a, 0x70, 0xc9, 0xce, 0x1d, 0xd7, 0x2b, 0xf3, 0xc0, 0x62, 0x29, 0x63, 0xf2, 0xec, 0xcb };
static const uint8_t pkg_vita_3[] = { 0x42, 0x3a, 0xca, 0x3a, 0x2b, 0xd5, 0x64, 0x9f, 0x96, 0x86, 0xab, 0xad, 0x6f, 0xd8, 0x80, 0x1f };
static const uint8_t pkg_vita_4[] = { 0xaf, 0x07, 0xfd, 0x59, 0x65, 0x25, 0x27, 0xba, 0xf1, 0x33, 0x89, 0x66, 0x8b, 0x17, 0xd9, 0xea };

int inflate_from_str(uint8_t* inp, uint8_t* out, uint32_t len) {
    mz_stream stream;

    memset(&stream, 0, sizeof(stream));

    stream.next_in = inp;
    stream.next_out = out;
    stream.avail_in = 0;
    stream.avail_out = len;

    if (inflateInit(&stream)) {
        return -1;
    };

    inflate(&stream, Z_SYNC_FLUSH);

    return true;
}

bool zrif_decode(uint8_t* org, uint8_t* target, uint32_t len) {
    uint8_t raw[1024];
    base64_decode(org, raw, len);

    uint8_t out[2048];
    inflate_from_str(raw, out, sizeof(out));

    memcpy(target, out, len);

    return true;
}

bool load_pkg(Ptr<const void> &entry_point, IOState &io, MemState &mem, std::string &game_title, std::string& title_id, const std::wstring& path) {
    FILE* pkg_file;

#ifdef WIN32
    if (_wfopen_s(&pkg_file, path.c_str(), L"rb")) {
#else
    if (!(pkg_file = fopen(wide_to_utf(path).c_str(), "rb"))) {
#endif
        return false;
    }

    PkgHeader header;
    ContentType ctType;

    fread(&header, 1, sizeof(PkgHeader), pkg_file);

    std::vector<PkgInfo> infos;

    infos.resize(header.pkgInfoCount);

    for (uint32_t i = 0; i < header.pkgInfoCount; i++) {
         fread(&infos[i], 1, 8, pkg_file);

         if (infos[i].ident == PkgIdentifier::Cagetory) {
             fread(&infos[i].contentType, 1, sizeof(uint32_t), pkg_file);
             ctType = infos[i].contentType;
         }
         else if (infos[i].ident == PkgIdentifier::Item || infos[i].ident == PkgIdentifier::SfoFile) {
             fread(&infos[i].itemOffset, 1, sizeof(uint32_t), pkg_file);
             fread(&infos[i].itemSize, 1, sizeof(uint32_t), pkg_file);
         }
    }

    return true;
}
