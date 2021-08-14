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

#include "SceUsbSerial.h"

EXPORT(int, sceUsbSerialClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbSerialGetRecvBufferSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbSerialRecv) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbSerialSend) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbSerialSetup) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbSerialStart) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbSerialStatus) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceUsbSerialClose)
BRIDGE_IMPL(sceUsbSerialGetRecvBufferSize)
BRIDGE_IMPL(sceUsbSerialRecv)
BRIDGE_IMPL(sceUsbSerialSend)
BRIDGE_IMPL(sceUsbSerialSetup)
BRIDGE_IMPL(sceUsbSerialStart)
BRIDGE_IMPL(sceUsbSerialStatus)
