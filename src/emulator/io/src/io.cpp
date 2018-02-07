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

#include <io/functions.h>

#include <io/state.h>

#include <psp2/io/fcntl.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/stat.h>
#endif

#include <algorithm>
#include <cassert>
#include <string>
#include <iostream>

static ZipFilePtr open_zip(mz_zip_archive &zip, const char *entry_path) {
    const int index = mz_zip_reader_locate_file(&zip, entry_path, nullptr, 0);
    if (index < 0) {
        return ZipFilePtr();
    }

    const ZipFilePtr zip_file(mz_zip_reader_extract_iter_new(&zip, index, 0), mz_zip_reader_extract_iter_free);
    if (!zip_file) {
        return ZipFilePtr();
    }

    return zip_file;
}

static void delete_file(FILE *file) {
    if (file != nullptr) {
        fclose(file);
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

std::string translate_path(const char *part_name, const char *path, const char* pref_path){
    std::string res = pref_path;
    res += part_name;
    int i = strlen(part_name);
    res += "/";
	
    if (path[i+1] == '/') i++;
    res += &path[i+1];
    
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

SceUID open_file(IOState &io, const char *path, int flags, const char *pref_path) {
    // TODO Hacky magic numbers.
    assert((strcmp(path, "tty0:") == 0) || (strncmp(path, "app0:", 5) == 0) || (strncmp(path, "ux0:", 4) == 0) || (strncmp(path, "uma0:", 5) == 0));

    if (strcmp(path, "tty0:") == 0) {
        assert(flags >= 0);

        TtyType type;
        if(flags==SCE_O_RDONLY){
            type = TTY_IN;
        } else if (flags==SCE_O_WRONLY) {
            type = TTY_OUT;
        }

        const SceUID fd = io.next_fd++;
        io.tty_files.emplace(fd, type);

        return fd;
    } else if (strncmp(path, "app0:", 5) == 0) {
        assert(flags == SCE_O_RDONLY);

        if (!io.vpk) {
            return -1;
        }

        int i = 5;
        if (path[5] == '/') i++;
        const ZipFilePtr file = open_zip(*io.vpk, &path[i]);
		
        if (!file) {
            return -1;
        }

        const SceUID fd = io.next_fd++;
        io.zip_files.emplace(fd, file);

        return fd;
    } else if (strncmp(path, "ux0:", 4) == 0) {
        std::string file_path = translate_path("ux0", path, pref_path);

        const char *const open_mode = translate_open_mode(flags);
        const FilePtr file(fopen(file_path.c_str(), open_mode), delete_file);
        if (!file) {
            return -1;
        }

        const SceUID fd = io.next_fd++;
        io.std_files.emplace(fd, file);

        return fd;
    } else if (strncmp(path, "uma0:", 5) == 0) {
        std::string file_path = translate_path("uma0", path, pref_path);

        const char *const open_mode = translate_open_mode(flags);
        const FilePtr file(fopen(file_path.c_str(), open_mode), delete_file);
        if (!file) {
            return -1;
        }

        const SceUID fd = io.next_fd++;
        io.std_files.emplace(fd, file);

        return fd;
    } else {
        return -1;
    }
}

int read_file(void *data, IOState &io, SceUID fd, SceSize size) {
    assert(data != nullptr);
    assert(fd >= 0);
    assert(size >= 0);

    const ZipFiles::const_iterator zip_file = io.zip_files.find(fd);
    if (zip_file != io.zip_files.end()) {
        return mz_zip_reader_extract_iter_read(zip_file->second.get(), data, size);
    }

    const StdFiles::const_iterator file = io.std_files.find(fd);
    if (file != io.std_files.end()) {
        return fread(data, 1, size, file->second.get());
    }

    const TtyFiles::const_iterator tty_file = io.tty_files.find(fd);
    if (tty_file != io.tty_files.end()) {
        if(tty_file->second == TTY_IN){
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
        if(tty_file->second == TTY_OUT){
            std::string s(reinterpret_cast<char const*>(data),size);
            std::cout << s;
            return size;
        }

        return -1;
    }

    return -1;
}

int seek_file(SceUID fd, int offset, int whence, const IOState &io) {
    assert(fd >= 0);
    assert((whence == SCE_SEEK_SET) || (whence == SCE_SEEK_CUR) || (whence == SCE_SEEK_END));

    const StdFiles::const_iterator file = io.std_files.find(fd);
    assert(file != io.std_files.end());
    if (file == io.std_files.end()) {
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

    const int ret = fseek(file->second.get(), offset, base);
    if (ret != 0) {
        return -1;
    }

    const long pos = ftell(file->second.get());
    return pos;
}

void close_file(IOState &io, SceUID fd) {
    assert(fd >= 0);

    io.tty_files.erase(fd);
    io.std_files.erase(fd);
    io.zip_files.erase(fd);
}

int remove_file(const char *file, const char *pref_path){
    // TODO Hacky magic numbers.
    assert((strncmp(file, "ux0:", 4) == 0) || (strncmp(file, "uma0:", 5) == 0));
    if (strncmp(file, "ux0:", 4) == 0) {
        std::string file_path = translate_path("ux0", file, pref_path);

#ifdef WIN32
        DeleteFileA(file_path.c_str());
        return 0;
#else
        return unlink(file_path.c_str());
#endif
    } else if (strncmp(file, "uma0:", 5) == 0) {
        std::string file_path = translate_path("uma0", file, pref_path);
        
#ifdef WIN32
        DeleteFileA(file_path.c_str());
        return 0;
#else
        return unlink(file_path.c_str());
#endif 
    } else {
        return -1;
    }
}

int create_dir(const char *dir, int mode, const char *pref_path){
    // TODO Hacky magic numbers.
    assert((strncmp(dir, "ux0:", 4) == 0) || (strncmp(dir, "uma0:", 5) == 0));
    if (strncmp(dir, "ux0:", 4) == 0) {
        std::string dir_path = translate_path("ux0", dir, pref_path);

#ifdef WIN32
        CreateDirectoryA(dir_path.c_str(), nullptr);
        return 0;
#else
        return mkdir(dir_path.c_str(), mode);
#endif 
    } else if (strncmp(dir, "uma0:", 5) == 0) {
        std::string dir_path = translate_path("uma0", dir, pref_path);
        
#ifdef WIN32
        CreateDirectoryA(dir_path.c_str(), nullptr);
        return 0;
#else
        return mkdir(dir_path.c_str(), mode);
#endif 
    } else {
        return -1;
    }
}

int remove_dir(const char *dir, const char *pref_path){
    // TODO Hacky magic numbers.
    assert((strncmp(dir, "ux0:", 4) == 0) || (strncmp(dir, "uma0:", 5) == 0));
    if (strncmp(dir, "ux0:", 4) == 0) {
        std::string dir_path = translate_path("ux0", dir, pref_path);

#ifdef WIN32
        RemoveDirectoryA(dir_path.c_str());
        return 0;
#else
        return rmdir(dir_path.c_str());
#endif
    } else if (strncmp(dir, "uma0:", 5) == 0) {
        std::string dir_path = translate_path("uma0", dir, pref_path);
        
#ifdef WIN32
        RemoveDirectoryA(dir_path.c_str());
        return 0;
#else
        return rmdir(dir_path.c_str());
#endif 
    } else {
        return -1;
    }
}
