// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <http/state.h>

#include <openssl/ssl.h>

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

void HTTPState::shutdown_connections() {
    for (auto &[id, conn] : connections) {
#ifdef _WIN32
        shutdown(conn.sockfd, SD_BOTH);
#else
        shutdown(conn.sockfd, SHUT_RDWR);
#endif
    }
}

void HTTPState::deinit() {
    for (auto &[id, tmpl] : templates) {
        if (tmpl.ssl) {
            SSL_free((SSL *)tmpl.ssl);
        }
    }
    templates.clear();

    for (auto &[id, conn] : connections) {
#ifdef _WIN32
        closesocket(conn.sockfd);
#else
        close(conn.sockfd);
#endif
    }
    connections.clear();

    for (auto &[id, req] : requests) {
        delete[] req.res.responseRaw;
    }
    requests.clear();

    guestPointers.clear();

    if (ssl_ctx) {
        SSL_CTX_free((SSL_CTX *)ssl_ctx);
        ssl_ctx = nullptr;
    }

    inited = false;
    sslInited = false;
    defaultResponseHeaderSize = SCE_HTTP_DEFAULT_RESPONSE_HEADER_MAX;
    next_temp = 0;
    next_conn = 0;
    next_req = 0;
}
