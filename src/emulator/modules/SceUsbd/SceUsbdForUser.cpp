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
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdAttachCompositeLdd) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdClosePipe) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdEnd) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdGetDescriptor) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdGetDescriptorSize) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdGetDeviceAddress) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdGetDeviceList) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdGetDeviceSpeed) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdGetIsochTransferStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdGetTransferStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdIsochTransferData) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdOpenDefaultPipe) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdOpenPipe) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdReceiveEvent) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdRegisterCallback) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdRegisterCompositeLdd) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdRegisterLdd) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdResetDevice) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdTransferData) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdUnregisterCallback) {
    return unimplemented(export_name);
}

EXPORT(int, sceUsbdUnregisterLdd) {
    return unimplemented(export_name);
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
