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

#include "SceUsbdForUser.h"

EXPORT(int, sceUsbdAttach) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdAttachCompositeLdd) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdClosePipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdEnd) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdGetDescriptor) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdGetDescriptorSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdGetDeviceAddress) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdGetDeviceList) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdGetDeviceSpeed) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdGetIsochTransferStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdGetTransferStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdIsochTransferData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdOpenDefaultPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdOpenPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdReceiveEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdRegisterCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdRegisterCompositeLdd) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdRegisterLdd) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdResetDevice) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdTransferData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdUnregisterCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceUsbdUnregisterLdd) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceUsbdAttach)
BRIDGE_IMPL(sceUsbdAttachCompositeLdd)
BRIDGE_IMPL(sceUsbdClosePipe)
BRIDGE_IMPL(sceUsbdEnd)
BRIDGE_IMPL(sceUsbdGetDescriptor)
BRIDGE_IMPL(sceUsbdGetDescriptorSize)
BRIDGE_IMPL(sceUsbdGetDeviceAddress)
BRIDGE_IMPL(sceUsbdGetDeviceList)
BRIDGE_IMPL(sceUsbdGetDeviceSpeed)
BRIDGE_IMPL(sceUsbdGetIsochTransferStatus)
BRIDGE_IMPL(sceUsbdGetTransferStatus)
BRIDGE_IMPL(sceUsbdInit)
BRIDGE_IMPL(sceUsbdIsochTransferData)
BRIDGE_IMPL(sceUsbdOpenDefaultPipe)
BRIDGE_IMPL(sceUsbdOpenPipe)
BRIDGE_IMPL(sceUsbdReceiveEvent)
BRIDGE_IMPL(sceUsbdRegisterCallback)
BRIDGE_IMPL(sceUsbdRegisterCompositeLdd)
BRIDGE_IMPL(sceUsbdRegisterLdd)
BRIDGE_IMPL(sceUsbdResetDevice)
BRIDGE_IMPL(sceUsbdTransferData)
BRIDGE_IMPL(sceUsbdUnregisterCallback)
BRIDGE_IMPL(sceUsbdUnregisterLdd)
