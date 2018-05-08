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

#include "SceAudioIn.h"

EXPORT(int, sceAudioInGetAdopt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInInput) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInOpenPort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioInReleasePort) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceAudioInGetAdopt)
BRIDGE_IMPL(sceAudioInGetStatus)
BRIDGE_IMPL(sceAudioInInput)
BRIDGE_IMPL(sceAudioInOpenPort)
BRIDGE_IMPL(sceAudioInReleasePort)
