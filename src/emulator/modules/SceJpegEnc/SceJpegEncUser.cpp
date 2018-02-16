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

#include "SceJpegEncUser.h"

EXPORT(int, sceJpegEncoderCsc) {
    return unimplemented("sceJpegEncoderCsc");
}

EXPORT(int, sceJpegEncoderEncode) {
    return unimplemented("sceJpegEncoderEncode");
}

EXPORT(int, sceJpegEncoderEnd) {
    return unimplemented("sceJpegEncoderEnd");
}

EXPORT(int, sceJpegEncoderGetContextSize) {
    return unimplemented("sceJpegEncoderGetContextSize");
}

EXPORT(int, sceJpegEncoderInit) {
    return unimplemented("sceJpegEncoderInit");
}

EXPORT(int, sceJpegEncoderInitWithParam) {
    return unimplemented("sceJpegEncoderInitWithParam");
}

EXPORT(int, sceJpegEncoderSetCompressionRatio) {
    return unimplemented("sceJpegEncoderSetCompressionRatio");
}

EXPORT(int, sceJpegEncoderSetHeaderMode) {
    return unimplemented("sceJpegEncoderSetHeaderMode");
}

EXPORT(int, sceJpegEncoderSetOutputAddr) {
    return unimplemented("sceJpegEncoderSetOutputAddr");
}

EXPORT(int, sceJpegEncoderSetValidRegion) {
    return unimplemented("sceJpegEncoderSetValidRegion");
}

BRIDGE_IMPL(sceJpegEncoderCsc)
BRIDGE_IMPL(sceJpegEncoderEncode)
BRIDGE_IMPL(sceJpegEncoderEnd)
BRIDGE_IMPL(sceJpegEncoderGetContextSize)
BRIDGE_IMPL(sceJpegEncoderInit)
BRIDGE_IMPL(sceJpegEncoderInitWithParam)
BRIDGE_IMPL(sceJpegEncoderSetCompressionRatio)
BRIDGE_IMPL(sceJpegEncoderSetHeaderMode)
BRIDGE_IMPL(sceJpegEncoderSetOutputAddr)
BRIDGE_IMPL(sceJpegEncoderSetValidRegion)
