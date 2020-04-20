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

constexpr int SCE_ERROR_ERRNO_ENOENT = 0x80010002; // Associated file or directory does not exist
constexpr int SCE_ERROR_ERRNO_EEXIST = 0x80010011; // File exists
constexpr int SCE_ERROR_ERRNO_EMFILE = 0x80010018; // Too many files are open
constexpr int SCE_ERROR_ERRNO_EBADFD = 0x80010051; // File descriptor is invalid for this operation
constexpr int SCE_ERROR_ERRNO_EOPNOTSUPP = 0x8001005F; // Operation not supported
