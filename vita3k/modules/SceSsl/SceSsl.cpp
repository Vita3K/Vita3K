// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <module/module.h>

#include <http/state.h>
#include <openssl/ssl.h>
#include <util/tracy.h>
#include <util/types.h>

TRACY_MODULE_NAME(SceSsl);

enum SceSslErrorCode {
    SCE_SSL_ERROR_BEFORE_INIT = 0x80435001,
    SCE_SSL_ERROR_ALREADY_INITED = 0x80435020,
    SCE_SSL_ERROR_OUT_OF_MEMORY = 0x80435022,
    SCE_SSL_ERROR_INTERNAL = 0x80435026,
    SCE_SSL_ERROR_NOT_FOUND = 0x80435025,
    SCE_SSL_ERROR_INVALID_VALUE = 0x804351fe,
    SCE_SSL_ERROR_INVALID_FORMAT = 0x80435108,
    SCE_SSL_ERROR_R_ERROR_BASE = 0x80435200,
    SCE_SSL_ERROR_R_ERROR_FAILED = 0x80435201,
    SCE_SSL_ERROR_R_ERROR_UNKNOWN = 0x804352ff,
};

EXPORT(int, sceSslFreeSslCertName) {
    TRACY_FUNC(sceSslFreeSslCertName);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSslGetIssuerName) {
    TRACY_FUNC(sceSslGetIssuerName);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSslGetMemoryPoolStats) {
    TRACY_FUNC(sceSslGetMemoryPoolStats);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSslGetNameEntryCount) {
    TRACY_FUNC(sceSslGetNameEntryCount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSslGetNameEntryInfo) {
    TRACY_FUNC(sceSslGetNameEntryInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSslGetNotAfter) {
    TRACY_FUNC(sceSslGetNotAfter);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSslGetNotBefore) {
    TRACY_FUNC(sceSslGetNotBefore);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSslGetSerialNumber) {
    TRACY_FUNC(sceSslGetSerialNumber);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSslGetSubjectName) {
    TRACY_FUNC(sceSslGetSubjectName);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceSslInit, SceSize poolSize) {
    TRACY_FUNC(sceSslInit, poolSize);
    if (emuenv.http.sslInited)
        return RET_ERROR(SCE_SSL_ERROR_ALREADY_INITED);

    if (poolSize == 0)
        return RET_ERROR(SCE_SSL_ERROR_INVALID_VALUE);

    STUBBED("ignore poolSize");
    SSL_library_init();
    SSL_load_error_strings();

    if (emuenv.http.inited && !emuenv.http.ssl_ctx)
        emuenv.http.ssl_ctx = SSL_CTX_new(TLS_method());

    emuenv.http.sslInited = true;

    return 0;
}

EXPORT(SceInt32, sceSslTerm) {
    TRACY_FUNC(sceSslTerm);
    if (!emuenv.http.sslInited)
        return RET_ERROR(SCE_SSL_ERROR_BEFORE_INIT);

    emuenv.http.sslInited = false;

    return 0;
}
