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
 * @file SceAudio_tracy.cpp
 * @brief Tracy argument serializing functions for SceAudio
 */

#include "http/state.h"
#include <sstream>
#include <string>
#ifdef TRACY_ENABLE

#include "SceHttp_tracy.h"

void tracy_sceHttpAddRequestHeader(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, const char *name, const char *value, SceHttpAddHeaderMode mode) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string reqId = "reqId: ";
            std::string name = "name: ";
            std::string value = "value: ";
            std::string mode = "mode: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.reqId += std::to_string(reqId);
    tracy_settings.args.name += std::string(name ? name : "0x0 NULL");
    tracy_settings.args.value += std::string(value ? value : "0x0 NULL");
    tracy_settings.args.mode += mode == SCE_HTTP_HEADER_ADD ? "Add" : "Overwrite";

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.reqId.c_str(), tracy_settings.args.reqId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.name.c_str(), tracy_settings.args.name.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.value.c_str(), tracy_settings.args.value.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.mode.c_str(), tracy_settings.args.mode.size());
}

void tracy_sceHttpCreateConnectionWithURL(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId, const char *url, SceBool enableKeepalive) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string tmplId = "tmplId: ";
            std::string url = "url: ";
            std::string keepAlive = "keepAlive: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.tmplId += std::to_string(tmplId);
    tracy_settings.args.url += std::string(url ? url : "0x0 NULL");
    tracy_settings.args.keepAlive += std::to_string(enableKeepalive);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.tmplId.c_str(), tracy_settings.args.tmplId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.url.c_str(), tracy_settings.args.url.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.keepAlive.c_str(), tracy_settings.args.keepAlive.size());
}

void tracy_sceHttpCreateConnection(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId, const char *hostname, const char *scheme, SceUShort16 port, SceBool enableKeepalive) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string tmplId = "tmplId: ";
            std::string hostname = "hostname: ";
            std::string scheme = "scheme: ";
            std::string port = "port: ";
            std::string keepAlive = "keepAlive: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.tmplId += std::to_string(tmplId);
    tracy_settings.args.hostname += std::string(hostname ? hostname : "0x0 NULL");
    tracy_settings.args.scheme += std::string(scheme ? scheme : "0x0 NULL");
    tracy_settings.args.port += std::to_string(port);
    tracy_settings.args.keepAlive += std::to_string(enableKeepalive);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.tmplId.c_str(), tracy_settings.args.tmplId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.hostname.c_str(), tracy_settings.args.hostname.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.scheme.c_str(), tracy_settings.args.scheme.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.keepAlive.c_str(), tracy_settings.args.keepAlive.size());
}

void tracy_sceHttpCreateRequestWithURL(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt connId, SceInt method, const char *url, SceULong64 contentLength) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string connId = "connId: ";
            std::string method = "method: ";
            std::string url = "url: ";
            std::string contentLength = "contentLength: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.connId += std::to_string(connId);
    tracy_settings.args.method += std::to_string(method);
    tracy_settings.args.url += std::string(url ? url : "0x0 NULL");
    tracy_settings.args.contentLength += std::to_string(contentLength);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.connId.c_str(), tracy_settings.args.connId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.method.c_str(), tracy_settings.args.method.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.url.c_str(), tracy_settings.args.url.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.contentLength.c_str(), tracy_settings.args.contentLength.size());
}

void tracy_sceHttpCreateRequest(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt connId, SceInt method, const char *path, SceULong64 contentLength) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string connId = "connId: ";
            std::string method = "method: ";
            std::string path = "path: ";
            std::string contentLength = "contentLength: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.connId += std::to_string(connId);
    tracy_settings.args.method += std::to_string(method);
    tracy_settings.args.path += std::string(path ? path : "0x0 NULL");
    tracy_settings.args.contentLength += std::to_string(contentLength);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.connId.c_str(), tracy_settings.args.connId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.method.c_str(), tracy_settings.args.method.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.path.c_str(), tracy_settings.args.path.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.contentLength.c_str(), tracy_settings.args.contentLength.size());
}

void tracy_sceHttpCreateRequest2(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt connId, const char *method, const char *path, SceULong64 contentLength) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string connId = "connId: ";
            std::string method = "method: ";
            std::string path = "path: ";
            std::string contentLength = "contentLength: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.connId += std::to_string(connId);
    tracy_settings.args.method += std::string(method ? method : "0x0 NULL");
    tracy_settings.args.path += std::string(path ? path : "0x0 NULL");
    tracy_settings.args.contentLength += std::to_string(contentLength);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.connId.c_str(), tracy_settings.args.connId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.method.c_str(), tracy_settings.args.method.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.path.c_str(), tracy_settings.args.path.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.contentLength.c_str(), tracy_settings.args.contentLength.size());
}

void tracy_sceHttpCreateRequestWithURL2(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt connId, const char *method, const char *path, SceULong64 contentLength) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string connId = "connId: ";
            std::string method = "method: ";
            std::string path = "path: ";
            std::string contentLength = "contentLength: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.connId += std::to_string(connId);
    tracy_settings.args.method += std::string(method ? method : "0x0 NULL");
    tracy_settings.args.path += std::string(path ? path : "0x0 NULL");
    tracy_settings.args.contentLength += std::to_string(contentLength);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.connId.c_str(), tracy_settings.args.connId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.method.c_str(), tracy_settings.args.method.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.path.c_str(), tracy_settings.args.path.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.contentLength.c_str(), tracy_settings.args.contentLength.size());
}

void tracy_sceHttpCreateTemplate(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, const char *userAgent, SceHttpVersion httpVer, SceBool autoProxyConf) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string userAgent = "userAgent: ";
            std::string httpVer = "httpVer: ";
            std::string autoProxyConf = "autoProxyConf: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.userAgent += std::string(userAgent ? userAgent : "0x0 NULL");
    tracy_settings.args.httpVer += httpVer == SCE_HTTP_VERSION_1_0 ? "1.0" : "1.1";
    tracy_settings.args.autoProxyConf += std::to_string(autoProxyConf);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.userAgent.c_str(), tracy_settings.args.userAgent.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.httpVer.c_str(), tracy_settings.args.httpVer.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.autoProxyConf.c_str(), tracy_settings.args.autoProxyConf.size());
}

void tracy_sceHttpDeleteConnection(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt connId) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string connId = "connId: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.connId += std::to_string(connId);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.connId.c_str(), tracy_settings.args.connId.size());
}

void tracy_sceHttpDeleteRequest(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string reqId = "reqId: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.reqId += std::to_string(reqId);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.reqId.c_str(), tracy_settings.args.reqId.size());
}

void tracy_sceHttpDeleteTemplate(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string tmplId = "tmplId: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.tmplId += std::to_string(tmplId);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.tmplId.c_str(), tracy_settings.args.tmplId.size());
}

void tracy_sceHttpGetLastErrno(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, SceInt *errNum) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string reqId = "reqId: ";
            std::string errNum = "&errNum: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.reqId += std::to_string(reqId);
    std::stringstream ss;
    ss << errNum;
    tracy_settings.args.errNum += ss.str();

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.reqId.c_str(), tracy_settings.args.reqId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.errNum.c_str(), tracy_settings.args.errNum.size());
}

void tracy_sceHttpGetResponseContentLength(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, SceULong64 *contentLength) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string reqId = "reqId: ";
            std::string contentLength = "&contentLength: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.reqId += std::to_string(reqId);
    std::stringstream ss;
    ss << contentLength;
    tracy_settings.args.contentLength += ss.str();

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.reqId.c_str(), tracy_settings.args.reqId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.contentLength.c_str(), tracy_settings.args.contentLength.size());
}

void tracy_sceHttpGetStatusCode(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, SceInt *statusCode) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string reqId = "reqId: ";
            std::string statusCode = "&statusCode: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.reqId += std::to_string(reqId);

    std::stringstream ss;
    ss << statusCode;
    tracy_settings.args.statusCode += ss.str();

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.reqId.c_str(), tracy_settings.args.reqId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.statusCode.c_str(), tracy_settings.args.statusCode.size());
}

void tracy_sceHttpInit(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceSize poolSize) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string poolSize = "poolSize: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.poolSize += std::to_string(poolSize);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.poolSize.c_str(), tracy_settings.args.poolSize.size());
}

void tracy_sceHttpReadData(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, void *data, SceSize size) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string reqId = "reqId: ";
            std::string data = "data: ";
            std::string size = "size: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.reqId += std::to_string(reqId);
    std::stringstream ss;
    ss << data;
    tracy_settings.args.data += ss.str();
    tracy_settings.args.size += std::to_string(size);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.reqId.c_str(), tracy_settings.args.reqId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.data.c_str(), tracy_settings.args.data.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.size.c_str(), tracy_settings.args.size.size());
}

void tracy_sceHttpRemoveRequestHeader(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, const char *name) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string reqId = "reqId: ";
            std::string name = "name: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.reqId += std::to_string(reqId);
    tracy_settings.args.name += std::string(name ? name : "0x0 NULL");

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.reqId.c_str(), tracy_settings.args.reqId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.name.c_str(), tracy_settings.args.name.size());
}

void tracy_sceHttpSendRequest(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, const char *postData, SceSize size) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string reqId = "reqId: ";
            std::string postData = "postData: ";
            std::string postDataStr = "postDataStr: ";
            std::string size = "size: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.reqId += std::to_string(reqId);
    std::stringstream ss;
    ss << postData;
    tracy_settings.args.postData += ss.str();
    tracy_settings.args.postDataStr += std::string(postData ? postData : "0x0 NULL");
    tracy_settings.args.size += std::to_string(size);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.reqId.c_str(), tracy_settings.args.reqId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.postData.c_str(), tracy_settings.args.postData.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.postDataStr.c_str(), tracy_settings.args.postDataStr.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.size.c_str(), tracy_settings.args.size.size());
}

void tracy_sceHttpSetRequestContentLength(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, SceULong64 contentLength) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string reqId = "reqId: ";
            std::string contentLength = "contentLength: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.reqId += std::to_string(reqId);
    tracy_settings.args.contentLength += std::to_string(contentLength);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.reqId.c_str(), tracy_settings.args.reqId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.contentLength.c_str(), tracy_settings.args.contentLength.size());
}

void tracy_sceHttpSetResponseHeaderMaxSize(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt reqId, SceSize headerSize) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string reqId = "reqId: ";
            std::string headerSize = "headerSize: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.reqId += std::to_string(reqId);
    tracy_settings.args.headerSize += std::to_string(headerSize);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.reqId.c_str(), tracy_settings.args.reqId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.headerSize.c_str(), tracy_settings.args.headerSize.size());
}

void tracy_sceHttpTerm(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
        } args;
    } tracy_settings;

    // Nothing to send ¯\_(ツ)_/¯
}

void tracy_sceHttpUriBuild(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, char *out, SceSize *require, SceSize prepare, const SceHttpUriElement *srcElement, SceUInt option) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string out = "&out: ";
            std::string require = "&require: ";
            std::string prepare = "prepare: ";
            std::string srcElement = "&srcElement: ";
            std::string option = "option: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    std::stringstream outss;
    outss << out;
    tracy_settings.args.out += outss.str();

    std::stringstream requiress;
    requiress << require;
    tracy_settings.args.require += requiress.str();
    tracy_settings.args.prepare += std::to_string(prepare);

    std::stringstream srcElementss;
    srcElementss << srcElement;
    tracy_settings.args.srcElement += srcElementss.str();
    tracy_settings.args.option += std::to_string(option);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.out.c_str(), tracy_settings.args.out.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.require.c_str(), tracy_settings.args.require.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.prepare.c_str(), tracy_settings.args.prepare.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.srcElement.c_str(), tracy_settings.args.srcElement.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.option.c_str(), tracy_settings.args.option.size());
}

void tracy_sceHttpUriEscape(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, char *out, SceSize *require, SceSize prepare, const char *in) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string out = "&out: ";
            std::string require = "&require: ";
            std::string prepare = "prepare: ";
            std::string in = "&in: ";
            std::string inStr = "in: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    std::stringstream outss;
    outss << out;
    tracy_settings.args.out += outss.str();

    std::stringstream requiress;
    requiress << require;
    tracy_settings.args.require += requiress.str();
    tracy_settings.args.prepare += std::to_string(prepare);

    std::stringstream inss;
    inss << in;
    tracy_settings.args.in += inss.str();
    tracy_settings.args.inStr += std::string(in ? in : "0x0 NULL");

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.out.c_str(), tracy_settings.args.out.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.require.c_str(), tracy_settings.args.require.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.prepare.c_str(), tracy_settings.args.prepare.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.in.c_str(), tracy_settings.args.in.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.inStr.c_str(), tracy_settings.args.inStr.size());
}

void tracy_sceHttpUriMerge(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, char *mergedUrl, const char *url, const char *relativeUrl, SceSize *require, SceSize prepare, SceUInt option) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string mergedUrl = "&mergedUrl: ";
            std::string url = "url: ";
            std::string relativeUrl = "relativeUrl: ";
            std::string require = "&require: ";
            std::string prepare = "prepare: ";
            std::string option = "option: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    std::stringstream mergedUrlss;
    mergedUrlss << mergedUrl;
    tracy_settings.args.mergedUrl += mergedUrlss.str();

    tracy_settings.args.url += std::string(url ? url : "0x0 NULL");
    tracy_settings.args.relativeUrl += std::string(relativeUrl ? relativeUrl : "0x0 NULL");

    std::stringstream requiress;
    requiress << require;
    tracy_settings.args.require += requiress.str();
    tracy_settings.args.prepare += std::to_string(prepare);
    tracy_settings.args.option += std::to_string(option);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.mergedUrl.c_str(), tracy_settings.args.mergedUrl.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.url.c_str(), tracy_settings.args.url.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.relativeUrl.c_str(), tracy_settings.args.relativeUrl.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.require.c_str(), tracy_settings.args.require.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.prepare.c_str(), tracy_settings.args.prepare.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.option.c_str(), tracy_settings.args.option.size());
}

void tracy_sceHttpUriParse(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceHttpUriElement *out, const char *srcUrl, void *pool, SceSize *require, SceSize prepare) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string out = "&out: ";
            std::string srcUrl = "srcUrl: ";
            std::string pool = "&pool: ";
            std::string require = "&require: ";
            std::string prepare = "prepare: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    std::stringstream outss;
    outss << out;
    tracy_settings.args.out += outss.str();

    tracy_settings.args.srcUrl += std::string(srcUrl ? srcUrl : "0x0 NULL");

    std::stringstream poolss;
    poolss << pool;
    tracy_settings.args.pool += poolss.str();

    std::stringstream requiress;
    requiress << require;
    tracy_settings.args.require += requiress.str();
    tracy_settings.args.prepare += std::to_string(prepare);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.out.c_str(), tracy_settings.args.out.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.srcUrl.c_str(), tracy_settings.args.srcUrl.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.pool.c_str(), tracy_settings.args.pool.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.require.c_str(), tracy_settings.args.require.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.prepare.c_str(), tracy_settings.args.prepare.size());
}

void tracy_sceHttpUriSweepPath(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, char *dst, const char *src, SceSize srcSize) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string dst = "&dst: ";
            std::string src = "src: ";
            std::string srcSize = "srcSize: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    std::stringstream dstss;
    dstss << dst;
    tracy_settings.args.dst += dstss.str();

    tracy_settings.args.src += std::string(src ? src : "0x0 NULL");
    tracy_settings.args.srcSize += std::to_string(srcSize);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.dst.c_str(), tracy_settings.args.dst.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.src.c_str(), tracy_settings.args.src.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.srcSize.c_str(), tracy_settings.args.srcSize.size());
}

void tracy_sceHttpUriUnescape(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, char *out, SceSize *require, SceSize prepare, const char *in) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string out = "&out: ";
            std::string require = "&require: ";
            std::string prepare = "prepare: ";
            std::string in = "in: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    std::stringstream outss;
    outss << out;
    tracy_settings.args.out += outss.str();

    std::stringstream requiress;
    requiress << require;
    tracy_settings.args.require += requiress.str();
    tracy_settings.args.prepare += std::to_string(prepare);
    tracy_settings.args.in += std::string(in ? in : "0x0 NULL");

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.out.c_str(), tracy_settings.args.out.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.require.c_str(), tracy_settings.args.require.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.prepare.c_str(), tracy_settings.args.prepare.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.in.c_str(), tracy_settings.args.in.size());
}

void tracy_sceHttpsDisableOption(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt sslFlags) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string sslFlags = "sslFlags: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.sslFlags += std::to_string(sslFlags);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.sslFlags.c_str(), tracy_settings.args.sslFlags.size());
}

void tracy_sceHttpsDisableOption2(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId, SceUInt sslFlags) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string tmplId = "tmplId: ";
            std::string sslFlags = "sslFlags: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.tmplId += std::to_string(tmplId);
    tracy_settings.args.sslFlags += std::to_string(sslFlags);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.tmplId.c_str(), tracy_settings.args.tmplId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.sslFlags.c_str(), tracy_settings.args.sslFlags.size());
}

void tracy_sceHttpsEnableOption2(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId, SceUInt sslFlags) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string tmplId = "tmplId: ";
            std::string sslFlags = "sslFlags: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.tmplId += std::to_string(tmplId);
    tracy_settings.args.sslFlags += std::to_string(sslFlags);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.tmplId.c_str(), tracy_settings.args.tmplId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.sslFlags.c_str(), tracy_settings.args.sslFlags.size());
}

void tracy_sceHttpsEnableOption(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt sslFlags) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string sslFlags = "sslFlags: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.sslFlags += std::to_string(sslFlags);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.sslFlags.c_str(), tracy_settings.args.sslFlags.size());
}

void tracy_sceHttpsGetSslError(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId, SceInt *errNum, SceUInt *detail) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string tmplId = "tmplId: ";
            std::string errNum = "&errNum: ";
            std::string detail = "&detail: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.tmplId += std::to_string(tmplId);

    std::stringstream errNumss;
    errNumss << errNum;
    tracy_settings.args.errNum += errNumss.str();

    std::stringstream detailss;
    detailss << detail;
    tracy_settings.args.detail += detailss.str();

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.tmplId.c_str(), tracy_settings.args.tmplId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.errNum.c_str(), tracy_settings.args.errNum.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.detail.c_str(), tracy_settings.args.detail.size());
}

void tracy_sceHttpsSetSslCallback(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceInt tmplId, SceHttpsCallback cbFunction, void *userArg) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string tmplId = "tmplId: ";
            std::string cbFunction = "&cbFunction: ";
            std::string userArg = "&userArg: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.tmplId += std::to_string(tmplId);

    std::stringstream cbFunctionss;
    cbFunctionss << cbFunction;
    tracy_settings.args.cbFunction += cbFunctionss.str();

    std::stringstream userArgss;
    userArgss << userArg;
    tracy_settings.args.userArg += userArgss.str();

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.tmplId.c_str(), tracy_settings.args.tmplId.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.cbFunction.c_str(), tracy_settings.args.cbFunction.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.userArg.c_str(), tracy_settings.args.userArg.size());
}

#endif // TRACY_ENABLE
