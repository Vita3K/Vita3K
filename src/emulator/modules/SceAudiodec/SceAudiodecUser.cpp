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

#include "SceAudiodecUser.h"

EXPORT(int, sceAudiodecClearContext) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecCreateDecoder) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecCreateDecoderExternal) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecCreateDecoderResident) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecDecode) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecDecodeNFrames) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecDecodeNStreams) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecDeleteDecoder) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecDeleteDecoderExternal) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecDeleteDecoderResident) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecGetContextSize) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecGetInternalError) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecInitLibrary) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecPartlyDecode) {
    return unimplemented(export_name);
}

EXPORT(int, sceAudiodecTermLibrary) {
    return unimplemented(export_name);
}

BRIDGE_IMPL(sceAudiodecClearContext)
BRIDGE_IMPL(sceAudiodecCreateDecoder)
BRIDGE_IMPL(sceAudiodecCreateDecoderExternal)
BRIDGE_IMPL(sceAudiodecCreateDecoderResident)
BRIDGE_IMPL(sceAudiodecDecode)
BRIDGE_IMPL(sceAudiodecDecodeNFrames)
BRIDGE_IMPL(sceAudiodecDecodeNStreams)
BRIDGE_IMPL(sceAudiodecDeleteDecoder)
BRIDGE_IMPL(sceAudiodecDeleteDecoderExternal)
BRIDGE_IMPL(sceAudiodecDeleteDecoderResident)
BRIDGE_IMPL(sceAudiodecGetContextSize)
BRIDGE_IMPL(sceAudiodecGetInternalError)
BRIDGE_IMPL(sceAudiodecInitLibrary)
BRIDGE_IMPL(sceAudiodecPartlyDecode)
BRIDGE_IMPL(sceAudiodecTermLibrary)
