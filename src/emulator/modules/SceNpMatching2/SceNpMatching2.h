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

BRIDGE_DECL(sceNpMatching2AbortContextStart)
BRIDGE_DECL(sceNpMatching2AbortRequest)
BRIDGE_DECL(sceNpMatching2ContextStart)
BRIDGE_DECL(sceNpMatching2ContextStop)
BRIDGE_DECL(sceNpMatching2CreateContext)
BRIDGE_DECL(sceNpMatching2CreateJoinRoom)
BRIDGE_DECL(sceNpMatching2DestroyContext)
BRIDGE_DECL(sceNpMatching2GetLobbyInfoList)
BRIDGE_DECL(sceNpMatching2GetMemoryInfo)
BRIDGE_DECL(sceNpMatching2GetRoomDataExternalList)
BRIDGE_DECL(sceNpMatching2GetRoomDataInternal)
BRIDGE_DECL(sceNpMatching2GetRoomMemberDataExternalList)
BRIDGE_DECL(sceNpMatching2GetRoomMemberDataInternal)
BRIDGE_DECL(sceNpMatching2GetRoomMemberIdListLocal)
BRIDGE_DECL(sceNpMatching2GetRoomPasswordLocal)
BRIDGE_DECL(sceNpMatching2GetServerLocal)
BRIDGE_DECL(sceNpMatching2GetSignalingOptParamLocal)
BRIDGE_DECL(sceNpMatching2GetUserInfoList)
BRIDGE_DECL(sceNpMatching2GetWorldInfoList)
BRIDGE_DECL(sceNpMatching2GrantRoomOwner)
BRIDGE_DECL(sceNpMatching2Init)
BRIDGE_DECL(sceNpMatching2JoinLobby)
BRIDGE_DECL(sceNpMatching2JoinRoom)
BRIDGE_DECL(sceNpMatching2KickoutRoomMember)
BRIDGE_DECL(sceNpMatching2LeaveLobby)
BRIDGE_DECL(sceNpMatching2LeaveRoom)
BRIDGE_DECL(sceNpMatching2RegisterContextCallback)
BRIDGE_DECL(sceNpMatching2RegisterLobbyEventCallback)
BRIDGE_DECL(sceNpMatching2RegisterLobbyMessageCallback)
BRIDGE_DECL(sceNpMatching2RegisterRoomEventCallback)
BRIDGE_DECL(sceNpMatching2RegisterRoomMessageCallback)
BRIDGE_DECL(sceNpMatching2RegisterSignalingCallback)
BRIDGE_DECL(sceNpMatching2SearchRoom)
BRIDGE_DECL(sceNpMatching2SendLobbyChatMessage)
BRIDGE_DECL(sceNpMatching2SendRoomChatMessage)
BRIDGE_DECL(sceNpMatching2SendRoomMessage)
BRIDGE_DECL(sceNpMatching2SetDefaultRequestOptParam)
BRIDGE_DECL(sceNpMatching2SetRoomDataExternal)
BRIDGE_DECL(sceNpMatching2SetRoomDataInternal)
BRIDGE_DECL(sceNpMatching2SetRoomMemberDataInternal)
BRIDGE_DECL(sceNpMatching2SetSignalingOptParam)
BRIDGE_DECL(sceNpMatching2SetUserInfo)
BRIDGE_DECL(sceNpMatching2SignalingCancelPeerNetInfo)
BRIDGE_DECL(sceNpMatching2SignalingGetConnectionInfo)
BRIDGE_DECL(sceNpMatching2SignalingGetConnectionStatus)
BRIDGE_DECL(sceNpMatching2SignalingGetLocalNetInfo)
BRIDGE_DECL(sceNpMatching2SignalingGetPeerNetInfo)
BRIDGE_DECL(sceNpMatching2SignalingGetPeerNetInfoResult)
BRIDGE_DECL(sceNpMatching2SignalingGetPingInfo)
BRIDGE_DECL(sceNpMatching2Term)
