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

#include <SceDeflt/exports.h>

EXPORT(int, sceDeflateDecompress) {
    return unimplemented("sceDeflateDecompress");
}

EXPORT(int, sceDeflateDecompressPartial) {
    return unimplemented("sceDeflateDecompressPartial");
}

EXPORT(int, sceGzipCrc32) {
    return unimplemented("sceGzipCrc32");
}

EXPORT(int, sceGzipDecompress) {
    return unimplemented("sceGzipDecompress");
}

EXPORT(int, sceGzipGetComment) {
    return unimplemented("sceGzipGetComment");
}

EXPORT(int, sceGzipGetCompressedData) {
    return unimplemented("sceGzipGetCompressedData");
}

EXPORT(int, sceGzipGetInfo) {
    return unimplemented("sceGzipGetInfo");
}

EXPORT(int, sceGzipGetName) {
    return unimplemented("sceGzipGetName");
}

EXPORT(int, sceGzipIsValid) {
    return unimplemented("sceGzipIsValid");
}

EXPORT(int, sceZipGetInfo) {
    return unimplemented("sceZipGetInfo");
}

EXPORT(int, sceZlibAdler32) {
    return unimplemented("sceZlibAdler32");
}

EXPORT(int, sceZlibDecompress) {
    return unimplemented("sceZlibDecompress");
}

EXPORT(int, sceZlibGetCompressedData) {
    return unimplemented("sceZlibGetCompressedData");
}

EXPORT(int, sceZlibGetInfo) {
    return unimplemented("sceZlibGetInfo");
}

EXPORT(int, sceZlibIsValid) {
    return unimplemented("sceZlibIsValid");
}

BRIDGE_IMPL(sceDeflateDecompress)
BRIDGE_IMPL(sceDeflateDecompressPartial)
BRIDGE_IMPL(sceGzipCrc32)
BRIDGE_IMPL(sceGzipDecompress)
BRIDGE_IMPL(sceGzipGetComment)
BRIDGE_IMPL(sceGzipGetCompressedData)
BRIDGE_IMPL(sceGzipGetInfo)
BRIDGE_IMPL(sceGzipGetName)
BRIDGE_IMPL(sceGzipIsValid)
BRIDGE_IMPL(sceZipGetInfo)
BRIDGE_IMPL(sceZlibAdler32)
BRIDGE_IMPL(sceZlibDecompress)
BRIDGE_IMPL(sceZlibGetCompressedData)
BRIDGE_IMPL(sceZlibGetInfo)
BRIDGE_IMPL(sceZlibIsValid)
