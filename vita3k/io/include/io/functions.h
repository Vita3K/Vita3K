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

#undef st_atime
#undef st_ctime
#undef st_mtime

#include <io/VitaIoDevice.h>
#include <io/state.h>
#include <io/types.h>

#include <util/fs.h>

#include <string>

struct IOState;

inline SceUID invalid_fd = -1;

void init_device_paths(IOState &io);
bool init_savedata_app_path(IOState &io, const fs::path &pref_path);
bool init(IOState &io, const fs::path &cache_path, const fs::path &log_path, const fs::path &pref_path, bool redirect_stdio);

bool find_case_isens_path(IOState &io, VitaIoDevice &device, const fs::path &translated_path, const fs::path &system_path);
fs::path find_in_cache(IOState &io, const std::string &system_path);

fs::path expand_path(IOState &io, const char *path, const fs::path &pref_path);
std::string convert_path(const std::string &path);
std::string translate_path(const char *path, VitaIoDevice &device, const IOState &io);

/**
 * @brief Copy all directories and files from one location into another
 *
 * @param src_path Source path to copy directories and files from
 * @param dst_path Destination path where the directories and files contained in `src_path` are going to be copied
 * @return true Success
 * @return false Error
 */
bool copy_directories(const fs::path &src_path, const fs::path &dst_path);

/**
 * @brief Copy all files from a source path to the corresponding path in the emulated PS Vita filesystem
 * (Vita3K pref path) and delete the source path
 *
 * @param src_path Path from the host filesystem
 * @param pref_path Vita3K pref path
 * @param app_title_id App title ID (`PCSXXXXXX`)
 * @param app_category Content type ID as specified by `param.sfo` file
 * @return true Success
 * @return false Error
 */
bool copy_path(const fs::path &src_path, const fs::path &pref_path, const std::string &app_title_id, const std::string &app_category);

SceUID open_file(IOState &io, const char *path, const int flags, const fs::path &pref_path, const char *export_name);
int read_file(void *data, IOState &io, SceUID fd, SceSize size, const char *export_name);
int write_file(SceUID fd, const void *data, SceSize size, const IOState &io, const char *export_name);
int truncate_file(SceUID fd, unsigned long long length, const IOState &io, const char *export_name);
SceOff seek_file(SceUID fd, SceOff offset, SceIoSeekMode whence, IOState &io, const char *export_name);
SceOff tell_file(IOState &io, const SceUID fd, const char *export_name);
int stat_file(IOState &io, const char *file, SceIoStat *statp, const fs::path &pref_path, const char *export_name, SceUID fd = invalid_fd);
int stat_file_by_fd(IOState &io, const SceUID fd, SceIoStat *statp, const fs::path &pref_path, const char *export_name);
int close_file(IOState &io, SceUID fd, const char *export_name);
int remove_file(IOState &io, const char *file, const fs::path &pref_path, const char *export_name);
int rename(IOState &io, const char *old_name, const char *new_name, const fs::path &pref_path, const char *export_name);

SceUID open_dir(IOState &io, const char *path, const fs::path &pref_path, const char *export_name);
SceUID read_dir(IOState &io, SceUID fd, SceIoDirent *dent, const fs::path &pref_path, const char *export_name);
int create_dir(IOState &io, const char *dir, int mode, const fs::path &pref_path, const char *export_name, const bool recursive = false);
int close_dir(IOState &io, SceUID fd, const char *export_name);
int remove_dir(IOState &io, const char *dir, const fs::path &pref_path, const char *export_name);

// SceFios functions
SceUID create_overlay(IOState &io, SceFiosProcessOverlay *fios_overlay);
std::string resolve_path(IOState &io, const char *input, const SceUInt32 min_order = 0, const SceUInt32 max_order = 0x7F);
