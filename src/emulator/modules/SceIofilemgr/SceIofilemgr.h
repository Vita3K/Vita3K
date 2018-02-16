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

BRIDGE_DECL(sceIoCancel)
BRIDGE_DECL(sceIoClose)
BRIDGE_DECL(sceIoCloseAsync)
BRIDGE_DECL(sceIoComplete)
BRIDGE_DECL(sceIoDclose)
BRIDGE_DECL(sceIoFlockForSystem)
BRIDGE_DECL(sceIoGetPriority)
BRIDGE_DECL(sceIoGetProcessDefaultPriority)
BRIDGE_DECL(sceIoGetThreadDefaultPriority)
BRIDGE_DECL(sceIoLseek32)
BRIDGE_DECL(sceIoRead)
BRIDGE_DECL(sceIoReadAsync)
BRIDGE_DECL(sceIoSetPriority)
BRIDGE_DECL(sceIoSetPriorityForSystem)
BRIDGE_DECL(sceIoSetProcessDefaultPriority)
BRIDGE_DECL(sceIoSetThreadDefaultPriority)
BRIDGE_DECL(sceIoSetThreadDefaultPriorityForSystem)
BRIDGE_DECL(sceIoSyncByFd)
BRIDGE_DECL(sceIoSyncByFdAsync)
BRIDGE_DECL(sceIoWrite)
BRIDGE_DECL(sceIoWriteAsync)
