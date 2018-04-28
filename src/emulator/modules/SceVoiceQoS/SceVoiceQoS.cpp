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

#include "SceVoiceQoS.h"

EXPORT(int, sceVoiceQoSConnect) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSCreateLocalEndpoint) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSCreateRemoteEndpoint) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSDeleteLocalEndpoint) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSDeleteRemoteEndpoint) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSDisconnect) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSEnd) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSGetConnectionAttribute) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSGetLocalEndpoint) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSGetLocalEndpointAttribute) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSGetRemoteEndpoint) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSReadPacket) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSSetConnectionAttribute) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSSetLocalEndpointAttribute) {
    return unimplemented(export_name);
}

EXPORT(int, sceVoiceQoSWritePacket) {
    return unimplemented(export_name);
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
