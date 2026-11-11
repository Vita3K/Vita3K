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

#include <io/VitaIoDevice.h>
#include <io/filesystem.h>
#include <io/types.h>
#include <io/util.h>
#include <pfsmgr/mounts.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>

class SimpleEvent;
namespace pfsmgr {
class File;
class Layer;
class PfsSession;
} // namespace pfsmgr

struct SavedataAccess {
#if defined(__cpp_lib_atomic_shared_ptr)
    std::atomic<std::shared_ptr<pfsmgr::PfsSession>> session;

    std::shared_ptr<pfsmgr::PfsSession> load_session() const { return session.load(); }
    void store_session(std::shared_ptr<pfsmgr::PfsSession> value) { session.store(std::move(value)); }
#else
    std::shared_ptr<pfsmgr::PfsSession> session;

    std::shared_ptr<pfsmgr::PfsSession> load_session() const { return std::atomic_load(&session); }
    void store_session(std::shared_ptr<pfsmgr::PfsSession> value) { std::atomic_store(&session, std::move(value)); }
#endif
    std::atomic<bool> needs_recovery{ false };
};

struct AsyncIoRequest {
    std::shared_ptr<SimpleEvent> event;
    std::thread worker;
    std::function<void()> cleanup;
    bool finished = false;

    ~AsyncIoRequest();
};

// Class for all needed information to access files on Vita3K.
class FileStats : public VitaStats {
    // Host file pointer
    FilePtr wrapped_file{ nullptr, std::fclose };
    std::shared_ptr<std::mutex> operations_mutex = std::make_shared<std::mutex>();
    std::shared_ptr<pfsmgr::File> pfs_file;
    std::shared_ptr<SceOff> pfs_offset;
    std::shared_ptr<SavedataAccess> savedata_access;
    fs::path savedata_relative;
    std::shared_ptr<void> mount_lease;
    pfsmgr::MountScope mount_scope = pfsmgr::MountScope::Application;

public:
    std::mutex mutex;
    bool closed = false;
    size_t pending_async_reads = 0;

    // Constructor used for files
    // Based on https://codereview.stackexchange.com/questions/4679/
    explicit FileStats(const char *vita, const std::string &t, const fs::path &file, const int open, FilePtr stream)
        : wrapped_file(std::move(stream)) {
        file_info.vita_loc = vita;
        file_info.translated = t;
        file_info.sys_loc = file;
        file_info.open_mode = open;
        file_info.file_mode = SCE_SO_IFREG | SCE_SO_IROTH;
        file_info.access_mode = SCE_S_IFREG;
    }

    FileStats(const char *vita, const fs::path &file, int open, std::shared_ptr<pfsmgr::File> pfs);
    void set_savedata_access(std::shared_ptr<SavedataAccess> access, fs::path relative) {
        savedata_access = std::move(access);
        savedata_relative = std::move(relative);
    }

    void hold_mount(std::shared_ptr<void> lease) { mount_lease = std::move(lease); }
    void set_mount_scope(pfsmgr::MountScope scope) { mount_scope = scope; }
    bool is_shell_file() const { return mount_scope == pfsmgr::MountScope::Shell; }

    bool is_open() const { return wrapped_file || pfs_file; }

    bool is_regular_file() const {
        return file_info.file_mode & SCE_SO_IFREG;
    }

    // Check if the file is writable
    bool can_write_file() const {
        if (!is_regular_file())
            return false;

        return can_write(file_info.open_mode);
    }

    // File operations
    FILE *get_file_pointer() const {
        return wrapped_file.get();
    }

    int close() {
        FILE *const stream = wrapped_file.release();
        return stream ? std::fclose(stream) : 0;
    }

    std::unique_lock<std::mutex> lock_operations() const {
        return std::unique_lock<std::mutex>(*operations_mutex);
    }

    // File functions
    SceOff read(void *input_data, int element_size, SceSize element_count) const;
    SceOff read_unlocked(void *input_data, size_t length) const;
    bool has_read_error_unlocked() const;
    SceOff write(const void *data, SceSize size, int count) const;
    int truncate(const SceSize size) const;
    bool seek(SceOff offset, SceIoSeekMode seek_mode) const;
    SceOff tell() const;
};

// Class for implementing Directory structure; path names are wide for Windows, normal for else
class DirStats : public VitaStats {
    // Shared directory pointer
    std::vector<DirPtr> dir_ptrs;
    size_t dir_index = 0;
    std::unordered_set<std::string> seen_entries;
    std::shared_ptr<void> mount_lease;
    pfsmgr::MountScope mount_scope = pfsmgr::MountScope::Application;

public:
    std::mutex mutex;

    DirStats(const char *vita, const std::string &t, const fs::path &file, std::vector<DirPtr> ptrs) {
        dir_ptrs = std::move(ptrs);

        file_info.vita_loc = vita;
        file_info.translated = t;
        file_info.sys_loc = file;
        file_info.open_mode = SCE_O_RDONLY;
        file_info.file_mode = SCE_SO_IFDIR | SCE_SO_IROTH;
        file_info.access_mode = SCE_S_IFDIR | SCE_S_IRUSR;
    }

    auto get_dir_ptr() {
        using EntryPtr = decltype(get_system_dir_ptr(dir_ptrs.front()));
        while (dir_index < dir_ptrs.size()) {
            if (auto entry = get_system_dir_ptr(dir_ptrs[dir_index]))
                return entry;
            ++dir_index;
        }
        return EntryPtr{};
    }

    bool add_entry(const std::string &name) {
        return seen_entries.insert(name).second;
    }

    void hold_mount(std::shared_ptr<void> lease) { mount_lease = std::move(lease); }
    void set_mount_scope(pfsmgr::MountScope scope) { mount_scope = scope; }
    bool is_shell_directory() const { return mount_scope == pfsmgr::MountScope::Shell; }

    bool is_directory() const {
        return file_info.file_mode & SCE_SO_IFDIR;
    }
};

using TtyFiles = std::map<SceUID, TtyType>;
using StdFiles = std::map<SceUID, std::shared_ptr<FileStats>>;
using DirEntries = std::map<SceUID, std::shared_ptr<DirStats>>;

struct IOState {
    struct DevicePaths {
        std::string app0;
        VitaIoDevice app0_device = VitaIoDevice::_INVALID;
        std::string savedata0;
        std::string addcont0;
        std::string cache0;
        std::string user_id;
    } device_paths;

    std::string addcont;
    std::string content_id;
    std::string savedata;
    std::string title_id;
    std::string app_path;

    std::string user_id;
    std::string user_name;

    bool redirect_stdio;

    mutable std::mutex fd_mutex;
    SceUID next_fd = 0;
    TtyFiles tty_files;
    StdFiles std_files;
    DirEntries dir_entries;

    std::mutex async_mutex;
    bool async_stopping = false;
    std::unordered_map<SceUID, std::shared_ptr<AsyncIoRequest>> async_requests;

    std::unordered_map<std::string, std::string> cachemap;
    bool case_isens_find_enabled = false;

    std::mutex appmeta_mutex;
    std::map<pfsmgr::MountKey, std::string> virtual_pfs_mounts;
    std::string addcont_pfs_mount;
    pfsmgr::Mounts pfs_mounts;
    std::string app_game_base_mount;
    std::string app_game_patch_mount;
    std::string savedata_pfs_mount;
    mutable std::recursive_mutex savedata_mutex;
    std::shared_ptr<SavedataAccess> savedata_access;

    std::mutex overlay_mutex;
    SceUID next_overlay_id = 1;
    SceUID app_overlay_id = -1;
    // overlay in the order they should be applied
    std::vector<FiosOverlay> overlays;
};
