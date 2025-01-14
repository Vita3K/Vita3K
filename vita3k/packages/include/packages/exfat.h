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

#pragma once

#include <util/log.h>

// Credits to relan for their original work on this https://github.com/relan/exfat

#define EXFAT_ENAME_MAX 15

struct ExFATSuperBlock {
    uint8_t jump[3]; /* 0x00 jmp and nop instructions */
    uint8_t oem_name[8]; /* 0x03 "EXFAT   " */
    uint8_t __unused1[53]; /* 0x0B always 0 */
    uint64_t sector_start; /* 0x40 partition first sector */
    uint64_t sector_count; /* 0x48 partition sectors count */
    uint32_t fat_sector_start; /* 0x50 FAT first sector */
    uint32_t fat_sector_count; /* 0x54 FAT sectors count */
    uint32_t cluster_sector_start; /* 0x58 first cluster sector */
    uint32_t cluster_count; /* 0x5C total clusters count */
    uint32_t rootdir_cluster; /* 0x60 first cluster of the root dir */
    uint32_t volume_serial; /* 0x64 volume serial number */

    struct { /* 0x68 FS version */
        uint8_t minor; /* Minor version */
        uint8_t major; /* Major version */
    } version;

    uint16_t volume_state; /* 0x6A volume state flags */
    uint8_t sector_bits; /* 0x6C sector size as (1 << n) */
    uint8_t spc_bits; /* 0x6D sectors per cluster as (1 << n) */
    uint8_t fat_count; /* 0x6E always 1 */
    uint8_t drive_no; /* 0x6F always 0x80 */
    uint8_t allocated_percent; /* 0x70 percentage of allocated space */
    uint8_t __unused2[397]; /* 0x71 always 0 */
    uint16_t boot_signature; /* 0xAA55 */
};

struct ExFATFileEntry {
    uint8_t type; /* EXFAT_ENTRY_FILE */
    uint8_t continuations;
    uint16_t checksum;
    uint16_t attrib; /* combination of EXFAT_ATTRIB_xxx */
    uint16_t __unknown1;
    uint16_t crtime, crdate; /* creation date and time */
    uint16_t mtime, mdate; /* latest modification date and time */
    uint16_t atime, adate; /* latest access date and time */
    uint8_t crtime_cs; /* creation time in cs (centiseconds) */
    uint8_t mtime_cs; /* latest modification time in cs */
    uint8_t crtime_tzo, mtime_tzo, atime_tzo; /* timezone offset encoded */
    uint8_t __unknown2[7];
};

struct ExFATFileEntryInfo {
    uint8_t type; /* EXFAT_ENTRY_FILE_INFO */
    uint8_t flags; /* combination of EXFAT_FLAG_xxx */
    uint8_t __unknown1;
    uint8_t name_length;
    uint16_t name_hash;
    uint16_t __unknown2;
    uint64_t valid_size; /* in bytes, less or equal to size */
    uint8_t __unknown3[4];
    uint32_t start_cluster;
    uint64_t size; /* in bytes */
};

struct ExFATEntryName {
    uint8_t type; /* EXFAT_ENTRY_FILE_NAME */
    uint8_t __unknown;
    uint16_t name[EXFAT_ENAME_MAX]; /* in UTF-16LE */
};

enum ExFATAttrib {
    EXFAT_ATTRIB_RO = 0x01,
    EXFAT_ATTRIB_HIDDEN = 0x02,
    EXFAT_ATTRIB_SYSTEM = 0x04,
    EXFAT_ATTRIB_VOLUME = 0x08,
    EXFAT_ATTRIB_DIR = 0x10,
    EXFAT_ATTRIB_ARCH = 0x20
};

enum ExFATEntryType {
    EXFAT_ENTRY_VALID = 0x80,
    EXFAT_ENTRY_CONTINUED = 0x40,
    EXFAT_ENTRY_OPTIONAL = 0x20,

    EXFAT_ENTRY_BITMAP = (0x01 | EXFAT_ENTRY_VALID),
    EXFAT_ENTRY_UPCASE = (0x02 | EXFAT_ENTRY_VALID),
    EXFAT_ENTRY_LABEL = (0x03 | EXFAT_ENTRY_VALID),
    EXFAT_ENTRY_FILE = (0x05 | EXFAT_ENTRY_VALID),
    EXFAT_ENTRY_FILE_INFO = (0x00 | EXFAT_ENTRY_VALID | EXFAT_ENTRY_CONTINUED),
    EXFAT_ENTRY_FILE_NAME = (0x01 | EXFAT_ENTRY_VALID | EXFAT_ENTRY_CONTINUED),
    EXFAT_ENTRY_FILE_TAIL = (0x00 | EXFAT_ENTRY_VALID | EXFAT_ENTRY_CONTINUED | EXFAT_ENTRY_OPTIONAL)
};

namespace exfat {
void extract_exfat(const fs::path &partition_path, const std::string &partition, const fs::path &pref_path);
} // namespace exfat
