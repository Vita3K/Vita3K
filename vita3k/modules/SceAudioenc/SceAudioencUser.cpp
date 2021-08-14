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

#include "SceAudioencUser.h"

EXPORT(int, sceAudioencClearContext) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioencCreateEncoder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioencCreateEncoderExternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioencCreateEncoderResident) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioencDeleteEncoder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioencDeleteEncoderExternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioencDeleteEncoderResident) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioencEncode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioencEncodeNFrames) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioencGetContextSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioencGetInternalError) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioencGetOptInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioencInitLibrary) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioencTermLibrary) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceAudioencClearContext)
BRIDGE_IMPL(sceAudioencCreateEncoder)
BRIDGE_IMPL(sceAudioencCreateEncoderExternal)
BRIDGE_IMPL(sceAudioencCreateEncoderResident)
BRIDGE_IMPL(sceAudioencDeleteEncoder)
BRIDGE_IMPL(sceAudioencDeleteEncoderExternal)
BRIDGE_IMPL(sceAudioencDeleteEncoderResident)
BRIDGE_IMPL(sceAudioencEncode)
BRIDGE_IMPL(sceAudioencEncodeNFrames)
BRIDGE_IMPL(sceAudioencGetContextSize)
BRIDGE_IMPL(sceAudioencGetInternalError)
BRIDGE_IMPL(sceAudioencGetOptInfo)
BRIDGE_IMPL(sceAudioencInitLibrary)
BRIDGE_IMPL(sceAudioencTermLibrary)
