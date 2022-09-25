// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

/**
 * @file SceHttp_tracy.h
 * @brief Tracy argument serializing functions for SceHttp
 */

#pragma once

#ifdef TRACY_ENABLE

#include "SceHttp.h"
#include "Tracy.hpp"
#include "client/TracyScoped.hpp"

#include <http/state.h>

/**
 * @brief Module name used for retrieving Tracy activation state for logging using Tracy profiler
 *
 * This constant is used by functions in module to know which module name they have
 * to use as a key to retrieve the activation state for logging of the module towards the Tracy profiler.
 */
const std::string tracy_module_name = "SceHttp";

void tracy_sceHttpAddRequestHeader(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, const char *name, const char *value, SceHttpAddHeaderMode mode);
void tracy_sceHttpCreateConnectionWithURL(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId, const char *url, SceBool enableKeepalive);
void tracy_sceHttpCreateConnection(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId, const char *hostname, const char *scheme, SceUShort16 port, SceBool enableKeepalive);
void tracy_sceHttpCreateRequestWithURL(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt connId, SceInt method, const char *url, SceULong64 contentLength);
void tracy_sceHttpCreateRequest(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt connId, SceInt method, const char *path, SceULong64 contentLength);
void tracy_sceHttpCreateRequest2(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt connId, const char *method, const char *path, SceULong64 contentLength);
void tracy_sceHttpCreateRequestWithURL2(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt connId, const char *method, const char *path, SceULong64 contentLength);
void tracy_sceHttpCreateTemplate(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, const char *userAgent, SceHttpVersion httpVer, SceBool autoProxyConf);
void tracy_sceHttpDeleteConnection(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt connId);
void tracy_sceHttpDeleteRequest(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId);
void tracy_sceHttpDeleteTemplate(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId);
void tracy_sceHttpGetLastErrno(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, SceInt *errNum);
void tracy_sceHttpGetResponseContentLength(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, SceULong64 *contentLength);
void tracy_sceHttpGetStatusCode(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, SceInt *statusCode);
void tracy_sceHttpInit(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceSize poolSize);
void tracy_sceHttpReadData(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, void *data, SceSize size);
void tracy_sceHttpRemoveRequestHeader(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, const char *name);
void tracy_sceHttpSendRequest(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, const char *postData, SceSize size);
void tracy_sceHttpSetRequestContentLength(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, SceULong64 contentLength);
void tracy_sceHttpSetResponseHeaderMaxSize(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, SceSize headerSize);
void tracy_sceHttpTerm(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation);
void tracy_sceHttpUriBuild(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, char *out, SceSize *require, SceSize prepare, const SceHttpUriElement *srcElement, SceUInt option);
void tracy_sceHttpUriEscape(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, char *out, SceSize *require, SceSize prepare, const char *in);
void tracy_sceHttpUriMerge(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, char *mergedUrl, const char *url, const char *relativeUrl, SceSize *require, SceSize prepare, SceUInt option);
void tracy_sceHttpUriParse(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceHttpUriElement *out, const char *srcUrl, void *pool, SceSize *require, SceSize prepare);
void tracy_sceHttpUriSweepPath(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, char *dst, const char *src, SceSize srcSize);
void tracy_sceHttpUriUnescape(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, char *out, SceSize *require, SceSize prepare, const char *in);
void tracy_sceHttpsDisableOption(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt sslFlags);
void tracy_sceHttpsDisableOption2(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId, SceUInt sslFlags);
void tracy_sceHttpsEnableOption2(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId, SceUInt sslFlags);
void tracy_sceHttpsEnableOption(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt sslFlags);
void tracy_sceHttpsGetSslError(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId, SceInt *errNum, SceUInt *detail);
void tracy_sceHttpsSetSslCallback(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId, SceHttpsCallback cbFunction, void *userArg);

#endif // TRACY_ENABLE
