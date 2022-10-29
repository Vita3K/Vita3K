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

#include "SceNet.h"

#include <kernel/state.h>
#include <net/functions.h>
#include <net/state.h>
#include <net/types.h>
#include <util/lock_and_find.h>

#include <chrono>
#include <thread>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceNet);

EXPORT(int, sceNetAccept, int sid, SceNetSockaddr *addr, unsigned int *addrlen) {
    TRACY_FUNC(sceNetAccept, sid, addr, addrlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return -1;
    }
    auto newsock = sock->accept(addr, addrlen);
    if (!newsock) {
        return -1;
    }
    auto id = ++emuenv.net.next_id;
    emuenv.net.socks.emplace(id, sock);
    return id;
}

EXPORT(int, sceNetBind, int sid, const SceNetSockaddr *addr, unsigned int addrlen) {
    TRACY_FUNC(sceNetBind, sid, addr, addrlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return -1;
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
        return -1;
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

EXPORT(int, sceNetEpollControl) {
    TRACY_FUNC(sceNetEpollControl);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetEpollCreate, const char *name) {
    TRACY_FUNC(sceNetEpollCreate, name);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetEpollDestroy) {
    TRACY_FUNC(sceNetEpollDestroy);
    return UNIMPLEMENTED();
}

struct SceNetEpollEvent {}; // TODO fill this

EXPORT(int, sceNetEpollWait, int sid, SceNetEpollEvent *events, int maxevents, int timeout) {
    TRACY_FUNC(sceNetEpollWait, sid, events, maxevents, timeout);
    auto x = std::chrono::microseconds(timeout);
    std::this_thread::sleep_for(x);
    return STUBBED("only timeout");
}

EXPORT(int, sceNetEpollWaitCB) {
    TRACY_FUNC(sceNetEpollWaitCB);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetErrnoLoc) {
    TRACY_FUNC(sceNetErrnoLoc);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetEtherNtostr) {
    TRACY_FUNC(sceNetEtherNtostr);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetEtherStrton) {
    TRACY_FUNC(sceNetEtherStrton);
    return UNIMPLEMENTED();
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
        memcpy(addr->data, AdapterInfo->Address, 6);
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
        return -1;
    }
    return sock->get_socket_address(name, namelen);
}

EXPORT(int, sceNetGetsockopt) {
    TRACY_FUNC(sceNetGetsockopt);
    return UNIMPLEMENTED();
}

EXPORT(unsigned int, sceNetHtonl, unsigned int n) {
    TRACY_FUNC(sceNetHtonl, n);
    return htonl(n);
}

EXPORT(int, sceNetHtonll) {
    TRACY_FUNC(sceNetHtonll);
    return UNIMPLEMENTED();
}

EXPORT(unsigned short int, sceNetHtons, unsigned short int n) {
    TRACY_FUNC(sceNetHtons, n);
    return htons(n);
}

EXPORT(Ptr<const char>, sceNetInetNtop, int af, const void *src, Ptr<char> dst, unsigned int size) {
    TRACY_FUNC(sceNetInetNtop, af, src, dst, size);
    char *dst_ptr = dst.get(emuenv.mem);
#ifdef WIN32
    const char *res = InetNtop(af, const_cast<void *>(src), dst_ptr, size);
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
        return -1;
    }
    return res;
}

EXPORT(int, sceNetInit, SceNetInitParam *param) {
    TRACY_FUNC(sceNetInit, param);
    if (emuenv.net.inited) {
        return RET_ERROR(SCE_NET_ERROR_EBUSY);
    }
#ifdef WIN32
    WORD versionWanted = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(versionWanted, &wsaData);
#endif
    emuenv.net.state = 0;
    emuenv.net.inited = true;
    return 0;
}

EXPORT(int, sceNetListen, int sid, int backlog) {
    TRACY_FUNC(sceNetListen, sid, backlog);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return -1;
    }
    return sock->listen(backlog);
}

EXPORT(unsigned int, sceNetNtohl, unsigned int n) {
    TRACY_FUNC(sceNetNtohl, n);
    return ntohl(n);
}

EXPORT(int, sceNetNtohll) {
    TRACY_FUNC(sceNetNtohll);
    return UNIMPLEMENTED();
}

EXPORT(unsigned short int, sceNetNtohs, unsigned short int n) {
    TRACY_FUNC(sceNetNtohs, n);
    return ntohs(n);
}

EXPORT(int, sceNetRecv, int sid, void *buf, unsigned int len, int flags) {
    TRACY_FUNC(sceNetRecv, sid, buf, len, flags);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return -1;
    }
    return sock->recv_packet(buf, len, flags, nullptr, 0);
}

EXPORT(int, sceNetRecvfrom, int sid, void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) {
    TRACY_FUNC(sceNetRecvfrom, sid, buf, len, flags, from, fromlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return -1;
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

EXPORT(int, sceNetResolverCreate) {
    TRACY_FUNC(sceNetResolverCreate);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetResolverDestroy) {
    TRACY_FUNC(sceNetResolverDestroy);
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
    memcpy(addr, resolved->h_addr, sizeof(uint32_t));
    return 0;
}

EXPORT(int, sceNetSend, int sid, const void *msg, unsigned int len, int flags) {
    TRACY_FUNC(sceNetSend, sid, msg, len, flags);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return -1;
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
        return -1;
    }
    return sock->send_packet(msg, len, flags, to, tolen);
}

EXPORT(int, sceNetSetDnsInfo) {
    TRACY_FUNC(sceNetSetDnsInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetSetsockopt, int sid, int level, int optname, const void *optval, unsigned int optlen) {
    TRACY_FUNC(sceNetSetsockopt, sid, level, optname, optval, optlen);
    auto sock = lock_and_find(sid, emuenv.net.socks, emuenv.kernel.mutex);
    if (!sock) {
        return -1;
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

EXPORT(int, sceNetSocket, const char *name, int domain, int type, int protocol) {
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
        return -1;
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

BRIDGE_IMPL(sceNetAccept)
BRIDGE_IMPL(sceNetBind)
BRIDGE_IMPL(sceNetClearDnsCache)
BRIDGE_IMPL(sceNetConnect)
BRIDGE_IMPL(sceNetDumpAbort)
BRIDGE_IMPL(sceNetDumpCreate)
BRIDGE_IMPL(sceNetDumpDestroy)
BRIDGE_IMPL(sceNetDumpRead)
BRIDGE_IMPL(sceNetEmulationGet)
BRIDGE_IMPL(sceNetEmulationSet)
BRIDGE_IMPL(sceNetEpollAbort)
BRIDGE_IMPL(sceNetEpollControl)
BRIDGE_IMPL(sceNetEpollCreate)
BRIDGE_IMPL(sceNetEpollDestroy)
BRIDGE_IMPL(sceNetEpollWait)
BRIDGE_IMPL(sceNetEpollWaitCB)
BRIDGE_IMPL(sceNetErrnoLoc)
BRIDGE_IMPL(sceNetEtherNtostr)
BRIDGE_IMPL(sceNetEtherStrton)
BRIDGE_IMPL(sceNetGetMacAddress)
BRIDGE_IMPL(sceNetGetSockIdInfo)
BRIDGE_IMPL(sceNetGetSockInfo)
BRIDGE_IMPL(sceNetGetStatisticsInfo)
BRIDGE_IMPL(sceNetGetpeername)
BRIDGE_IMPL(sceNetGetsockname)
BRIDGE_IMPL(sceNetGetsockopt)
BRIDGE_IMPL(sceNetHtonl)
BRIDGE_IMPL(sceNetHtonll)
BRIDGE_IMPL(sceNetHtons)
BRIDGE_IMPL(sceNetInetNtop)
BRIDGE_IMPL(sceNetInetPton)
BRIDGE_IMPL(sceNetInit)
BRIDGE_IMPL(sceNetListen)
BRIDGE_IMPL(sceNetNtohl)
BRIDGE_IMPL(sceNetNtohll)
BRIDGE_IMPL(sceNetNtohs)
BRIDGE_IMPL(sceNetRecv)
BRIDGE_IMPL(sceNetRecvfrom)
BRIDGE_IMPL(sceNetRecvmsg)
BRIDGE_IMPL(sceNetResolverAbort)
BRIDGE_IMPL(sceNetResolverCreate)
BRIDGE_IMPL(sceNetResolverDestroy)
BRIDGE_IMPL(sceNetResolverGetError)
BRIDGE_IMPL(sceNetResolverStartAton)
BRIDGE_IMPL(sceNetResolverStartNtoa)
BRIDGE_IMPL(sceNetSend)
BRIDGE_IMPL(sceNetSendmsg)
BRIDGE_IMPL(sceNetSendto)
BRIDGE_IMPL(sceNetSetDnsInfo)
BRIDGE_IMPL(sceNetSetsockopt)
BRIDGE_IMPL(sceNetShowIfconfig)
BRIDGE_IMPL(sceNetShowNetstat)
BRIDGE_IMPL(sceNetShowRoute)
BRIDGE_IMPL(sceNetShutdown)
BRIDGE_IMPL(sceNetSocket)
BRIDGE_IMPL(sceNetSocketAbort)
BRIDGE_IMPL(sceNetSocketClose)
BRIDGE_IMPL(sceNetTerm)
