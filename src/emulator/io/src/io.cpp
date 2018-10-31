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

constexpr const char *
get_device_string(VitaIoDevice dev, bool with_colon) {
    switch (dev) {
#define DEVICE(path, name)  \
    case JOIN_DEVICE(name): \
        return with_colon ? UNWRAP(path) ":" : path;
#include <io/VitaIoDevice.def>
#undef DEVICE
    default:
        return "";
    }
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

bool read_file(VitaIoDevice device, FileBuffer &buf, const fs::path &pref_path, const std::string &file_path) {
    fs::path cur_file = file_path;
    fs::path host_file_path{ pref_path / get_device_string(device) / cur_file.relative_path() };
    if (!fs::exists(host_file_path)) {
        return false;
    }

    fs::ifstream f(host_file_path, std::ios_base::binary);
    f.unsetf(std::ios::skipws);
    std::streampos size;
    f.seekg(0, std::ios::end);
    size = f.tellg();
    f.seekg(0, std::ios::beg);
    buf.reserve(size);
    buf.insert(buf.begin(), std::istream_iterator<uint8_t>(f), std::istream_iterator<uint8_t>());
    return true;
}

bool read_app_file(FileBuffer &buf, const fs::path &pref_path, const std::string title_id, const std::string &file_path) {
    std::string game_file_path = "app/" + title_id + "/" + file_path;
    return read_file(VitaIoDevice::UX0, buf, pref_path, game_file_path);
}

} // namespace vfs

bool init(IOState &io, fs::path &pref_path) {
    std::string base = pref_path.generic_path().string();

    const std::string os0 = base + "os0/";
    const std::string sa0 = base + "sa0/";
    const std::string ux0 = base + "ux0/";
    const std::string uma0 = base + "uma0/";
    const std::string vd0 = base + "vd0/";
    const std::string vs0 = base + "vs0/";

    const std::string userpath = "user/00/";
    const std::string SceIoTrash = "SceIoTrash/";
    const std::string app = "app/";
    const std::string data = "data/";
    const std::string external = "external/";
    const std::string font = "font/";
    const std::string vsh = "vsh/";

    fs::create_directories(os0 + "ks/");
    fs::create_directory(os0 + "kd/");
    fs::create_directory(os0 + "sm/");
    fs::create_directory(os0 + "ue/");
    fs::create_directory(os0 + "us/");

    fs::create_directories(sa0 + data + "dic/");
    fs::create_directories(sa0 + data + font + "pgf/");
    fs::create_directory(sa0 + data + font + "pvf/");

    fs::create_directories(base + "pd0/" + app);
    fs::create_directories(base + "sd0/PSP2");
    fs::create_directories(base + "ud0/" + SceIoTrash);
    fs::create_directories(base + "tm0/" + SceIoTrash);
    fs::create_directory(base + "ur0/");

    fs::create_directories(vd0 + "history/");
    fs::create_directory(vd0 + "network/");
    fs::create_directory(vd0 + "registry/");

    fs::create_directories(vs0 + data + external + font);
    fs::create_directories(vs0 + data + "internal/theme/");
    fs::create_directories(vs0 + "sys/" + external);
    fs::create_directories(vs0 + vsh + "shell/");
    fs::create_directory(vs0 + vsh + "common/");
    fs::create_directory(vs0 + app);
    fs::create_directory(vs0 + "etc/");

    fs::create_directories(ux0 + userpath);
    fs::create_directory(ux0 + app);

    fs::create_directories(uma0 + userpath);
    fs::create_directory(uma0 + data);

    return true;
}

void init_game_paths(IOState &io, fs::path pref_path) {
    io.device_paths.app0 = "ux0/app/" + io.title_id;

    io.device_paths.ux0_savedata = "ux0/user/00/savedata/" + io.title_id;
    fs::create_directories(pref_path.generic_path().string() + io.device_paths.ux0_savedata);

    io.device_paths.appmeta0 = "ux0/appmeta/" + io.title_id;
    fs::create_directories(pref_path.generic_path().string() + io.device_paths.appmeta0);

    io.device_paths.cache0 = "ux0/cache/" + io.title_id;
    fs::create_directories(pref_path.generic_path().string() + io.device_paths.cache0);

    io.device_paths.addcont0 = "ux0/addcont/" + io.title_id;
    fs::create_directories(pref_path.generic_path().string() + io.device_paths.addcont0);

    io.device_paths.patch0 = "ux0/patch/" + io.title_id;
    fs::create_directories(pref_path.generic_path().string() + io.device_paths.patch0);

    io.device_paths.ur0_savedata = "ur0/user/00/savedata/" + io.title_id;
    fs::create_directories(pref_path.generic_path().string() + io.device_paths.ur0_savedata);

    io.device_paths.trophy0 = "ur0/00/trophy/data/" + io.title_id;
    fs::create_directories(pref_path.generic_path().string() + io.device_paths.trophy0);

    io.device_paths.uma0_savedata = "uma0/user/00/savedata" + io.title_id;
    fs::create_directories(pref_path.generic_path().string() + io.device_paths.uma0_savedata);
}

std::string translate_path(const fs::path &input_path, VitaIoDevice &device, const IOState::DevicePaths &device_paths) {
    fs::path file_path = input_path.relative_path();
    std::string root_name = vfs::remove_colon(input_path.root_name().string());
    device = vfs::get_device(root_name);

    fs::path temp{ root_name / file_path };
    switch (device) {
    case VitaIoDevice::SAVEDATA0: { // Redirect savedata0:/ to ux0:/user/00/savedata/<title_id>
        temp = device_paths.ux0_savedata / file_path;
        break;
    }
    case VitaIoDevice::GRO0: { // Game card read-only access gro0:/
        if (file_path.root_path().string() == "addcont") {
            temp = device_paths.addcont0 / file_path;
        } // otherwise assume it is the app path
    }
    case VitaIoDevice::APP0: { // Redirect app0:/ to ux0:/app/<title_id>
        temp = device_paths.app0 / file_path;
        break;
    }
    case VitaIoDevice::GRW0: { // Game card read and write access grw0:/
        if (file_path.root_path().string() == "savedata") {
            temp = device_paths.ux0_savedata / file_path;
        } // otherwise assume it is the addcont path
    }
    case VitaIoDevice::ADDCONT0: { // Redirect addcont0:/ to ux0:/addcont/<title_id>
        temp = device_paths.addcont0 / file_path;
        break;
    }
    case VitaIoDevice::CACHE0: { // Redirect cache0:/ to ux0:/cache/<title_id>
        temp = device_paths.cache0 / file_path;
        break;
    }
    case VitaIoDevice::TTY0: // Virtual device tty0:/
    case VitaIoDevice::TTY1: { // Virtual device tty1:/
        break;
    }
    case VitaIoDevice::IMC0: // Internal eMMC int0:/
    case VitaIoDevice::XMC0: { // External memory card xmc0:/
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
    case VitaIoDevice::HOST0: // ?
    default: {
        LOG_ERROR("Unimplemented device {}: used", root_name);
        device = VitaIoDevice::_INVALID;
        temp = "";
        break;
    }
    }
    return temp.make_preferred().generic_path().string();
}

void check_path(IOState &io, std::string &path, const fs::path &pref_path) {
    VitaIoDevice device = VitaIoDevice::_UNKNOWN;
    fs::path translated_path = translate_path(path, device, io.device_paths);
    if (device == VitaIoDevice::IMC0) { // redirect to the base Vita path (TODO: needs checking)
        path = pref_path.generic_path().string();
    } else if (device == VitaIoDevice::XMC0) { // assume it is uma0:/ (TODO: needs checking)
        path = pref_path.generic_path().string() + "uma0/";
    } else if (!translated_path.empty()) {
        path = pref_path.generic_path().string() + translated_path.generic_path().string();
    } else {
        path = "";
    }
}

SceUID open_file(IOState &io, std::string path, int flags, const fs::path &pref_path, const char *export_name) {
    VitaIoDevice device = VitaIoDevice::_UNKNOWN;
    fs::path translated_path{ pref_path / translate_path(path, device, io.device_paths) };
    if (device == VitaIoDevice::_INVALID) {
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    LOG_TRACE("{}: Opening file {}", export_name, path);

    const SceUID fd = io.next_fd++;

    if (device == VitaIoDevice::TTY0 || device == VitaIoDevice::TTY1) {
        assert(flags >= 0);

        std::underlying_type<TtyType>::type tty_type = 0;
        if (flags & SCE_O_RDONLY)
            tty_type |= TTY_IN;
        if (flags & SCE_O_WRONLY)
            tty_type |= TTY_OUT;

        io.tty_files.emplace(fd, static_cast<TtyType>(tty_type));
    } else {
        io.std_files.emplace(fd, translated_path);
    }

    return fd;
}

int read_file(void *data, IOState &io, SceUID fd, SceSize size, const char *export_name) {
    assert(data != nullptr);
    assert(fd >= 0);
    assert(size >= 0);

    const auto file = io.std_files.find(fd);
    if (file != io.std_files.end() && fs::exists(file->second)) {
        LOG_TRACE("{}: Reading file: fd: {}, size: {}", export_name, log_hex(fd), size);

        FILE *f = fopen(file->second.generic_path().string().c_str(), "rb");
        fs::permissions(file->second, fs::add_perms | fs::others_read | fs::group_read | fs::owner_read);
        return fread(data, 1, size, f);
    }

    const auto tty_file = io.tty_files.find(fd);
    if (tty_file != io.tty_files.end()) {
        if (tty_file->second == TTY_IN) {
            LOG_TRACE("{}: Reading file: fd: {}, size: {}", export_name, log_hex(fd), size);

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
            std::string s(reinterpret_cast<const char *>(data), size);

            // trim newline
            if (s.back() == '\n')
                s.pop_back();

            LOG_INFO("*** TTY: {}", s);
            return size;
        }

        return IO_ERROR_UNK();
    }

    const auto file = io.std_files.find(fd);
    if (file != io.std_files.end()) {
        LOG_TRACE("{}: Writing file: fd: {}, size: {}", export_name, log_hex(fd), size);

        FILE *f = fopen(file->second.generic_path().string().c_str(), "wb+");
        fs::permissions(file->second, fs::add_perms | fs::others_write | fs::group_write | fs::owner_write);
        return fwrite(data, 1, size, f);
    }

    return IO_ERROR(SCE_ERROR_ERRNO_EBADF);
}

int seek_file(SceUID fd, int offset, int whence, IOState &io, const char *export_name) {
    assert(fd >= 0);
    assert((whence == SCE_SEEK_SET) || (whence == SCE_SEEK_CUR) || (whence == SCE_SEEK_END));

    const auto std_file = io.std_files.find(fd);
    if (std_file == io.std_files.end() || !fs::exists(std_file->second)) {
        return IO_ERROR(SCE_ERROR_ERRNO_EBADF);
    }

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
    FILE *f = fopen(std_file->second.generic_path().string().c_str(), "rb");

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

int stat_file(IOState &io, std::string file, SceIoStat *statp, const fs::path &pref_path, uint64_t base_tick, const char *export_name) {
    assert(statp != NULL);
    memset(statp, '\0', sizeof(SceIoStat));

    std::string temp = file;
    check_path(io, file, pref_path);
    if (file.empty() || !fs::exists(file)) {
        LOG_WARN("{}: Cannot find file {}", export_name, temp);
        return SCE_ERROR_ERRNO_ENOENT;
    }

    LOG_TRACE("{}: Statting file {}", export_name, temp);

    fs::path fs_handle = file;

    std::uint64_t last_access_time_ticks;
    std::uint64_t creation_time_ticks;

#ifdef WIN32
    WIN32_FIND_DATAW find_data;
    HANDLE handle = FindFirstFileW(fs_handle.generic_path().wstring().c_str(), &find_data);
    FindClose(handle);

    last_access_time_ticks = convert_filetime(find_data.ftLastAccessTime);
    creation_time_ticks = convert_filetime(find_data.ftCreationTime);
#else
    struct stat sb;
    stat(fs_handle.generic_path().string().c_str(), &sb);

    last_access_time_ticks = (uint64_t)sb.st_atime * VITA_CLOCKS_PER_SEC;
    creation_time_ticks = (uint64_t)sb.st_ctime * VITA_CLOCKS_PER_SEC;

#undef st_atime
#undef st_ctime
#endif

    std::time_t last_modification_time_ticks = fs::last_write_time(fs_handle);

    if (fs::is_regular_file(fs_handle)) {
        statp->st_size = fs::file_size(fs_handle);
    }

    if (fs::permissions_present(fs::status(fs_handle))) {
        fs::file_status f_status = fs::status(fs_handle);
        statp->st_attr = f_status.permissions();
    }

    __RtcTicksToPspTime(statp->st_atime, last_access_time_ticks);
    __RtcTicksToPspTime(statp->st_mtime, last_modification_time_ticks);
    __RtcTicksToPspTime(statp->st_ctime, creation_time_ticks);

    return 0;
}

int stat_file_by_fd(IOState &io, const int fd, SceIoStat *statp, const fs::path &pref_path, uint64_t base_tick, const char *export_name) {
    assert(statp != NULL);
    memset(statp, '\0', sizeof(SceIoStat));

    if (io.std_files.find(fd) != io.std_files.end()) {
        return stat_file(io, io.std_files[fd].generic_path().string(), statp, pref_path, base_tick, export_name);
    } else if (io.dir_entries.find(fd) != io.dir_entries.end()) {
        return stat_file(io, io.dir_entries[fd].generic_path().string(), statp, pref_path, base_tick, export_name);
    }

    return IO_ERROR(SCE_ERROR_ERRNO_EBADF);
}

int create_dir(IOState &io, std::string dir, int mode, const fs::path &pref_path, const char *export_name) {
    check_path(io, dir, pref_path);
    if (dir.empty()) {
        return IO_ERROR(SCE_ERROR_ERRNO_EBADF);
    }

    if (!fs::exists(dir)) {
        LOG_TRACE("{}: Creating new dir {}", export_name, dir);
        fs::create_directory(dir);
    }

    return 0;
}

int open_dir(IOState &io, std::string path, const fs::path &pref_path, const char *export_name) {
    VitaIoDevice device = VitaIoDevice::_UNKNOWN;
    fs::path translated_path{ pref_path / translate_path(path, device, io.device_paths) };
    if (device == VitaIoDevice::_INVALID) {
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    LOG_TRACE("{}: Opening dir {}", export_name, path);

    const SceUID fd = io.next_fd++;
    io.dir_entries.emplace(fd, translated_path);

    return fd;
}

int read_dir(IOState &io, const SceUID fd, emu::SceIoDirent *dirent, const char *export_name) {
    assert(dirent != NULL);
    if (!fs::exists(io.dir_entries[fd])) {
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

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
        fs::is_regular_file(p) ? dirent->d_stat.st_mode = SCE_S_IFREG : dirent->d_stat.st_mode = SCE_S_IFDIR;
    }
    return 0;
}

int close_dir(IOState &io, SceUID fd, const char *export_name) {
    const auto erased_entries = io.dir_entries.erase(fd);

    LOG_TRACE("{}: Closing dir fd: {}", export_name, log_hex(fd));

    if (erased_entries == 0)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADF);
    else
        return 0;
}

int io_remove(IOState &io, std::string path, const fs::path &pref_path, const char *export_name) {
    check_path(io, path, pref_path);
    if (path.empty() || !fs::exists(path)) {
        return 0;
    }

    LOG_TRACE("{}: Removing {}", export_name, path);

    fs::remove(path);
    return 0;
}
