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

#ifdef WIN32
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

namespace vfs {

bool read_file(const VitaIoDevice device, FileBuffer &buf, const std::wstring &pref_path, const fs::path &vfs_file_path) {
    const auto host_file_path = device::construct_emulated_path(device, vfs_file_path, pref_path).generic_path();

    fs::ifstream f{ host_file_path, fs::ifstream::binary };
    if (!f)
        return false;

    f.unsetf(fs::ifstream::skipws);
    buf.reserve(fs::file_size(host_file_path));
    buf.insert(buf.begin(), std::istream_iterator<uint8_t>(f), std::istream_iterator<uint8_t>());
    return true;
}

bool read_app_file(FileBuffer &buf, const std::wstring &pref_path, const std::string &app_path, const fs::path &vfs_file_path) {
    return read_file(VitaIoDevice::ux0, buf, pref_path, fs::path("app") / app_path / vfs_file_path);
}

SpaceInfo get_space_info(const VitaIoDevice device, const std::string &vfs_path, const std::wstring &pref_path) {
    SpaceInfo space_info;
    const auto host_path = device::construct_emulated_path(device, vfs_path, pref_path);
    space_info.max_capacity = fs::space(host_path).capacity;
    space_info.free = fs::space(host_path).available;
    space_info.used = fs::space(host_path).capacity - space_info.free;
    return space_info;
}

} // namespace vfs

// ****************************
// * End utility functions *
// ****************************

bool init(IOState &io, const fs::path &base_path, const fs::path &pref_path, bool redirect_stdio) {
    // Iterate through the entire list of devices and create the subdirectories if they do not exist
    for (auto i : VitaIoDevice::_names()) {
        if (!device::is_valid_output_path(i))
            continue;
        if (!fs::exists(pref_path / i))
            fs::create_directories(pref_path / i);
    }

    const fs::path ux0{ pref_path / (+VitaIoDevice::ux0)._to_string() };
    const fs::path uma0{ pref_path / (+VitaIoDevice::uma0)._to_string() };
    const fs::path ux0_data{ ux0 / "data" };
    const fs::path uma0_data{ uma0 / "data" };
    const fs::path ux0_app{ ux0 / "app" };
    const fs::path ux0_theme{ ux0 / "theme" };
    const fs::path ux0_user{ ux0 / "user" };

    if (!fs::exists(ux0))
        fs::create_directories(ux0);
    if (!fs::exists(ux0_data))
        fs::create_directory(ux0_data);
    if (!fs::exists(ux0_app))
        fs::create_directory(ux0_app);
    if (!fs::exists(ux0_theme))
        fs::create_directory(ux0_theme);
    if (!fs::exists(ux0_user))
        fs::create_directory(ux0_user);
    if (!fs::exists(uma0))
        fs::create_directory(uma0);
    if (!fs::exists(uma0_data))
        fs::create_directory(uma0_data);

    fs::create_directories(base_path / "cache/shaders");
    fs::create_directory(base_path / "shaderlog");
    fs::create_directory(base_path / "texturelog");

    io.redirect_stdio = redirect_stdio;

#ifndef WIN32
    io.case_isens_find_enabled = true;
#endif

    return true;
}

void init_device_paths(IOState &io) {
    io.device_paths.savedata0 = "user/" + io.user_id + "/savedata/" + io.savedata;
    io.device_paths.app0 = "app/" + io.app_path;
    io.device_paths.addcont0 = "addcont/" + io.addcont;
}

bool init_savedata_app_path(IOState &io, const fs::path &pref_path) {
    const fs::path user_id_path{ pref_path / (+VitaIoDevice::ux0)._to_string() / "user" / io.user_id };
    const fs::path savedata_path{ user_id_path / "savedata" };
    const fs::path savedata_game_path{ savedata_path / io.savedata };

    if (!fs::exists(user_id_path))
        fs::create_directory(user_id_path);
    if (!fs::exists(savedata_path))
        fs::create_directory(savedata_path);
    if (!fs::exists(savedata_game_path))
        fs::create_directory(savedata_game_path);

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
    default: {
        return false;
    }
    }

    for (const auto &file : fs::recursive_directory_iterator(final_path)) {
        io.cachemap.insert(std::make_pair(string_utils::tolower(file.path().string()), file.path().string()));
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

std::string translate_path(const char *path, VitaIoDevice &device, const IOState::DevicePaths &device_paths) {
    auto relative_path = device::remove_duplicate_device(path, device);

    // replace invalid slashes with proper forward slash
    string_utils::replace(relative_path, "\\", "/");
    string_utils::replace(relative_path, "/./", "/");
    string_utils::replace(relative_path, "//", "/");
    // TODO: Handle dot-dot paths

    switch (device) {
    case +VitaIoDevice::savedata0: // Redirect savedata0: to ux0:user/00/savedata/<title_id>
    case +VitaIoDevice::savedata1: {
        relative_path = device::remove_device_from_path(relative_path, device, device_paths.savedata0);
        device = VitaIoDevice::ux0;
        break;
    }
    case +VitaIoDevice::app0: { // Redirect app0: to ux0:app/<title_id>
        relative_path = device::remove_device_from_path(relative_path, device, device_paths.app0);
        device = VitaIoDevice::ux0;
        break;
    }
    case +VitaIoDevice::addcont0: { // Redirect addcont0: to ux0:addcont/<title_id>
        relative_path = device::remove_device_from_path(relative_path, device, device_paths.addcont0);
        device = VitaIoDevice::ux0;
        break;
    }
    case +VitaIoDevice::host0:
    case +VitaIoDevice::gro0:
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
    case +VitaIoDevice::tty1: {
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

std::string expand_path(IOState &io, const char *path, const std::wstring &pref_path) {
    auto device = device::get_device(path);

    const auto translated_path = translate_path(path, device, io.device_paths);
    return device::construct_emulated_path(device, translated_path, pref_path, io.redirect_stdio).string();
}

SceUID open_file(IOState &io, const char *path, const int flags, const std::wstring &pref_path, const char *export_name) {
    auto device = device::get_device(path);
    auto device_for_icase = device;
    if (device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot find device for path: {}", path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    if (device == VitaIoDevice::tty0 || device == VitaIoDevice::tty1) {
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

    const auto translated_path = translate_path(path, device, io.device_paths);
    if (translated_path.empty()) {
        LOG_ERROR("Cannot translate path: {}", path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    auto system_path = device::construct_emulated_path(device, translated_path, pref_path, io.redirect_stdio);
    // Do not allow any new files if they do not have a write flag.
    if (!fs::exists(system_path) && !can_write(flags)) {
        if (io.case_isens_find_enabled) {
            // Attempt a case-insensitive file search.
            const auto original_system_path = system_path;
            const auto cached_path = find_in_cache(io, string_utils::tolower(system_path.string()));
            if (!cached_path.empty()) {
                system_path = cached_path;
                LOG_TRACE("Found cached filepath at {}", system_path.string());
            } else {
                const bool path_found = find_case_isens_path(io, device_for_icase, translated_path, system_path);
                system_path = find_in_cache(io, string_utils::tolower(system_path.string()));
                if (!system_path.empty() && path_found) {
                    LOG_TRACE("Found file on case-sensitive filesystem at {}", system_path.string());
                } else {
                    LOG_ERROR("Missing file at {} (target path: {})", original_system_path.string(), path);
                    return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
                }
            }
        } else {
            LOG_ERROR("Missing file at {} (target path: {})", system_path.string(), path);
            return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
        }
    } else if (!fs::exists(system_path) && (flags & SCE_O_CREAT)) {
        if (!fs::exists(system_path.parent_path())) {
            fs::create_directories(system_path.parent_path());
        }
        std::ofstream file(system_path.string());
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
        LOG_TRACE_IF(log_file_op, "{}: Reading {} bytes of fd {}", export_name, read, log_hex(fd));
        return static_cast<int>(read);
    }

    const auto tty_file = io.tty_files.find(fd);
    if (tty_file != io.tty_files.end()) {
        if (tty_file->second == TTY_IN) {
            std::cin.read(reinterpret_cast<char *>(data), size);
            LOG_TRACE_IF(log_file_op, "{}: Reading terminal fd: {}, size: {}", export_name, log_hex(fd), size);
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
        LOG_WARN("Error writing fd: {}, size: {}", fd, size);
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    }

    const auto tty_file = io.tty_files.find(fd);
    if (tty_file != io.tty_files.end()) {
        if (tty_file->second & TTY_OUT) {
            std::string s(reinterpret_cast<char const *>(data), size);

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

    LOG_TRACE_IF(log_file_op, "{}: Seeking fd: {}, offset: {}, whence: {}", export_name, log_hex(fd), log_hex(offset), log_mode(whence));
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

int stat_file(IOState &io, const char *file, SceIoStat *statp, const std::wstring &pref_path, const char *export_name,
    const SceUID fd) {
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

        const auto translated_path = translate_path(file, device, io.device_paths);
        file_path = device::construct_emulated_path(device, translated_path, pref_path, io.redirect_stdio);

        if (!fs::exists(file_path)) {
            if (io.case_isens_find_enabled) {
                // Attempt a case-insensitive file search.
                const auto original_file_path = file_path;
                const auto cached_path = find_in_cache(io, string_utils::tolower(file_path.string()));
                if (!cached_path.empty()) {
                    file_path = cached_path;
                    LOG_TRACE("Found cached filepath at {}", file_path.string());
                } else {
                    const bool path_found = find_case_isens_path(io, device_for_icase, translated_path, file_path);
                    file_path = find_in_cache(io, string_utils::tolower(file_path.string()));
                    if (!file_path.empty() && path_found) {
                        LOG_TRACE("Found file on case-sensitive filesystem at {}", file_path.string());
                    } else {
                        LOG_ERROR("Missing file at {} (target path: {})", original_file_path.string(), file);
                        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
                    }
                }
            } else {
                LOG_ERROR("Missing file at {} (target path: {})", file_path.string(), file);
                return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
            }
        }
        LOG_TRACE_IF(log_file_op, "{}: Statting file: {} ({})", export_name, file, device::construct_normalized_path(device, translated_path));
    } else { // We have previously opened and defined the location
        const auto fd_file = io.std_files.find(fd);
        if (fd_file == io.std_files.end())
            return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

        file_path = fd_file->second.get_system_location();
        LOG_TRACE_IF(log_file_op, "{}: Statting fd: {}", export_name, log_hex(fd));

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

#ifndef WIN32
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

int stat_file_by_fd(IOState &io, const SceUID fd, SceIoStat *statp, const std::wstring &pref_path, const char *export_name) {
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

int remove_file(IOState &io, const char *file, const std::wstring &pref_path, const char *export_name) {
    auto device = device::get_device(file);
    if (device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot find device for path: {}", file);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto translated_path = translate_path(file, device, io.device_paths);
    if (translated_path.empty()) {
        LOG_ERROR("Cannot translate path: {}", translated_path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto emulated_path = device::construct_emulated_path(device, translated_path, pref_path, io.redirect_stdio);
    if (!fs::exists(emulated_path) || fs::is_directory(emulated_path)) {
        LOG_ERROR("File does not exist at path: {} (target path: {})", emulated_path.string(), file);
    }

    LOG_TRACE_IF(log_file_op, "{}: Removing file {} ({})", export_name, file, device::construct_normalized_path(device, translated_path));

    if (!fs::remove(emulated_path)) {
        LOG_ERROR("Cannot remove file: {} ({})", file, device::construct_normalized_path(device, translated_path));
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    return 0;
}

SceUID open_dir(IOState &io, const char *path, const std::wstring &pref_path, const char *export_name) {
    auto device = device::get_device(path);
    auto device_for_icase = device;
    const auto translated_path = translate_path(path, device, io.device_paths);

    auto dir_path = device::construct_emulated_path(device, translated_path, pref_path, io.redirect_stdio) / "/";
    if (!fs::exists(dir_path)) {
        if (io.case_isens_find_enabled) {
            // Attempt a case-insensitive file search.
            const auto original_dir_path = dir_path;
            const auto cached_path = find_in_cache(io, string_utils::tolower(dir_path.string()));
            if (!cached_path.empty()) {
                dir_path = cached_path;
                LOG_TRACE("Found cached directory path at {}", dir_path.string());
            } else {
                const bool path_found = find_case_isens_path(io, device_for_icase, translated_path, dir_path);
                dir_path = find_in_cache(io, string_utils::tolower(dir_path.string().substr(0, dir_path.string().size() - 1)));
                if (!dir_path.empty() && path_found) {
                    LOG_TRACE("Found directory on case-sensitive filesystem at {}", dir_path.string());
                } else {
                    LOG_ERROR("Directory does not exist at {} (target path: {})", original_dir_path.string(), path);
                    return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
                }
            }
        } else {
            LOG_ERROR("Directory does not exist at: {} (target path: {})", dir_path.string(), path);
            return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
        }
    }

    const DirPtr opened = create_shared_dir(dir_path);
    if (!opened) {
        LOG_ERROR("Failed to open directory at: {} (target path: {})", dir_path.string(), path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto normalized = device::construct_normalized_path(device, translated_path);
    const DirStats d{ path, normalized, dir_path, opened };
    const auto fd = io.next_fd++;
    io.dir_entries.emplace(fd, d);

    LOG_TRACE_IF(log_file_op, "{}: Opening dir {} ({}), fd: {}", export_name, path, normalized, log_hex(fd));

    return fd;
}

SceUID read_dir(IOState &io, const SceUID fd, SceIoDirent *dent, const std::wstring &pref_path, const char *export_name) {
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

int create_dir(IOState &io, const char *dir, int mode, const std::wstring &pref_path, const char *export_name, const bool recursive) {
    auto device = device::get_device(dir);
    const auto translated_path = translate_path(dir, device, io.device_paths);
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
        LOG_ERROR("Failed to create directory at {} (target path: {})", emulated_path.string(), dir);
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

int remove_dir(IOState &io, const char *dir, const std::wstring &pref_path, const char *export_name) {
    auto device = device::get_device(dir);
    if (device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot find device for path: {}", dir);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto translated_path = translate_path(dir, device, io.device_paths);
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
