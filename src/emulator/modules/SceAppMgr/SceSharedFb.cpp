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
    return unimplemented(export_name);
}

EXPORT(int, sceSharedFbBegin) {
    return unimplemented(export_name);
}

EXPORT(int, sceSharedFbClose) {
    return unimplemented(export_name);
}

EXPORT(int, sceSharedFbCreate) {
    return unimplemented(export_name);
}

EXPORT(int, sceSharedFbDelete) {
    return unimplemented(export_name);
}

EXPORT(int, sceSharedFbEnd) {
    return unimplemented(export_name);
}

EXPORT(int, sceSharedFbGetInfo) {
    return unimplemented(export_name);
}

EXPORT(int, sceSharedFbGetRenderingInfo) {
    return unimplemented(export_name);
}

EXPORT(int, sceSharedFbGetShellRenderPort) {
    return unimplemented(export_name);
}

EXPORT(int, sceSharedFbUpdateProcess) {
    return unimplemented(export_name);
}

EXPORT(int, sceSharedFbUpdateProcessBegin) {
    return unimplemented(export_name);
}

EXPORT(int, sceSharedFbUpdateProcessEnd) {
    return unimplemented(export_name);
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
