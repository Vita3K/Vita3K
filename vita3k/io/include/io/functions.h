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

#undef st_atime
#undef st_ctime
#undef st_mtime

#include <io/state.h>
#include <io/types.h>
#include <io/vfs.h>

#include <util/fs.h>

#include <string>

struct IOState;

inline SceUID invalid_fd = -1;

void init_device_paths(IOState &io);
bool init_savedata_game_path(IOState &io, const fs::path &pref_path);
bool init(IOState &io, const fs::path &base_path, const fs::path &pref_path);

std::string expand_path(IOState &io, const char *path, const std::string &pref_path);
std::string translate_path(const char *path, VitaIoDevice &device, const IOState::DevicePaths &device_paths);

SceUID open_file(IOState &io, const char *path, const int flags, const std::string &pref_path, const char *export_name);
int read_file(void *data, IOState &io, SceUID fd, SceSize size, const char *export_name);
int write_file(SceUID fd, const void *data, SceSize size, const IOState &io, const char *export_name);
int truncate_file(SceUID fd, unsigned long long length, const IOState &io, const char *export_name);
SceOff seek_file(SceUID fd, SceOff offset, SceIoSeekMode whence, IOState &io, const char *export_name);
SceOff tell_file(IOState &io, const SceUID fd, const char *export_name);
int stat_file(IOState &io, const char *file, SceIoStat *statp, const std::string &pref_path, SceUInt64 base_tick, const char *export_name, SceUID fd = invalid_fd);
int stat_file_by_fd(IOState &io, const SceUID fd, SceIoStat *statp, const std::string &pref_path, SceUInt64 base_tick, const char *export_name);
int close_file(IOState &io, SceUID fd, const char *export_name);
int remove_file(IOState &io, const char *file, const std::string &pref_path, const char *export_name);

SceUID open_dir(IOState &io, const char *path, const std::string &pref_path, const char *export_name);
SceUID read_dir(IOState &io, SceUID fd, SceIoDirent *dent, const std::string &pref_path, const SceUInt64 base_tick, const char *export_name);
int create_dir(IOState &io, const char *dir, int mode, const std::string &pref_path, const char *export_name, const bool recursive = false);
int close_dir(IOState &io, SceUID fd, const char *export_name);
int remove_dir(IOState &io, const char *dir, const std::string &pref_path, const char *export_name);
