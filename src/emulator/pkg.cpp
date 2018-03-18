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

// Implementation based on weaknespase work

#include "load_self.h"
#include "pkg.h"
#include "sfo.h"

#include <miniz.h>

extern "C" {
#include <base64.h>
}

#include <aes.hpp>

#include <util/string_convert.h>
#include <util/byteswap.h>
#include <util/log.h>
#include <io/vfs.h>

#include <vector>
#include <string>
#include <cmath>

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

unsigned int zrif_dict_size = 1024;

static const unsigned char zrif_dict[] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 48, 48, 48, 48, 57, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 48, 48,
	48, 48, 54, 48, 48, 48, 48, 55, 48, 48, 48, 48, 56, 0, 48, 48, 48, 48, 51, 48, 48, 48, 48, 52, 48, 48, 48, 48,
	53, 48, 95, 48, 48, 45, 65, 68, 68, 67, 79, 78, 84, 48, 48, 48, 48, 50, 45, 80, 67, 83, 71, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 49, 45, 80, 67, 83, 69, 48, 48, 48, 45, 80, 67, 83, 70, 48, 48, 48, 45, 80, 67, 83,
	67, 48, 48, 48, 45, 80, 67, 83, 68, 48, 48, 48, 45, 80, 67, 83, 65, 48, 48, 48, 45, 80, 67, 83, 66, 48, 48,
	48, 0, 1, 0, 1, 0, 1, 0, 2, 239, 205, 171, 137, 103, 69, 35, 1, 0 };

unsigned int pdb_part_01_len = 210;
static const unsigned char pdb_part_01[] =
{
	0, 0, 0, 0, 100, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 101, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 2, 0, 0, 0, 102, 0,
	0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 107, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 7, 0, 0, 0, 104, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0,
	0, 0, 0, 0, 108, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 1, 0, 0, 0, 109, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 110, 0,
	0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 112, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 113, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 114,
	0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 115, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 116, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0,
	0, 0, 111, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0 };

unsigned int pdb_part_02_len = 185;
static const unsigned char pdb_part_02[] =
{
	230, 0, 0, 0, 29, 0, 0, 0, 29, 0, 0, 0, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0, 217, 0, 0, 0, 37, 0, 0, 0, 37, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 218, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 206,
	0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 0, 144, 1, 0, 0, 0, 0, 0, 208, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 0, 144, 1, 0, 0, 0, 0,
	0, 204, 0, 0, 0, 30, 0, 0, 0, 30, 0, 0, 0, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0, 0 };

unsigned int pdb_part_03_len = 205;
static const unsigned char pdb_part_03[] =
{
	232, 0, 0, 0, 120, 0, 0, 0, 120, 0, 0, 0, 2, 0, 0, 0, 22, 0, 0, 0, 14, 0, 0, 128, 13, 0, 0, 0, 16, 15, 0, 0, 0, 0,
	0, 0, 0, 144, 1, 0, 0, 0, 0, 0, 0, 144, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 205, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 236, 0, 0,
	0, 4, 0, 0, 0, 4, 0, 0, 0, 199, 8, 120, 149, 237, 0, 0, 0, 32, 0, 0, 0, 32, 0, 0, 0, 191, 31, 176, 182, 101, 19,
	244, 6, 161, 144, 115, 57, 24, 86, 53, 208, 34, 131, 37, 93, 67, 148, 147, 158, 117, 166, 119, 106, 126,
	3, 133, 198, 0 };

// https://wiki.henkaku.xyz/vita/Packages#AES_Keys
static const uint8_t pkg_ps3_key[] = { 0x2e, 0x7b, 0x71, 0xd7, 0xc9, 0xc9, 0xa1, 0x4e, 0xa3, 0x22, 0x1f, 0x18, 0x88, 0x28, 0xb8, 0xf8 };
static const uint8_t pkg_psp_key[] = { 0x07, 0xf2, 0xc6, 0x82, 0x90, 0xb5, 0x0d, 0x2c, 0x33, 0x81, 0x8d, 0x70, 0x9b, 0x60, 0xe6, 0x2b };
static const uint8_t pkg_vita_2[] = { 0xe3, 0x1a, 0x70, 0xc9, 0xce, 0x1d, 0xd7, 0x2b, 0xf3, 0xc0, 0x62, 0x29, 0x63, 0xf2, 0xec, 0xcb };
static const uint8_t pkg_vita_3[] = { 0x42, 0x3a, 0xca, 0x3a, 0x2b, 0xd5, 0x64, 0x9f, 0x96, 0x86, 0xab, 0xad, 0x6f, 0xd8, 0x80, 0x1f };
static const uint8_t pkg_vita_4[] = { 0xaf, 0x07, 0xfd, 0x59, 0x65, 0x25, 0x27, 0xba, 0xf1, 0x33, 0x89, 0x66, 0x8b, 0x17, 0xd9, 0xea };

// Wont work yet, because MINIZ dont have function to set dictionary
int inflate_from_str(uint8_t* inp, uint8_t* out, uint32_t len) {
    mz_stream stream;

    memset(&stream, 0, sizeof(stream));

    stream.next_in = inp;
    stream.next_out = out;
    stream.avail_in = 0;
    stream.avail_out = len;

    if (inflateInit2(&stream, 10)) {
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

void ctr128_add(unsigned char *counter, unsigned long long int value) {
	uint32_t n = 16;

	do {
		--n;
		value += counter[n];
		counter[n] = (uint8_t)value;
		value >>= 8;
	} while (n);
}

uint8_t* put_string_param(uint8_t *buf, uint32_t code, std::string str) {
	uint32_t len = str.length() + 1;
	*((uint32_t*)buf) = code;
	buf += sizeof(uint32_t);
	*((uint32_t*)buf) = len;
	buf += sizeof(uint32_t);
	*((uint32_t*)buf) = len;
	memcpy(buf, str.c_str(), len);

	return buf + len;
}

uint32_t pdb_gen(uint8_t *buffer, uint32_t len, std::string title,
	std::string title_id, std::string pkg_name, std::string pkg_url,
	uint64_t pkg_size, uint32_t install_id) {
	uint32_t total = pdb_part_01_len +
		13 + title.length() + 13 + pkg_name.length() +
		13 + pkg_url.length() + 13 + 0x1D + pdb_part_02_len +
		13 + 10 + pdb_part_03_len;

	if (total < len) {
		uint8_t* start = buffer;
		memcpy(buffer, pdb_part_01, pdb_part_01_len);
		buffer += pdb_part_01_len;

		buffer = put_string_param(buffer, 0x69, title);
		buffer = put_string_param(buffer, 0xCB, pkg_name);
		buffer = put_string_param(buffer, 0xCA, pkg_url);

		std::string icon_path = "ux0:bgdl/t/" + install_id;
		icon_path += "/icon.png";

		buffer = put_string_param(buffer, 0x6A, icon_path);

		memcpy(buffer, pdb_part_02, pdb_part_02_len);
		buffer += pdb_part_02_len;

		buffer = put_string_param(buffer, 0xDC, title_id);

		memcpy(buffer, pdb_part_03, pdb_part_02_len);
		memcpy(buffer + 64, title_id.c_str(), 0xA);

		buffer += pdb_part_03_len;

		return buffer - start;
	}

	return 0;
}

void pkg_seek(FILE* file, uint64_t offset, 
	uint64_t dataOffset,
	uint8_t* orgIv, 
	uint8_t* nextIv,
	bool encrypted) {
	fseek(file, offset, SEEK_SET);
	memcpy(nextIv, orgIv, 0x10);

	if (encrypted) {
		ctr128_add(nextIv, (offset - dataOffset) / 0x10);
	}
}

size_t pkg_read(FILE* file, uint8_t* buf,
	size_t pkgDataOffset,
	size_t pkgDataSize,
	size_t len,
	uint8_t* orgIv,
	uint8_t* nextIv,
	AES_ctx* ctrKey) {

	AES_ctx_set_iv(ctrKey, nextIv);

	size_t crrPos = ftell(file);
	size_t read = 0;

	if (crrPos < pkgDataOffset) {
		size_t td = std::min(pkgDataOffset - crrPos, len);
		read += fread(buf, 1, td, file);
		len -= read;
		buf += read;
		crrPos += read;
		
		if (read < td) {
			return read;
		}
	}

	size_t totalLen = len;
	len = std::min(pkgDataOffset - crrPos + pkgDataSize, len);

	if (len > 0) {
		uint64_t reldata = crrPos - pkgDataOffset;

		if (reldata & 0xF != 0) {
			LOG_INFO("Unaligned access");
			int64_t reldata_aligned = reldata & 0xFFFFFFFFFFFFFFF0ull;
			uint8_t enc[0x10];
			fseek(file, pkgDataOffset + reldata_aligned, SEEK_SET);
			fread(enc, 1, 0x10, file);
			crrPos = pkgDataOffset + reldata_aligned;

			uint32_t td = std::min(0x10 - reldata_aligned + reldata, len);

			AES_CTR_xcrypt_buffer(ctrKey, enc, 0x10);

			memcpy(buf, enc + reldata - reldata_aligned, td);

			buf += td;
			read += td;
			reldata = reldata_aligned + 0x10;
			len -= td;

			memcpy(nextIv, ctrKey->Iv, 0x10);
		}

		size_t aligned_read = fread(buf, 1, len, file);
		read += aligned_read;
		crrPos += aligned_read;

		while (aligned_read > 0) {
			uint32_t nlen = std::min((size_t)0x10, aligned_read);
			AES_CTR_xcrypt_buffer(ctrKey, buf, nlen);

			buf += nlen;
			aligned_read -= nlen;

			memcpy(nextIv, ctrKey->Iv, 0x10);
		}

		if (len & 0xF != 0) {
			memcpy(nextIv, orgIv, 0x10);

			if (crrPos > pkgDataOffset) {
				ctr128_add(nextIv, crrPos - pkgDataOffset);
			}
		}
	}

	len = totalLen - len;
	if (len > 0) {
		size_t td = fread(buf, 1, len, file);
		len -= td;
		buf += td;
		crrPos += td;

		read += td;
		crrPos += td;
	}

	return read;
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
	extHeader.pkgDataSize = read_swap64(pkg_file);
	extHeader.padding01 = read_swap32(pkg_file);
	extHeader.dataType2 = read_swap32(pkg_file);
	extHeader.padding02 = read_swap32(pkg_file);
	extHeader.padding03 = read_swap64(pkg_file);
	extHeader.padding04 = read_swap64(pkg_file);

	char key = extHeader.dataType2 & 7;

	uint32_t itemsOffset;
	uint32_t itemsSize;

	uint32_t sfoOffset;
	uint32_t sfoSize;

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

			sfoOffset = infos[i].sfoOffset;
			sfoSize = infos[i].sfoSize;

			fseek(pkg_file, infos[i].size - 8, SEEK_CUR);
		}
		else if (infos[i].ident == PkgIdentifier::DrmType) {
			infos[i].drmType = read_swap32(pkg_file);

			fseek(pkg_file, infos[i].size - 4, SEEK_CUR);
		}
		else if (infos[i].ident == PkgIdentifier::Flags) {
			infos[i].packageFlag = read_swap32(pkg_file);

			fseek(pkg_file, infos[i].size - 4, SEEK_CUR);
		}
		else {
			fseek(pkg_file, infos[i].size, SEEK_CUR);
		}
	}

	fseek(pkg_file, sfoOffset, SEEK_SET);

	std::vector<uint8_t> sfoData;
	sfoData.resize(sfoSize);

	fread(sfoData.data(), 1, sfoSize, pkg_file);

	SfoFile sfoFile;
	load_sfo(sfoFile, sfoData);

	game_title = find_data(sfoFile, "TITLE");
	title_id = find_data(sfoFile, "TITLE_ID");

	vfs::mount("app0", fs::absolute(vfs::physical_mount_path("ux0") + "/app/" + title_id + "/").string());

	// If the folder exists, dont reinstall it
	if (!fs::exists(vfs::physical_mount_path("app0"))) {
		// If there is missing files, its user fault
		LOG_INFO("Try to install {} ({}) if you are done with the app/game and you want to remove, please remove folder: {}", game_title, title_id,
			vfs::physical_mount_path("app0"));
	}

	AES_ctx ctx;
	BYTE main_key[0x10];

	if (key == 2) {
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

	uint8_t nextIv[0x10];

	pkg_seek(pkg_file, header.dataOffset + itemsOffset,
		header.dataOffset, header.dataRiv, nextIv,
		true);

	std::vector<uint8_t> raw_fh;
	raw_fh.resize(itemsSize);

	pkg_read(pkg_file, raw_fh.data(),
		header.dataOffset, header.dataSize, itemsSize, header.dataRiv,
		nextIv, &final_ctx);

	PkgFileHeader* file_headers = reinterpret_cast<PkgFileHeader*>(raw_fh.data());

	auto extract_file_main = [&](PkgFileHeader& fileHeader,
		fs::path output, bool encrypt) {
		pkg_seek(pkg_file, fileHeader.dataOffset + header.dataOffset, header.dataOffset,
			header.dataRiv, nextIv, encrypt);

		FILE *temp = fopen(output.string().c_str(), "wb");
		
		std::vector<uint8_t> data;
		data.resize(0x10000);

		uint64_t left = fileHeader.dataSize;

		while (left > 0) {
			// Read chunk by chunk
			size_t required = std::min(left, (uint64_t)0x10000);
			size_t read = 0;

			// Check if file is normal and no need to decrypt
			if (!encrypt) {
				read = fread(data.data(), 1, required, pkg_file);
			} else {
				read = pkg_read(pkg_file, data.data(), header.dataOffset,
					header.dataSize, required, header.dataRiv, nextIv, &final_ctx);
			}

			if (read > 0) {
				int written = 0;

				while (written < read) {
					written += fwrite(
						data.data() + written,	1, 
						read - written,
						temp);
				}

				left -= read;
			}
		}

		fclose(temp);
	};

	auto get_file_dir = [&](PkgFileHeader& fileHeader) -> fs::path {
		std::string tryp;
		tryp.resize(1024);

		memcpy(tryp.data(), raw_fh.data() + 
			fileHeader.fileNameOffset - itemsOffset, 
			fileHeader.fileNameSize);

		fs::path output_path
			= fs::absolute(vfs::physical_mount_path("app0") + tryp);

		return output_path;
	};

	auto get_file_name = [&](PkgFileHeader& fileHeader) -> std::string {
		std::string tryp;
		tryp.resize(1024);

		memcpy(tryp.data(), raw_fh.data() +
			fileHeader.fileNameOffset - itemsOffset,
			fileHeader.fileNameSize);

		return tryp;
	};

	for (uint32_t i = 0; i < header.itemCount; i++) {
		file_headers[i].fileNameOffset = swap_int(file_headers[i].fileNameOffset);
		file_headers[i].fileNameSize = swap_int(file_headers[i].fileNameSize);
		file_headers[i].dataOffset = swap_int64(file_headers[i].dataOffset);
		file_headers[i].dataSize = swap_int64(file_headers[i].dataSize);
		file_headers[i].flags = (FileType)(swap_int((uint32_t)file_headers[i].flags) & 0xFF);

		assert(file_headers[i].fileNameOffset % 16 == 0 && file_headers[i].dataOffset % 16 == 0);
		
		switch (file_headers[i].flags) {
			case FileType::SData: case FileType::SData2: {
				fs::path output_path = get_file_dir(file_headers[i]);
				fs::create_directories(output_path);

				break;
			}

			case FileType::Unknown: case FileType::Self: 
			case FileType::EData: case FileType::Keystone: 
			case FileType::Keystone2: case FileType::PFSFile: 
			case FileType::TempBin: case FileType::ClearSign:
			case FileType::ClearSign2: case FileType::RightSprx: 
			case FileType::OO: {
				fs::path output_path = get_file_dir(file_headers[i]);
				extract_file_main(file_headers[i], output_path, true);
				break;
			}

			case FileType::DigsBin: {
				std::string name = get_file_name(file_headers[i]);
				
				if (name != "digs.bin") {
					LOG_WARN("Filetype is DigsBin, but file is not digs.bin, normal extraction");
					fs::path output_path = get_file_dir(file_headers[i]);
					extract_file_main(file_headers[i], output_path, false);
				}
				else {
					fs::path output_path = vfs::get("app0:/sce_sys/package/body.bin");
					extract_file_main(file_headers[i], output_path, true);
				}

				break;
			}
		}
	
	}

	// Output package file
	// head.bin
	size_t len = header.dataOffset + itemsSize;

	std::vector<uint8_t> data;
	data.resize(len);

	fseek(pkg_file, 0, SEEK_SET);
	fread(data.data(), 1, len, pkg_file);

	std::string head_path = vfs::get("app0:/sce_sys/package/head.bin");
	std::string tail_path = vfs::get("app0:/sce_sys/package/tail.bin");
	std::string body_path = vfs::get("app0:/sce_sys/package/body.bin");
	std::string temp_path = vfs::get("app0:/sce_sys/package/temp.bin");
	std::string stat_path = vfs::get("app0:/sce_sys/package/stat.bin");

	FILE* head_bin = fopen(head_path.c_str(), "wb");

	fwrite(data.data(), 1, len, head_bin);
	fclose(head_bin);

	// tail.bin

	size_t off = len;
	len = header.totalSize - off;

	data.resize(len);

	fseek(pkg_file, off, SEEK_SET);
	fread(data.data(), 1, len, pkg_file);

	FILE* tail_bin = fopen(tail_path.c_str(), "wb");

	fwrite(data.data(), 1, len, tail_bin);
	fclose(tail_bin);

	// stat.bin
	len = 0x300;
	data.resize(len);

	memset(data.data(), 0, len);

	FILE* stat_bin = fopen(stat_path.c_str(), "wb");
	fwrite(data.data(), 1, len, stat_bin);

	fclose(stat_bin);

	if (ctType == ContentType::PSVitaDLC) {
		std::vector<uint8_t> pkg_db(0x2000);

		// I like atBus
		uint32_t dblen = pdb_gen(pkg_db.data(),
			0x2000, game_title, title_id,
			fs::path(wide_to_utf(path).c_str()).filename().string(),
			"www.atBust.com",
			header.totalSize,
			0);

		std::string pdb0 = vfs::get("app0:/d0.pdb");
		std::string pdb1 = vfs::get("app0:/d1.pdb");
		std::string pdb2 = vfs::get("app0:/f0.pdb");

		FILE* temp = fopen(pdb0.c_str(), "wb");
		fwrite(pkg_db.data(), 1, dblen, temp);

		fclose(temp);

		pkg_db[0x20] = 0;

		temp = fopen(pdb1.c_str(), "wb");
		fwrite(pkg_db.data(), 1, dblen, temp);

		fclose(temp);

		temp = fopen(pdb2.c_str(), "wb");
		fwrite(nullptr, 0, 0, temp);

		fclose(temp);
	}

	FILE* eboot_file = fopen(vfs::get("app0:/eboot.bin").c_str(), "wb");
	fseek(eboot_file, 0, SEEK_END);
	
	size_t eboot_size = ftell(eboot_file);

	fseek(eboot_file, 0, SEEK_SET);

	std::vector<uint8_t> eboot(eboot_size);
	fread(eboot.data(), 1, eboot_size, eboot_file);

	if (!load_self(entry_point, mem, eboot.data())) {
		return false;
	}

    return true;
}
