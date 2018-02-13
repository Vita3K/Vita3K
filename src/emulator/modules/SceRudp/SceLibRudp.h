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

BRIDGE_DECL(sceRudpActivate)
BRIDGE_DECL(sceRudpBind)
BRIDGE_DECL(sceRudpCreateContext)
BRIDGE_DECL(sceRudpEnableInternalIOThread)
BRIDGE_DECL(sceRudpEnableInternalIOThread2)
BRIDGE_DECL(sceRudpEnd)
BRIDGE_DECL(sceRudpFlush)
BRIDGE_DECL(sceRudpGetContextStatus)
BRIDGE_DECL(sceRudpGetLocalInfo)
BRIDGE_DECL(sceRudpGetMaxSegmentSize)
BRIDGE_DECL(sceRudpGetNumberOfPacketsToRead)
BRIDGE_DECL(sceRudpGetOption)
BRIDGE_DECL(sceRudpGetRemoteInfo)
BRIDGE_DECL(sceRudpGetSizeReadable)
BRIDGE_DECL(sceRudpGetSizeWritable)
BRIDGE_DECL(sceRudpGetStatus)
BRIDGE_DECL(sceRudpInit)
BRIDGE_DECL(sceRudpInitiate)
BRIDGE_DECL(sceRudpNetReceived)
BRIDGE_DECL(sceRudpPollCancel)
BRIDGE_DECL(sceRudpPollControl)
BRIDGE_DECL(sceRudpPollCreate)
BRIDGE_DECL(sceRudpPollDestroy)
BRIDGE_DECL(sceRudpPollWait)
BRIDGE_DECL(sceRudpProcessEvents)
BRIDGE_DECL(sceRudpRead)
BRIDGE_DECL(sceRudpSetEventHandler)
BRIDGE_DECL(sceRudpSetMaxSegmentSize)
BRIDGE_DECL(sceRudpSetOption)
BRIDGE_DECL(sceRudpTerminate)
BRIDGE_DECL(sceRudpWrite)
