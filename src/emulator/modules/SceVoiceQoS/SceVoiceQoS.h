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

BRIDGE_DECL(sceVoiceQoSConnect)
BRIDGE_DECL(sceVoiceQoSCreateLocalEndpoint)
BRIDGE_DECL(sceVoiceQoSCreateRemoteEndpoint)
BRIDGE_DECL(sceVoiceQoSDeleteLocalEndpoint)
BRIDGE_DECL(sceVoiceQoSDeleteRemoteEndpoint)
BRIDGE_DECL(sceVoiceQoSDisconnect)
BRIDGE_DECL(sceVoiceQoSEnd)
BRIDGE_DECL(sceVoiceQoSGetConnectionAttribute)
BRIDGE_DECL(sceVoiceQoSGetLocalEndpoint)
BRIDGE_DECL(sceVoiceQoSGetLocalEndpointAttribute)
BRIDGE_DECL(sceVoiceQoSGetRemoteEndpoint)
BRIDGE_DECL(sceVoiceQoSGetStatus)
BRIDGE_DECL(sceVoiceQoSInit)
BRIDGE_DECL(sceVoiceQoSReadPacket)
BRIDGE_DECL(sceVoiceQoSSetConnectionAttribute)
BRIDGE_DECL(sceVoiceQoSSetLocalEndpointAttribute)
BRIDGE_DECL(sceVoiceQoSWritePacket)
