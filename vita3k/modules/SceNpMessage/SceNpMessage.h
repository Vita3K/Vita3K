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

BRIDGE_DECL(sceNpMessageAbort)
BRIDGE_DECL(sceNpMessageGetAttachedData)
BRIDGE_DECL(sceNpMessageGetMessage)
BRIDGE_DECL(sceNpMessageGetMessageEntries)
BRIDGE_DECL(sceNpMessageGetMessageEntry)
BRIDGE_DECL(sceNpMessageGetMessageEntryCount)
BRIDGE_DECL(sceNpMessageInit)
BRIDGE_DECL(sceNpMessageInitWithParam)
BRIDGE_DECL(sceNpMessageSetAttachedDataUsedFlag)
BRIDGE_DECL(sceNpMessageSyncMessage)
BRIDGE_DECL(sceNpMessageTerm)
