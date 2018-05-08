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
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecCreateDecoder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecCreateDecoderExternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecCreateDecoderResident) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecDecode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecDecodeNFrames) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecDecodeNStreams) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecDeleteDecoder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecDeleteDecoderExternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecDeleteDecoderResident) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecGetContextSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecGetInternalError) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecInitLibrary) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecPartlyDecode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecTermLibrary) {
    return UNIMPLEMENTED();
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
