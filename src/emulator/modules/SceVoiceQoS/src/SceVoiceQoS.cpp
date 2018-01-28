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

#include <SceVoiceQoS/exports.h>

EXPORT(int, sceVoiceQoSConnect) {
    return unimplemented("sceVoiceQoSConnect");
}

EXPORT(int, sceVoiceQoSCreateLocalEndpoint) {
    return unimplemented("sceVoiceQoSCreateLocalEndpoint");
}

EXPORT(int, sceVoiceQoSCreateRemoteEndpoint) {
    return unimplemented("sceVoiceQoSCreateRemoteEndpoint");
}

EXPORT(int, sceVoiceQoSDeleteLocalEndpoint) {
    return unimplemented("sceVoiceQoSDeleteLocalEndpoint");
}

EXPORT(int, sceVoiceQoSDeleteRemoteEndpoint) {
    return unimplemented("sceVoiceQoSDeleteRemoteEndpoint");
}

EXPORT(int, sceVoiceQoSDisconnect) {
    return unimplemented("sceVoiceQoSDisconnect");
}

EXPORT(int, sceVoiceQoSEnd) {
    return unimplemented("sceVoiceQoSEnd");
}

EXPORT(int, sceVoiceQoSGetConnectionAttribute) {
    return unimplemented("sceVoiceQoSGetConnectionAttribute");
}

EXPORT(int, sceVoiceQoSGetLocalEndpoint) {
    return unimplemented("sceVoiceQoSGetLocalEndpoint");
}

EXPORT(int, sceVoiceQoSGetLocalEndpointAttribute) {
    return unimplemented("sceVoiceQoSGetLocalEndpointAttribute");
}

EXPORT(int, sceVoiceQoSGetRemoteEndpoint) {
    return unimplemented("sceVoiceQoSGetRemoteEndpoint");
}

EXPORT(int, sceVoiceQoSGetStatus) {
    return unimplemented("sceVoiceQoSGetStatus");
}

EXPORT(int, sceVoiceQoSInit) {
    return unimplemented("sceVoiceQoSInit");
}

EXPORT(int, sceVoiceQoSReadPacket) {
    return unimplemented("sceVoiceQoSReadPacket");
}

EXPORT(int, sceVoiceQoSSetConnectionAttribute) {
    return unimplemented("sceVoiceQoSSetConnectionAttribute");
}

EXPORT(int, sceVoiceQoSSetLocalEndpointAttribute) {
    return unimplemented("sceVoiceQoSSetLocalEndpointAttribute");
}

EXPORT(int, sceVoiceQoSWritePacket) {
    return unimplemented("sceVoiceQoSWritePacket");
}

BRIDGE_IMPL(sceVoiceQoSConnect)
BRIDGE_IMPL(sceVoiceQoSCreateLocalEndpoint)
BRIDGE_IMPL(sceVoiceQoSCreateRemoteEndpoint)
BRIDGE_IMPL(sceVoiceQoSDeleteLocalEndpoint)
BRIDGE_IMPL(sceVoiceQoSDeleteRemoteEndpoint)
BRIDGE_IMPL(sceVoiceQoSDisconnect)
BRIDGE_IMPL(sceVoiceQoSEnd)
BRIDGE_IMPL(sceVoiceQoSGetConnectionAttribute)
BRIDGE_IMPL(sceVoiceQoSGetLocalEndpoint)
BRIDGE_IMPL(sceVoiceQoSGetLocalEndpointAttribute)
BRIDGE_IMPL(sceVoiceQoSGetRemoteEndpoint)
BRIDGE_IMPL(sceVoiceQoSGetStatus)
BRIDGE_IMPL(sceVoiceQoSInit)
BRIDGE_IMPL(sceVoiceQoSReadPacket)
BRIDGE_IMPL(sceVoiceQoSSetConnectionAttribute)
BRIDGE_IMPL(sceVoiceQoSSetLocalEndpointAttribute)
BRIDGE_IMPL(sceVoiceQoSWritePacket)
