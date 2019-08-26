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

#include "SceNpMessage.h"

EXPORT(int, sceNpMessageAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageGetAttachedData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageGetMessage) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageGetMessageEntries) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageGetMessageEntry) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageGetMessageEntryCount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageInitWithParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageSetAttachedDataUsedFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageSyncMessage) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageTerm) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceNpMessageAbort)
BRIDGE_IMPL(sceNpMessageGetAttachedData)
BRIDGE_IMPL(sceNpMessageGetMessage)
BRIDGE_IMPL(sceNpMessageGetMessageEntries)
BRIDGE_IMPL(sceNpMessageGetMessageEntry)
BRIDGE_IMPL(sceNpMessageGetMessageEntryCount)
BRIDGE_IMPL(sceNpMessageInit)
BRIDGE_IMPL(sceNpMessageInitWithParam)
BRIDGE_IMPL(sceNpMessageSetAttachedDataUsedFlag)
BRIDGE_IMPL(sceNpMessageSyncMessage)
BRIDGE_IMPL(sceNpMessageTerm)
