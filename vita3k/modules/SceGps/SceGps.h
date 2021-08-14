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

#pragma once

#include <module/module.h>

BRIDGE_DECL(_sceGpsClose)
BRIDGE_DECL(_sceGpsGetData)
BRIDGE_DECL(_sceGpsGetDeviceInfo)
BRIDGE_DECL(_sceGpsGetState)
BRIDGE_DECL(_sceGpsIoctl)
BRIDGE_DECL(_sceGpsIsDevice)
BRIDGE_DECL(_sceGpsOpen)
BRIDGE_DECL(_sceGpsResumeCallback)
BRIDGE_DECL(_sceGpsSelectDevice)
BRIDGE_DECL(_sceGpsStart)
BRIDGE_DECL(_sceGpsStop)
