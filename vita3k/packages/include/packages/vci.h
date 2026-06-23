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

// Credits to oestriot https://github.com/oestriot/VCI-TOOLS

#pragma once

#include <emuenv/state.h>
#include <packages/license.h>

#include <cstdint>
#include <functional>

constexpr uint32_t VCI_HEADER_SIZE = 0x200;
constexpr uint16_t VCI_MAJOR_VER = 1;
constexpr uint32_t SECTOR_SIZE = 0x200;
static const char VCI_MAGIC[4] = { 'V', 'C', 'I', '\0' };
static const char SCE_MBR_MAGIC[] = "Sony Computer Entertainment Inc.";

const uint8_t BIND_KEY[0x10] = { 0x90, 0x1a, 0x84, 0xfb, 0x13, 0xa7, 0x44, 0xa3, 0x78, 0xc5, 0x01, 0x8a, 0x60, 0xf5, 0x8c, 0x22 };

#pragma pack(push, 1)
// CMD56 per-cartridge keys. sha256(GcCmd56Keys) == cart_secret — used to unwrap klicensee from the RIF.
struct GcCmd56Keys {
    uint8_t packet20_key[0x20];
    uint8_t packet18_key[0x20];
};
static_assert(sizeof(GcCmd56Keys) == 0x40);

struct VciHeader {
    char magic[0x4]; // "VCI\0"
    uint16_t major_version;
    uint16_t minor_version;
    uint64_t device_size; // physical card size in bytes (NOT the MBR-used size)
    GcCmd56Keys keys; // 0x10
    uint8_t padding[0x1B0]; // -> 0x200
};
static_assert(sizeof(VciHeader) == VCI_HEADER_SIZE);

// SceMBR partition table. Block size is 0x200.
enum ScePartitionCode : uint8_t {
    ScePartitionCode_GRO0 = 0x9, // Game Card read-only
    ScePartitionCode_GRW0 = 0xA, // Game Card read-write
};

enum ScePartitionType : uint8_t {
    ScePartitionType_FAT16 = 0x6,
    ScePartitionType_EXFAT = 0x7,
    ScePartitionType_RAW = 0xDA,
};

struct ScePartition {
    uint32_t offset; // in blocks (0x200)
    uint32_t size; // in blocks (0x200)
    uint8_t code;
    uint8_t type;
    uint8_t active;
    uint32_t flags;
    uint16_t unk;
};
static_assert(sizeof(ScePartition) == 0x11); // 17 bytes

struct SceMbr {
    char magic[0x20]; // "Sony Computer Entertainment Inc."
    uint32_t version; // 0x3
    uint32_t device_size; // in blocks
    uint8_t unk1[0x28];
    ScePartition partitions[0x10]; // 0x50 .. 0x160
    uint8_t reserved[0x9E];
    uint16_t boot_signature; // 0xAA55
};
static_assert(sizeof(SceMbr) == 0x200);

struct VciBindData { // 0x90
    uint8_t cart_secret[0x20];
    uint8_t license[0x70];
};
static_assert(sizeof(VciBindData) == 0x90);
#pragma pack(pop)

// The on-card RIF and the NoNpDrm work.bin both use SceNpDrmLicense (license.h),
static_assert(sizeof(SceNpDrmLicense) == 0x200);

bool install_vci(const fs::path &vci_path, EmuEnvState &emuenv, const std::function<void(float)> &progress_callback = nullptr);
