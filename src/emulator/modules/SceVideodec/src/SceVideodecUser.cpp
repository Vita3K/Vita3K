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

#include <SceVideodec/exports.h>

EXPORT(int, sceAvcdecCreateDecoder) {
    return unimplemented("sceAvcdecCreateDecoder");
}

EXPORT(int, sceAvcdecCsc) {
    return unimplemented("sceAvcdecCsc");
}

EXPORT(int, sceAvcdecCscInternal) {
    return unimplemented("sceAvcdecCscInternal");
}

EXPORT(int, sceAvcdecDecode) {
    return unimplemented("sceAvcdecDecode");
}

EXPORT(int, sceAvcdecDecodeAvailableSize) {
    return unimplemented("sceAvcdecDecodeAvailableSize");
}

EXPORT(int, sceAvcdecDecodeFlush) {
    return unimplemented("sceAvcdecDecodeFlush");
}

EXPORT(int, sceAvcdecDecodeStop) {
    return unimplemented("sceAvcdecDecodeStop");
}

EXPORT(int, sceAvcdecDeleteDecoder) {
    return unimplemented("sceAvcdecDeleteDecoder");
}

EXPORT(int, sceAvcdecQueryDecoderMemSize) {
    return unimplemented("sceAvcdecQueryDecoderMemSize");
}

EXPORT(int, sceM4vdecCreateDecoder) {
    return unimplemented("sceM4vdecCreateDecoder");
}

EXPORT(int, sceM4vdecCsc) {
    return unimplemented("sceM4vdecCsc");
}

EXPORT(int, sceM4vdecDecode) {
    return unimplemented("sceM4vdecDecode");
}

EXPORT(int, sceM4vdecDecodeAvailableSize) {
    return unimplemented("sceM4vdecDecodeAvailableSize");
}

EXPORT(int, sceM4vdecDecodeFlush) {
    return unimplemented("sceM4vdecDecodeFlush");
}

EXPORT(int, sceM4vdecDecodeStop) {
    return unimplemented("sceM4vdecDecodeStop");
}

EXPORT(int, sceM4vdecDeleteDecoder) {
    return unimplemented("sceM4vdecDeleteDecoder");
}

EXPORT(int, sceM4vdecQueryDecoderMemSize) {
    return unimplemented("sceM4vdecQueryDecoderMemSize");
}

EXPORT(int, sceVideodecInitLibrary) {
    return unimplemented("sceVideodecInitLibrary");
}

EXPORT(int, sceVideodecInitLibraryWithUnmapMem) {
    return unimplemented("sceVideodecInitLibraryWithUnmapMem");
}

EXPORT(int, sceVideodecQueryMemSize) {
    return unimplemented("sceVideodecQueryMemSize");
}

EXPORT(int, sceVideodecTermLibrary) {
    return unimplemented("sceVideodecTermLibrary");
}

BRIDGE_IMPL(sceAvcdecCreateDecoder)
BRIDGE_IMPL(sceAvcdecCsc)
BRIDGE_IMPL(sceAvcdecCscInternal)
BRIDGE_IMPL(sceAvcdecDecode)
BRIDGE_IMPL(sceAvcdecDecodeAvailableSize)
BRIDGE_IMPL(sceAvcdecDecodeFlush)
BRIDGE_IMPL(sceAvcdecDecodeStop)
BRIDGE_IMPL(sceAvcdecDeleteDecoder)
BRIDGE_IMPL(sceAvcdecQueryDecoderMemSize)
BRIDGE_IMPL(sceM4vdecCreateDecoder)
BRIDGE_IMPL(sceM4vdecCsc)
BRIDGE_IMPL(sceM4vdecDecode)
BRIDGE_IMPL(sceM4vdecDecodeAvailableSize)
BRIDGE_IMPL(sceM4vdecDecodeFlush)
BRIDGE_IMPL(sceM4vdecDecodeStop)
BRIDGE_IMPL(sceM4vdecDeleteDecoder)
BRIDGE_IMPL(sceM4vdecQueryDecoderMemSize)
BRIDGE_IMPL(sceVideodecInitLibrary)
BRIDGE_IMPL(sceVideodecInitLibraryWithUnmapMem)
BRIDGE_IMPL(sceVideodecQueryMemSize)
BRIDGE_IMPL(sceVideodecTermLibrary)
