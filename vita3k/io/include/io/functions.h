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

#pragma once

#undef st_atime
#undef st_ctime
#undef st_mtime

#include <io/VitaIoDevice.h>
#include <io/state.h>
#include <io/types.h>
#include <pfsmgr/functions.h>

#include <util/fs.h>

#include <functional>
#include <limits>
#include <span>
#include <string>

struct IOState;
struct KernelState;
struct MemState;
template <typename T>
class Ptr;

inline SceUID invalid_fd = -1;

void init_device_paths(IOState &io, const fs::path &vita_fs_path);
bool init_savedata_app_path(IOState &io, const fs::path &vita_fs_path, const std::function<pfsmgr::SavedataIdentity()> &identity_provider);
// Requires savedata0 to be configured and any protected savedata to be mounted.
// Returns 0, EBADFD for an unavailable/unconfigured mount, EIO for invalid or
// inaccessible data/publication, EEXIST on a creation race, or ENOMEM. Errors
// are negative SCE_ERROR_ERRNO_* values; existing slot files are never modified.
int initialize_savedata_slots(IOState &io, const fs::path &vita_fs_path);
int create_protected_savedata_directories(IOState &io, const fs::path &relative_path);
int write_protected_savedata(IOState &io, const fs::path &relative_path, uint64_t offset,
    std::span<const uint8_t> data, std::optional<uint64_t> truncate_size = std::nullopt);
int remove_protected_savedata(IOState &io, const fs::path &relative_path);
bool init(IOState &io, const fs::path &cache_path, const fs::path &log_path, const fs::path &vita_fs_path, bool redirect_stdio);
void io_deinit(IOState &io);
// open_error is zero after a successful open, even if a later read fails.
bool read_file_to_buffer(IOState &io, const char *path, const fs::path &vita_fs_path, std::vector<uint8_t> &buffer, const char *export_name,
    size_t max_size = std::numeric_limits<size_t>::max(), int *open_error = nullptr, bool quiet_missing_mount = false);

bool find_case_isens_path(IOState &io, VitaIoDevice &device, const fs::path &translated_path, const fs::path &system_path);
fs::path find_in_cache(IOState &io, const std::string &system_path);

fs::path expand_path(IOState &io, const char *path, const fs::path &vita_fs_path);
std::string translate_path(const char *path, VitaIoDevice &device, const IOState::DevicePaths &device_paths);

SceUID open_file(IOState &io, const char *path, const int flags, const fs::path &vita_fs_path, const char *export_name, bool quiet_missing_mount = false);
SceUID open_file_guest(IOState &io, MemState &mem, Ptr<const char> path, int flags, const fs::path &vita_fs_path, const char *export_name);
int read_file(void *data, IOState &io, SceUID fd, SceSize size, const char *export_name);
int read_file_guest(IOState &io, MemState &mem, SceUID fd, Ptr<uint8_t> data, SceSize size, const char *export_name);
int read_file_async(IOState &io, KernelState &kernel, MemState &mem, SceUID thread_id, SceUID fd, Ptr<uint8_t> data, SceSize size, Ptr<uint8_t> result, const char *export_name);
int complete_async_io(IOState &io, SceUID request_id, const char *export_name);
int write_file(SceUID fd, const void *data, SceSize size, const IOState &io, const char *export_name);
int write_file_guest(IOState &io, MemState &mem, SceUID fd, Ptr<const uint8_t> data, SceSize size, const char *export_name);
int pread_file_guest(IOState &io, MemState &mem, SceUID fd, Ptr<uint8_t> data, SceSize size, SceOff offset, const char *export_name);
int pwrite_file_guest(IOState &io, MemState &mem, SceUID fd, Ptr<const uint8_t> data, SceSize size, SceOff offset, const char *export_name);
int truncate_file(SceUID fd, unsigned long long length, const IOState &io, const char *export_name);
SceOff seek_file(SceUID fd, SceOff offset, SceIoSeekMode whence, IOState &io, const char *export_name);
SceOff tell_file(IOState &io, const SceUID fd, const char *export_name);
int stat_file(IOState &io, const char *file, SceIoStat *statp, const fs::path &vita_fs_path, const char *export_name, SceUID fd = invalid_fd);
int stat_file_by_fd(IOState &io, const SceUID fd, SceIoStat *statp, const fs::path &vita_fs_path, const char *export_name);
int stat_file_guest(IOState &io, MemState &mem, Ptr<const char> path, Ptr<SceIoStat> statp, const fs::path &vita_fs_path, const char *export_name);
int stat_file_by_fd_guest(IOState &io, MemState &mem, SceUID fd, Ptr<SceIoStat> statp, const fs::path &vita_fs_path, const char *export_name);
int close_file(IOState &io, SceUID fd, const char *export_name);
int remove_file(IOState &io, const char *file, const fs::path &vita_fs_path, const char *export_name);
int rename(IOState &io, const char *old_name, const char *new_name, const fs::path &vita_fs_path, const char *export_name);
int remove_file_guest(IOState &io, MemState &mem, Ptr<const char> path, const fs::path &vita_fs_path, const char *export_name);
int rename_guest(IOState &io, MemState &mem, Ptr<const char> old_path, Ptr<const char> new_path, const fs::path &vita_fs_path, const char *export_name);

SceUID open_dir(IOState &io, const char *path, const fs::path &vita_fs_path, const char *export_name);
SceUID read_dir(IOState &io, SceUID fd, SceIoDirent *dent, const fs::path &vita_fs_path, const char *export_name);
int create_dir(IOState &io, const char *dir, int mode, const fs::path &vita_fs_path, const char *export_name, const bool recursive = false);
int close_dir(IOState &io, SceUID fd, const char *export_name);
int remove_dir(IOState &io, const char *dir, const fs::path &vita_fs_path, const char *export_name);
SceUID open_dir_guest(IOState &io, MemState &mem, Ptr<const char> path, const fs::path &vita_fs_path, const char *export_name);
SceUID read_dir_guest(IOState &io, MemState &mem, SceUID fd, Ptr<SceIoDirent> dent, const fs::path &vita_fs_path, const char *export_name);
int create_dir_guest(IOState &io, MemState &mem, Ptr<const char> path, int mode, const fs::path &vita_fs_path, const char *export_name);
int remove_dir_guest(IOState &io, MemState &mem, Ptr<const char> path, const fs::path &vita_fs_path, const char *export_name);

// SceFios functions
SceUID create_overlay(IOState &io, SceFiosProcessOverlay *fios_overlay);
std::vector<std::string> resolve_overlay_paths(IOState &io, const char *input, const fs::path &vita_fs_path, bool for_write = false, SceUInt32 min_order = 0, SceUInt32 max_order = 0xFF, bool for_directory = false);
std::string resolve_path(IOState &io, const char *input, const fs::path &vita_fs_path, bool for_write = false, SceUInt32 min_order = 0, SceUInt32 max_order = 0xFF);
