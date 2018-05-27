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

#include <io/types.h>

#include <miniz.h>
#include <string>

struct IOState;
struct SceIoStat;
struct SceIoDirent;

void init_device_paths(IOState &io);

bool init(IOState &io, const char *pref_path);
SceUID open_file(IOState &io, const std::string &path_, int flags, const char *pref_path, const char *export_name);
int read_file(void *data, IOState &io, SceUID fd, SceSize size, const char *export_name);
int write_file(SceUID fd, const void *data, SceSize size, const IOState &io, const char *export_name);
int seek_file(SceUID fd, int offset, int whence, IOState &io, const char *export_name);
void close_file(IOState &io, SceUID fd, const char *export_name);
int create_dir(IOState &io, const char *dir, int mode, const char *pref_path, const char *export_name);
int remove_file(IOState &io, const char *file, const char *pref_path, const char *export_name);
int remove_dir(IOState &io, const char *dir, const char *pref_path, const char *export_name);
int stat_file(IOState &io, const char *file, SceIoStat *stat, const char *pref_path, uint64_t base_tick, const char *export_name);

int open_dir(IOState &io, const char *path, const char *pref_path, const char *export_name);
int read_dir(IOState &io, SceUID fd, emu::SceIoDirent *dent, const char *export_name);
int close_dir(IOState &io, SceUID fd, const char *export_name);
