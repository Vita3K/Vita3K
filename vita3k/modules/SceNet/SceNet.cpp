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

#include "SceNet.h"

#include <cstdio>
#include <kernel/state.h>
#include <net/state.h>
#include <net/types.h>
#include <util/lock_and_find.h>

#include <chrono>
#include <thread>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceNet);

template <>
std::string to_debug_str<SceNetEpollControlFlag>(const MemState &mem, SceNetEpollControlFlag type) {
    switch (type) {
    case SCE_NET_EPOLL_CTL_ADD:
        return "SCE_NET_EPOLL_CTL_ADD";
    case SCE_NET_EPOLL_CTL_MOD:
        return "SCE_NET_EPOLL_CTL_MOD";
    case SCE_NET_EPOLL_CTL_DEL:
        return "SCE_NET_EPOLL_CTL_DEL";
    }
    return std::to_string(type);
}

template <>
std::string to_debug_str<SceNetProtocol>(const MemState &mem, SceNetProtocol type) {
    switch (type) {
    case SCE_NET_IPPROTO_IP: return "SCE_NET_IPPROTO_IP";
    case SCE_NET_IPPROTO_ICMP: return "SCE_NET_IPPROTO_ICMP";
    case SCE_NET_IPPROTO_IGMP: return "SCE_NET_IPPROTO_IGMP";
    case SCE_NET_IPPROTO_TCP: return "SCE_NET_IPPROTO_TCP";
    case SCE_NET_IPPROTO_UDP: return "SCE_NET_IPPROTO_UDP";
    case SCE_NET_SOL_SOCKET: return "SCE_NET_SOL_SOCKET";
    }
    return std::to_string(type);
}

template <>
std::string to_debug_str<SceNetSocketType>(const MemState &mem, SceNetSocketType type) {
    switch (type) {
    case SCE_NET_SOCK_STREAM: return "SCE_NET_SOCK_STREAM";
    case SCE_NET_SOCK_DGRAM: return "SCE_NET_SOCK_DGRAM";
    case SCE_NET_SOCK_RAW: return "SCE_NET_SOCK_RAW";
    case SCE_NET_SOCK_DGRAM_P2P: return "SCE_NET_SOCK_DGRAM_P2P";
    case SCE_NET_SOCK_STREAM_P2P: return "SCE_NET_SOCK_STREAM_P2P";
    }
    return std::to_string(type);
}

template <>
std::string to_debug_str<SceNetSocketOption>(const MemState &mem, SceNetSocketOption type) {
    switch (type) {
    /* IP */
    case SCE_NET_IP_HDRINCL: return "SCE_NET_IP_HDRINCL or SCE_NET_TCP_MAXSEG";
    case SCE_NET_IP_TOS: return "SCE_NET_IP_TOS or SCE_NET_TCP_MSS_TO_ADVERTISE";
    case SCE_NET_IP_TTL: return "SCE_NET_IP_TTL or SCE_NET_SO_REUSEADDR";
    case SCE_NET_IP_MULTICAST_IF: return "SCE_NET_IP_MULTICAST_IF";
    case SCE_NET_IP_MULTICAST_TTL: return "SCE_NET_IP_MULTICAST_TTL";
    case SCE_NET_IP_MULTICAST_LOOP: return "SCE_NET_IP_MULTICAST_LOOP";
    case SCE_NET_IP_ADD_MEMBERSHIP: return "SCE_NET_IP_ADD_MEMBERSHIP";
    case SCE_NET_IP_DROP_MEMBERSHIP: return "SCE_NET_IP_DROP_MEMBERSHIP";
    case SCE_NET_IP_TTLCHK: return "SCE_NET_IP_TTLCHK";
    case SCE_NET_IP_MAXTTL: return "SCE_NET_IP_MAXTTL";
    /* TCP */
    case SCE_NET_TCP_NODELAY: return "SCE_NET_TCP_NODELAY";
    // case SCE_NET_TCP_MAXSEG: return "SCE_NET_TCP_MAXSEG";
    // case SCE_NET_TCP_MSS_TO_ADVERTISE: return "SCE_NET_TCP_MSS_TO_ADVERTISE";
    /* SOCKET */
    // case SCE_NET_SO_REUSEADDR: return "SCE_NET_SO_REUSEADDR";
    case SCE_NET_SO_KEEPALIVE: return "SCE_NET_SO_KEEPALIVE";
    case SCE_NET_SO_BROADCAST: return "SCE_NET_SO_BROADCAST";
    case SCE_NET_SO_LINGER: return "SCE_NET_SO_LINGER";
    case SCE_NET_SO_OOBINLINE: return "SCE_NET_SO_OOBINLINE";
    case SCE_NET_SO_REUSEPORT: return "SCE_NET_SO_REUSEPORT";
    case SCE_NET_SO_ONESBCAST: return "SCE_NET_SO_ONESBCAST";
    case SCE_NET_SO_USECRYPTO: return "SCE_NET_SO_USECRYPTO";
    case SCE_NET_SO_USESIGNATURE: return "SCE_NET_SO_USESIGNATURE";
    case SCE_NET_SO_SNDBUF: return "SCE_NET_SO_SNDBUF";
    case SCE_NET_SO_RCVBUF: return "SCE_NET_SO_RCVBUF";
    case SCE_NET_SO_SNDLOWAT: return "SCE_NET_SO_SNDLOWAT";
    case SCE_NET_SO_RCVLOWAT: return "SCE_NET_SO_RCVLOWAT";
    case SCE_NET_SO_SNDTIMEO: return "SCE_NET_SO_SNDTIMEO";
    case SCE_NET_SO_RCVTIMEO: return "SCE_NET_SO_RCVTIMEO";
    case SCE_NET_SO_ERROR: return "SCE_NET_SO_ERROR";
    case SCE_NET_SO_TYPE: return "SCE_NET_SO_TYPE";
    case SCE_NET_SO_NBIO: return "SCE_NET_SO_NBIO";
    case SCE_NET_SO_TPPOLICY: return "SCE_NET_SO_TPPOLICY";
    case SCE_NET_SO_NAME: return "SCE_NET_SO_NAME";
    }
    return std::to_string(type);
}

EXPORT(int, sceNetAccept, int sid, SceNetSockaddr *addr, unsigned int *addrlen) {
    TRACY_FUNC(sceNetAccept, sid, addr, addrlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return RET_ERROR(SCE_NET_EBADF);
    }
    auto newsock = sock->accept(addr, addrlen);
    if (!newsock) {
        return RET_ERROR(-1);
    }
    auto id = ++emuenv.net.next_id;
    emuenv.net.socks.emplace(id, sock);
    return id;
}

EXPORT(int, sceNetBind, int sid, const SceNetSockaddr *addr, unsigned int addrlen) {
    TRACY_FUNC(sceNetBind, sid, addr, addrlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return RET_ERROR(SCE_NET_EBADF);
    }
    return sock->bind(addr, addrlen);
}

EXPORT(int, sceNetClearDnsCache) {
    TRACY_FUNC(sceNetClearDnsCache);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetConnect, int sid, const SceNetSockaddr *addr, unsigned int addrlen) {
    TRACY_FUNC(sceNetConnect, sid, addr, addrlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return RET_ERROR(SCE_NET_EBADF);
    }
    return sock->connect(addr, addrlen);
}

EXPORT(int, sceNetDumpAbort) {
    TRACY_FUNC(sceNetDumpAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetDumpCreate) {
    TRACY_FUNC(sceNetDumpCreate);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetDumpDestroy) {
    TRACY_FUNC(sceNetDumpDestroy);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetDumpRead) {
    TRACY_FUNC(sceNetDumpRead);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetEmulationGet) {
    TRACY_FUNC(sceNetEmulationGet);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetEmulationSet) {
    TRACY_FUNC(sceNetEmulationSet);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetEpollAbort) {
    TRACY_FUNC(sceNetEpollAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetEpollControl, int eid, SceNetEpollControlFlag op, int id, SceNetEpollEvent *ev) {
    TRACY_FUNC(sceNetEpollControl, eid, op, id, ev);

    auto epoll = lock_and_find(eid, emuenv.net.epolls, emuenv.kernel.mutex);
    if (!epoll) {
        return RET_ERROR(SCE_NET_ERROR_EBADF);
    }
    if (id == emuenv.net.resolver_id) {
        STUBBED("Async DNS resolve is not supported");
        return 0;
    }
    auto sock = lock_and_find(id, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return RET_ERROR(SCE_NET_ERROR_EBADF);
    }

    auto posixSocket = std::dynamic_pointer_cast<PosixSocket>(sock);
    if (!posixSocket) {
        return RET_ERROR(SCE_NET_ERROR_EBADF);
    }

    switch (op) {
    case SCE_NET_EPOLL_CTL_ADD:
        return epoll->add(id, posixSocket->sock, ev);
    case SCE_NET_EPOLL_CTL_DEL:
        return epoll->del(id, posixSocket->sock, ev);
    case SCE_NET_EPOLL_CTL_MOD:
        return epoll->mod(id, posixSocket->sock, ev);
    default:
        return RET_ERROR(SCE_NET_ERROR_EINVAL);
    }
}

EXPORT(int, sceNetEpollCreate, const char *name, int flags) {
    TRACY_FUNC(sceNetEpollCreate, name, flags);
    auto id = ++emuenv.net.next_epoll_id;
    auto epoll = std::make_shared<Epoll>();
    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
    emuenv.net.epolls.emplace(id, epoll);
    return id;
}

EXPORT(int, sceNetEpollDestroy, int eid) {
    TRACY_FUNC(sceNetEpollDestroy, eid);

    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
    if (emuenv.net.epolls.erase(eid) == 0) {
        return RET_ERROR(SCE_NET_EBADF);
    }

    return 0;
}

EXPORT(int, sceNetEpollWait, int eid, SceNetEpollEvent *events, int maxevents, int timeout) {
    TRACY_FUNC(sceNetEpollWait, eid, events, maxevents, timeout);
    auto epoll = lock_and_find(eid, emuenv.net.epolls, emuenv.kernel.mutex);
    if (!epoll) {
        return RET_ERROR(SCE_NET_ERROR_EBADF);
    }

    return epoll->wait(events, maxevents, timeout);
}

EXPORT(int, sceNetEpollWaitCB) {
    TRACY_FUNC(sceNetEpollWaitCB);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<int>, sceNetErrnoLoc) {
    TRACY_FUNC(sceNetErrnoLoc);
    // TLS id was taken from disasm source
    auto addr = emuenv.kernel.get_thread_tls_addr(emuenv.mem, thread_id, 0x40);
    return addr.cast<int>();
}

EXPORT(int, sceNetEtherNtostr, SceNetEtherAddr *n, char *str, unsigned int len) {
    TRACY_FUNC(sceNetEtherNtostr, n, str, len);
    if (!emuenv.net.inited)
        return RET_ERROR(SCE_NET_ERROR_ENOTINIT);

    if (!n || !str || len <= 0x11)
        return RET_ERROR(SCE_NET_ERROR_EINVAL);

    snprintf(str, len, "%02x:%02x:%02x:%02x:%02x:%02x",
        n->data[0], n->data[1], n->data[2], n->data[3], n->data[4], n->data[5]);
    return 0;
}

EXPORT(int, sceNetEtherStrton, const char *str, SceNetEtherAddr *n) {
    TRACY_FUNC(sceNetEtherStrton, str, n);
    if (!emuenv.net.inited)
        return RET_ERROR(SCE_NET_ERROR_ENOTINIT);

    if (!str || !n)
        return RET_ERROR(SCE_NET_ERROR_EINVAL);

    sscanf(str, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
        &n->data[0], &n->data[1], &n->data[2], &n->data[3], &n->data[4], &n->data[5]);

    return 0;
}

EXPORT(int, sceNetGetMacAddress, SceNetEtherAddr *addr, int flags) {
    TRACY_FUNC(sceNetGetMacAddress, addr, flags);
    if (addr == nullptr) {
        return RET_ERROR(SCE_NET_EINVAL);
    }
#ifdef WIN32
    IP_ADAPTER_INFO AdapterInfo[16];
    DWORD dwBufLen = sizeof(AdapterInfo);
    if (GetAdaptersInfo(AdapterInfo, &dwBufLen) != ERROR_SUCCESS) {
        return RET_ERROR(SCE_NET_EINVAL);
    } else {
        memcpy(addr->data, AdapterInfo[0].Address, 6);
    }
#else
    // TODO: Implement the function for non Windows OS
    return UNIMPLEMENTED();
#endif
    return 0;
}

EXPORT(int, sceNetGetSockIdInfo) {
    TRACY_FUNC(sceNetGetSockIdInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetGetSockInfo) {
    TRACY_FUNC(sceNetGetSockInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetGetStatisticsInfo) {
    TRACY_FUNC(sceNetGetStatisticsInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetGetpeername) {
    TRACY_FUNC(sceNetGetpeername);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetGetsockname, int sid, SceNetSockaddr *name, unsigned int *namelen) {
    TRACY_FUNC(sceNetGetsockname, sid, name, namelen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return RET_ERROR(SCE_NET_ERROR_EBADF);
    }
    return sock->get_socket_address(name, namelen);
}

EXPORT(int, sceNetGetsockopt, int sid, int level, int optname, void *optval, unsigned int *optlen) {
    TRACY_FUNC(sceNetGetsockopt, sid, level, optname, optval, optlen);

    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return RET_ERROR(SCE_NET_ERROR_EBADF);
    }
    return sock->get_socket_options(level, optname, optval, optlen);
}

EXPORT(unsigned int, sceNetHtonl, unsigned int n) {
    TRACY_FUNC(sceNetHtonl, n);
    return htonl(n);
}

EXPORT(int, sceNetHtonll, SceUInt64 n) {
    TRACY_FUNC(sceNetHtonll, n);
    return HTONLL(n);
}

EXPORT(unsigned short int, sceNetHtons, unsigned short int n) {
    TRACY_FUNC(sceNetHtons, n);
    return htons(n);
}

EXPORT(Ptr<const char>, sceNetInetNtop, int af, const void *src, Ptr<char> dst, unsigned int size) {
    TRACY_FUNC(sceNetInetNtop, af, src, dst, size);
    char *dst_ptr = dst.get(emuenv.mem);
#ifdef WIN32
    const char *res = InetNtop(af, src, dst_ptr, size);
#else
    const char *res = inet_ntop(af, src, dst_ptr, size);
#endif
    if (res == nullptr) {
        RET_ERROR(0x0);
        return Ptr<char>();
    }
    return dst;
}

EXPORT(int, sceNetInetPton, int af, const char *src, void *dst) {
    TRACY_FUNC(sceNetInetPton, af, src, dst);
#ifdef WIN32
    int res = InetPton(af, src, dst);
#else
    int res = inet_pton(af, src, dst);
#endif
    if (res < 0) {
        return RET_ERROR(-1);
    }
    return res;
}

EXPORT(int, sceNetInit, SceNetInitParam *param) {
    TRACY_FUNC(sceNetInit, param);
    if (emuenv.net.inited)
        return RET_ERROR(SCE_NET_ERROR_EBUSY);

    if (!param || !param->memory.address() || param->size < 0x4000 || param->flags != 0)
        return RET_ERROR(SCE_NET_ERROR_EINVAL);

#ifdef WIN32
    WORD versionWanted = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(versionWanted, &wsaData);
#endif
    emuenv.net.state = 0;
    emuenv.net.inited = true;
    emuenv.net.resolver_id = ++emuenv.net.next_id;
    return 0;
}

EXPORT(int, sceNetListen, int sid, int backlog) {
    TRACY_FUNC(sceNetListen, sid, backlog);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return RET_ERROR(SCE_NET_ERROR_EBADF);
    }
    return sock->listen(backlog);
}

EXPORT(unsigned int, sceNetNtohl, unsigned int n) {
    TRACY_FUNC(sceNetNtohl, n);
    return ntohl(n);
}

EXPORT(int, sceNetNtohll, SceUInt64 n) {
    TRACY_FUNC(sceNetNtohll, n);
    return NTOHLL(n);
}

EXPORT(unsigned short int, sceNetNtohs, unsigned short int n) {
    TRACY_FUNC(sceNetNtohs, n);
    return ntohs(n);
}

EXPORT(int, sceNetRecv, int sid, void *buf, unsigned int len, int flags) {
    TRACY_FUNC(sceNetRecv, sid, buf, len, flags);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return RET_ERROR(SCE_NET_ERROR_EBADF);
    }
    return sock->recv_packet(buf, len, flags, nullptr, 0);
}

EXPORT(int, sceNetRecvfrom, int sid, void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) {
    TRACY_FUNC(sceNetRecvfrom, sid, buf, len, flags, from, fromlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return RET_ERROR(SCE_NET_ERROR_EBADF);
    }
    return sock->recv_packet(buf, len, flags, from, fromlen);
}

EXPORT(int, sceNetRecvmsg) {
    TRACY_FUNC(sceNetRecvmsg);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetResolverAbort) {
    TRACY_FUNC(sceNetResolverAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetResolverCreate, const char *name, void *param, int flags) {
    TRACY_FUNC(sceNetResolverCreate, name, param, flags);
    STUBBED("Fake id");
    return emuenv.net.resolver_id;
}

EXPORT(int, sceNetResolverDestroy, int rid) {
    TRACY_FUNC(sceNetResolverDestroy, rid);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetResolverGetError) {
    TRACY_FUNC(sceNetResolverGetError);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetResolverStartAton, int rid, const SceNetInAddr *addr, char *hostname, int len, int timeout, int retry, int flags) {
    TRACY_FUNC(sceNetResolverStartAton, rid, addr, hostname, len, timeout, retry, flags);
    struct hostent *resolved = gethostbyaddr((const char *)addr, len, AF_INET);
    strcpy(hostname, resolved->h_name);
    return 0;
}

EXPORT(int, sceNetResolverStartNtoa, int rid, const char *emuenvname, SceNetInAddr *addr, int timeout, int retry, int flags) {
    TRACY_FUNC(sceNetResolverStartNtoa, rid, emuenvname, addr, timeout, retry, flags);
    struct hostent *resolved = gethostbyname(emuenvname);
    if (resolved == nullptr) {
        memset(addr, 0, sizeof(*addr));
        return RET_ERROR(-1);
    }
    memcpy(addr, resolved->h_addr, sizeof(uint32_t));
    return 0;
}

EXPORT(int, sceNetSend, int sid, const void *msg, unsigned int len, int flags) {
    TRACY_FUNC(sceNetSend, sid, msg, len, flags);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return RET_ERROR(SCE_NET_EBADF);
    }
    return sock->send_packet(msg, len, flags, nullptr, 0);
}

EXPORT(int, sceNetSendmsg) {
    TRACY_FUNC(sceNetSendmsg);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSendto, int sid, const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) {
    TRACY_FUNC(sceNetSendto, sid, msg, len, flags, to, tolen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return RET_ERROR(SCE_NET_EBADF);
    }
    return sock->send_packet(msg, len, flags, to, tolen);
}

EXPORT(int, sceNetSetDnsInfo) {
    TRACY_FUNC(sceNetSetDnsInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSetsockopt, int sid, SceNetProtocol level, SceNetSocketOption optname, const int *optval, unsigned int optlen) {
    TRACY_FUNC(sceNetSetsockopt, sid, level, optname, *optval, optlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return RET_ERROR(SCE_NET_EBADF);
    }
    if (optname == 0x40000) {
        LOG_ERROR("Unknown socket option {}", log_hex(optname));
        return 0;
    }
    return sock->set_socket_options(level, optname, optval, optlen);
}

EXPORT(int, sceNetShowIfconfig) {
    TRACY_FUNC(sceNetShowIfconfig);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetShowNetstat) {
    TRACY_FUNC(sceNetShowNetstat);
    if (!emuenv.net.inited) {
        return RET_ERROR(SCE_NET_ERROR_ENOTINIT);
    }
    return 0;
}

EXPORT(int, sceNetShowRoute) {
    TRACY_FUNC(sceNetShowRoute);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetShutdown) {
    TRACY_FUNC(sceNetShutdown);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSocket, const char *name, int domain, SceNetSocketType type, SceNetProtocol protocol) {
    TRACY_FUNC(sceNetSocket, name, domain, type, protocol);
    SocketPtr sock;
    if (type < SCE_NET_SOCK_STREAM || type > SCE_NET_SOCK_RAW) {
        sock = std::make_shared<P2PSocket>(domain, type, protocol);
    } else {
        sock = std::make_shared<PosixSocket>(domain, type, protocol);
    }
    auto id = ++emuenv.net.next_id;
    emuenv.net.socks.emplace(id, sock);
    return id;
}

EXPORT(int, sceNetSocketAbort) {
    TRACY_FUNC(sceNetSocketAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSocketClose, int sid) {
    TRACY_FUNC(sceNetSocketClose, sid);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return RET_ERROR(SCE_NET_EBADF);
    }
    return sock->close();
}

EXPORT(int, sceNetTerm) {
    TRACY_FUNC(sceNetTerm);
    if (!emuenv.net.inited) {
        return RET_ERROR(SCE_NET_ERROR_ENOTINIT);
    }
#ifdef WIN32
    WSACleanup();
#endif
    emuenv.net.inited = false;
    return 0;
}
