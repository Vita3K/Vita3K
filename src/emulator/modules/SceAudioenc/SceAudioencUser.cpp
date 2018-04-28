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

#include "SceAudioencUser.h"

EXPORT(int, sceAudioencClearContext) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudioencCreateEncoder) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudioencCreateEncoderExternal) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudioencCreateEncoderResident) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudioencDeleteEncoder) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudioencDeleteEncoderExternal) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudioencDeleteEncoderResident) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudioencEncode) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudioencEncodeNFrames) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudioencGetContextSize) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudioencGetInternalError) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudioencGetOptInfo) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudioencInitLibrary) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudioencTermLibrary) {
    return unimplemented(export_name);
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
