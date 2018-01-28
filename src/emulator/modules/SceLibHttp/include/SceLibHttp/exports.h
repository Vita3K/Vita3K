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

// SceHttp
BRIDGE_DECL(sceHttpAbortRequest)
BRIDGE_DECL(sceHttpAbortRequestForce)
BRIDGE_DECL(sceHttpAbortWaitRequest)
BRIDGE_DECL(sceHttpAddCookie)
BRIDGE_DECL(sceHttpAddRequestHeader)
BRIDGE_DECL(sceHttpAddRequestHeaderRaw)
BRIDGE_DECL(sceHttpAuthCacheFlush)
BRIDGE_DECL(sceHttpCookieExport)
BRIDGE_DECL(sceHttpCookieFlush)
BRIDGE_DECL(sceHttpCookieImport)
BRIDGE_DECL(sceHttpCreateConnection)
BRIDGE_DECL(sceHttpCreateConnectionWithURL)
BRIDGE_DECL(sceHttpCreateEpoll)
BRIDGE_DECL(sceHttpCreateRequest)
BRIDGE_DECL(sceHttpCreateRequestWithURL)
BRIDGE_DECL(sceHttpCreateTemplate)
BRIDGE_DECL(sceHttpDeleteConnection)
BRIDGE_DECL(sceHttpDeleteRequest)
BRIDGE_DECL(sceHttpDeleteTemplate)
BRIDGE_DECL(sceHttpDestroyEpoll)
BRIDGE_DECL(sceHttpGetAcceptEncodingGZIPEnabled)
BRIDGE_DECL(sceHttpGetAllResponseHeaders)
BRIDGE_DECL(sceHttpGetAuthEnabled)
BRIDGE_DECL(sceHttpGetAutoRedirect)
BRIDGE_DECL(sceHttpGetCookie)
BRIDGE_DECL(sceHttpGetCookieEnabled)
BRIDGE_DECL(sceHttpGetCookieStats)
BRIDGE_DECL(sceHttpGetEpoll)
BRIDGE_DECL(sceHttpGetEpollId)
BRIDGE_DECL(sceHttpGetIcmOption)
BRIDGE_DECL(sceHttpGetLastErrno)
BRIDGE_DECL(sceHttpGetMemoryPoolStats)
BRIDGE_DECL(sceHttpGetNonblock)
BRIDGE_DECL(sceHttpGetResponseContentLength)
BRIDGE_DECL(sceHttpGetStatusCode)
BRIDGE_DECL(sceHttpInit)
BRIDGE_DECL(sceHttpParseResponseHeader)
BRIDGE_DECL(sceHttpParseStatusLine)
BRIDGE_DECL(sceHttpReadData)
BRIDGE_DECL(sceHttpRedirectCacheFlush)
BRIDGE_DECL(sceHttpRemoveRequestHeader)
BRIDGE_DECL(sceHttpRequestGetAllHeaders)
BRIDGE_DECL(sceHttpSendRequest)
BRIDGE_DECL(sceHttpSetAcceptEncodingGZIPEnabled)
BRIDGE_DECL(sceHttpSetAuthEnabled)
BRIDGE_DECL(sceHttpSetAuthInfoCallback)
BRIDGE_DECL(sceHttpSetAutoRedirect)
BRIDGE_DECL(sceHttpSetConnectTimeOut)
BRIDGE_DECL(sceHttpSetCookieEnabled)
BRIDGE_DECL(sceHttpSetCookieMaxNum)
BRIDGE_DECL(sceHttpSetCookieMaxNumPerDomain)
BRIDGE_DECL(sceHttpSetCookieMaxSize)
BRIDGE_DECL(sceHttpSetCookieRecvCallback)
BRIDGE_DECL(sceHttpSetCookieSendCallback)
BRIDGE_DECL(sceHttpSetCookieTotalMaxSize)
BRIDGE_DECL(sceHttpSetDefaultAcceptEncodingGZIPEnabled)
BRIDGE_DECL(sceHttpSetEpoll)
BRIDGE_DECL(sceHttpSetEpollId)
BRIDGE_DECL(sceHttpSetIcmOption)
BRIDGE_DECL(sceHttpSetNonblock)
BRIDGE_DECL(sceHttpSetRecvTimeOut)
BRIDGE_DECL(sceHttpSetRedirectCallback)
BRIDGE_DECL(sceHttpSetRequestContentLength)
BRIDGE_DECL(sceHttpSetResolveRetry)
BRIDGE_DECL(sceHttpSetResolveTimeOut)
BRIDGE_DECL(sceHttpSetSendTimeOut)
BRIDGE_DECL(sceHttpSslIsCtxCreated)
BRIDGE_DECL(sceHttpTerm)
BRIDGE_DECL(sceHttpUnsetEpoll)
BRIDGE_DECL(sceHttpUriBuild)
BRIDGE_DECL(sceHttpUriEscape)
BRIDGE_DECL(sceHttpUriMerge)
BRIDGE_DECL(sceHttpUriParse)
BRIDGE_DECL(sceHttpUriSweepPath)
BRIDGE_DECL(sceHttpUriUnescape)
BRIDGE_DECL(sceHttpWaitRequest)
BRIDGE_DECL(sceHttpWaitRequestCB)
BRIDGE_DECL(sceHttpsDisableOption)
BRIDGE_DECL(sceHttpsDisableOptionPrivate)
BRIDGE_DECL(sceHttpsEnableOption)
BRIDGE_DECL(sceHttpsEnableOptionPrivate)
BRIDGE_DECL(sceHttpsFreeCaList)
BRIDGE_DECL(sceHttpsGetCaList)
BRIDGE_DECL(sceHttpsGetSslError)
BRIDGE_DECL(sceHttpsLoadCert)
BRIDGE_DECL(sceHttpsSetSslCallback)
BRIDGE_DECL(sceHttpsUnloadCert)
