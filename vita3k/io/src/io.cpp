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

#include <io/device.h>
#include <io/functions.h>
#include <io/io.h>
#include <io/state.h>
#include <io/types.h>
#include <io/util.h>
#include <io/vfs.h>

#include <rtc/rtc.h>
#include <util/log.h>
#include <util/preprocessor.h>
#include <util/string_utils.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cassert>
#include <iostream>
#include <iterator>
#include <string>

#if defined(__aarch64__) && defined(__APPLE__)
#define stat64 stat
#endif

// ****************************
// * Utility functions *
// ****************************

static int io_error_impl(const int retval, const char *export_name, const char *func_name) {
    LOG_WARN("{} ({}) returned {}", func_name, export_name, log_hex(retval));
    return retval;
}

#define IO_ERROR(retval) io_error_impl(retval, export_name, __func__)
#define IO_ERROR_UNK() IO_ERROR(-1)

constexpr bool log_file_op = true;
constexpr bool log_file_read = false;
constexpr bool log_file_seek = false;
constexpr bool log_file_stat = false;

namespace vfs {

bool read_file(const VitaIoDevice device, FileBuffer &buf, const fs::path &pref_path, const fs::path &vfs_file_path) {
    const auto host_file_path = device::construct_emulated_path(device, vfs_file_path, pref_path).generic_path();
    return fs_utils::read_data(host_file_path, buf);
}

bool read_app_file(FileBuffer &buf, const fs::path &pref_path, const std::string &app_path, const fs::path &vfs_file_path) {
    const auto app_device = device::get_device(app_path);
    return read_file(app_device, buf, pref_path, device::remove_device_from_path(app_path, app_device) / vfs_file_path);
}

SceSize get_directory_used_size(const VitaIoDevice device, const std::string &vfs_path, const fs::path &pref_path) {
    const auto emuenv_path = device::construct_emulated_path(device, vfs_path, pref_path);

    SceSize total_size = 0;
    for (const auto &entry : fs::recursive_directory_iterator(emuenv_path)) {
        if (fs::is_regular_file(entry.path()))
            total_size += fs::file_size(entry.path());
    }

    return total_size;
}

} // namespace vfs

// ****************************
// * End utility functions *
// ****************************

bool init(IOState &io, const fs::path &cache_path, const fs::path &log_path, const fs::path &pref_path, bool redirect_stdio) {
    // Iterate through the entire list of devices and create the subdirectories if they do not exist
    for (auto i : VitaIoDevice::_names()) {
        if (!device::is_valid_output_path(i))
            continue;
        fs::create_directories(pref_path / i);
    }

    const fs::path ux0{ pref_path / (+VitaIoDevice::ux0)._to_string() };
    const fs::path uma0{ pref_path / (+VitaIoDevice::uma0)._to_string() };
    const fs::path vd0{ pref_path / (+VitaIoDevice::vd0)._to_string() };

    fs::create_directories(ux0 / "data");
    fs::create_directories(ux0 / "app");
    fs::create_directories(ux0 / "cache");
    fs::create_directories(ux0 / "music");
    fs::create_directories(ux0 / "picture");
    fs::create_directories(ux0 / "theme");
    fs::create_directories(ux0 / "video");
    fs::create_directories(ux0 / "user");
    fs::create_directories(uma0 / "data");
    fs::create_directories(vd0 / "registry");
    fs::create_directories(vd0 / "network");

    fs::create_directories(cache_path / "shaders");
    fs::create_directory(log_path / "shaderlog");
    fs::create_directory(log_path / "texturelog");

    io.redirect_stdio = redirect_stdio;

#ifndef _WIN32
    io.case_isens_find_enabled = true;
#endif

    return true;
}

void init_device_paths(IOState &io) {
    io.device_paths.savedata0 = "user/" + io.user_id + "/savedata/" + io.savedata;
    io.device_paths.app0 = device::remove_device_from_path(io.app_path, device::get_device(io.app_path));
    io.device_paths.addcont0 = "addcont/" + io.addcont;
}

bool init_savedata_app_path(IOState &io, const fs::path &pref_path) {
    const fs::path user_id_path{ pref_path / (+VitaIoDevice::ux0)._to_string() / "user" / io.user_id };
    const fs::path savedata_path{ user_id_path / "savedata" };
    const fs::path savedata_game_path{ savedata_path / io.savedata };

    fs::create_directories(user_id_path);
    fs::create_directories(savedata_path);
    fs::create_directories(savedata_game_path);

    return true;
}

bool find_case_isens_path(IOState &io, VitaIoDevice &device, const fs::path &translated_path, const fs::path &system_path) {
    std::string final_path{};

    switch (device) {
    case +VitaIoDevice::app0: {
        std::string app_id = translated_path.string().substr(0, 14);
        final_path = system_path.string().substr(0, system_path.string().find(app_id)) + app_id;
        break;
    }
    case +VitaIoDevice::addcont0: {
        std::string addcont_id = translated_path.string().substr(0, 18);
        final_path = system_path.string().substr(0, system_path.string().find(addcont_id)) + addcont_id;
        break;
    }
    case +VitaIoDevice::vs0: {
        // This only works if ALL the parent folders of the path are the correct case or are in a case insensitive fs
        // Only the file's name is searched for, not the parent folders
        final_path = system_path.string().substr(0, system_path.string().find_last_of('/'));
        break;
    }
    default: {
        return false;
    }
    }

    if (!fs::exists(final_path))
        return false;

    for (const auto &file : fs::recursive_directory_iterator(final_path)) {
        io.cachemap.emplace(string_utils::tolower(file.path().string()), file.path().string());
    }

    return true;
}

fs::path find_in_cache(IOState &io, const std::string &system_path) {
    const auto find_path = io.cachemap.find(system_path);

    if (find_path != io.cachemap.end()) {
        return fs::path{ find_path->second.c_str() };
    } else {
        return fs::path{};
    }
}

std::string convert_path(const std::string &path) {
    auto convert_path = path;

    // replace colon with slash
    string_utils::replace(convert_path, ":", "/");

    return convert_path;
}

std::string translate_path(const char *path, VitaIoDevice &device, const IOState &io) {
    auto relative_path = device::remove_duplicate_device(path, device);

    // replace invalid slashes with proper forward slash
    string_utils::replace(relative_path, "\\", "/");
    string_utils::replace(relative_path, "/./", "/");
    string_utils::replace(relative_path, "//", "/");
    // TODO: Handle dot-dot paths

    switch (device) {
    case +VitaIoDevice::savedata0: // Redirect savedata0: to ux0:user/00/savedata/<title_id>
    case +VitaIoDevice::savedata1: {
        relative_path = device::remove_device_from_path(relative_path, device, io.device_paths.savedata0);
        device = VitaIoDevice::ux0;
        break;
    }
    case +VitaIoDevice::app0: { // Redirect app0: to ux0:app/<title_id>
        relative_path = device::remove_device_from_path(relative_path, device, io.device_paths.app0);
        device = device::get_device(io.app_path);
        break;
    }
    case +VitaIoDevice::addcont0: { // Redirect addcont0: to ux0:addcont/<title_id>
        relative_path = device::remove_device_from_path(relative_path, device, io.device_paths.addcont0);
        device = VitaIoDevice::ux0;
        break;
    }
    case +VitaIoDevice::cache0: { // Redirect cache0: to ux0:cache/<title_id>
        relative_path = device::remove_device_from_path(relative_path, device, "cache/" + io.title_id);
        device = VitaIoDevice::ux0;
        break;
    }
    case +VitaIoDevice::music0: { // Redirect music0: to ux0:music
        relative_path = device::remove_device_from_path(relative_path, device, "music");
        device = VitaIoDevice::ux0;
        break;
    }
    case +VitaIoDevice::photo0: { // Redirect photo0: to ux0:picture
        relative_path = device::remove_device_from_path(relative_path, device, "picture");
        device = VitaIoDevice::ux0;
        break;
    }
    case +VitaIoDevice::video0: { // Redirect video0: to ux0:video
        relative_path = device::remove_device_from_path(relative_path, device, "video");
        device = VitaIoDevice::ux0;
        break;
    }

    case +VitaIoDevice::host0:
    case +VitaIoDevice::grw0:
    case +VitaIoDevice::imc0:
    case +VitaIoDevice::os0:
    case +VitaIoDevice::pd0:
    case +VitaIoDevice::sa0:
    case +VitaIoDevice::sd0:
    case +VitaIoDevice::tm0:
    case +VitaIoDevice::ud0:
    case +VitaIoDevice::uma0:
    case +VitaIoDevice::ur0:
    case +VitaIoDevice::ux0:
    case +VitaIoDevice::vd0:
    case +VitaIoDevice::vs0:
    case +VitaIoDevice::xmc0: {
        relative_path = device::remove_device_from_path(relative_path, device);
        break;
    }
    case +VitaIoDevice::tty0:
    case +VitaIoDevice::tty1:
    case +VitaIoDevice::tty2:
    case +VitaIoDevice::tty3: {
        return std::string{};
    }
    default: {
        LOG_CRITICAL_IF(relative_path.find(':') != std::string::npos, "Unknown device with path {} used. Report this to the developers!", relative_path);
        return std::string{};
    }
    }

    // If the path is empty, the request is the device itself
    if (relative_path.empty())
        return std::string{};

    if (relative_path.front() == '/' || relative_path.front() == '\\')
        relative_path.erase(0, 1);

    return relative_path;
}

fs::path expand_path(IOState &io, const char *path, const fs::path &pref_path) {
    auto device = device::get_device(path);

    const auto translated_path = translate_path(path, device, io);
    return device::construct_emulated_path(device, translated_path, pref_path, io.redirect_stdio).string();
}

SceUID open_file(IOState &io, const char *path, const int flags, const fs::path &pref_path, const char *export_name) {
    auto device = device::get_device(path);
    auto device_for_icase = device;
    if (device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot find device for path: {}", path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    if ((device == VitaIoDevice::tty0) || (device == VitaIoDevice::tty1) || (device == VitaIoDevice::tty2) || (device == VitaIoDevice::tty3)) {
        assert(flags >= 0);

        auto tty_type = TTY_UNKNOWN;
        if (flags & SCE_O_RDONLY)
            tty_type |= TTY_IN;
        if (flags & SCE_O_WRONLY)
            tty_type |= TTY_OUT;

        const auto fd = io.next_fd++;
        io.tty_files.emplace(fd, tty_type);

        LOG_TRACE_IF(log_file_op, "{}: Opening terminal {}:", export_name, device._to_string());
        return fd;
    }

    const auto translated_path = translate_path(path, device, io);
    if (translated_path.empty()) {
        LOG_ERROR("Cannot translate path: {}", path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    auto system_path = device::construct_emulated_path(device, translated_path, pref_path, io.redirect_stdio);
    if (fs::is_directory(system_path)) {
        LOG_ERROR("Cannot open directory: {}", system_path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    // Do not allow any new files if they do not have a write flag.
    if (!fs::exists(system_path)) {
        if (!(flags & SCE_O_CREAT)) {
            if (io.case_isens_find_enabled) {
                // Attempt a case-insensitive file search.
                const auto original_system_path = system_path;
                const auto cached_path = find_in_cache(io, string_utils::tolower(system_path.string()));
                if (!cached_path.empty()) {
                    system_path = cached_path;
                    LOG_TRACE("Found cached filepath at {}", system_path);
                } else {
                    const bool path_found = find_case_isens_path(io, device_for_icase, translated_path, system_path);
                    system_path = find_in_cache(io, string_utils::tolower(system_path.string()));
                    if (!system_path.empty() && path_found) {
                        LOG_TRACE("Found file on case-sensitive filesystem at {}", system_path);
                    } else {
                        LOG_ERROR("Missing file at {} (target path: {})", original_system_path, path);
                        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
                    }
                }
            } else {
                LOG_ERROR("Missing file at {} (target path: {})", system_path, path);
                return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
            }
        } else {
            if (!fs::exists(system_path.parent_path())) {
                fs::create_directories(system_path.parent_path());
            }
            fs::ofstream file(system_path);
        }
    }

    const auto normalized_path = device::construct_normalized_path(device, translated_path);

    FileStats f{ path, normalized_path, system_path, flags };
    const auto fd = io.next_fd++;
    io.std_files.emplace(fd, f);

    LOG_TRACE_IF(log_file_op, "{}: Opening file {} ({}), fd: {}", export_name, path, normalized_path, log_hex(fd));
    return fd;
}

int read_file(void *data, IOState &io, const SceUID fd, const SceSize size, const char *export_name) {
    assert(data != nullptr);
    assert(size >= 0);

    const auto file = io.std_files.find(fd);
    if (file != io.std_files.end()) {
        const auto read = file->second.read(data, 1, size);
        LOG_TRACE_IF(log_file_op && log_file_read, "{}: Reading {} bytes of fd {}", export_name, read, log_hex(fd));
        return static_cast<int>(read);
    }

    const auto tty_file = io.tty_files.find(fd);
    if (tty_file != io.tty_files.end()) {
        if (tty_file->second == TTY_IN) {
            std::cin.read(static_cast<char *>(data), size);
            LOG_TRACE_IF(log_file_op && log_file_read, "{}: Reading terminal fd: {}, size: {}", export_name, log_hex(fd), size);
            return size;
        }
        return IO_ERROR_UNK();
    }

    return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
}

int write_file(SceUID fd, const void *data, const SceSize size, const IOState &io, const char *export_name) {
    assert(data != nullptr);
    assert(size >= 0);

    if (fd < 0) {
        LOG_WARN("Error writing fd: {}, size: {}", log_hex(fd), size);
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    }

    const auto tty_file = io.tty_files.find(fd);
    if (tty_file != io.tty_files.end()) {
        if (tty_file->second & TTY_OUT) {
            std::string s(static_cast<char const *>(data), size);

            // trim newline
            if (io.redirect_stdio) {
                std::cout << s;
            } else {
                if (s.back() == '\n')
                    s.pop_back();
                LOG_TRACE_IF(log_file_op, "*** TTY: {}", s);
            }

            return size;
        }
        return IO_ERROR_UNK();
    }

    const auto file = io.std_files.find(fd);
    if (file == io.std_files.end())
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

    if (!fs::is_directory(file->second.get_system_location().parent_path())) {
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT); // TODO: Is it the right error code?
    }

    if (file->second.can_write_file()) {
        const auto written = file->second.write(data, 1, size);
        LOG_TRACE_IF(log_file_op, "{}: Writing to fd: {}, size: {}", export_name, log_hex(fd), size);
        return static_cast<int>(written);
    }

    return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
}

int truncate_file(const SceUID fd, unsigned long long length, const IOState &io, const char *export_name) {
    if (fd < 0)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

    const auto file = io.std_files.find(fd);
    if (file == io.std_files.end())
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    auto trunc = file->second.truncate(length);
    LOG_TRACE_IF(log_file_op, "{}: Truncating fd: {}, to size: {}", export_name, log_hex(fd), length);
    return trunc;
}

SceOff seek_file(const SceUID fd, const SceOff offset, const SceIoSeekMode whence, IOState &io, const char *export_name) {
    if (!(whence == SCE_SEEK_SET || whence == SCE_SEEK_CUR || whence == SCE_SEEK_END))
        return IO_ERROR(SCE_ERROR_ERRNO_EOPNOTSUPP);

    if (fd < 0)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

    const auto file = io.std_files.find(fd);
    if (file == io.std_files.end())
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    if (!file->second.seek(offset, whence))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

    const auto log_mode = [](const SceIoSeekMode whence) -> const char * {
        switch (whence) {
            STR_CASE(SCE_SEEK_SET);
            STR_CASE(SCE_SEEK_CUR);
            STR_CASE(SCE_SEEK_END);
        default:
            return "INVALID";
        }
    };

    LOG_TRACE_IF(log_file_op && log_file_seek, "{}: Seeking fd: {}, offset: {}, whence: {}", export_name, log_hex(fd), log_hex(offset), log_mode(whence));
    return file->second.tell();
}

SceOff tell_file(IOState &io, const SceUID fd, const char *export_name) {
    if (fd < 0)
        return IO_ERROR(SCE_ERROR_ERRNO_EMFILE);

    const auto std_file = io.std_files.find(fd);

    if (std_file == io.std_files.end()) {
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    }

    return std_file->second.tell();
}

int stat_file(IOState &io, const char *file, SceIoStat *statp, const fs::path &pref_path, const char *export_name, const SceUID fd) {
    assert(statp != nullptr);

    memset(statp, '\0', sizeof(SceIoStat));

    fs::path file_path = "";
    if (fd == invalid_fd) {
        auto device = device::get_device(file);
        auto device_for_icase = device;
        if (device == VitaIoDevice::_INVALID) {
            LOG_ERROR("Cannot find device for path: {}", file);
            return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
        }

        const auto translated_path = translate_path(file, device, io);
        file_path = device::construct_emulated_path(device, translated_path, pref_path, io.redirect_stdio);

        if (!fs::exists(file_path)) {
            if (io.case_isens_find_enabled) {
                // Attempt a case-insensitive file search.
                const auto original_file_path = file_path;
                const auto cached_path = find_in_cache(io, string_utils::tolower(file_path.string()));
                if (!cached_path.empty()) {
                    file_path = cached_path;
                    LOG_TRACE("Found cached filepath at {}", file_path);
                } else {
                    const bool path_found = find_case_isens_path(io, device_for_icase, translated_path, file_path);
                    file_path = find_in_cache(io, string_utils::tolower(file_path.string()));
                    if (!file_path.empty() && path_found) {
                        LOG_TRACE("Found file on case-sensitive filesystem at {}", file_path);
                    } else {
                        LOG_ERROR("Missing file at {} (target path: {})", original_file_path, file);
                        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
                    }
                }
            } else {
                LOG_ERROR("Missing file at {} (target path: {})", file_path, file);
                return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
            }
        }
        LOG_TRACE_IF(log_file_op && log_file_stat, "{}: Statting file: {} ({})", export_name, file, device::construct_normalized_path(device, translated_path));
    } else { // We have previously opened and defined the location
        const auto fd_file = io.std_files.find(fd);
        if (fd_file == io.std_files.end())
            return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

        file_path = fd_file->second.get_system_location();
        LOG_TRACE_IF(log_file_op && log_file_stat, "{}: Statting fd: {}", export_name, log_hex(fd));

        statp->st_attr = fd_file->second.get_file_mode();
    }

    std::uint64_t last_access_time_ticks;
    std::uint64_t creation_time_ticks;
    std::uint64_t last_modification_time_ticks;

#ifdef _WIN32
    struct _stati64 sb;
    if (_wstati64(file_path.generic_path().wstring().c_str(), &sb) < 0)
        return IO_ERROR_UNK();
#else
    struct stat64 sb;
    if (stat64(file_path.generic_path().string().c_str(), &sb) < 0)
        return IO_ERROR_UNK();
#endif

    last_access_time_ticks = (uint64_t)sb.st_atime * VITA_CLOCKS_PER_SEC;
    creation_time_ticks = (uint64_t)sb.st_ctime * VITA_CLOCKS_PER_SEC;
    last_modification_time_ticks = (uint64_t)sb.st_mtime * VITA_CLOCKS_PER_SEC;

#ifndef _WIN32
#undef st_atime
#undef st_mtime
#undef st_ctime
#endif

    statp->st_mode = SCE_S_IRUSR | SCE_S_IRGRP | SCE_S_IROTH | SCE_S_IXUSR | SCE_S_IXGRP | SCE_S_IXOTH;

    if (fs::is_regular_file(file_path)) {
        statp->st_size = fs::file_size(file_path);
        statp->st_attr = SCE_SO_IFREG;
        statp->st_mode |= SCE_S_IFREG;
    }
    if (fs::is_directory(file_path)) {
        statp->st_attr = SCE_SO_IFDIR;
        statp->st_mode |= SCE_S_IFDIR;
    }

    __RtcTicksToPspTime(&statp->st_atime, last_access_time_ticks);
    __RtcTicksToPspTime(&statp->st_mtime, last_modification_time_ticks);
    __RtcTicksToPspTime(&statp->st_ctime, creation_time_ticks);

    return 0;
}

int stat_file_by_fd(IOState &io, const SceUID fd, SceIoStat *statp, const fs::path &pref_path, const char *export_name) {
    assert(statp != nullptr);
    memset(statp, '\0', sizeof(SceIoStat));

    const auto std_file = io.std_files.find(fd);
    if (std_file == io.std_files.end()) {
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    }

    return stat_file(io, std_file->second.get_vita_loc(), statp, pref_path, export_name, fd);
}

int close_file(IOState &io, const SceUID fd, const char *export_name) {
    if (fd < 0)
        return IO_ERROR(SCE_ERROR_ERRNO_EMFILE);

    LOG_TRACE_IF(log_file_op, "{}: Closing file fd: {}", export_name, log_hex(fd));

    io.tty_files.erase(fd);
    io.std_files.erase(fd);

    return 0;
}

int remove_file(IOState &io, const char *file, const fs::path &pref_path, const char *export_name) {
    auto device = device::get_device(file);
    if (device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot find device for path: {}", file);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto translated_path = translate_path(file, device, io);
    if (translated_path.empty()) {
        LOG_ERROR("Cannot translate path: {}", translated_path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto emulated_path = device::construct_emulated_path(device, translated_path, pref_path, io.redirect_stdio);
    if (!fs::exists(emulated_path) || fs::is_directory(emulated_path)) {
        LOG_ERROR("File does not exist at path: {} (target path: {})", emulated_path, file);
    }

    LOG_TRACE_IF(log_file_op, "{}: Removing file {} ({})", export_name, file, device::construct_normalized_path(device, translated_path));

    boost::system::error_code error_code{};
    auto res = fs::detail::remove(emulated_path, &error_code);

    if (!(res && !(error_code.value()))) {
        LOG_ERROR("Cannot remove file: {} ({})", file, device::construct_normalized_path(device, translated_path));
        LOG_ERROR("Error code: {} ({})", error_code.value(), error_code.message());
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    return 0;
}

int rename(IOState &io, const char *old_name, const char *new_name, const fs::path &pref_path, const char *export_name) {
    auto device = device::get_device(old_name);
    if (device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot find device for path: {}", old_name);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto translated_old_path = translate_path(old_name, device, io);
    if (translated_old_path.empty()) {
        LOG_ERROR("Cannot translate path: {}", translated_old_path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto translated_new_path = translate_path(new_name, device, io);
    if (translated_new_path.empty()) {
        LOG_ERROR("Cannot translate path: {}", translated_new_path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto emulated_old_path = device::construct_emulated_path(device, translated_old_path, pref_path, io.redirect_stdio);
    if (!fs::exists(emulated_old_path)) {
        LOG_ERROR("File does not exist at path: {} (target path: {})", emulated_old_path, old_name);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto emulated_new_path = device::construct_emulated_path(device, translated_new_path, pref_path, io.redirect_stdio);

    LOG_TRACE_IF(log_file_op, "{}: Renaming file {} to {} ({} to {})", export_name, old_name, new_name, emulated_old_path, emulated_new_path);

    boost::system::error_code error_code{};
    fs::rename(emulated_old_path, emulated_new_path, error_code);

    if (error_code.value()) {
        LOG_ERROR("Cannot rename file: {} to {} ({} to {})", old_name, new_name, emulated_old_path, emulated_new_path);
        LOG_ERROR("Error code: {} ({})", error_code.value(), error_code.message());
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    return 0;
}

SceUID open_dir(IOState &io, const char *path, const fs::path &pref_path, const char *export_name) {
    auto device = device::get_device(path);
    auto device_for_icase = device;
    const auto translated_path = translate_path(path, device, io);

    auto dir_path = device::construct_emulated_path(device, translated_path, pref_path, io.redirect_stdio) / "";
    if (!fs::exists(dir_path)) {
        if (io.case_isens_find_enabled) {
            // Attempt a case-insensitive file search.
            const auto original_dir_path = dir_path;
            const auto cached_path = find_in_cache(io, string_utils::tolower(dir_path.string()));
            if (!cached_path.empty()) {
                dir_path = cached_path;
                LOG_TRACE("Found cached directory path at {}", dir_path);
            } else {
                const bool path_found = find_case_isens_path(io, device_for_icase, translated_path, dir_path);
                dir_path = find_in_cache(io, string_utils::tolower(dir_path.string().substr(0, dir_path.string().size() - 1)));
                if (!dir_path.empty() && path_found) {
                    LOG_TRACE("Found directory on case-sensitive filesystem at {}", dir_path);
                } else {
                    LOG_ERROR("Directory does not exist at {} (target path: {})", original_dir_path, path);
                    return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
                }
            }
        } else {
            LOG_ERROR("Directory does not exist at: {} (target path: {})", dir_path, path);
            return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
        }
    }

    const DirPtr opened = create_shared_dir(dir_path);
    if (!opened) {
        LOG_ERROR("Failed to open directory at: {} (target path: {})", dir_path, path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto normalized = device::construct_normalized_path(device, translated_path);
    const DirStats d{ path, normalized, dir_path, opened };
    const auto fd = io.next_fd++;
    io.dir_entries.emplace(fd, d);

    LOG_TRACE_IF(log_file_op, "{}: Opening dir {} ({}), fd: {}", export_name, path, normalized, log_hex(fd));

    return fd;
}

SceUID read_dir(IOState &io, const SceUID fd, SceIoDirent *dent, const fs::path &pref_path, const char *export_name) {
    assert(dent != nullptr);

    memset(dent->d_name, '\0', sizeof(dent->d_name));

    const auto dir = io.dir_entries.find(fd);

    if (dir != io.dir_entries.end()) {
        // Refuse any fd that is not explicitly a directory
        if (!dir->second.is_directory())
            return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

        const auto d = dir->second.get_dir_ptr();
        if (!d)
            return 0;

        const auto d_name_utf8 = get_file_in_dir(d);
        strncpy(dent->d_name, d_name_utf8.c_str(), sizeof(dent->d_name));

        const auto cur_path = dir->second.get_system_location() / d_name_utf8;
        if (!(cur_path.filename_is_dot() || cur_path.filename_is_dot_dot())) {
            const auto file_path = std::string(dir->second.get_vita_loc()) + '/' + d_name_utf8;

            LOG_TRACE_IF(log_file_op, "{}: Reading entry {} of fd: {}", export_name, file_path, log_hex(fd));
            if (stat_file(io, file_path.c_str(), &dent->d_stat, pref_path, export_name) < 0)
                return IO_ERROR(SCE_ERROR_ERRNO_EMFILE);
            else
                return 1; // move to the next file
        }
        return read_dir(io, fd, dent, pref_path, export_name);
    }

    return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
}

bool copy_directories(const fs::path &src_path, const fs::path &dst_path) {
    try {
        fs::create_directories(dst_path);

        for (const auto &src : fs::recursive_directory_iterator(src_path)) {
            const auto dst_parent_path = dst_path / fs::relative(src, src_path).parent_path();
            const auto dst_path = dst_parent_path / src.path().filename();

            LOG_INFO("Copy {}", dst_path);

            if (fs::is_regular_file(src))
                fs::copy_file(src, dst_path, fs::copy_options::overwrite_existing);
            else
                fs::create_directories(dst_path);
        }

        return true;
    } catch (std::exception &e) {
        std::cout << e.what();
        return false;
    }
}

bool copy_path(const fs::path &src_path, const fs::path &pref_path, const std::string &app_title_id, const std::string &app_category) {
    // Check if is path
    if (app_category.find("gp") != std::string::npos) {
        const auto app_path{ pref_path / "ux0/app" / app_title_id };
        const auto result = copy_directories(src_path, app_path);

        fs::remove_all(src_path);

        return result;
    }

    return true;
}

int create_dir(IOState &io, const char *dir, int mode, const fs::path &pref_path, const char *export_name, const bool recursive) {
    auto device = device::get_device(dir);
    const auto translated_path = translate_path(dir, device, io);
    if (translated_path.empty()) {
        LOG_ERROR("Failed to translate path: {}", dir);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto emulated_path = device::construct_emulated_path(device, translated_path, pref_path, io.redirect_stdio);
    if (recursive)
        return fs::create_directories(emulated_path);
    if (fs::exists(emulated_path))
        return IO_ERROR(SCE_ERROR_ERRNO_EEXIST);

    const auto parent_path = fs::path(emulated_path).remove_trailing_separator().parent_path();
    if (!fs::exists(parent_path)) // Vita cannot recursively create directories
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);

    LOG_TRACE_IF(log_file_op, "{}: Creating new dir {} ({})", export_name, dir, device::construct_normalized_path(device, translated_path));

    if (!fs::create_directory(emulated_path)) {
        LOG_ERROR("Failed to create directory at {} (target path: {})", emulated_path, dir);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    return 0;
}

int close_dir(IOState &io, const SceUID fd, const char *export_name) {
    if (fd < 0)
        return IO_ERROR(SCE_ERROR_ERRNO_EMFILE);

    const auto erased_entries = io.dir_entries.erase(fd);

    LOG_TRACE_IF(log_file_op, "{}: Closing dir fd: {}", export_name, log_hex(fd));

    if (erased_entries == 0)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

    return 0;
}

int remove_dir(IOState &io, const char *dir, const fs::path &pref_path, const char *export_name) {
    auto device = device::get_device(dir);
    if (device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot find device for path: {}", dir);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto translated_path = translate_path(dir, device, io);
    if (translated_path.empty()) {
        LOG_ERROR("Cannot translate path: {}", dir);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    LOG_TRACE_IF(log_file_op, "{}: Removing dir {} ({})", export_name, dir, device::construct_normalized_path(device, translated_path));

    if (!fs::remove_all(device::construct_emulated_path(device, translated_path, pref_path, io.redirect_stdio))) {
        LOG_ERROR("Cannot remove dir: {} ({})", dir, device::construct_normalized_path(device, translated_path));
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    return 0;
}

static std::string standardize_path(std::string_view path) {
    // replace app0:... by app0:/...
    bool start_with_app0 = path.starts_with("app0:");
    if (start_with_app0 && path.size() >= 6 && path[5] != '/')
        return "app0:/" + std::string(path.substr(5));
    else
        return std::string(path);
}

SceUID create_overlay(IOState &io, SceFiosProcessOverlay *fios_overlay) {
    std::lock_guard<std::mutex> lock(io.overlay_mutex);

    FiosOverlay overlay{
        .id = io.next_overlay_id++,
        .type = fios_overlay->type,
        .order = fios_overlay->order,
        .process_id = fios_overlay->process_id,
        .dst = standardize_path(fios_overlay->dst),
        .src = standardize_path(fios_overlay->src)
    };

    // find location where to put it
    size_t overlay_index = 0;
    // lower order first and in case of equality, last one inserted first
    while (overlay_index < io.overlays.size() && overlay.order < io.overlays[overlay_index].order)
        overlay_index++;
    auto res = overlay.id;
    io.overlays.insert(io.overlays.begin() + overlay_index, std::move(overlay));

    return res;
}

std::string resolve_path(IOState &io, const char *input, const SceUInt32 min_order, const SceUInt32 max_order) {
    std::lock_guard<std::mutex> lock(io.overlay_mutex);

    std::string curr_path = input;

    size_t overlay_idx = 0;
    while (overlay_idx < io.overlays.size() && io.overlays[overlay_idx].order < min_order)
        overlay_idx++;

    while (overlay_idx < io.overlays.size()) {
        const FiosOverlay &overlay = io.overlays[overlay_idx];
        overlay_idx++;

        if (overlay.order > max_order)
            break;

        if (!curr_path.starts_with(overlay.dst))
            continue;

        // replace dst with src
        curr_path = overlay.src + curr_path.substr(overlay.dst.size());
    }

    return curr_path;
}
