// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#undef st_atime
#undef st_ctime
#undef st_mtime

#include <mem/ptr.h>
#include <util/types.h>

typedef enum SceIoMode {
    SCE_O_RDONLY = 0x0001, //!< Read-only
    SCE_O_WRONLY = 0x0002, //!< Write-only
    SCE_O_RDWR = (SCE_O_RDONLY | SCE_O_WRONLY), //!< Read/Write
    SCE_O_NBLOCK = 0x0004, //!< Non blocking
    SCE_O_DIROPEN = 0x0008, //!< Internal use for ::sceIoDopen
    SCE_O_RDLOCK = 0x0010, //!< Read locked (non-shared)
    SCE_O_WRLOCK = 0x0020, //!< Write locked (non-shared)
    SCE_O_APPEND = 0x0100, //!< Append
    SCE_O_CREAT = 0x0200, //!< Create
    SCE_O_TRUNC = 0x0400, //!< Truncate
    SCE_O_EXCL = 0x0800, //!< Exclusive create
    SCE_O_SCAN = 0x1000, //!< Scan type
    SCE_O_RCOM = 0x2000, //!< Remote command entry
    SCE_O_NOBUF = 0x4000, //!< Number device buffer
    SCE_O_NOWAIT = 0x8000, //!< Asynchronous I/O
    SCE_O_FDEXCL = 0x01000000, //!< Exclusive access
    SCE_O_PWLOCK = 0x02000000, //!< Power control lock
    SCE_O_FGAMEDATA = 0x40000000 //!< Gamedata access
} SceIoMode;

typedef enum SceIoAccessMode {
    SCE_S_IXUSR = 0x0001, //!< User execute permission
    SCE_S_IWUSR = 0x0002, //!< User write permission
    SCE_S_IRUSR = 0x0004, //!< User read permission
    SCE_S_IRWXU = 0x0007, //!< User access rights mask

    SCE_S_IXGRP = 0x0008, //!< Group execute permission
    SCE_S_IWGRP = 0x0010, //!< Group write permission
    SCE_S_IRGRP = 0x0020, //!< Group read permission
    SCE_S_IRWXG = 0x0038, //!< Group access rights mask

    SCE_S_IXOTH = 0x0040, //!< Others execute permission
    SCE_S_IWOTH = 0x0080, //!< Others write permission
    SCE_S_IROTH = 0x0100, //!< Others read permission
    SCE_S_IRWXO = 0x01C0, //!< Others access rights mask

    SCE_S_ISVTX = 0x0200, //!< Sticky
    SCE_S_ISGID = 0x0400, //!< Set GID
    SCE_S_ISUID = 0x0800, //!< Set UID

    SCE_S_IFDIR = 0x1000, //!< Directory
    SCE_S_IFREG = 0x2000, //!< Regular file
    SCE_S_IFLNK = 0x4000, //!< Symbolic link
    SCE_S_IFMT = 0xF000, //!< Format bits mask
} SceIoAccessMode;

enum SceIoFileMode {
    SCE_SO_IXOTH = 0x0001, //!< Hidden execute permission
    SCE_SO_IWOTH = 0x0002, //!< Hidden write permission
    SCE_SO_IROTH = 0x0004, //!< Hidden read permission
    SCE_SO_IFLNK = 0x0008, //!< Symbolic link
    SCE_SO_IFDIR = 0x0010, //!< Directory
    SCE_SO_IFREG = 0x0020, //!< Regular file
    SCE_SO_IFMT = 0x0038, //!< Format mask
};

enum SceIoSeekMode {
    SCE_SEEK_SET, //!< Starts from the begin of the file
    SCE_SEEK_CUR, //!< Starts from current position
    SCE_SEEK_END //!< Starts from the end of the file
};

struct SceIoStat {
    SceMode st_mode; //!< One or more ::SceIoAccessMode
    unsigned int st_attr; //!< One or more ::SceIoFileMode
    SceOff st_size; //!< Size of the file in bytes
    SceDateTime st_ctime; //!< Creation time
    SceDateTime st_atime; //!< Last access time
    SceDateTime st_mtime; //!< Last modification time
    unsigned int st_private[6]; //!< Device-specific data
};

struct SceIoDirent {
    SceIoStat d_stat;
    char d_name[256];
    Ptr<void> d_private;
    int dummy;
};
