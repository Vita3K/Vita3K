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

#include <psp2/types.h>
#include <util/types.h>

struct IOState;
struct SceIoStat;
struct SceIoDirent;

bool init(IOState &io, const char *pref_path);
SceUID open_file(IOState &io, const char *path, s32 flags, const char *pref_path);
s32 read_file(void *data, IOState &io, SceUID fd, SceSize size);
s32 write_file(SceUID fd, const void *data, SceSize size, const IOState &io);
s32 seek_file(SceUID fd, s32 offset, s32 whence, const IOState &io);
void close_file(IOState &io, SceUID fd);
s32 create_dir(const char *dir, s32 mode, const char *pref_path);
s32 remove_file(const char *file, const char *pref_path);
s32 create_dir(const char *dir, s32 mode, const char *pref_path);
s32 remove_dir(const char *dir, const char *pref_path);
s32 stat_file(const char* file, SceIoStat* stat, const char *pref_path);

s32 open_dir(IOState &io, const char *path, const char *pref_path);
s32 read_dir(IOState &io, SceUID fd, SceIoDirent *dent);
s32 close_dir(IOState &io, SceUID fd);
