// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <mem/ptr.h>

#include <map>
#include <string>
#include <util/types.h>
#include <vector>

#define SCE_HTTP_DEFAULT_RESPONSE_HEADER_MAX KB(5)
#define SCE_HTTP_USERNAME_MAX_SIZE 256
#define SCE_HTTP_PASSWORD_MAX_SIZE 256

typedef int (*SceHttpsCallback)(unsigned int verifyEsrr, Ptr<void> const sslCert[], int certNum, Ptr<void> userArg);

#include <boost/algorithm/string/predicate.hpp>

struct ci_compare {
    bool operator()(std::string_view const &a,
        std::string_view const &b) const {
        return boost::ilexicographical_compare(a, b);
    }
};

typedef std::map<std::string, std::string, ci_compare> HeadersMapType;

enum SceHttpsErrorCode : uint32_t {
    SCE_HTTPS_ERROR_CERT = 0x80435060,
    SCE_HTTPS_ERROR_HANDSHAKE = 0x80435061,
    SCE_HTTPS_ERROR_IO = 0x80435062,
    SCE_HTTPS_ERROR_INTERNAL = 0x80435063,
    SCE_HTTPS_ERROR_PROXY = 0x80435064
};

enum SceHttpStatusCode {
    // 1xx Informational
    SCE_HTTP_STATUS_CODE_CONTINUE = 100,
    SCE_HTTP_STATUS_CODE_SWITCHING_PROTOCOLS = 101,
    SCE_HTTP_STATUS_CODE_PROCESSING = 102,
    SCE_HTTP_STATUS_CODE_EARLY_HINTS = 103,
    // 2xx Sucessful
    SCE_HTTP_STATUS_CODE_OK = 200,
    SCE_HTTP_STATUS_CODE_CREATED = 201,
    SCE_HTTP_STATUS_CODE_ACCEPTED = 202,
    SCE_HTTP_STATUS_CODE_NON_AUTHORITATIVE_INFORMATION = 203,
    SCE_HTTP_STATUS_CODE_NO_CONTENT = 204,
    SCE_HTTP_STATUS_CODE_RESET_CONTENT = 205,
    SCE_HTTP_STATUS_CODE_PARTIAL_CONTENT = 206,
    SCE_HTTP_STATUS_CODE_MULTI_STATUS = 207,
    SCE_HTTP_STATUS_CODE_ALREADY_REPORTED = 208,
    SCE_HTTP_STATUS_CODE_IM_USED = 226,
    // 3xx Redirection
    SCE_HTTP_STATUS_CODE_MULTIPLE_CHOICES = 300,
    SCE_HTTP_STATUS_CODE_MOVED_PERMANENTLY = 301,
    SCE_HTTP_STATUS_CODE_FOUND = 302,
    SCE_HTTP_STATUS_CODE_SEE_OTHER = 303,
    SCE_HTTP_STATUS_CODE_NOT_MODIFIED = 304,
    SCE_HTTP_STATUS_CODE_USE_PROXY = 305,
    SCE_HTTP_STATUS_CODE_UNUSED = 306,
    SCE_HTTP_STATUS_CODE_TEMPORARY_REDIRECT = 307,
    SCE_HTTP_STATUS_CODE_PERMANENT_REDIRECT = 308,
    // 4xx Client Error
    SCE_HTTP_STATUS_CODE_BAD_REQUEST = 400,
    SCE_HTTP_STATUS_CODE_UNAUTHORIZED = 401,
    SCE_HTTP_STATUS_CODE_PAYMENT_REQUIRED = 402,
    SCE_HTTP_STATUS_CODE_FORBIDDDEN = 403,
    SCE_HTTP_STATUS_CODE_NOT_FOUND = 404,
    SCE_HTTP_STATUS_CODE_METHOD_NOT_ALLOWED = 405,
    SCE_HTTP_STATUS_CODE_NOT_ACCEPTABLE = 406,
    SCE_HTTP_STATUS_CODE_PROXY_AUTHENTICATION_REQUIRED = 407,
    SCE_HTTP_STATUS_CODE_REQUEST_TIME_OUT = 408,
    SCE_HTTP_STATUS_CODE_CONFLICT = 409,
    SCE_HTTP_STATUS_CODE_GONE = 410,
    SCE_HTTP_STATUS_CODE_LENGTH_REQUIRED = 411,
    SCE_HTTP_STATUS_CODE_PRECONDITION_FAILED = 412,
    SCE_HTTP_STATUS_CODE_REQUEST_ENTITY_TOO_LARGE = 413,
    SCE_HTTP_STATUS_CODE_REQUEST_URI_TOO_LARGE = 414,
    SCE_HTTP_STATUS_CODE_UNSUPPORTED_MEDIA_TYPE = 415,
    SCE_HTTP_STATUS_CODE_REQUEST_RANGE_NOT_SATISFIBLE = 416,
    SCE_HTTP_STATUS_CODE_EXPECTATION_FAILED = 417,
    SCE_HTTP_STATUS_CODE_TEAPOT = 418, // lol
    SCE_HTTP_STATUS_CODE_MISDIRECTED_REQUEST = 421,
    SCE_HTTP_STATUS_CODE_UNPROCESSABLE_ENTITY = 422,
    SCE_HTTP_STATUS_CODE_LOCKED = 423,
    SCE_HTTP_STATUS_CODE_FAILED_DEPENDENCY = 424,
    SCE_HTTP_STATUS_CODE_TOO_EARLY = 425,
    SCE_HTTP_STATUS_CODE_UPGRADE_REQUIRED = 426,
    SCE_HTTP_STATUS_CODE_PRECONDITION_REQUIRED = 428,
    SCE_HTTP_STATUS_CODE_TOO_MANY_REQUESTS = 429,
    SCE_HTTP_STATUS_CODE_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
    SCE_HTTP_STATUS_CODE_UNAVAILABLE_FOR_LEGAL_REASONS = 451,
    // 5xx Server Error
    SCE_HTTP_STATUS_CODE_INTERNAL_SERVER_ERROR = 500,
    SCE_HTTP_STATUS_CODE_NOT_IMPLEMENTED = 501,
    SCE_HTTP_STATUS_CODE_BAD_GATEWAY = 502,
    SCE_HTTP_STATUS_CODE_SERVICE_UNAVAILABLE = 503,
    SCE_HTTP_STATUS_CODE_GATEWAY_TIME_OUT = 504,
    SCE_HTTP_STATUS_CODE_HTTP_VERSION_NOT_SUPPORTED = 505,
    SCE_HTTP_STATUS_CODE_VARIANT_ALSO_NEGOTIATES = 506,
    SCE_HTTP_STATUS_CODE_INSUFFICIENT_STORAGE = 507,
    SCE_HTTP_STATUS_CODE_LOOP_DETECTED = 508,
    SCE_HTTP_STATUS_CODE_NOT_EXTENDED = 510,
    SCE_HTTP_STATUS_CODE_NETWORK_AUTHENTICATION_REQUIRED = 511,
};

enum SceHttpMethods {
    SCE_HTTP_METHOD_GET,
    SCE_HTTP_METHOD_POST,
    SCE_HTTP_METHOD_HEAD,
    SCE_HTTP_METHOD_OPTIONS,
    SCE_HTTP_METHOD_PUT,
    SCE_HTTP_METHOD_DELETE,
    SCE_HTTP_METHOD_TRACE,
    SCE_HTTP_METHOD_CONNECT,
    SCE_HTTP_METHOD_INVALID,

};

enum SceHttpProxyMode {
    SCE_HTTP_PROXY_AUTO,
    SCE_HTTP_PROXY_MANUAL
};

enum SceHttpsFlags : unsigned int {
    SCE_HTTPS_FLAG_SERVER_VERIFY = 1 << 0,
    SCE_HTTPS_FLAG_CLIENT_VERIFY = 1 << 1,
    SCE_HTTPS_FLAG_CN_CHECK = 1 << 2,
    SCE_HTTPS_FLAG_NOT_AFTER_CHECK = 1 << 3,
    SCE_HTTPS_FLAG_NOT_BEFORE_CHECK = 1 << 4,
    SCE_HTTPS_FLAG_KNOWN_CA_CHECK = 1 << 5,
};

struct SceHttpUriElement {
    bool opaque; // always false, http uses //
    Ptr<char> scheme; // http/https
    Ptr<char> username;
    Ptr<char> password;
    Ptr<char> hostname;
    Ptr<char> path; // /foo.html
    Ptr<char> query; // ?a=b
    Ptr<char> fragment; // #middle
    SceUShort16 port;
    SceUChar8 reserved[10];
};

static_assert(sizeof(SceHttpUriElement) == 44, "SceHttpUriElement has incorrect size");

enum SceHttpAddHeaderMode : SceUInt32 {
    SCE_HTTP_HEADER_OVERWRITE,
    SCE_HTTP_HEADER_ADD
};

enum SceHttpErrorCode : uint32_t {
    SCE_HTTP_ERROR_BEFORE_INIT = 0x80431001,
    SCE_HTTP_ERROR_ALREADY_INITED = 0x80431020,
    SCE_HTTP_ERROR_BUSY = 0x80431021,
    SCE_HTTP_ERROR_OUT_OF_MEMORY = 0x80431022,
    SCE_HTTP_ERROR_NOT_FOUND = 0x80431025,
    SCE_HTTP_ERROR_INVALID_VERSION = 0x8043106A,
    SCE_HTTP_ERROR_INVALID_ID = 0x80431100,
    SCE_HTTP_ERROR_OUT_OF_SIZE = 0x80431104,
    SCE_HTTP_ERROR_INVALID_VALUE = 0x804311FE,
    SCE_HTTP_ERROR_INVALID_URL = 0x80433060,
    SCE_HTTP_ERROR_UNKNOWN_SCHEME = 0x80431061,
    SCE_HTTP_ERROR_NETWORK = 0x80431063,
    SCE_HTTP_ERROR_BAD_RESPONSE = 0x80431064,
    SCE_HTTP_ERROR_BEFORE_SEND = 0x80431065,
    SCE_HTTP_ERROR_AFTER_SEND = 0x80431066,
    SCE_HTTP_ERROR_TIMEOUT = 0x80431068,
    SCE_HTTP_ERROR_UNKOWN_AUTH_TYPE = 0x80431069,
    SCE_HTTP_ERROR_UNKNOWN_METHOD = 0x8043106B,
    SCE_HTTP_ERROR_READ_BY_HEAD_METHOD = 0x8043106F,
    SCE_HTTP_ERROR_NOT_IN_COM = 0x80431070,
    SCE_HTTP_ERROR_NO_CONTENT_LENGTH = 0x80431071,
    SCE_HTTP_ERROR_CHUNK_ENC = 0x80431072,
    SCE_HTTP_ERROR_TOO_LARGE_RESPONSE_HEADER = 0x80431073,
    SCE_HTTP_ERROR_SSL = 0x80431073,
    SCE_HTTP_ERROR_ABORTED = 0x80431080,
    SCE_HTTP_ERROR_UNKNOWN = 0x80431081,

    SCE_HTTP_ERROR_PARSE_HTTP_NOT_FOUND = 0x80432025,
    SCE_HTTP_ERROR_PARSE_HTTP_INVALID_RESPONSE = 0x80432060,
    SCE_HTTP_ERROR_PARSE_HTTP_INVALID_VALUE = 0x804321FE,

    SCE_HTTP_ERROR_RESOLVER_EPACKET = 0x80436001,
    SCE_HTTP_ERROR_RESOLVER_ENODNS = 0x80436002,
    SCE_HTTP_ERROR_RESOLVER_ETIMEDOUT = 0x80436003,
    SCE_HTTP_ERROR_RESOLVER_ENOSUPPORT = 0x80436004,
    SCE_HTTP_ERROR_RESOLVER_EFORMAT = 0x80436005,
    SCE_HTTP_ERROR_RESOLVER_ESERVERFAILURE = 0x80436006,
    SCE_HTTP_ERROR_RESOLVER_ENOHOST = 0x80436007,
    SCE_HTTP_ERROR_RESOLVER_ENOTIMPLEMENTED = 0x80436008,
    SCE_HTTP_ERROR_RESOLVER_ESERVERREFUSED = 0x80436009,
    SCE_HTTP_ERROR_RESOLVER_ENORECORD = 0x8043600A
};

struct SceHttpMemoryPoolStats {
    SceSize poolSize;
    SceSize maxInuseSize;
    SceSize currentInuseSize;
    SceInt32 reserved;
};

enum SceHttpVersion : SceInt32 {
    SCE_HTTP_VERSION_1_0 = 1,
    SCE_HTTP_VERSION_1_1 = 2
};

struct SceTemplate {
    std::string userAgent;
    SceHttpVersion httpVersion;
    SceBool autoProxyConf;
    void *ssl;
};

struct SceConnection {
    SceInt tmplId;
    std::string url;
    SceBool keepAlive;
    bool isSecure;
    int sockfd;
};

struct SceRequestResponse {
    std::string httpVer;
    SceInt statusCode = 0;
    std::string reasonPhrase;
    HeadersMapType headers;

    SceULong64 contentLength = 0;
    SceSize responseRead = 0;

    // No need to store the headers as raw as everything is already parsed
    uint8_t *responseRaw = nullptr;
    uint8_t *body = nullptr;
};

struct SceRequest {
    int connId;
    int method;
    std::string url;
    std::string message;
    std::string requestLine;
    SceULong64 contentLength;
    HeadersMapType headers;
    SceRequestResponse res;
    std::vector<Ptr<void>> guestPointers;
};

struct HTTPState {
    bool inited = false;
    bool sslInited = false;
    SceInt defaultResponseHeaderSize = SCE_HTTP_DEFAULT_RESPONSE_HEADER_MAX;
    SceInt next_temp = 0;
    SceInt next_conn = 0;
    SceInt next_req = 0;
    std::map<SceInt, SceTemplate> templates;
    std::map<SceInt, SceConnection> connections;
    std::map<SceInt, SceRequest> requests;
    std::vector<Ptr<void>> guestPointers;
    void *ssl_ctx = nullptr;
};
