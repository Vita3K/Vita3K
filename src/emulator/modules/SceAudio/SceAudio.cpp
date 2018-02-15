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

#include "SceAudio.h"

EXPORT(int, sceAudioOutGetAdopt) {
    return unimplemented("sceAudioOutGetAdopt");
}

EXPORT(int, sceAudioOutGetConfig) {
    return unimplemented("sceAudioOutGetConfig");
}

EXPORT(int, sceAudioOutGetRestSample) {
    return unimplemented("sceAudioOutGetRestSample");
}

EXPORT(int, sceAudioOutOpenPort) {
    return unimplemented("sceAudioOutOpenPort");
}

EXPORT(int, sceAudioOutOutput) {
    return unimplemented("sceAudioOutOutput");
}

EXPORT(int, sceAudioOutReleasePort) {
    return unimplemented("sceAudioOutReleasePort");
}

EXPORT(int, sceAudioOutSetAlcMode) {
    return unimplemented("sceAudioOutSetAlcMode");
}

EXPORT(int, sceAudioOutSetConfig) {
    return unimplemented("sceAudioOutSetConfig");
}

EXPORT(int, sceAudioOutSetVolume) {
    return unimplemented("sceAudioOutSetVolume");
}

BRIDGE_IMPL(sceAudioOutGetAdopt)
BRIDGE_IMPL(sceAudioOutGetConfig)
BRIDGE_IMPL(sceAudioOutGetRestSample)
BRIDGE_IMPL(sceAudioOutOpenPort)
BRIDGE_IMPL(sceAudioOutOutput)
BRIDGE_IMPL(sceAudioOutReleasePort)
BRIDGE_IMPL(sceAudioOutSetAlcMode)
BRIDGE_IMPL(sceAudioOutSetConfig)
BRIDGE_IMPL(sceAudioOutSetVolume)
