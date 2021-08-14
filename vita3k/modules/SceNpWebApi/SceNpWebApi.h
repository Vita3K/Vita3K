// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

BRIDGE_DECL(sceNpWebApiAbortHandle)
BRIDGE_DECL(sceNpWebApiAbortRequest)
BRIDGE_DECL(sceNpWebApiAddHttpRequestHeader)
BRIDGE_DECL(sceNpWebApiAddMultipartPart)
BRIDGE_DECL(sceNpWebApiCheckCallback)
BRIDGE_DECL(sceNpWebApiCreateExtdPushEventFilter)
BRIDGE_DECL(sceNpWebApiCreateHandle)
BRIDGE_DECL(sceNpWebApiCreateMultipartRequest)
BRIDGE_DECL(sceNpWebApiCreatePushEventFilter)
BRIDGE_DECL(sceNpWebApiCreateRequest)
BRIDGE_DECL(sceNpWebApiCreateServicePushEventFilter)
BRIDGE_DECL(sceNpWebApiDeleteExtdPushEventFilter)
BRIDGE_DECL(sceNpWebApiDeleteHandle)
BRIDGE_DECL(sceNpWebApiDeletePushEventFilter)
BRIDGE_DECL(sceNpWebApiDeleteRequest)
BRIDGE_DECL(sceNpWebApiDeleteServicePushEventFilter)
BRIDGE_DECL(sceNpWebApiGetErrorCode)
BRIDGE_DECL(sceNpWebApiGetHttpResponseHeaderValue)
BRIDGE_DECL(sceNpWebApiGetHttpResponseHeaderValueLength)
BRIDGE_DECL(sceNpWebApiGetHttpStatusCode)
BRIDGE_DECL(sceNpWebApiGetMemoryPoolStats)
BRIDGE_DECL(sceNpWebApiGetNpTitleId)
BRIDGE_DECL(sceNpWebApiInitialize)
BRIDGE_DECL(sceNpWebApiReadData)
BRIDGE_DECL(sceNpWebApiRegisterExtdPushEventCallback)
BRIDGE_DECL(sceNpWebApiRegisterPushEventCallback)
BRIDGE_DECL(sceNpWebApiRegisterServicePushEventCallback)
BRIDGE_DECL(sceNpWebApiSendMultipartRequest)
BRIDGE_DECL(sceNpWebApiSendMultipartRequest2)
BRIDGE_DECL(sceNpWebApiSendRequest)
BRIDGE_DECL(sceNpWebApiSendRequest2)
BRIDGE_DECL(sceNpWebApiSetMultipartContentType)
BRIDGE_DECL(sceNpWebApiSetNpTitleId)
BRIDGE_DECL(sceNpWebApiTerminate)
BRIDGE_DECL(sceNpWebApiUnregisterExtdPushEventCallback)
BRIDGE_DECL(sceNpWebApiUnregisterPushEventCallback)
BRIDGE_DECL(sceNpWebApiUnregisterServicePushEventCallback)
BRIDGE_DECL(sceNpWebApiUtilityParseNpId)
