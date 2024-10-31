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

#include <../SceNet/sceNet.h>

#include <net/state.h>

enum SceNetAdhocPdpErrorCodes {
    SCE_ERROR_NET_ADHOCCTL_INVALID_ARG = 0x80410b04, /* Invalid argument was specified. */
    SCE_ERROR_NET_ADHOCCTL_ALREADY_INITIALIZED = 0x80410b07, /* Service is already started. */
    SCE_ERROR_NET_ADHOCCTL_NOT_INITIALIZED = 0x80410b08,/* Service is not ready. */
};


EXPORT(int, sceNetAdhocGetPdpStat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocGetPtpStat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocGetSocketAlert) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocPdpCreate, const SceNetEtherAddr* saddr, SceUShort16 sport, int bufsize, int flag) {
    if (!saddr ) {
        return SCE_ERROR_NET_ADHOCCTL_INVALID_ARG;
    }

    /*const auto recv_id = CALL_EXPORT(sceNetSocket, "SceNetPspAdhoc", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, SCE_NET_IPPROTO_IP);
    if (recv_id < 0) {
        LOG_ERROR("Failed to create adhoc recv socket: {}", log_hex(recv_id));
        continue;
    }

    // set socket options: reuse, broadcast if needed
    int one = 1;
    setsockopt(posix_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    setsockopt(posix_sock, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));

    // bind: if sport == 0, pick ephemeral port
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // or map from saddr if you want
    addr.sin_port = htons(sport ? sport : 0);

    if (bind(posix_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ::close(posix_sock);
        return SCE_ERROR_NET_ADHOC_PORT_IN_USE;
    }

    // allocate a PdpSocket object that stores posix_sock, saddr, bound port etc.
    int vita_socket_id = register_pdp_socket(posix_sock, saddr, sport, bufsize);
    return vita_socket_id;*/
    return UNIMPLEMENTED();
}


EXPORT(int, sceNetAdhocPdpDelete) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocPdpRecv) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocPdpSend) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocPollSocket) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocPtpAccept) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocPtpClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocPtpConnect) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocPtpFlush) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocPtpListen) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocPtpOpen) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocPtpRecv) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocPtpSend) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocSetSocketAlert) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocctlGetAddrByName) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocctlGetAdhocId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocctlGetEtherAddr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocctlGetNameByAddr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocctlGetParameter) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocctlGetPeerInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocctlGetPeerList) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocctlInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetAdhocctlTerm) {
    return UNIMPLEMENTED();
}
