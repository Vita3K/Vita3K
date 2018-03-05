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

#include "SceUsbPspcm.h"

EXPORT(int, sceUsbPspcmClose) {
    return unimplemented("sceUsbPspcmClose");
}

EXPORT(int, sceUsbPspcmDevctl) {
    return unimplemented("sceUsbPspcmDevctl");
}

EXPORT(int, sceUsbPspcmIoctlCancelWaitData) {
    return unimplemented("sceUsbPspcmIoctlCancelWaitData");
}

EXPORT(int, sceUsbPspcmIoctlPollData) {
    return unimplemented("sceUsbPspcmIoctlPollData");
}

EXPORT(int, sceUsbPspcmIoctlReset) {
    return unimplemented("sceUsbPspcmIoctlReset");
}

EXPORT(int, sceUsbPspcmIoctlWaitData) {
    return unimplemented("sceUsbPspcmIoctlWaitData");
}

EXPORT(int, sceUsbPspcmOpen) {
    return unimplemented("sceUsbPspcmOpen");
}

EXPORT(int, sceUsbPspcmRead) {
    return unimplemented("sceUsbPspcmRead");
}

EXPORT(int, sceUsbPspcmSetTitle) {
    return unimplemented("sceUsbPspcmSetTitle");
}

EXPORT(int, sceUsbPspcmStartDevice) {
    return unimplemented("sceUsbPspcmStartDevice");
}

EXPORT(int, sceUsbPspcmStopDevice) {
    return unimplemented("sceUsbPspcmStopDevice");
}

EXPORT(int, sceUsbPspcmWrite) {
    return unimplemented("sceUsbPspcmWrite");
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
