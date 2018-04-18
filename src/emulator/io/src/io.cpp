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

#include <io/io.h>
#include <io/functions.h>

#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>

#include <io/state.h>
#include <util/log.h>

#include <psp2/io/fcntl.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dirent.h>
#include <util/string_convert.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <tuple>

static ReadOnlyInMemFile open_zip(mz_zip_archive &zip, const char *entry_path) {
    const int index = mz_zip_reader_locate_file(&zip, entry_path, nullptr, 0);

    if (index < 0) {
        return ReadOnlyInMemFile();
    }

    const auto zip_file = mz_zip_reader_extract_iter_new(&zip, index, 0);

    if (!zip_file) {
        return ReadOnlyInMemFile();
    }

    ReadOnlyInMemFile res;
    char *data = res.alloc_data(zip_file->file_stat.m_uncomp_size);

    mz_zip_reader_extract_iter_read(zip_file, data, zip_file->file_stat.m_uncomp_size);
    mz_zip_reader_extract_iter_free(zip_file);

    return res;
}

static void delete_file(FILE *file) {
    if (file != nullptr) {
        fclose(file);
    }
}

static void delete_dir(DIR *dir) {
    if (dir != nullptr) {
        closedir(dir);
    }
}

const char *translate_open_mode(int flags) {
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

std::string translate_path(const std::string &part_name, const char *path, const char *pref_path) {
    std::string res = pref_path;
    res += part_name;
    int i = part_name.length();
    res += "/";

    if (path[i + 1] == '/')
        i++;
    res += &path[i + 1];

    return res;
}

bool init(IOState &io, const char *pref_path) {
    std::string ux0 = pref_path;
    std::string uma0 = pref_path;
    ux0 += "ux0";
    uma0 += "uma0";
    const std::string ux0_data = ux0 + "/data";
    const std::string uma0_data = uma0 + "/data";

#ifdef WIN32
    CreateDirectoryA(ux0.c_str(), nullptr);
    CreateDirectoryA(ux0_data.c_str(), nullptr);
    CreateDirectoryA(uma0.c_str(), nullptr);
    CreateDirectoryA(uma0_data.c_str(), nullptr);
#else
    const int mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    mkdir(ux0.c_str(), mode);
    mkdir(ux0_data.c_str(), mode);
    mkdir(uma0.c_str(), mode);
    mkdir(uma0_data.c_str(), mode);
#endif

    return true;
}

static std::pair<VitaIoDevice, std::string>
translate_device(const std::string &path_) {
    std::string path(path_);

    // trim leading '/'
    if (path[0] == '/')
        path.erase(0, 1);

    // find which IO device the path is associated with
    const auto device_end_idx = path.find(":");
    if (device_end_idx == std::string::npos) {
        return { VitaIoDevice::_INVALID, "" };
    }

    std::string device_name(path.substr(0, device_end_idx));

#define JOIN_DEVICE(p) VitaIoDevice::p
#define DEVICE(path, name)   \
    if (device_name == path) \
        return { JOIN_DEVICE(name), path };
#include <io/VitaIoDevice.def>
#undef DEVICE
#undef JOIN_DEVICE
    return { VitaIoDevice::_UKNONWN, "" };
}

SceUID open_file(IOState &io, const char *path, int flags, const char *pref_path) {
    VitaIoDevice device;
    std::string device_name;
    std::tie(device, device_name) = translate_device(path);

    switch (device) {
    case VitaIoDevice::TTY0: {
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
    case VitaIoDevice::APP0: {
        assert(flags == SCE_O_RDONLY);

        if (!io.vpk) {
            return -1;
        }

        int i = 5;
        if (path[5] == '/')
            i++;

        const ReadOnlyInMemFile file = open_zip(*io.vpk, &path[i]);

        const SceUID fd = io.next_fd++;
        io.app_files.emplace(fd, file);

        return fd;
    }
    case VitaIoDevice::UX0:
    case VitaIoDevice::UMA0: {
        std::string file_path = translate_path(device_name, path, pref_path);

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
        LOG_CRITICAL("Unknown device {} used. Report this to developers!", path);
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

    AppFiles::iterator app_file = io.app_files.find(fd);
    if (app_file != io.app_files.end()) {
        auto is = app_file->second.read(data, size);
        return is;
    }

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
    assert(fd >= 0);
    assert(size >= 0);

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
    AppFiles::iterator app_file = io.app_files.find(fd);

    assert(std_file != io.std_files.end() || app_file != io.app_files.end());

    if (std_file == io.std_files.end() && app_file == io.app_files.end()) {
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
    } else {
        ret = !(app_file->second.seek(offset, whence));
    }

    if (ret != 0) {
        return -1;
    }

    if (std_file != io.std_files.end()) {
        pos = ftell(std_file->second.get());
    } else {
        pos = app_file->second.tell();
    }
    return pos;
}

void close_file(IOState &io, SceUID fd) {
    assert(fd >= 0);

    io.tty_files.erase(fd);
    io.std_files.erase(fd);
    io.app_files.erase(fd);
}

int remove_file(const char *file, const char *pref_path) {
    VitaIoDevice device;
    std::string device_name;
    std::tie(device, device_name) = translate_device(file);

    switch (device) {
    case VitaIoDevice::UX0:
    case VitaIoDevice::UMA0: {
        std::string file_path = translate_path(device_name, file, pref_path);

#ifdef WIN32
        DeleteFileA(file_path.c_str());
        return 0;
#else
        return unlink(file_path.c_str());
#endif
    }
    default: {
        LOG_CRITICAL("Unknown file {} used. Report this to developers!", file);
        return -1;
    }
    }
}

int create_dir(const char *dir, int mode, const char *pref_path) {
    VitaIoDevice device;
    std::string device_name;
    std::tie(device, device_name) = translate_device(dir);

    switch (device) {
    case VitaIoDevice::UX0:
    case VitaIoDevice::UMA0: {
        std::string dir_path = translate_path(device_name, dir, pref_path);

#ifdef WIN32
        CreateDirectoryA(dir_path.c_str(), nullptr);
        return 0;
#else
        return mkdir(dir_path.c_str(), mode);
#endif
    }
    default: {
        LOG_CRITICAL("Unknown dir {} used. Report this to developers!", dir);
        return -1;
    }
    }
}

int remove_dir(const char *dir, const char *pref_path) {
    VitaIoDevice device;
    std::string device_name;
    std::tie(device, device_name) = translate_device(dir);

    switch (device) {
    case VitaIoDevice::UX0:
    case VitaIoDevice::UMA0: {
        std::string dir_path = translate_path(device_name, dir, pref_path);

#ifdef WIN32
        RemoveDirectoryA(dir_path.c_str());
        return 0;
#else
        return rmdir(dir_path.c_str());
#endif
    }
    default: {
        LOG_CRITICAL("Unknown dir {} used. Report this to developers!", dir);
        return -1;
    }
    }
}

int stat_file(const char *file, SceIoStat *statp, const char *pref_path) {
    assert(statp != NULL);

    memset(statp, '\0', sizeof(SceIoStat));

    VitaIoDevice device;
    std::string device_name;
    std::tie(device, device_name) = translate_device(file);

    std::string file_path;

    switch (device) {
    case VitaIoDevice::UX0:
    case VitaIoDevice::UMA0: {
        file_path = translate_path(device_name, file, pref_path);
        break;
    }
    default: {
        LOG_CRITICAL("Unknown file {} used. Report this to developers!", file);
        return -1;
    }
    }

#ifdef WIN32
    WIN32_FIND_DATAW find_data;
    HANDLE handle = FindFirstFileW(utf_to_wide(file_path).c_str(), &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return -1;
    }
    FindClose(handle);

    statp->st_mode = SCE_S_IFREG;
    if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
        statp->st_mode = SCE_S_IFDIR;
    }

    statp->st_size = (find_data.nFileSizeHigh * ((size_t)MAXDWORD + 1)) + find_data.nFileSizeLow;
#else
    struct stat sb;
    if (stat(file_path.c_str(), &sb) < 0) {
        return -1;
    }

    if (S_ISREG(sb.st_mode)) {
        statp->st_mode = SCE_S_IFREG;
    } else if (S_ISDIR(sb.st_mode)) {
        statp->st_mode = SCE_S_IFDIR;
    }

    statp->st_size = sb.st_size;
#endif

    return 0;
}

int open_dir(IOState &io, const char *path, const char *pref_path) {
    VitaIoDevice device;
    std::string device_name;
    std::tie(device, device_name) = translate_device(path);

    std::string dir_path;
    switch (device) {
    case VitaIoDevice::UX0:
    case VitaIoDevice::UMA0: {
        dir_path = translate_path(device_name, path, pref_path);
        break;
    }
    default: {
        LOG_CRITICAL("Unknown dir {} used. Report this to developers!", path);
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

int read_dir(IOState &io, SceUID fd, SceIoDirent *dent) {
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
