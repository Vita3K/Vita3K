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

#include "SceUsbPspcm.h"

EXPORT(int, sceUsbPspcmClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbPspcmDevctl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbPspcmIoctlCancelWaitData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbPspcmIoctlPollData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbPspcmIoctlReset) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbPspcmIoctlWaitData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbPspcmOpen) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbPspcmRead) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbPspcmSetTitle) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbPspcmStartDevice) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbPspcmStopDevice) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbPspcmWrite) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceUsbPspcmClose)
BRIDGE_IMPL(sceUsbPspcmDevctl)
BRIDGE_IMPL(sceUsbPspcmIoctlCancelWaitData)
BRIDGE_IMPL(sceUsbPspcmIoctlPollData)
BRIDGE_IMPL(sceUsbPspcmIoctlReset)
BRIDGE_IMPL(sceUsbPspcmIoctlWaitData)
BRIDGE_IMPL(sceUsbPspcmOpen)
BRIDGE_IMPL(sceUsbPspcmRead)
BRIDGE_IMPL(sceUsbPspcmSetTitle)
BRIDGE_IMPL(sceUsbPspcmStartDevice)
BRIDGE_IMPL(sceUsbPspcmStopDevice)
BRIDGE_IMPL(sceUsbPspcmWrite)
