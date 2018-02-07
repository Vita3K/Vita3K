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

#include <SceAvcodecUser/exports.h>

EXPORT(int, sceAvcencCreateEncoder) {
    return unimplemented("sceAvcencCreateEncoder");
}

EXPORT(int, sceAvcencCreateEncoderBasic) {
    return unimplemented("sceAvcencCreateEncoderBasic");
}

EXPORT(int, sceAvcencCreateEncoderInternal) {
    return unimplemented("sceAvcencCreateEncoderInternal");
}

EXPORT(int, sceAvcencCsc) {
    return unimplemented("sceAvcencCsc");
}

EXPORT(int, sceAvcencDeleteEncoder) {
    return unimplemented("sceAvcencDeleteEncoder");
}

EXPORT(int, sceAvcencEncode) {
    return unimplemented("sceAvcencEncode");
}

EXPORT(int, sceAvcencEncodeFlush) {
    return unimplemented("sceAvcencEncodeFlush");
}

EXPORT(int, sceAvcencEncodeStop) {
    return unimplemented("sceAvcencEncodeStop");
}

EXPORT(int, sceAvcencGetNalUnit) {
    return unimplemented("sceAvcencGetNalUnit");
}

EXPORT(int, sceAvcencQueryEncoderMemSize) {
    return unimplemented("sceAvcencQueryEncoderMemSize");
}

EXPORT(int, sceAvcencQueryEncoderMemSizeBasic) {
    return unimplemented("sceAvcencQueryEncoderMemSizeBasic");
}

EXPORT(int, sceAvcencQueryEncoderMemSizeInternal) {
    return unimplemented("sceAvcencQueryEncoderMemSizeInternal");
}

EXPORT(int, sceAvcencSetAvailablePreset) {
    return unimplemented("sceAvcencSetAvailablePreset");
}

EXPORT(int, sceAvcencSetEncoderParameter) {
    return unimplemented("sceAvcencSetEncoderParameter");
}

EXPORT(int, sceVideoencInitLibrary) {
    return unimplemented("sceVideoencInitLibrary");
}

EXPORT(int, sceVideoencInitLibraryInternal) {
    return unimplemented("sceVideoencInitLibraryInternal");
}

EXPORT(int, sceVideoencInitLibraryWithUnmapMem) {
    return unimplemented("sceVideoencInitLibraryWithUnmapMem");
}

EXPORT(int, sceVideoencInitLibraryWithUnmapMemInternal) {
    return unimplemented("sceVideoencInitLibraryWithUnmapMemInternal");
}

EXPORT(int, sceVideoencQueryMemSize) {
    return unimplemented("sceVideoencQueryMemSize");
}

EXPORT(int, sceVideoencQueryMemSizeInternal) {
    return unimplemented("sceVideoencQueryMemSizeInternal");
}

EXPORT(int, sceVideoencTermLibrary) {
    return unimplemented("sceVideoencTermLibrary");
}

BRIDGE_IMPL(sceAvcencCreateEncoder)
BRIDGE_IMPL(sceAvcencCreateEncoderBasic)
BRIDGE_IMPL(sceAvcencCreateEncoderInternal)
BRIDGE_IMPL(sceAvcencCsc)
BRIDGE_IMPL(sceAvcencDeleteEncoder)
BRIDGE_IMPL(sceAvcencEncode)
BRIDGE_IMPL(sceAvcencEncodeFlush)
BRIDGE_IMPL(sceAvcencEncodeStop)
BRIDGE_IMPL(sceAvcencGetNalUnit)
BRIDGE_IMPL(sceAvcencQueryEncoderMemSize)
BRIDGE_IMPL(sceAvcencQueryEncoderMemSizeBasic)
BRIDGE_IMPL(sceAvcencQueryEncoderMemSizeInternal)
BRIDGE_IMPL(sceAvcencSetAvailablePreset)
BRIDGE_IMPL(sceAvcencSetEncoderParameter)
BRIDGE_IMPL(sceVideoencInitLibrary)
BRIDGE_IMPL(sceVideoencInitLibraryInternal)
BRIDGE_IMPL(sceVideoencInitLibraryWithUnmapMem)
BRIDGE_IMPL(sceVideoencInitLibraryWithUnmapMemInternal)
BRIDGE_IMPL(sceVideoencQueryMemSize)
BRIDGE_IMPL(sceVideoencQueryMemSizeInternal)
BRIDGE_IMPL(sceVideoencTermLibrary)
