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

BRIDGE_DECL(_sceSharedFbOpen)
BRIDGE_DECL(sceSharedFbBegin)
BRIDGE_DECL(sceSharedFbClose)
BRIDGE_DECL(sceSharedFbCreate)
BRIDGE_DECL(sceSharedFbDelete)
BRIDGE_DECL(sceSharedFbEnd)
BRIDGE_DECL(sceSharedFbGetInfo)
BRIDGE_DECL(sceSharedFbGetRenderingInfo)
BRIDGE_DECL(sceSharedFbGetShellRenderPort)
BRIDGE_DECL(sceSharedFbUpdateProcess)
BRIDGE_DECL(sceSharedFbUpdateProcessBegin)
BRIDGE_DECL(sceSharedFbUpdateProcessEnd)
