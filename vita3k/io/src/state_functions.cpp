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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <io.h>
#else
#define _FILE_OFFSET_BITS 64
#include <cstdio>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <io/state.h>
#include <pfsmgr/functions.h>
#include <util/log.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

FileStats::FileStats(const char *vita, const fs::path &file, int open, std::shared_ptr<pfsmgr::File> pfs)
    : pfs_file(std::move(pfs))
    , pfs_offset(std::make_shared<SceOff>(0)) {
    file_info.vita_loc = vita;
    file_info.translated = vita;
    file_info.sys_loc = file;
    file_info.open_mode = open;
    file_info.file_mode = SCE_SO_IFREG | SCE_SO_IROTH;
    file_info.access_mode = SCE_S_IFREG;
}

static const uint32_t page_size = []() -> uint32_t {
#ifdef _WIN32
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    return system_info.dwPageSize;
#else
    return static_cast<uint32_t>(sysconf(_SC_PAGESIZE));
#endif
}();

SceOff FileStats::read(void *input_data, const int element_size, const SceSize element_count) const {
    const auto lock = lock_operations();
    if (!is_open())
        return -1;

    if (element_size == 0 || element_count == 0)
        return 0;

    // we are filling this buffer this data, why would we have to set some parts to 0 before ?
    // that's because host io does not work well with memory trapping and read-only buffer
    // so set 1 byte to 0 in all pages to trigger all possible pagefaults in this range
    // todo: call a mem function to check this instead
    volatile uint8_t *input_addr = reinterpret_cast<volatile uint8_t *>(input_data);
    for (int i = 0; i < element_size * element_count; i += page_size)
        input_addr[i] = 0;
    input_addr[element_size * element_count - 1] = 0;

    const auto bytes = read_unlocked(input_data, static_cast<size_t>(element_size) * element_count);
    return bytes < 0 ? bytes : bytes / element_size;
}

SceOff FileStats::read_unlocked(void *input_data, size_t length) const {
    if (!pfs_file)
        return static_cast<SceOff>(fread(input_data, 1, length, wrapped_file.get()));
    try {
        if (savedata_access) {
            const auto session = savedata_access->load_session();
            if (session) {
                const auto data = session->read(savedata_relative);
                if (!data || savedata_access->needs_recovery.load())
                    throw std::runtime_error("savedata file is unavailable or requires recovery");
                const auto position = static_cast<uint64_t>(*pfs_offset);
                if (position >= data->size())
                    return 0;
                const auto count = std::min(length, data->size() - static_cast<size_t>(position));
                std::memcpy(input_data, data->data() + static_cast<size_t>(position), count);
                *pfs_offset += static_cast<SceOff>(count);
                return static_cast<SceOff>(count);
            }
        }
        const auto count = pfs_file->read(static_cast<uint64_t>(*pfs_offset), input_data, length);
        *pfs_offset += static_cast<SceOff>(count);
        return static_cast<SceOff>(count);
    } catch (const std::runtime_error &error) {
        LOG_ERROR("Cannot read PFS file '{}': {}", file_info.vita_loc, error.what());
        return -1;
    }
}

bool FileStats::has_read_error_unlocked() const {
    return wrapped_file && std::ferror(wrapped_file.get());
}

SceOff FileStats::write(const void *data, const SceSize size, const int count) const {
    const auto lock = lock_operations();
    if (!can_write_file())
        return -1;

    return fwrite(data, size, count, get_file_pointer());
}

int FileStats::truncate(const SceSize size) const {
    const auto lock = lock_operations();
#ifdef _WIN32
    return _chsize_s(_fileno(get_file_pointer()), size);
#else
    return ftruncate(fileno(get_file_pointer()), size);
#endif
}

bool FileStats::seek(const SceOff offset, const SceIoSeekMode seek_mode) const {
    const auto lock = lock_operations();
    if (!is_open())
        return false;

    if (pfs_file) {
        SceOff base = 0;
        if (seek_mode == SCE_SEEK_CUR)
            base = *pfs_offset;
        else if (seek_mode == SCE_SEEK_END) {
            try {
                const auto session = savedata_access ? savedata_access->load_session() : nullptr;
                if (session) {
                    const auto data = session->read(savedata_relative);
                    if (!data || savedata_access->needs_recovery.load())
                        throw std::runtime_error("savedata file is unavailable or requires recovery");
                    base = static_cast<SceOff>(data->size());
                } else {
                    base = static_cast<SceOff>(pfs_file->size());
                }
            } catch (const std::runtime_error &error) {
                LOG_ERROR("Cannot seek PFS file '{}': {}", file_info.vita_loc, error.what());
                return false;
            }
        } else if (seek_mode != SCE_SEEK_SET)
            return false;
        if ((offset < 0 && offset < -base) || (offset > 0 && base > std::numeric_limits<SceOff>::max() - offset))
            return false;
        *pfs_offset = base + offset;
        return true;
    }

    auto base = SEEK_SET;
    switch (seek_mode) {
    case SCE_SEEK_SET:
        base = SEEK_SET;
        break;
    case SCE_SEEK_CUR:
        base = SEEK_CUR;
        break;
    case SCE_SEEK_END:
        base = SEEK_END;
        break;
    default:
        return false;
    }

#ifdef _WIN32
    return _fseeki64(wrapped_file.get(), offset, base) == 0;
#else
    return fseeko(wrapped_file.get(), offset, base) == 0;
#endif
}

SceOff FileStats::tell() const {
    const auto lock = lock_operations();
    if (pfs_file)
        return *pfs_offset;
    if (!wrapped_file)
        return -1;

#ifdef _WIN32
    return _ftelli64(wrapped_file.get());
#else
    return ftello(wrapped_file.get());
#endif
}
