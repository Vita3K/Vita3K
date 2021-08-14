// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include "SceVideoencUser.h"

EXPORT(int, sceAvcencCreateEncoder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencCreateEncoderBasic) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencCreateEncoderInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencCsc) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencDeleteEncoder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencEncode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencEncodeFlush) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencEncodeStop) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencGetNalUnit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencQueryEncoderMemSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencQueryEncoderMemSizeBasic) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencQueryEncoderMemSizeInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencSetAvailablePreset) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAvcencSetEncoderParameter) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencInitLibrary) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencInitLibraryInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencInitLibraryWithUnmapMem) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencInitLibraryWithUnmapMemInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencQueryMemSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencQueryMemSizeInternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoencTermLibrary) {
    return UNIMPLEMENTED();
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
