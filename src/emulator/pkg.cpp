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

extern "C" {
#include <base64.h>
}

#include <aes.hpp>

#include <util/string_convert.h>
#include <util/byteswap.h>
#include <util/log.h>

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

uint32_t read_swap32(FILE* file, uint32_t elementCount = 1) {
	uint32_t temp;
	fread(&temp, sizeof(uint32_t), elementCount, file);

	return swap_int(temp);
}

uint16_t read_swap16(FILE* file, uint32_t elementCount = 1) {
	uint16_t temp;
	fread(&temp, sizeof(uint16_t), elementCount, file);

	return swap_short(temp);
}

uint64_t read_swap64(FILE* file, uint32_t elementCount = 1) {
	uint64_t temp;
	fread(&temp, sizeof(uint64_t), elementCount, file);

	return swap_int64(temp);
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
	PkgExtendHeader extHeader;

    ContentType ctType;

	header.magicNumber = read_swap32(pkg_file);
	header.pkgRevision = read_swap16(pkg_file);
	header.pkgType = (PkgType)read_swap16(pkg_file);
	header.pkgInfoOffset = read_swap32(pkg_file);
	header.pkgInfoCount = read_swap32(pkg_file);
	header.headerSize = read_swap32(pkg_file);
	header.itemCount = read_swap32(pkg_file);
	header.totalSize = read_swap64(pkg_file);
	header.dataOffset = read_swap64(pkg_file);
	header.dataSize = read_swap64(pkg_file);

	// All bytes, should be fine
	fread(header.contentid, 1, 0x30, pkg_file);
	fread(header.digest, 1, 0x10, pkg_file);
	fread(header.dataRiv, 1, 0x10, pkg_file);
	fread(header.cmacHash, 1, 0x10, pkg_file);
	fread(header.npdrmSig, 1, 0x28, pkg_file);
	fread(header.sha1Hash, 1, 0x08, pkg_file);

	extHeader.magic = read_swap32(pkg_file);
	extHeader.unknown01 = read_swap32(pkg_file);
	extHeader.headerSize = read_swap32(pkg_file);
	extHeader.dataSize = read_swap32(pkg_file);
	extHeader.dataOffset = read_swap32(pkg_file);
	extHeader.dataType = read_swap32(pkg_file);
	extHeader.pkgDataSize = read_swap32(pkg_file);
	extHeader.padding01 = read_swap32(pkg_file);
	extHeader.dataType2 = read_swap32(pkg_file);
	extHeader.padding02 = read_swap32(pkg_file);
	extHeader.padding03 = read_swap32(pkg_file);
	extHeader.padding04 = read_swap32(pkg_file);

	char key = extHeader.dataType & 7;

	uint32_t itemsOffset;
	uint32_t itemsSize;

    std::vector<PkgInfo> infos;

	infos.resize(header.pkgInfoCount);
	fseek(pkg_file, header.pkgInfoOffset, SEEK_SET);

    for (uint32_t i = 0; i < header.pkgInfoCount; i++) {
		uint32_t tell = ftell(pkg_file);

		 infos[i].ident = (PkgIdentifier)read_swap32(pkg_file);
		 infos[i].size = read_swap32(pkg_file);

         if (infos[i].ident == PkgIdentifier::Cagetory) {
			 infos[i].contentType = (ContentType)read_swap32(pkg_file);
             ctType = (ContentType)infos[i].contentType;

			 fseek(pkg_file, infos[i].size - 4, SEEK_CUR);
         }
         else if (infos[i].ident == PkgIdentifier::Item) {
			 infos[i].itemOffset = read_swap32(pkg_file);
			 infos[i].itemSize = read_swap32(pkg_file);

			 itemsOffset = infos[i].itemOffset;
			 itemsSize = infos[i].itemSize;

			 fseek(pkg_file, infos[i].size - 8, SEEK_CUR);
		 }
		 else if (infos[i].ident == PkgIdentifier::SfoFile) {
			 infos[i].sfoOffset = read_swap32(pkg_file);
			 infos[i].sfoSize = read_swap32(pkg_file);

			 fseek(pkg_file, infos[i].size - 8, SEEK_CUR);
		 }
		 else if (infos[i].ident == PkgIdentifier::DrmType) {
			 infos[i].drmType = read_swap32(pkg_file);

			 fseek(pkg_file, infos[i].size - 4, SEEK_CUR);
		 } else if (infos[i].ident == PkgIdentifier::Flags) {
			 infos[i].packageFlag = read_swap32(pkg_file);

			 fseek(pkg_file, infos[i].size - 4, SEEK_CUR);
		 } else {
			 fseek(pkg_file, infos[i].size, SEEK_CUR);
		 }
    }

	AES_ctx ctx;
	BYTE main_key[0x10];

	if (key == 1) {
		memcpy(main_key, pkg_psp_key, 0x10);
		AES_init_ctx(&ctx, pkg_ps3_key);
	}
	else if (key == 2) {
		AES_ctx sub_ctx;
		AES_init_ctx(&sub_ctx, pkg_vita_2);
		
		memcpy(main_key, header.dataRiv, 0x10);

		AES_ECB_encrypt(&sub_ctx, main_key);
	}
	else if (key == 3) {
		AES_ctx sub_ctx;
		AES_init_ctx(&sub_ctx, pkg_vita_3);

		memcpy(main_key, header.dataRiv, 0x10);

		AES_ECB_encrypt(&sub_ctx, main_key);
	}
	else if (key == 4) {
		AES_ctx sub_ctx;
		AES_init_ctx(&sub_ctx, pkg_vita_4);

		memcpy(main_key, header.dataRiv, 0x10);

		AES_ECB_encrypt(&sub_ctx, main_key);
	}

	AES_ctx final_ctx;
	AES_init_ctx_iv(&final_ctx, main_key, header.dataRiv);

	std::vector<PkgFileHeader> file_headers;
	file_headers.resize(header.itemCount);

	fseek(pkg_file, header.dataOffset + itemsOffset, SEEK_SET);

	for (uint32_t i = 0; i < header.itemCount; i++) {
		uint8_t item_header_raw[sizeof(PkgFileHeader)];
		fread(item_header_raw, 1, sizeof(PkgFileHeader), pkg_file);

		AES_CTR_xcrypt_buffer(&final_ctx, item_header_raw, sizeof(PkgFileHeader));

		memcpy(&file_headers[i].fileNameOffset, item_header_raw, 4);
		memcpy(&file_headers[i].fileNameSize, item_header_raw + 4, 4);
		memcpy(&file_headers[i].dataOffset, item_header_raw + 8, 8);
		memcpy(&file_headers[i].dataSize, item_header_raw + 16, 8);

		file_headers[i].fileNameOffset = swap_int(file_headers[i].fileNameOffset);
		file_headers[i].fileNameSize = swap_int(file_headers[i].fileNameSize);
		file_headers[i].dataOffset = swap_int64(file_headers[i].dataOffset);
		file_headers[i].dataSize = swap_int64(file_headers[i].dataSize);
	}

    return true;
}
