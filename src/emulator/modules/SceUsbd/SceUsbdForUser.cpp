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
    return unimplemented("sceUsbdAttach");
}

EXPORT(int, sceUsbdAttachCompositeLdd) {
    return unimplemented("sceUsbdAttachCompositeLdd");
}

EXPORT(int, sceUsbdClosePipe) {
    return unimplemented("sceUsbdClosePipe");
}

EXPORT(int, sceUsbdEnd) {
    return unimplemented("sceUsbdEnd");
}

EXPORT(int, sceUsbdGetDescriptor) {
    return unimplemented("sceUsbdGetDescriptor");
}

EXPORT(int, sceUsbdGetDescriptorSize) {
    return unimplemented("sceUsbdGetDescriptorSize");
}

EXPORT(int, sceUsbdGetDeviceAddress) {
    return unimplemented("sceUsbdGetDeviceAddress");
}

EXPORT(int, sceUsbdGetDeviceList) {
    return unimplemented("sceUsbdGetDeviceList");
}

EXPORT(int, sceUsbdGetDeviceSpeed) {
    return unimplemented("sceUsbdGetDeviceSpeed");
}

EXPORT(int, sceUsbdGetIsochTransferStatus) {
    return unimplemented("sceUsbdGetIsochTransferStatus");
}

EXPORT(int, sceUsbdGetTransferStatus) {
    return unimplemented("sceUsbdGetTransferStatus");
}

EXPORT(int, sceUsbdInit) {
    return unimplemented("sceUsbdInit");
}

EXPORT(int, sceUsbdIsochTransferData) {
    return unimplemented("sceUsbdIsochTransferData");
}

EXPORT(int, sceUsbdOpenDefaultPipe) {
    return unimplemented("sceUsbdOpenDefaultPipe");
}

EXPORT(int, sceUsbdOpenPipe) {
    return unimplemented("sceUsbdOpenPipe");
}

EXPORT(int, sceUsbdReceiveEvent) {
    return unimplemented("sceUsbdReceiveEvent");
}

EXPORT(int, sceUsbdRegisterCallback) {
    return unimplemented("sceUsbdRegisterCallback");
}

EXPORT(int, sceUsbdRegisterCompositeLdd) {
    return unimplemented("sceUsbdRegisterCompositeLdd");
}

EXPORT(int, sceUsbdRegisterLdd) {
    return unimplemented("sceUsbdRegisterLdd");
}

EXPORT(int, sceUsbdResetDevice) {
    return unimplemented("sceUsbdResetDevice");
}

EXPORT(int, sceUsbdTransferData) {
    return unimplemented("sceUsbdTransferData");
}

EXPORT(int, sceUsbdUnregisterCallback) {
    return unimplemented("sceUsbdUnregisterCallback");
}

EXPORT(int, sceUsbdUnregisterLdd) {
    return unimplemented("sceUsbdUnregisterLdd");
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
