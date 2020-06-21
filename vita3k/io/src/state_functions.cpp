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

#include <io/state.h>

#ifdef _WIN32
#include <io.h>
#else
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#endif
SceOff FileStats::read(void *input_data, const int element_size, const SceSize element_count) const {
    if (!wrapped_file)
        return -1;

    return fread(input_data, element_size, element_count, wrapped_file.get());
}

SceOff FileStats::write(const void *data, const SceSize size, const int count) const {
    if (!can_write_file())
        return -1;

    return fwrite(data, size, count, get_file_pointer());
}

int FileStats::truncate(const SceSize size) const {
#ifdef _WIN32
    return _chsize_s(_fileno(get_file_pointer()), size);
#else
    return ftruncate(fileno(get_file_pointer()), size);
#endif
}

bool FileStats::seek(const SceOff offset, const SceIoSeekMode seek_mode) const {
    if (!wrapped_file)
        return false;

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

    return fseek(wrapped_file.get(), static_cast<long>(offset), base) == 0;
}

SceOff FileStats::tell() const {
    if (!wrapped_file)
        return -1;

    return ftell(wrapped_file.get());
}
