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

#include "SceSharedFb.h"

EXPORT(int, _sceSharedFbOpen) {
    return unimplemented("_sceSharedFbOpen");
}

EXPORT(int, sceSharedFbBegin) {
    return unimplemented("sceSharedFbBegin");
}

EXPORT(int, sceSharedFbClose) {
    return unimplemented("sceSharedFbClose");
}

EXPORT(int, sceSharedFbCreate) {
    return unimplemented("sceSharedFbCreate");
}

EXPORT(int, sceSharedFbDelete) {
    return unimplemented("sceSharedFbDelete");
}

EXPORT(int, sceSharedFbEnd) {
    return unimplemented("sceSharedFbEnd");
}

EXPORT(int, sceSharedFbGetInfo) {
    return unimplemented("sceSharedFbGetInfo");
}

EXPORT(int, sceSharedFbGetRenderingInfo) {
    return unimplemented("sceSharedFbGetRenderingInfo");
}

EXPORT(int, sceSharedFbGetShellRenderPort) {
    return unimplemented("sceSharedFbGetShellRenderPort");
}

EXPORT(int, sceSharedFbUpdateProcess) {
    return unimplemented("sceSharedFbUpdateProcess");
}

EXPORT(int, sceSharedFbUpdateProcessBegin) {
    return unimplemented("sceSharedFbUpdateProcessBegin");
}

EXPORT(int, sceSharedFbUpdateProcessEnd) {
    return unimplemented("sceSharedFbUpdateProcessEnd");
}

BRIDGE_IMPL(_sceSharedFbOpen)
BRIDGE_IMPL(sceSharedFbBegin)
BRIDGE_IMPL(sceSharedFbClose)
BRIDGE_IMPL(sceSharedFbCreate)
BRIDGE_IMPL(sceSharedFbDelete)
BRIDGE_IMPL(sceSharedFbEnd)
BRIDGE_IMPL(sceSharedFbGetInfo)
BRIDGE_IMPL(sceSharedFbGetRenderingInfo)
BRIDGE_IMPL(sceSharedFbGetShellRenderPort)
BRIDGE_IMPL(sceSharedFbUpdateProcess)
BRIDGE_IMPL(sceSharedFbUpdateProcessBegin)
BRIDGE_IMPL(sceSharedFbUpdateProcessEnd)
