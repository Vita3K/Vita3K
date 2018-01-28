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

#include <SceNpSignaling/exports.h>

EXPORT(int, sceNpSignalingActivateConnection) {
    return unimplemented("sceNpSignalingActivateConnection");
}

EXPORT(int, sceNpSignalingCancelPeerNetInfo) {
    return unimplemented("sceNpSignalingCancelPeerNetInfo");
}

EXPORT(int, sceNpSignalingCreateCtx) {
    return unimplemented("sceNpSignalingCreateCtx");
}

EXPORT(int, sceNpSignalingDeactivateConnection) {
    return unimplemented("sceNpSignalingDeactivateConnection");
}

EXPORT(int, sceNpSignalingDestroyCtx) {
    return unimplemented("sceNpSignalingDestroyCtx");
}

EXPORT(int, sceNpSignalingGetConnectionFromNpId) {
    return unimplemented("sceNpSignalingGetConnectionFromNpId");
}

EXPORT(int, sceNpSignalingGetConnectionFromPeerAddress) {
    return unimplemented("sceNpSignalingGetConnectionFromPeerAddress");
}

EXPORT(int, sceNpSignalingGetConnectionInfo) {
    return unimplemented("sceNpSignalingGetConnectionInfo");
}

EXPORT(int, sceNpSignalingGetConnectionStatus) {
    return unimplemented("sceNpSignalingGetConnectionStatus");
}

EXPORT(int, sceNpSignalingGetCtxOpt) {
    return unimplemented("sceNpSignalingGetCtxOpt");
}

EXPORT(int, sceNpSignalingGetLocalNetInfo) {
    return unimplemented("sceNpSignalingGetLocalNetInfo");
}

EXPORT(int, sceNpSignalingGetMemoryInfo) {
    return unimplemented("sceNpSignalingGetMemoryInfo");
}

EXPORT(int, sceNpSignalingGetPeerNetInfo) {
    return unimplemented("sceNpSignalingGetPeerNetInfo");
}

EXPORT(int, sceNpSignalingGetPeerNetInfoResult) {
    return unimplemented("sceNpSignalingGetPeerNetInfoResult");
}

EXPORT(int, sceNpSignalingInit) {
    return unimplemented("sceNpSignalingInit");
}

EXPORT(int, sceNpSignalingSetCtxOpt) {
    return unimplemented("sceNpSignalingSetCtxOpt");
}

EXPORT(int, sceNpSignalingTerm) {
    return unimplemented("sceNpSignalingTerm");
}

EXPORT(int, sceNpSignalingTerminateConnection) {
    return unimplemented("sceNpSignalingTerminateConnection");
}

BRIDGE_IMPL(sceNpSignalingActivateConnection)
BRIDGE_IMPL(sceNpSignalingCancelPeerNetInfo)
BRIDGE_IMPL(sceNpSignalingCreateCtx)
BRIDGE_IMPL(sceNpSignalingDeactivateConnection)
BRIDGE_IMPL(sceNpSignalingDestroyCtx)
BRIDGE_IMPL(sceNpSignalingGetConnectionFromNpId)
BRIDGE_IMPL(sceNpSignalingGetConnectionFromPeerAddress)
BRIDGE_IMPL(sceNpSignalingGetConnectionInfo)
BRIDGE_IMPL(sceNpSignalingGetConnectionStatus)
BRIDGE_IMPL(sceNpSignalingGetCtxOpt)
BRIDGE_IMPL(sceNpSignalingGetLocalNetInfo)
BRIDGE_IMPL(sceNpSignalingGetMemoryInfo)
BRIDGE_IMPL(sceNpSignalingGetPeerNetInfo)
BRIDGE_IMPL(sceNpSignalingGetPeerNetInfoResult)
BRIDGE_IMPL(sceNpSignalingInit)
BRIDGE_IMPL(sceNpSignalingSetCtxOpt)
BRIDGE_IMPL(sceNpSignalingTerm)
BRIDGE_IMPL(sceNpSignalingTerminateConnection)
