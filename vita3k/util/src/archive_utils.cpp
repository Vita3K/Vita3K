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

#include <util/archive_utils.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>

extern "C" {
#include <7z.h>
#include <7zAlloc.h>
#include <7zCrc.h>
#include <7zTypes.h>
}

#include <cstring>
#include <vector>

namespace {
struct MemSeekInStream {
    ISeekInStream vt;
    const uint8_t *data;
    size_t size;
    size_t pos;
};

SRes mem_read(const ISeekInStream *pp, void *buf, size_t *size) {
    auto *p = reinterpret_cast<MemSeekInStream *>(const_cast<ISeekInStream *>(pp));
    const size_t avail = p->size - p->pos;
    if (*size > avail)
        *size = avail;
    std::memcpy(buf, p->data + p->pos, *size);
    p->pos += *size;
    return SZ_OK;
}

SRes mem_seek(const ISeekInStream *pp, Int64 *pos, ESzSeek origin) {
    auto *p = reinterpret_cast<MemSeekInStream *>(const_cast<ISeekInStream *>(pp));
    Int64 base = 0;
    switch (origin) {
    case SZ_SEEK_SET: base = 0; break;
    case SZ_SEEK_CUR: base = static_cast<Int64>(p->pos); break;
    case SZ_SEEK_END: base = static_cast<Int64>(p->size); break;
    default: return SZ_ERROR_PARAM;
    }
    const Int64 new_pos = base + *pos;
    if (new_pos < 0 || static_cast<size_t>(new_pos) > p->size)
        return SZ_ERROR_PARAM;
    p->pos = static_cast<size_t>(new_pos);
    *pos = new_pos;
    return SZ_OK;
}
} // namespace

namespace archive_utils {

bool extract_7z(std::span<const uint8_t> data, const std::filesystem::path &out_dir) {
    static bool crc_init = [] { CrcGenerateTable(); return true; }();
    (void)crc_init;

    MemSeekInStream mem_stream{};
    mem_stream.vt.Read = mem_read;
    mem_stream.vt.Seek = mem_seek;
    mem_stream.data = data.data();
    mem_stream.size = data.size();
    mem_stream.pos = 0;

    ISzAlloc alloc_main{ SzAlloc, SzFree };
    ISzAlloc alloc_temp{ SzAllocTemp, SzFreeTemp };

    CLookToRead2 look_stream;
    LookToRead2_CreateVTable(&look_stream, False);
    look_stream.realStream = &mem_stream.vt;
    LookToRead2_INIT(&look_stream);

    constexpr size_t kInputBufSize = 1u << 16;
    look_stream.buf = static_cast<Byte *>(ISzAlloc_Alloc(&alloc_main, kInputBufSize));
    if (!look_stream.buf) {
        LOG_ERROR("Failed to allocate 7z input buffer");
        return false;
    }
    look_stream.bufSize = kInputBufSize;

    CSzArEx db;
    SzArEx_Init(&db);

    bool success = false;
    if (SzArEx_Open(&db, &look_stream.vt, &alloc_main, &alloc_temp) == SZ_OK) {
        UInt32 block_index = 0xFFFFFFFF;
        Byte *out_buffer = nullptr;
        size_t out_buffer_size = 0;
        success = true;

        for (UInt32 i = 0; i < db.NumFiles; ++i) {
            if (SzArEx_IsDir(&db, i))
                continue;

            size_t offset = 0;
            size_t out_size_processed = 0;
            if (SzArEx_Extract(&db, &look_stream.vt, i, &block_index, &out_buffer, &out_buffer_size, &offset, &out_size_processed, &alloc_main, &alloc_temp) != SZ_OK) {
                LOG_ERROR("Failed to extract file {} from 7z", i);
                success = false;
                break;
            }

            // By design, filenames within 7z files are stored in UTF-16
            const size_t name_len = SzArEx_GetFileNameUtf16(&db, i, nullptr);
            std::vector<UInt16> name_utf16(name_len);
            SzArEx_GetFileNameUtf16(&db, i, name_utf16.data());

            const std::u16string u16(reinterpret_cast<const char16_t *>(name_utf16.data()), name_len > 0 ? name_len - 1 : 0);
            const std::string utf8_name = string_utils::utf16_to_utf8(u16);
            const fs::path out_path = fs::path(out_dir.native()) / fs_utils::utf8_to_path(utf8_name);

            fs::create_directories(out_path.parent_path());

            fs::ofstream out(out_path, std::ios::binary);
            if (!out) {
                LOG_ERROR("Failed to open {} for writing", out_path);
                success = false;
                break;
            }
            out.write(reinterpret_cast<const char *>(out_buffer + offset), static_cast<std::streamsize>(out_size_processed));
        }

        ISzAlloc_Free(&alloc_main, out_buffer);
    } else {
        LOG_ERROR("Failed to open 7z archive from memory");
    }

    SzArEx_Free(&db, &alloc_main);
    ISzAlloc_Free(&alloc_main, look_stream.buf);

    return success;
}

} // namespace archive_utils
