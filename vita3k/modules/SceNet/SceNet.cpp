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

#include "SceNet.h"

#include <kernel/state.h>

#include <net/state.h>

#include <util/lock_and_find.h>
#include <util/net_utils.h>

#include <chrono>
#include <cstdio>
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

static int ret_net_errno(EmuEnvState &emuenv, int thread_id, int ret) {
    if (ret < 0) {
        auto addr = emuenv.kernel.get_thread_tls_addr(emuenv.mem, thread_id, TLS_NET_ERRNO);
        if (addr) {
            auto inner_ptr = addr.get(emuenv.mem);
            if (inner_ptr)
                *reinterpret_cast<int *>(inner_ptr) = ret & 0xff;
        }
    }

    return ret;
}

#define RET_NET_ERRNO(ret)                                \
    do {                                                  \
        int _r = ret_net_errno(emuenv, thread_id, (ret)); \
        return (_r < 0 ? RET_ERROR(_r) : _r);             \
    } while (0)

EXPORT(int, sceNetAccept, int sid, SceNetSockaddr *addr, unsigned int *addrlen) {
    TRACY_FUNC(sceNetAccept, sid, addr, addrlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock)
        RET_NET_ERRNO(SCE_NET_ERROR_EBADF);

    int err = 0;
    auto newsock = sock->accept(addr, addrlen, err);
    LOG_DEBUG("sceNetAccept, sid: {}, err: {}", sid, log_hex(err));
    if (!newsock)
        RET_NET_ERRNO(err);

    auto id = ++emuenv.net.next_id;
    emuenv.net.socks.emplace(id, newsock);
    LOG_INFO("Success emplace newsock, id: {}, type: {}", id, newsock->sce_type);
    return id;
}

EXPORT(int, sceNetBind, int sid, const SceNetSockaddr *addr, unsigned int addrlen) {
    TRACY_FUNC(sceNetBind, sid, addr, addrlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock)
        RET_NET_ERRNO(SCE_NET_ERROR_EBADF);

    LOG_DEBUG("sid: {}", sid);
    RET_NET_ERRNO(sock ? sock->bind(addr, addrlen) : SCE_NET_ERROR_EBADF);
}

EXPORT(int, sceNetClearDnsCache) {
    TRACY_FUNC(sceNetClearDnsCache);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetConnect, int sid, const SceNetSockaddr *addr, unsigned int addrlen) {
    TRACY_FUNC(sceNetConnect, sid, addr, addrlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    LOG_INFO("sceNetConnect, sid: {}", sid);
    RET_NET_ERRNO(sock ? sock->connect(addr, addrlen) : SCE_NET_ERROR_EBADF);
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
    LOG_INFO("sceNetEpollControl, eid: {}, op: {}, id: {}", eid, to_debug_str(emuenv.mem, op), id);
    auto epoll = lock_and_find(eid, emuenv.net.epolls, emuenv.kernel.mutex);
    if (!epoll)
        RET_NET_ERRNO(SCE_NET_ERROR_EBADF);

    if (id == emuenv.net.resolver_id) {
        STUBBED("Async DNS resolve is not supported");
        return 0;
    }

    switch (op) {
    case SCE_NET_EPOLL_CTL_ADD: {
        const auto sock = lock_and_find(id, emuenv.net.socks, emuenv.kernel.mutex);
        RET_NET_ERRNO(sock ? epoll->add(id, sock, ev) : SCE_NET_ERROR_EBADF);
    }
    case SCE_NET_EPOLL_CTL_DEL:
        RET_NET_ERRNO(epoll->del(id));
    case SCE_NET_EPOLL_CTL_MOD:
        RET_NET_ERRNO(epoll->mod(id, ev));
    default:
        RET_NET_ERRNO(SCE_NET_ERROR_EINVAL);
    }
}

EXPORT(int, sceNetEpollCreate, const char *name, int flags) {
    TRACY_FUNC(sceNetEpollCreate, name, flags);
    auto id = ++emuenv.net.next_epoll_id;
    auto epoll = std::make_shared<Epoll>();
    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
    LOG_INFO("sceNetEpollCreate, id: {}, name: {}, flags: {}", id, name ? name : "null", flags);
    emuenv.net.epolls.emplace(id, epoll);
    return id;
}

EXPORT(int, sceNetEpollDestroy, int eid) {
    TRACY_FUNC(sceNetEpollDestroy, eid);
    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    RET_NET_ERRNO(emuenv.net.epolls.erase(eid) ? 0 : SCE_NET_ERROR_EBADF);
}

EXPORT(int, sceNetEpollWait, int eid, SceNetEpollEvent *events, int maxevents, int timeout) {
    TRACY_FUNC(sceNetEpollWait, eid, events, maxevents, timeout);
    auto epoll = lock_and_find(eid, emuenv.net.epolls, emuenv.kernel.mutex);
    //LOG_INFO("sceNetEpollWait, eid: {}, maxevents: {}, timeout: {}", eid, maxevents, timeout);
    RET_NET_ERRNO(epoll ? epoll->wait(events, maxevents, timeout) : SCE_NET_ERROR_EBADF);
}

EXPORT(int, sceNetEpollWaitCB) {
    TRACY_FUNC(sceNetEpollWaitCB);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<int>, sceNetErrnoLoc) {
    TRACY_FUNC(sceNetErrnoLoc);
    // TLS id was taken from disasm source
    auto addr = emuenv.kernel.get_thread_tls_addr(emuenv.mem, thread_id, TLS_NET_ERRNO);
    Ptr<void> *inner_ptr = addr.get(emuenv.mem);
    int value = 0;
    if (inner_ptr)
        value = *reinterpret_cast<int *>(inner_ptr);

    //LOG_INFO("sceNetErrnoLoc, thread_id: {}, value: {}", thread_id, log_hex(value));
    return addr.cast<int>();
}

EXPORT(int, sceNetEtherNtostr, SceNetEtherAddr *n, char *str, unsigned int len) {
    TRACY_FUNC(sceNetEtherNtostr, n, str, len);
    if (!emuenv.net.inited)
        RET_NET_ERRNO(SCE_NET_ERROR_ENOTINIT);

    if (!n || !str || len <= 0x11)
        RET_NET_ERRNO(SCE_NET_ERROR_EINVAL);

    snprintf(str, len, "%02x:%02x:%02x:%02x:%02x:%02x",
        n->data[0], n->data[1], n->data[2], n->data[3], n->data[4], n->data[5]);
    return 0;
}

EXPORT(int, sceNetEtherStrton, const char *str, SceNetEtherAddr *n) {
    TRACY_FUNC(sceNetEtherStrton, str, n);
    if (!emuenv.net.inited)
        RET_NET_ERRNO(SCE_NET_ERROR_ENOTINIT);

    if (!str || !n)
        RET_NET_ERRNO(SCE_NET_ERROR_EINVAL);

    sscanf(str, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
        &n->data[0], &n->data[1], &n->data[2], &n->data[3], &n->data[4], &n->data[5]);

    return 0;
}

EXPORT(int, sceNetGetMacAddress, SceNetEtherAddr *addr, int flags) {
    TRACY_FUNC(sceNetGetMacAddress, addr, flags);
    if (addr == nullptr) {
        RET_NET_ERRNO(SCE_NET_ERROR_EINVAL);
    }
#ifdef _WIN32
    IP_ADAPTER_INFO AdapterInfo[16];
    DWORD dwBufLen = sizeof(AdapterInfo);
    if (GetAdaptersInfo(AdapterInfo, &dwBufLen) != ERROR_SUCCESS)
        RET_NET_ERRNO(SCE_NET_ERROR_EINVAL);
    else
        memcpy(addr->data, AdapterInfo[0].Address, 6);
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

struct SceNetStatisticsInfo {
    int kernel_mem_free_size;
    int kernel_mem_free_min;
    int packet_count;
    int packet_qos_count;
    int libnet_mem_free_size;
    int libnet_mem_free_min;
};

EXPORT(int, sceNetGetStatisticsInfo, SceNetStatisticsInfo *info, int flags) {
    TRACY_FUNC(sceNetGetStatisticsInfo, info, flags);
    if (!info)
        return SCE_NET_EINVAL;
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetGetpeername, int sid, SceNetSockaddr *addr, unsigned int *addrlen) {
    TRACY_FUNC(sceNetGetpeername, sid, addr, addrlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    LOG_DEBUG("sceNetGetpeername, sid: {}", sid);
    RET_NET_ERRNO(sock ? sock->get_peer_address(addr, addrlen) : SCE_NET_ERROR_EBADF);
}

EXPORT(int, sceNetGetsockname, int sid, SceNetSockaddr *addr, unsigned int *addrlen) {
    TRACY_FUNC(sceNetGetsockname, sid, addr, addrlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    LOG_DEBUG("sceNetGetsockname, sid: {}", sid);
    RET_NET_ERRNO(sock ? sock->get_socket_address(addr, addrlen) : SCE_NET_ERROR_EBADF);
}

EXPORT(int, sceNetGetsockopt, int sid, int level, int optname, void *optval, unsigned int *optlen) {
    TRACY_FUNC(sceNetGetsockopt, sid, level, optname, optval, optlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);

    RET_NET_ERRNO(sock ? sock->get_socket_options(level, optname, optval, optlen) : SCE_NET_ERROR_EBADF);
}

EXPORT(SceUInt32, sceNetHtonl, SceUInt32 n) {
    TRACY_FUNC(sceNetHtonl, n);
    return htonl(n);
}

EXPORT(SceUInt64, sceNetHtonll, SceUInt64 n) {
    TRACY_FUNC(sceNetHtonll, n);
    return HTONLL(n);
}

EXPORT(SceUInt16, sceNetHtons, SceUInt16 n) {
    TRACY_FUNC(sceNetHtons, n);
    return htons(n);
}

EXPORT(Ptr<const char>, sceNetInetNtop, int af, const void *src, Ptr<char> dst, unsigned int size) {
    TRACY_FUNC(sceNetInetNtop, af, src, dst, size);
    char *dst_ptr = dst.get(emuenv.mem);
#ifdef _WIN32
    const char *res = InetNtop(af, src, dst_ptr, size);
#else
    const char *res = inet_ntop(af, src, dst_ptr, size);
#endif
    if (res == nullptr) {
        ret_net_errno(emuenv, thread_id, SCE_NET_ERROR_EAFNOSUPPORT);
        return Ptr<char>();
    }
    return dst;
}

EXPORT(int, sceNetInetPton, int af, const char *src, void *dst) {
    TRACY_FUNC(sceNetInetPton, af, src, dst);

    if (af != SCE_NET_AF_INET)
        RET_NET_ERRNO(SCE_NET_ERROR_EAFNOSUPPORT);

#ifdef _WIN32
    int res = InetPton(af, src, dst);
#else
    int res = inet_pton(af, src, dst);
#endif

    RET_NET_ERRNO(res == 0 ? SCE_NET_ERROR_EINVAL : PosixSocket::translate_return_value(res));
}

EXPORT(int, sceNetInit, SceNetInitParam *param) {
    TRACY_FUNC(sceNetInit, param);
    LOG_DEBUG("sceNetInit");
    if (emuenv.net.inited)
        RET_NET_ERRNO(SCE_NET_ERROR_EBUSY);

    if (!param || !param->memory.address() || param->size < 0x4000 || param->flags != 0)
        RET_NET_ERRNO(SCE_NET_ERROR_EINVAL);

#ifdef _WIN32
    WORD versionWanted = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(versionWanted, &wsaData);
#endif
    emuenv.net.state = 0;
    emuenv.net.inited = true;
    emuenv.net.resolver_id = ++emuenv.net.next_id;
    net_utils::init_address(emuenv.cfg.adhoc_addr, emuenv.net.netAddr, emuenv.net.broadcastAddr);
    emuenv.net.current_addr_index = emuenv.cfg.adhoc_addr;
    return 0;
}

EXPORT(int, sceNetListen, int sid, int backlog) {
    TRACY_FUNC(sceNetListen, sid, backlog);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        RET_NET_ERRNO(SCE_NET_ERROR_EBADF);
    }
    LOG_INFO("sceNetListen, sid: {}, backlog: {}", sid, backlog);
    RET_NET_ERRNO(sock->listen(backlog));
}

EXPORT(SceUInt32, sceNetNtohl, SceUInt32 n) {
    TRACY_FUNC(sceNetNtohl, n);
    return ntohl(n);
}

EXPORT(SceUInt64, sceNetNtohll, SceUInt64 n) {
    TRACY_FUNC(sceNetNtohll, n);
    return NTOHLL(n);
}

EXPORT(SceUInt16, sceNetNtohs, SceUInt16 n) {
    TRACY_FUNC(sceNetNtohs, n);
    return ntohs(n);
}

EXPORT(int, sceNetRecv, int sid, void *buf, unsigned int len, int flags) {
    TRACY_FUNC(sceNetRecv, sid, buf, len, flags);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    LOG_INFO("SceNetRecv, sid: {}", sid);
    RET_NET_ERRNO(sock ? sock->recv_packet(buf, len, flags, nullptr, 0) : SCE_NET_ERROR_EBADF);
}

EXPORT(int, sceNetRecvfrom, int sid, void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) {
    TRACY_FUNC(sceNetRecvfrom, sid, buf, len, flags, from, fromlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock)
        RET_NET_ERRNO(SCE_NET_ERROR_EBADF);

    LOG_INFO("sceNetRecvfrom, sid: {}, flags: {}", sid, log_hex(flags));
    const auto ret = sock->recv_packet(buf, len, flags, from, fromlen);
    if (ret < 0)
        LOG_ERROR("sceNetRecvfrom, sid: {}, ret: {}", sid, log_hex(ret));
    else 
        LOG_INFO("sceNetRecvfrom, sid: {}, ret: {}", sid, ret);

    RET_NET_ERRNO(ret);
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
    LOG_DEBUG("sceNetResolverCreate, name: {}, flags: {}", name ? name : "unnamed", flags);
    STUBBED("Fake id");
    return emuenv.net.resolver_id;
}

EXPORT(int, sceNetResolverDestroy, int rid) {
    TRACY_FUNC(sceNetResolverDestroy, rid);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetResolverGetError, int rid, int *result) {
    TRACY_FUNC(sceNetResolverGetError, rid, result);
    LOG_WARN("sceNetResolverGetError: stubbed, rid: {}", rid);

    if (!result)
        RET_NET_ERRNO(SCE_NET_EINVAL);

    *result = 0;
    return 0;
}

EXPORT(int, sceNetResolverStartAton, int rid, const SceNetInAddr *addr, char *hostname, int len, int timeout, int retry, int flags) {
    TRACY_FUNC(sceNetResolverStartAton, rid, addr, hostname, len, timeout, retry, flags);
    struct hostent *resolved = gethostbyaddr((const char *)addr, len, AF_INET);
    strcpy(hostname, resolved->h_name);
    return 0;
}

EXPORT(int, sceNetResolverStartNtoa, int rid, const char *hostname, SceNetInAddr *addr, int timeout, int retry, int flags) {
    TRACY_FUNC(sceNetResolverStartNtoa, rid, hostname, addr, timeout, retry, flags);
    LOG_DEBUG("sceNetResolverStartNtoa, rid: {}, hostname: {}, timeout: {}, retry: {}, flags: {}", rid, hostname, timeout, retry, flags);
    if (!hostname || !addr || strstr(hostname, "..") || hostname[0] == '.' || hostname[strlen(hostname) - 1] == '.') {
        RET_NET_ERRNO(SCE_NET_EINVAL);
    }

    /* if (flags & SCE_NET_RESOLVER_START_NTOA_DISABLE_IPADDRESS) {
        struct in_addr dummy;
        if (inet_aton(hostname, &dummy)) {
            LOG_WARN("IP address not allowed due to DISABLE_IPADDRESS flag.");
            RET_NET_ERRNO(SCE_NET_EINVAL);
        }
    }*/

    struct hostent *resolved = gethostbyname(hostname);
    if (!resolved || !resolved->h_addr_list || !resolved->h_addr_list[0]) {
        LOG_WARN("sceNetResolverStartNtoa: failed to resolve '{}'", hostname);
        addr->s_addr = 0;
        RET_NET_ERRNO(SCE_NET_EHOSTUNREACH);
    }

    memcpy(&addr->s_addr, resolved->h_addr_list[0], sizeof(addr->s_addr));
    struct in_addr tmp;
    memcpy(&tmp.S_un.S_addr, resolved->h_addr_list[0], sizeof(tmp.S_un.S_addr));
    LOG_INFO("sceNetResolverStartNtoa: resolved '{}' to {}", hostname, inet_ntoa(tmp));
    return 0;
}

EXPORT(int, sceNetSend, int sid, const void *msg, unsigned int len, int flags) {
    TRACY_FUNC(sceNetSend, sid, msg, len, flags);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    LOG_INFO("SceNetSend, sid: {}", sid); 
    RET_NET_ERRNO(sock ? sock->send_packet(msg, len, flags, nullptr, 0) : SCE_NET_ERROR_EBADF);
}

EXPORT(int, sceNetSendmsg, int sid, const SceNetMsghdr *msg, int flags) {
    TRACY_FUNC(sceNetSendmsg, sid, msg, flags);
    LOG_INFO("sceNetSendmsg, sid: {}, flags: {}, msg iovlen: {}", sid, flags, msg->msg_iovlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock)
        RET_NET_ERRNO(SCE_NET_ERROR_EBADF);

    if (flags & SCE_NET_MSG_DONTWAIT) {
        // sock->set_non_blocking(true);
    }

    const SceNetSockaddr *dest_addr = static_cast<const SceNetSockaddr *>(msg->msg_name);
    size_t total_sent = 0;
    for (int i = 0; i < msg->msg_iovlen; ++i) {
        const SceNetIovec &iov = msg->msg_iov[i];
        auto res = sock->send_packet(iov.iov_base, iov.iov_len, flags, dest_addr, msg->msg_namelen);
        total_sent += res;
    }

    if (flags & SCE_NET_MSG_DONTWAIT) {
        // sock->set_non_blocking(false);
    }

    return (int)total_sent;
}

EXPORT(int, sceNetSendto, int sid, const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) {
    TRACY_FUNC(sceNetSendto, sid, msg, len, flags, to, tolen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    LOG_DEBUG("sceNetSendto, sid: {}", sid);

    if (!sock)
        RET_NET_ERRNO(SCE_NET_ERROR_EBADF);

    SceNetSockaddrIn to_in;
    std::memcpy(&to_in, to, sizeof(SceNetSockaddrIn));

    if (/*!sock->sockopt_so_onesbcast && */(to_in.sin_addr.s_addr == INADDR_BROADCAST))
        to_in.sin_addr.s_addr = emuenv.net.broadcastAddr;

    RET_NET_ERRNO(sock->send_packet(msg, len, flags, (SceNetSockaddr *)&to_in, tolen));
}

EXPORT(int, sceNetSetDnsInfo) {
    TRACY_FUNC(sceNetSetDnsInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSetsockopt, int sid, SceNetProtocol level, SceNetSocketOption optname, const void *optval, unsigned int optlen) {
    TRACY_FUNC(sceNetSetsockopt, sid, level, optname, optval, optlen);
    LOG_INFO("sceNetSetsockopt, sid: {}, level: {}, optname: {}, optval: {}, optlen: {}", sid,
        to_debug_str(emuenv.mem, level), to_debug_str(emuenv.mem, optname), *reinterpret_cast<const int *>(optval), optlen);

    if (optname == 0x40000) {
        LOG_ERROR("Unknown socket option {}", log_hex(optname));
        return 0;
    }
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);

    RET_NET_ERRNO(sock ? sock->set_socket_options(level, optname, optval, optlen) : SCE_NET_ERROR_EBADF);
}

EXPORT(int, sceNetShowIfconfig) {
    TRACY_FUNC(sceNetShowIfconfig);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetShowNetstat) {
    TRACY_FUNC(sceNetShowNetstat);
    if (!emuenv.net.inited) {
        RET_NET_ERRNO(SCE_NET_ERROR_ENOTINIT);
    }
    return 0;
}

EXPORT(int, sceNetShowRoute) {
    TRACY_FUNC(sceNetShowRoute);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetShutdown, int sid, int how) {
    TRACY_FUNC(sceNetShutdown, sid, how);
    LOG_INFO("sceNetShutdown, sid: {}, how: {}", sid, how);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);

    RET_NET_ERRNO(sock ? sock->shutdown_socket(how) : SCE_NET_ERROR_EBADF);
}

EXPORT(int, sceNetSocket, const char *name, int domain, SceNetSocketType type, SceNetProtocol protocol) {
    TRACY_FUNC(sceNetSocket, name, domain, type, protocol);
    bool isP2P = (type == SCE_NET_SOCK_DGRAM_P2P || type == SCE_NET_SOCK_STREAM_P2P);

    SocketPtr sock = isP2P ? std::make_shared<P2PSocket>(domain, type, protocol) : std::make_shared<PosixSocket>(domain, type, protocol);

    auto id = ++emuenv.net.next_id;
    LOG_DEBUG("Socket, name: {}, type: {}, id: {}, type: {}", name, to_debug_str(emuenv.mem, type), id, sock->sce_type);
    emuenv.net.socks.emplace(id, sock);
    return id;
}

EXPORT(int, sceNetSocketAbort, int sid, int flags) {
    TRACY_FUNC(sceNetSocketAbort, sid);
    LOG_INFO("sceNetSocketAbort, sid: {}, flags: {}", sid, flags);
    if ((sid < 0) || (flags < 0) || (flags > (SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION | SCE_NET_SOCKET_ABORT_FLAG_SND_PRESERVATION)))
        RET_NET_ERRNO(SCE_NET_ERROR_EINVAL);

    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    RET_NET_ERRNO(sock ? sock->abort(flags) : SCE_NET_ERROR_EBADF);
}

EXPORT(int, sceNetSocketClose, int sid) {
    TRACY_FUNC(sceNetSocketClose, sid);
    LOG_DEBUG("sceNetSocketClose, sid: {}", sid);
    int result = 0;
    {
        std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
        const auto sock = util::find(sid, emuenv.net.socks);
        if (sock) {
            result = sock->close();
            if (result >= 0)
                emuenv.net.socks.erase(sid);
        } else
            result = SCE_NET_ERROR_EBADF;
    }
    LOG_DEBUG("sceNetSocketClose, sid: {}, result: {}", sid, log_hex(result));
    RET_NET_ERRNO(result);
}

EXPORT(int, sceNetTerm) {
    TRACY_FUNC(sceNetTerm);
    LOG_INFO("sceNetTerm");
    if (!emuenv.net.inited) {
        RET_NET_ERRNO(SCE_NET_ERROR_ENOTINIT);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    emuenv.net.inited = false;
    return 0;
}
