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

#pragma once

#include <module/module.h>

// SceUsbdForUser
BRIDGE_DECL(sceUsbdAttach)
BRIDGE_DECL(sceUsbdAttachCompositeLdd)
BRIDGE_DECL(sceUsbdClosePipe)
BRIDGE_DECL(sceUsbdEnd)
BRIDGE_DECL(sceUsbdGetDescriptor)
BRIDGE_DECL(sceUsbdGetDescriptorSize)
BRIDGE_DECL(sceUsbdGetDeviceAddress)
BRIDGE_DECL(sceUsbdGetDeviceList)
BRIDGE_DECL(sceUsbdGetDeviceSpeed)
BRIDGE_DECL(sceUsbdGetIsochTransferStatus)
BRIDGE_DECL(sceUsbdGetTransferStatus)
BRIDGE_DECL(sceUsbdInit)
BRIDGE_DECL(sceUsbdIsochTransferData)
BRIDGE_DECL(sceUsbdOpenDefaultPipe)
BRIDGE_DECL(sceUsbdOpenPipe)
BRIDGE_DECL(sceUsbdReceiveEvent)
BRIDGE_DECL(sceUsbdRegisterCallback)
BRIDGE_DECL(sceUsbdRegisterCompositeLdd)
BRIDGE_DECL(sceUsbdRegisterLdd)
BRIDGE_DECL(sceUsbdResetDevice)
BRIDGE_DECL(sceUsbdTransferData)
BRIDGE_DECL(sceUsbdUnregisterCallback)
BRIDGE_DECL(sceUsbdUnregisterLdd)
