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

#include "rtc/rtc.h"

#include <io/functions.h>
#include <io/io.h>

#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>

#include <io/state.h>
#include <io/types.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/preprocessor.h>

#include <psp2/io/fcntl.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dirent.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <tuple>
#include <type_traits>

// ****************************
// * Utility functions *
// ****************************

int io_error_impl(int retval, const char *export_name, const char *func_name) {
    LOG_WARN("{} ({}) returned {}", func_name, export_name, log_hex(retval));
    return retval;
}

#define IO_ERROR(retval) io_error_impl(retval, export_name, __func__)
#define IO_ERROR_UNK() IO_ERROR(-1)

namespace vfs {

const char *
translate_open_mode(int flags) {
    if (flags & SCE_O_WRONLY) {
        if (flags & SCE_O_RDONLY) {
            if (flags & SCE_O_CREAT) {
                if (flags & SCE_O_APPEND) {
                    return "ab+";
                } else {
                    return "wb+";
                }
            } else {
                return "rb+";
            }
        } else {
            if (flags & SCE_O_APPEND) {
                return "ab";
            } else {
                return "wb";
            }
        }
    } else {
        return "rb";
    }
}

VitaIoDevice
get_device(const std::string &path) {
    // find which IO device the path is associated with
    const std::string &device_name(path);

#define DEVICE(path, name)   \
    if (device_name == path) \
        return JOIN_DEVICE(name);
#include <io/VitaIoDevice.def>
#undef DEVICE

    LOG_CRITICAL("Unknown device {} used. Report this to developers!", path);
    return VitaIoDevice::_UNKNOWN;
}

std::string
remove_colon(const std::string device_string) {
    if (!device_string.empty()) {
        std::string temp = device_string;
        temp.erase(temp.end() - 1);
        return temp;
    } else {
        return device_string;
    }
}

// ****************************
// * End of utility functions *
// ****************************

bool read_file(FileBuffer &buf, const BOOST_FSPATH &pref_path, const std::string &file_path) {
    BOOST_FSPATH host_file_path{ pref_path / file_path };

    fs::ifstream f(host_file_path, std::ios_base::binary);
    if (f.fail()) {
        return false;
    }
    f.unsetf(std::ios::skipws);
    std::streampos size;
    f.seekg(0, std::ios::end);
    size = f.tellg();
    f.seekg(0, std::ios::beg);
    buf.reserve(size);
    buf.insert(buf.begin(), std::istream_iterator<uint8_t>(f), std::istream_iterator<uint8_t>());
    return true;
}

bool read_app_file(FileBuffer &buf, const BOOST_FSPATH &pref_path, const std::string title_id, const std::string &vfs_file_path) {
    std::string game_file_path = "ux0/app/" + title_id + "/" + vfs_file_path;
    return read_file(buf, pref_path, game_file_path);
}

} // namespace vfs

bool init(IOState &io, BOOST_FSPATH &pref_path) {
    std::string ux0 = pref_path.string();
    std::string uma0 = pref_path.string();
    ux0 += "ux0";
    uma0 += "uma0";
    const std::string ux0_data = ux0 + "/data";
    const std::string uma0_data = uma0 + "/data";
    const std::string ux0_app = ux0 + "/app";
    const std::string ux0_user = ux0 + "/user";
    const std::string ux0_user00 = ux0_user + "/00";
    const std::string ux0_savedata = ux0_user00 + "/savedata";

    fs::create_directory(ux0);
    fs::create_directory(ux0_data);
    fs::create_directory(ux0_app);
    fs::create_directory(ux0_user);
    fs::create_directory(ux0_user00);
    fs::create_directory(ux0_savedata);
    fs::create_directory(uma0);
    fs::create_directory(uma0_data);

    return true;
}

void init_device_paths(IOState &io) {
    io.device_paths.savedata0 = "ux0/user/00/savedata/" + io.title_id;
    io.device_paths.app0 = "ux0/app/" + io.title_id;
    io.device_paths.addcont0 = "ux0/addcont/" + io.title_id;
}

BOOST_FSPATH translate_path(const BOOST_FSPATH &input_path, VitaIoDevice &device, const IOState::DevicePaths &device_paths) {
    BOOST_FSPATH relative_path = input_path.relative_path();
    std::string root_name = vfs::remove_colon(input_path.root_name().generic_string());
    device = vfs::get_device(root_name);

    BOOST_FSPATH temp{ root_name / relative_path };
    switch (device) {
    case VitaIoDevice::SAVEDATA0: { // Redirect savedata0:/ to ux0:/user/00/savedata/<title_id>
        temp = device_paths.savedata0 / relative_path;
        break;
    }
    case VitaIoDevice::GRO0: // Game card read-only access gro0:/
    case VitaIoDevice::GRW0: // Game card read and write access grw0:/
    case VitaIoDevice::APP0: { // Redirect app0:/ to ux0:/app/<title_id>
        temp = device_paths.app0 / relative_path;
        break;
    }
    case VitaIoDevice::ADDCONT0: { // Redirect addcont0:/ to ux0:/addcont/<title_id>
        temp = device_paths.addcont0 / relative_path;
        break;
    }
    case VitaIoDevice::TTY0: // Virtual device tty0:/
    case VitaIoDevice::TTY1: { // Virtual device tty1:/
        break;
    }
    case VitaIoDevice::OS0: // OS partition os0:/
    case VitaIoDevice::PD0: // "Welcome Park" pd0:/
    case VitaIoDevice::SA0: // Dictionary and font data sa0:/
    case VitaIoDevice::SD0: // ?Devkit?/PSP2 update partition sd0:/
    case VitaIoDevice::TM0: // NpDrm, Marlin, and DevKit/Testkit activation files tm0:/
    case VitaIoDevice::UD0: // Update partition ud0:/
    case VitaIoDevice::UMA0: // User memory card uma0:/
    case VitaIoDevice::UR0: // User resources partition ur0:/
    case VitaIoDevice::UX0: // User storage partition ux0:/
    case VitaIoDevice::VS0: // Sysapp and Library partition vs0:/
    case VitaIoDevice::VD0: { // Registry and error partition vd0:/
        break;
    }
    case VitaIoDevice::SAVEDATA1: // ?
    default: {
        LOG_ERROR("Unimplemented device {} used ({})", root_name, input_path.generic_path().string());
        device = VitaIoDevice::_INVALID;
        break;
    }
    }
    return temp.make_preferred();
}

SceUID open_file(IOState &io, const BOOST_FSPATH &path, int flags, const BOOST_FSPATH &pref_path, const char *export_name) {
    VitaIoDevice device = VitaIoDevice::_UNKNOWN;

    BOOST_FSPATH file_path{ pref_path / translate_path(path, device, io.device_paths) };
    if (device == VitaIoDevice::_UNKNOWN) {
        return IO_ERROR_UNK();
    }

    LOG_TRACE("{}: Opening file {} ({})", export_name, path.string(), file_path.string());

    if (device == VitaIoDevice::TTY0 || device == VitaIoDevice::TTY1) {
        assert(flags >= 0);

        std::underlying_type<TtyType>::type tty_type = 0;
        if (flags & SCE_O_RDONLY)
            tty_type |= TTY_IN;
        if (flags & SCE_O_WRONLY)
            tty_type |= TTY_OUT;

        const SceUID fd = io.next_fd++;
        io.tty_files.emplace(fd, static_cast<TtyType>(tty_type));

        return fd;
    } else {
        // NOTE: Analyze flags?
        const SceUID fd = io.next_fd++;
        io.std_files.emplace(fd, file_path);
        return fd;
    }
}

int read_file(void *data, IOState &io, SceUID fd, SceSize size, const char *export_name) {
    assert(data != nullptr);
    assert(fd >= 0);
    assert(size >= 0);

    LOG_TRACE("{}: Reading file: fd: {}, size: {}", export_name, log_hex(fd), size);

    const auto file = io.std_files.find(fd);
    if (file != io.std_files.end()) {
        FILE *f = fopen(file->second.string().c_str(), "rb");
        return fread(data, 1, size, f);
    }

    const auto tty_file = io.tty_files.find(fd);
    if (tty_file != io.tty_files.end()) {
        if (tty_file->second == TTY_IN) {
            std::cin.read(reinterpret_cast<char *>(data), size);
            return size;
        }
        return IO_ERROR_UNK();
    }

    return IO_ERROR(SCE_ERROR_ERRNO_EBADF);
}

int write_file(SceUID fd, const void *data, SceSize size, const IOState &io, const char *export_name) {
    assert(data != nullptr);
    assert(size >= 0);
    if (fd < 0) {
        LOG_WARN("Error writing file fd: {}, size: {}", fd, size);

        return IO_ERROR(SCE_ERROR_ERRNO_EBADF);
    }

    const auto tty_file = io.tty_files.find(fd);
    if (tty_file != io.tty_files.end()) {
        if (tty_file->second & TTY_OUT) {
            std::string s(reinterpret_cast<char const *>(data), size);

            // trim newline
            if (s.back() == '\n')
                s.pop_back();

            LOG_INFO("*** TTY: {}", s);
            return size;
        }

        return IO_ERROR_UNK();
    }

    LOG_TRACE("{}: Writing file: fd: {}, size: {}", export_name, log_hex(fd), size);

    const auto file = io.std_files.find(fd);
    if (file != io.std_files.end()) {
        FILE *f = fopen(file->second.string().c_str(), "wb+");
        return fwrite(data, 1, size, f);
    }

    return IO_ERROR(SCE_ERROR_ERRNO_EBADF);
}

int seek_file(SceUID fd, int offset, int whence, IOState &io, const char *export_name) {
    assert(fd >= 0);
    assert((whence == SCE_SEEK_SET) || (whence == SCE_SEEK_CUR) || (whence == SCE_SEEK_END));

    const auto std_file = io.std_files.find(fd);

    auto log_whence = [](int whence) -> const char * {
        switch (whence) {
            STR_CASE(SCE_SEEK_SET);
            STR_CASE(SCE_SEEK_CUR);
            STR_CASE(SCE_SEEK_END);
        default:
            return "INVALID";
        }
    };

    LOG_TRACE("{}: Seeking file: fd: {} at offset: {}, whence: {}", export_name, log_hex(fd), log_hex(offset), log_whence(whence));

    if (std_file == io.std_files.end()) {
        return IO_ERROR(SCE_ERROR_ERRNO_EBADF);
    }

    int base = SEEK_SET;
    switch (whence) {
    case SCE_SEEK_SET:
        base = SEEK_SET;
        break;
    case SCE_SEEK_CUR:
        base = SEEK_CUR;
        break;
    case SCE_SEEK_END:
        base = SEEK_END;
        break;
    }

    int ret = 0;
    long pos = 0;
    FILE *f = fopen(std_file->second.string().c_str(), "rb+");

    if (std_file != io.std_files.end()) {
        ret = fseek(f, offset, base);
    }

    if (ret != 0) {
        return IO_ERROR_UNK();
    }

    if (std_file != io.std_files.end()) {
        pos = ftell(f);
    }

    return pos;
}

void close_file(IOState &io, SceUID fd, const char *export_name) {
    assert(fd >= 0);

    LOG_TRACE("{}: Closing file: fd: {}", export_name, log_hex(fd));

    io.tty_files.erase(fd);
    io.std_files.erase(fd);
}

int create_dir(IOState &io, const BOOST_FSPATH &dir, int mode, const BOOST_FSPATH &pref_path, const char *export_name) {
    VitaIoDevice device = VitaIoDevice::_UNKNOWN;
    BOOST_FSPATH dir_path{ pref_path / translate_path(dir, device, io.device_paths) };
    if (device == VitaIoDevice::_INVALID) {
        return IO_ERROR(SCE_ERROR_ERRNO_EBADF);
    }

    if (!fs::exists(dir_path)) {
        LOG_TRACE("{}: Creating new dir {} ({})", export_name, dir.string(), dir_path.string());
        fs::create_directory(dir_path);
    }

    return 0;
}

int stat_file(IOState &io, const BOOST_FSPATH &file, SceIoStat *statp, const BOOST_FSPATH &pref_path, uint64_t base_tick, const char *export_name) {
    assert(statp != NULL);

    memset(statp, '\0', sizeof(SceIoStat));

    VitaIoDevice device = VitaIoDevice::_UNKNOWN;
    BOOST_FSPATH translated_path{ pref_path / translate_path(file, device, io.device_paths) };
    if (!fs::exists(translated_path)) {
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    // read and execute access rights
    statp->st_mode = SCE_S_IRUSR | SCE_S_IRGRP | SCE_S_IROTH | SCE_S_IXUSR | SCE_S_IXGRP | SCE_S_IXOTH;

    LOG_TRACE("{}: Statting file {} ({})", export_name, file.string(), translated_path.string());

    std::uint64_t last_access_time_ticks;
    std::uint64_t creation_time_ticks;

#ifdef WIN32
    WIN32_FIND_DATAW find_data;
    HANDLE handle = FindFirstFileW(translated_path.wstring().c_str(), &find_data);
    FindClose(handle);

    last_access_time_ticks = convert_filetime(find_data.ftLastAccessTime);
    creation_time_ticks = convert_filetime(find_data.ftCreationTime);
#else
    struct stat sb;
    if (stat(translated_path.string().c_str(), &sb) < 0) {
        return IO_ERROR_UNK();
    }

    last_access_time_ticks = (uint64_t)sb.st_atime * VITA_CLOCKS_PER_SEC;
    creation_time_ticks = (uint64_t)sb.st_ctime * VITA_CLOCKS_PER_SEC;

#undef st_atime
#undef st_ctime
#endif

    std::time_t last_modification_time_ticks = fs::last_write_time(translated_path);

    statp->st_size = fs::file_size(translated_path);
    statp->st_mode = fs::permissions_present(fs::status(translated_path));

    __RtcTicksToPspTime(statp->st_atime, last_access_time_ticks);
    __RtcTicksToPspTime(statp->st_mtime, last_modification_time_ticks);
    __RtcTicksToPspTime(statp->st_ctime, creation_time_ticks);

    return 0;
}

int stat_file_by_fd(IOState &io, const int fd, SceIoStat *statp, const BOOST_FSPATH &pref_path, uint64_t base_tick, const char *export_name) {
    assert(statp != NULL);
    memset(statp, '\0', sizeof(SceIoStat));

    if (io.std_files.find(fd) != io.std_files.end()) {
        return stat_file(io, io.std_files[fd], statp, pref_path, base_tick, export_name);
    } else if (io.dir_entries.find(fd) != io.dir_entries.end()) {
        return stat_file(io, io.dir_entries[fd], statp, pref_path, base_tick, export_name);
    }

    return IO_ERROR(SCE_ERROR_ERRNO_EBADF);
}

int open_dir(IOState &io, const BOOST_FSPATH &path, const BOOST_FSPATH &pref_path, const char *export_name) {
    VitaIoDevice device = VitaIoDevice::_UNKNOWN;

    BOOST_FSPATH translated_path{ pref_path / translate_path(path, device, io.device_paths) };
    if (device == VitaIoDevice::_INVALID) {
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    LOG_TRACE("{}: Opening dir {}", export_name, translated_path.string().c_str());

    const SceUID fd = io.next_fd++;
    io.dir_entries.emplace(fd, translated_path);

    return fd;
}

int read_dir(IOState &io, const SceUID fd, emu::SceIoDirent *dirent, const char *export_name) {
    assert(dirent != NULL);

    LOG_TRACE("{}: Reading dir fd: {}", export_name, log_hex(fd));

    // NOTE: based on https://stackoverflow.com/a/12737930/2173875
    for (fs::recursive_directory_iterator end, dir(io.dir_entries[fd]); dir != end; ++dir) {
        const fs::path &p = dir->path();

        // Hidden directory, don't recurse into it
        if (fs::is_directory(p) && (p.filename_is_dot() || p.filename_is_dot_dot())) {
            dir.no_push();
            continue;
        }

        // NOTE: Check here if a hidden file?
        p.has_stem() ? dirent->d_stat.st_mode = SCE_S_IFREG : dirent->d_stat.st_mode = SCE_S_IFDIR;
    }
    return 1;
}

int close_dir(IOState &io, SceUID fd, const char *export_name) {
    const auto erased_entries = io.dir_entries.erase(fd);

    LOG_TRACE("{}: Closing dir fd: {}", export_name, log_hex(fd));

    if (erased_entries == 0)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADF);
    else
        return 0;
}

int io_remove(IOState &io, const BOOST_FSPATH &path, const BOOST_FSPATH &pref_path, const char *export_name) {
    VitaIoDevice device = VitaIoDevice::_UNKNOWN;
    BOOST_FSPATH file_path{ pref_path / translate_path(path, device, io.device_paths) };
    if (device == VitaIoDevice::_INVALID) {
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    LOG_TRACE("{}: Removing {} ({})", export_name, path.string(), file_path.string());

    fs::remove(file_path);
    return 0;
}
