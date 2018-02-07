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

#include <SceRudp/exports.h>

EXPORT(int, sceRudpActivate) {
    return unimplemented("sceRudpActivate");
}

EXPORT(int, sceRudpBind) {
    return unimplemented("sceRudpBind");
}

EXPORT(int, sceRudpCreateContext) {
    return unimplemented("sceRudpCreateContext");
}

EXPORT(int, sceRudpEnableInternalIOThread) {
    return unimplemented("sceRudpEnableInternalIOThread");
}

EXPORT(int, sceRudpEnableInternalIOThread2) {
    return unimplemented("sceRudpEnableInternalIOThread2");
}

EXPORT(int, sceRudpEnd) {
    return unimplemented("sceRudpEnd");
}

EXPORT(int, sceRudpFlush) {
    return unimplemented("sceRudpFlush");
}

EXPORT(int, sceRudpGetContextStatus) {
    return unimplemented("sceRudpGetContextStatus");
}

EXPORT(int, sceRudpGetLocalInfo) {
    return unimplemented("sceRudpGetLocalInfo");
}

EXPORT(int, sceRudpGetMaxSegmentSize) {
    return unimplemented("sceRudpGetMaxSegmentSize");
}

EXPORT(int, sceRudpGetNumberOfPacketsToRead) {
    return unimplemented("sceRudpGetNumberOfPacketsToRead");
}

EXPORT(int, sceRudpGetOption) {
    return unimplemented("sceRudpGetOption");
}

EXPORT(int, sceRudpGetRemoteInfo) {
    return unimplemented("sceRudpGetRemoteInfo");
}

EXPORT(int, sceRudpGetSizeReadable) {
    return unimplemented("sceRudpGetSizeReadable");
}

EXPORT(int, sceRudpGetSizeWritable) {
    return unimplemented("sceRudpGetSizeWritable");
}

EXPORT(int, sceRudpGetStatus) {
    return unimplemented("sceRudpGetStatus");
}

EXPORT(int, sceRudpInit) {
    return unimplemented("sceRudpInit");
}

EXPORT(int, sceRudpInitiate) {
    return unimplemented("sceRudpInitiate");
}

EXPORT(int, sceRudpNetReceived) {
    return unimplemented("sceRudpNetReceived");
}

EXPORT(int, sceRudpPollCancel) {
    return unimplemented("sceRudpPollCancel");
}

EXPORT(int, sceRudpPollControl) {
    return unimplemented("sceRudpPollControl");
}

EXPORT(int, sceRudpPollCreate) {
    return unimplemented("sceRudpPollCreate");
}

EXPORT(int, sceRudpPollDestroy) {
    return unimplemented("sceRudpPollDestroy");
}

EXPORT(int, sceRudpPollWait) {
    return unimplemented("sceRudpPollWait");
}

EXPORT(int, sceRudpProcessEvents) {
    return unimplemented("sceRudpProcessEvents");
}

EXPORT(int, sceRudpRead) {
    return unimplemented("sceRudpRead");
}

EXPORT(int, sceRudpSetEventHandler) {
    return unimplemented("sceRudpSetEventHandler");
}

EXPORT(int, sceRudpSetMaxSegmentSize) {
    return unimplemented("sceRudpSetMaxSegmentSize");
}

EXPORT(int, sceRudpSetOption) {
    return unimplemented("sceRudpSetOption");
}

EXPORT(int, sceRudpTerminate) {
    return unimplemented("sceRudpTerminate");
}

EXPORT(int, sceRudpWrite) {
    return unimplemented("sceRudpWrite");
}

BRIDGE_IMPL(sceRudpActivate)
BRIDGE_IMPL(sceRudpBind)
BRIDGE_IMPL(sceRudpCreateContext)
BRIDGE_IMPL(sceRudpEnableInternalIOThread)
BRIDGE_IMPL(sceRudpEnableInternalIOThread2)
BRIDGE_IMPL(sceRudpEnd)
BRIDGE_IMPL(sceRudpFlush)
BRIDGE_IMPL(sceRudpGetContextStatus)
BRIDGE_IMPL(sceRudpGetLocalInfo)
BRIDGE_IMPL(sceRudpGetMaxSegmentSize)
BRIDGE_IMPL(sceRudpGetNumberOfPacketsToRead)
BRIDGE_IMPL(sceRudpGetOption)
BRIDGE_IMPL(sceRudpGetRemoteInfo)
BRIDGE_IMPL(sceRudpGetSizeReadable)
BRIDGE_IMPL(sceRudpGetSizeWritable)
BRIDGE_IMPL(sceRudpGetStatus)
BRIDGE_IMPL(sceRudpInit)
BRIDGE_IMPL(sceRudpInitiate)
BRIDGE_IMPL(sceRudpNetReceived)
BRIDGE_IMPL(sceRudpPollCancel)
BRIDGE_IMPL(sceRudpPollControl)
BRIDGE_IMPL(sceRudpPollCreate)
BRIDGE_IMPL(sceRudpPollDestroy)
BRIDGE_IMPL(sceRudpPollWait)
BRIDGE_IMPL(sceRudpProcessEvents)
BRIDGE_IMPL(sceRudpRead)
BRIDGE_IMPL(sceRudpSetEventHandler)
BRIDGE_IMPL(sceRudpSetMaxSegmentSize)
BRIDGE_IMPL(sceRudpSetOption)
BRIDGE_IMPL(sceRudpTerminate)
BRIDGE_IMPL(sceRudpWrite)
