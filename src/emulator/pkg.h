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

#pragma once

#include <cstdint>
#include <string>

bool zrif_decode(uint8_t* org, uint8_t* target, uint32_t len);

enum class PkgType : uint16_t {
    PSP = 0x0002,
    PSVITA = 0x001
};

// http://www.psdevwiki.com/ps3/PKG_files
struct PkgHeader {
    uint32_t magicNumber;
    uint16_t pkgRevision;
    PkgType  pkgType;

    uint32_t pkgInfoOffset;
    uint32_t pkgInfoCount;

	uint32_t headerSize;
    uint32_t itemCount;

	uint64_t totalSize;

    uint64_t dataOffset; 
    uint64_t dataSize;

    char contentid[0x30]; 

    uint8_t digest[0x10]; 
	uint8_t dataRiv[0x10];
	uint8_t cmacHash[0x10];
	uint8_t npdrmSig[0x28];
    uint8_t sha1Hash[0x08]; 
};

struct PkgExtendHeader {
	uint32_t magic;
	uint32_t unknown01;
	uint32_t headerSize;
	uint32_t dataSize;
	uint32_t dataOffset;
	uint32_t dataType;
	uint64_t pkgDataSize;

	uint32_t padding01;
	uint32_t dataType2;
	uint32_t unknown02;
	uint32_t padding02;
	uint64_t padding03;
	uint64_t padding04;
};

enum class PkgIdentifier: uint32_t {
    DrmType = 1,
    Cagetory = 2,
    Flags = 3,
    Size = 4,
    PkgVer = 5,
    QADigest = 6,
    SysPkgAppVer = 7,
    Item = 13,
    SfoFile = 14
};

enum class ContentType: uint32_t {
    GameData = 0x00000004,
    GameExec = 0x00000005,
    PS1Emu = 0x00000006,
    PSPPCEngine = 0x00000007,
    Theme = 0x00000009,
    Widget = 0x0000000A,
    License = 0x0000000B,
    VSHModule = 0x0000000C,
    PSNAvatar = 0x0000000D,
    PSLogo = 0x0000000E,
    Minis = 0x0000000F,
    NEOGEO = 0x00000010,
    VMC = 0x00000011,
    PSP2Classic = 0x00000012,
    PSPRemastered = 0x00000014,
    PSVitaGameData = 0x00000015,
    PSVitaDLC = 0x00000016,
    PSVitaLA = 0x00000017,
    PSWebTV = 0x00000019
};

struct PkgInfo {
	uint32_t drmType;
	uint32_t packageFlag;
    PkgIdentifier ident;
    uint32_t size;
    ContentType contentType;
    uint32_t itemOffset;
    uint32_t itemSize;
	uint32_t sfoOffset;
	uint32_t sfoSize;
};

enum class FileType: uint32_t {
    Unknown = 0,
    Self = 1,
    Sprx = 2,
    EData = 3,
    SData = 4,
	Keystone=  14,
	Keystone2 = 15,
	PFSFile = 16,
	TempBin = 17,
	SData2 = 18,
	ClearSign = 19,
	ClearSign2 = 20,
	RightSprx = 21,
	OO = 22,
	DigsBin = 24,
};

enum class DRMLisenceType {
    Unknown = 0,
    Network = 1,
    Local = 2,
    Free = 3
};

struct PkgFileHeader {
    uint32_t fileNameOffset;
    uint32_t fileNameSize;
    uint64_t dataOffset;
    uint64_t dataSize;
    FileType flags;
    uint32_t pad;
};

struct IOState;
struct MemState;
template <class T>
class Ptr;

void ctr128_add(unsigned char *counter, unsigned long long int value);

uint32_t pdb_gen(uint8_t *buffer, uint32_t len, std::string title,
	std::string title_id, std::string pkg_name, std::string pkg_url,
	uint64_t pkg_size, uint32_t install_id);

// User can install a lisence after the game finish install
// uint32_t zrif_install(const std::string &title_id, uint8_t *zrif_buf);

bool load_pkg(Ptr<const void> &entry_point, IOState &io, MemState &mem, std::string &game_title, std::string& title_id, const std::wstring& path);
