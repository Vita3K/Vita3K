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

#include <psp2/io/fcntl.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dirent.h>
#include <util/string_convert.h>
#include <util/preprocessor.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <tuple>

static void
delete_file(FILE *file) {
    if (file != nullptr) {
        fclose(file);
    }
}

static void
delete_dir(DIR *dir) {
    if (dir != nullptr) {
        closedir(dir);
    }
}

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

void trim_leading_slash(std::string path) {
    if (path[0] == '/')
        path.erase(0, 1);
}

constexpr const char *
get_device_string(VitaIoDevice dev, bool with_colon = false) {
    switch (dev) {
#define DEVICE(path, name)  \
    case JOIN_DEVICE(name): \
        return with_colon ? CONCATENATE(path, ":") : path;
#include <io/VitaIoDevice.def>
#undef DEVICE
    default:
        return "";
    }
}

VitaIoDevice
get_device(const std::string &path) {
    trim_leading_slash(path);

    // find which IO device the path is associated with
    const auto device_end_idx = path.find(":");
    if (device_end_idx == std::string::npos) {
        return VitaIoDevice::_INVALID;
    }

    std::string device_name(path.substr(0, device_end_idx));

#define DEVICE(path, name)   \
    if (device_name == path) \
        return JOIN_DEVICE(name);
#include <io/VitaIoDevice.def>
#undef DEVICE

    LOG_CRITICAL("Unknown device {} used. Report this to developers!", path);
    return VitaIoDevice::_UKNONWN;
}

std::string
get_relative_path(const std::string &full_path, VitaIoDevice device) {
    if (device == VitaIoDevice::_UKNONWN)
        device = get_device(full_path);

    std::string rel_path(full_path);

    trim_leading_slash(rel_path);

    rel_path.erase(0, std::strlen(get_device_string(device, true)));
    return rel_path;
}

std::string
normalize_path(const std::string &path, VitaIoDevice device) {
    if (device == VitaIoDevice::_UKNONWN)
        device = get_device(path);

    std::string device_string = get_device_string(device);
    std::string normalized_path(device_string + "/");
    uint32_t dev_length = device_string.length();

    if (path.empty())
        return normalized_path;

    if (path[dev_length + 1] == '/') {
        // Skip "/" since we already added one
        normalized_path.erase(dev_length, 1);
    }

    normalized_path += path.substr(dev_length + 1);
    return normalized_path;
}

std::string
to_host_path(const std::string &path, const std::string &pref_path, VitaIoDevice device) {
    std::string res = normalize_path(path, device);
    return pref_path + res;
}

bool init(IOState &io, const char *pref_path) {
    std::string ux0 = pref_path;
    std::string uma0 = pref_path;
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
    io.device_paths.savedata0 = "ux0:/user/00/savedata/" + io.title_id + "/";
    io.device_paths.app0 = "ux0:/app/" + io.title_id + "/";
    io.device_paths.addcont0 = "ux0:/addcont/" + io.title_id + "/";
}

void translate_path(std::string &path, VitaIoDevice &device, const IOState::DevicePaths &device_paths) {
    // Redirect savedata0:/ to ux0:/user/00/savedata/<title_id>
    if (device == VitaIoDevice::SAVEDATA0) {
        const auto relative_path = get_relative_path(path, device);
        path = device_paths.savedata0 + relative_path;
        device = get_device(path);
    }

    // Redirect app0:/ to ux0:/app/<title_id>
    if (device == VitaIoDevice::APP0) {
        const auto relative_path = get_relative_path(path, device);
        path = device_paths.app0 + relative_path;
        device = get_device(path);
    }

    // Redirect addcont0:/ to ux0:/addcont/<title_id>
    if (device == VitaIoDevice::ADDCONT0) {
        const auto relative_path = get_relative_path(path, device);
        path = device_paths.addcont0 + relative_path;
        device = get_device(path);
    }
}

SceUID open_file(IOState &io, const std::string &path_, int flags, const char *pref_path) {
    std::string path(path_);
    VitaIoDevice device = get_device(path);

    translate_path(path, device, io.device_paths);

    switch (device) {
    case VitaIoDevice::TTY0:
    case VitaIoDevice::TTY1: {
        assert(flags >= 0);

        TtyType type;
        if (flags == SCE_O_RDONLY) {
            type = TTY_IN;
        } else if (flags == SCE_O_WRONLY) {
            type = TTY_OUT;
        }

        const SceUID fd = io.next_fd++;
        io.tty_files.emplace(fd, type);

        return fd;
    }
    case VitaIoDevice::UX0:
    case VitaIoDevice::UMA0: {
        std::string file_path = to_host_path(path, pref_path, device);

        const char *const open_mode = translate_open_mode(flags);
#ifdef WIN32
        const FilePtr file(_wfopen(utf_to_wide(file_path).c_str(), utf_to_wide(open_mode).c_str()), delete_file);
#else
        const FilePtr file(fopen(file_path.c_str(), open_mode), delete_file);
#endif
        if (!file) {
            return -1;
        }

        const SceUID fd = io.next_fd++;
        io.std_files.emplace(fd, file);

        return fd;
    }
    case VitaIoDevice::SAVEDATA1:
    case VitaIoDevice::_UKNONWN: {
        LOG_ERROR("Unimplemented device {} used", path_);
        // fall-through default behavior
    }
    default: {
        // TODO: Have a default behavior

        return -1;
    }
    }
}

int read_file(void *data, IOState &io, SceUID fd, SceSize size) {
    assert(data != nullptr);
    assert(fd >= 0);
    assert(size >= 0);

    const StdFiles::const_iterator file = io.std_files.find(fd);
    if (file != io.std_files.end()) {
        return fread(data, 1, size, file->second.get());
    }

    const TtyFiles::const_iterator tty_file = io.tty_files.find(fd);
    if (tty_file != io.tty_files.end()) {
        if (tty_file->second == TTY_IN) {
            std::cin.read(reinterpret_cast<char *>(data), size);
            return size;
        }
        return -1;
    }

    return -1;
}

int write_file(SceUID fd, const void *data, SceSize size, const IOState &io) {
    assert(data != nullptr);
    assert(size >= 0);
    if (fd < 0) {
        return -1;
    }

    const StdFiles::const_iterator file = io.std_files.find(fd);
    if (file != io.std_files.end()) {
        return fwrite(data, 1, size, file->second.get());
    }

    const TtyFiles::const_iterator tty_file = io.tty_files.find(fd);
    if (tty_file != io.tty_files.end()) {
        if (tty_file->second == TTY_OUT) {
            std::string s(reinterpret_cast<char const *>(data), size);

            // trim newline
            if (s.back() == '\n') {
                s.back() = '\0';
            }
            LOG_INFO("*** TTY: {}", s);
            return size;
        }

        return -1;
    }

    return -1;
}

int seek_file(SceUID fd, int offset, int whence, IOState &io) {
    assert(fd >= 0);
    assert((whence == SCE_SEEK_SET) || (whence == SCE_SEEK_CUR) || (whence == SCE_SEEK_END));

    const StdFiles::const_iterator std_file = io.std_files.find(fd);

    assert(std_file != io.std_files.end());

    if (std_file == io.std_files.end()) {
        return -1;
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

    if (std_file != io.std_files.end()) {
        ret = fseek(std_file->second.get(), offset, base);
    }

    if (ret != 0) {
        return -1;
    }

    if (std_file != io.std_files.end()) {
        pos = ftell(std_file->second.get());
    }
    return pos;
}

void close_file(IOState &io, SceUID fd) {
    assert(fd >= 0);

    io.tty_files.erase(fd);
    io.std_files.erase(fd);
}

int remove_file(IOState &io, const char *file, const char *pref_path) {
    VitaIoDevice device = get_device(file);
    std::string path = file;

    translate_path(path, device, io.device_paths);

    switch (device) {
    case VitaIoDevice::UX0:
    case VitaIoDevice::UMA0: {
        std::string file_path = to_host_path(path, pref_path, device);
        fs::remove(file_path);
        return 0;
    }
    default: {
        LOG_ERROR("Unimplemented device {} used", file);
        return -1;
    }
    }
}

int create_dir(IOState &io, const char *dir, int mode, const char *pref_path) {
    VitaIoDevice device = get_device(dir);
    std::string path = dir;

    translate_path(path, device, io.device_paths);

    switch (device) {
    case VitaIoDevice::UX0:
    case VitaIoDevice::UMA0: {
        std::string dir_path = to_host_path(path, pref_path, device);

        fs::create_directory(dir_path);
        return 0;
    }
    default: {
        LOG_ERROR("Unimplemented device {} used", dir);
        return -1;
    }
    }
}

int remove_dir(IOState &io, const char *dir, const char *pref_path) {
    VitaIoDevice device = get_device(dir);
    std::string path = dir;

    translate_path(path, device, io.device_paths);

    switch (device) {
    case VitaIoDevice::UX0:
    case VitaIoDevice::UMA0: {
        std::string dir_path = to_host_path(path, pref_path, device);
        fs::remove(dir_path);
        return 0;
    }
    default: {
        LOG_ERROR("Unimplemented device {} used", dir);
        return -1;
    }
    }
}

int stat_file(IOState &io, const char *file, SceIoStat *statp, const char *pref_path, uint64_t base_tick) {
    assert(statp != NULL);

    memset(statp, '\0', sizeof(SceIoStat));

    VitaIoDevice device = get_device(file);
    std::string path = file;

    // read and execute access rights
    statp->st_mode = SCE_S_IRUSR | SCE_S_IRGRP | SCE_S_IROTH | SCE_S_IXUSR | SCE_S_IXGRP | SCE_S_IXOTH;

    std::uint64_t last_access_time_ticks;
    std::uint64_t last_modification_time_ticks;
    std::uint64_t creation_time_ticks{ 0 };

    translate_path(path, device, io.device_paths);

    switch (device) {
    case VitaIoDevice::UX0:
    case VitaIoDevice::UMA0: {
        std::string file_path = to_host_path(path, pref_path, device);

#ifdef WIN32
        WIN32_FIND_DATAW find_data;
        HANDLE handle = FindFirstFileW(utf_to_wide(file_path).c_str(), &find_data);
        if (handle == INVALID_HANDLE_VALUE) {
            return -1;
        }
        FindClose(handle);

        statp->st_mode |= SCE_S_IFREG;
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
            statp->st_mode |= SCE_S_IFDIR;
        }

        statp->st_size = (std::uint64_t)find_data.nFileSizeHigh << 32 | (std::uint64_t)find_data.nFileSizeLow;

        last_access_time_ticks = convert_filetime(find_data.ftLastAccessTime);
        last_modification_time_ticks = convert_filetime(find_data.ftLastWriteTime);
        creation_time_ticks = convert_filetime(find_data.ftCreationTime);
#else
        struct stat sb;
        if (stat(file_path.c_str(), &sb) < 0) {
            return -1;
        }

        if (S_ISREG(sb.st_mode)) {
            statp->st_mode |= SCE_S_IFREG;
        } else if (S_ISDIR(sb.st_mode)) {
            statp->st_mode |= SCE_S_IFDIR;
        }

        statp->st_size = sb.st_size;

        last_access_time_ticks = (uint64_t)sb.st_atime * VITA_CLOCKS_PER_SEC;
        last_modification_time_ticks = (uint64_t)sb.st_mtime * VITA_CLOCKS_PER_SEC;
        creation_time_ticks = (uint64_t)sb.st_ctime * VITA_CLOCKS_PER_SEC;

#undef st_atime
#undef st_mtime
#undef st_ctime
#endif
        break;
    }
    default: {
        LOG_ERROR("Unimplemented device {} used", file);
        return -1;
    }
    }
    __RtcTicksToPspTime(statp->st_atime, last_access_time_ticks);
    __RtcTicksToPspTime(statp->st_mtime, last_modification_time_ticks);
    __RtcTicksToPspTime(statp->st_ctime, creation_time_ticks);

    return 0;
}

int open_dir(IOState &io, const char *path, const char *pref_path) {
    std::string spath = path;
    VitaIoDevice device = get_device(spath);

    translate_path(spath, device, io.device_paths);

    std::string dir_path;
    switch (device) {
    case VitaIoDevice::UX0:
    case VitaIoDevice::UMA0: {
        dir_path = to_host_path(spath, pref_path, device);
        break;
    }
    default: {
        LOG_ERROR("Unimplemented device {} used", path);
        return -1;
    }
    }

#ifdef WIN32
    const DirPtr dir(_wopendir((utf_to_wide(dir_path)).c_str()));
#else
    const DirPtr dir(opendir(dir_path.c_str()), delete_dir);
#endif
    if (!dir) {
        return -1;
    }

    const SceUID fd = io.next_fd++;
    io.dir_entries.emplace(fd, dir);

    return fd;
}

int read_dir(IOState &io, SceUID fd, emu::SceIoDirent *dent) {
    assert(dent != nullptr);

    memset(dent->d_name, '\0', sizeof(dent->d_name));

    const DirEntries::const_iterator dir = io.dir_entries.find(fd);
    if (dir != io.dir_entries.end()) {
#ifdef WIN32
        _wdirent *d = _wreaddir(dir->second.get());
#else
        dirent *d = readdir(dir->second.get());
#endif

        if (!d) {
            return 0;
        }

#ifdef WIN32
        std::string d_name_utf8 = wide_to_utf(d->d_name);
        strncpy(dent->d_name, d_name_utf8.c_str(), sizeof(dent->d_name));
#else
        strncpy(dent->d_name, d->d_name, sizeof(dent->d_name));
#endif
        if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) {
            // Skip . and .. folders
            return read_dir(io, fd, dent);
        }

        dent->d_stat.st_mode = d->d_type == DT_DIR ? SCE_S_IFDIR : SCE_S_IFREG;

        return 1;
    }

    return 0;
}

int close_dir(IOState &io, SceUID fd) {
    io.dir_entries.erase(fd);

    return 1;
}
