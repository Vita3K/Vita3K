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

#include "SceNet.h"

#ifdef WIN32
#undef s_addr
#define s_addr s_addr
#endif
#include <psp2/net/net.h>

#include <net/functions.h>

#ifndef WIN32
#include <errno.h>
#endif

#ifdef WIN32
#define ERROR_CASE(errname) \
    case (WSA##errname):    \
        return SCE_NET_##errname;
#else
#define ERROR_CASE(errname) \
    case (errname):         \
        return SCE_NET_##errname;
#endif

static int translate_errorcode() {
#ifdef WIN32
    switch (WSAGetLastError()) {
#else
    switch (errno) {
#endif
#ifndef WIN32 // These errorcodes don't exist in WinSock
        ERROR_CASE(EPERM)
        ERROR_CASE(ENOENT)
        ERROR_CASE(ESRCH)
        ERROR_CASE(EIO)
        ERROR_CASE(ENXIO)
        ERROR_CASE(E2BIG)
        ERROR_CASE(ENOEXEC)
        ERROR_CASE(EDEADLK)
        ERROR_CASE(ENOMEM)
        ERROR_CASE(ECHILD)
        ERROR_CASE(EBUSY)
        ERROR_CASE(EEXIST)
        ERROR_CASE(EXDEV)
        ERROR_CASE(ENODEV)
        ERROR_CASE(ENOTDIR)
        ERROR_CASE(EISDIR)
        ERROR_CASE(ENFILE)
        ERROR_CASE(ENOTTY)
        ERROR_CASE(ETXTBSY)
        ERROR_CASE(EFBIG)
        ERROR_CASE(ENOSPC)
        ERROR_CASE(ESPIPE)
        ERROR_CASE(EROFS)
        ERROR_CASE(EMLINK)
        ERROR_CASE(EPIPE)
        ERROR_CASE(EDOM)
        ERROR_CASE(ERANGE)
        ERROR_CASE(ENOLCK)
        ERROR_CASE(ENOSYS)
        ERROR_CASE(EIDRM)
        ERROR_CASE(EOVERFLOW)
        ERROR_CASE(EILSEQ)
        ERROR_CASE(ENOTSUP)
        ERROR_CASE(ECANCELED)
        ERROR_CASE(EBADMSG)
        ERROR_CASE(ENODATA)
        ERROR_CASE(ENOSR)
        ERROR_CASE(ENOSTR)
        ERROR_CASE(ETIME)
#endif
        ERROR_CASE(EINTR)
        ERROR_CASE(EBADF)
        ERROR_CASE(EACCES)
        ERROR_CASE(EFAULT)
        ERROR_CASE(EINVAL)
        ERROR_CASE(EMFILE)
        ERROR_CASE(EWOULDBLOCK)
        ERROR_CASE(EINPROGRESS)
        ERROR_CASE(EALREADY)
        ERROR_CASE(ENOTSOCK)
        ERROR_CASE(EDESTADDRREQ)
        ERROR_CASE(EMSGSIZE)
        ERROR_CASE(EPROTOTYPE)
        ERROR_CASE(ENOPROTOOPT)
        ERROR_CASE(EPROTONOSUPPORT)
#if defined(__APPLE__) || defined(WIN32)
        ERROR_CASE(EOPNOTSUPP)
#endif
        ERROR_CASE(EAFNOSUPPORT)
        ERROR_CASE(EADDRINUSE)
        ERROR_CASE(EADDRNOTAVAIL)
        ERROR_CASE(ENETDOWN)
        ERROR_CASE(ENETUNREACH)
        ERROR_CASE(ENETRESET)
        ERROR_CASE(ECONNABORTED)
        ERROR_CASE(ECONNRESET)
        ERROR_CASE(ENOBUFS)
        ERROR_CASE(EISCONN)
        ERROR_CASE(ENOTCONN)
        ERROR_CASE(ETIMEDOUT)
        ERROR_CASE(ECONNREFUSED)
        ERROR_CASE(ELOOP)
        ERROR_CASE(ENAMETOOLONG)
        ERROR_CASE(EHOSTUNREACH)
        ERROR_CASE(ENOTEMPTY)
    }
    return SCE_NET_ERROR_EINTERNAL;
}

EXPORT(int, sceNetAccept, int s, SceNetSockaddr *addr, unsigned int *addrlen) {
    int res = accept_socket(host.net, s, addr, addrlen);
    if (res < 0) {
        return RET_ERROR("sceNetAccept", translate_errorcode());
    } else {
        return res;
    }
}

EXPORT(int, sceNetBind, int s, const SceNetSockaddr *name, unsigned int addrlen) {
    int res = bind_socket(host.net, s, name, addrlen);
    if (res < 0) {
        return RET_ERROR("sceNetBind", translate_errorcode());
    } else {
        return res;
    }
}

EXPORT(int, sceNetClearDnsCache) {
    return unimplemented("sceNetClearDnsCache");
}

EXPORT(int, sceNetConnect, int s, const SceNetSockaddr *name, unsigned int namelen) {
    int res = connect_socket(host.net, s, name, namelen);
    if (res < 0) {
        return RET_ERROR("sceNetConnect", translate_errorcode());
    } else {
        return res;
    }
}

EXPORT(int, sceNetDumpAbort) {
    return unimplemented("sceNetDumpAbort");
}

EXPORT(int, sceNetDumpCreate) {
    return unimplemented("sceNetDumpCreate");
}

EXPORT(int, sceNetDumpDestroy) {
    return unimplemented("sceNetDumpDestroy");
}

EXPORT(int, sceNetDumpRead) {
    return unimplemented("sceNetDumpRead");
}

EXPORT(int, sceNetEmulationGet) {
    return unimplemented("sceNetEmulationGet");
}

EXPORT(int, sceNetEmulationSet) {
    return unimplemented("sceNetEmulationSet");
}

EXPORT(int, sceNetEpollAbort) {
    return unimplemented("sceNetEpollAbort");
}

EXPORT(int, sceNetEpollControl) {
    return unimplemented("sceNetEpollControl");
}

EXPORT(int, sceNetEpollCreate) {
    return unimplemented("sceNetEpollCreate");
}

EXPORT(int, sceNetEpollDestroy) {
    return unimplemented("sceNetEpollDestroy");
}

EXPORT(int, sceNetEpollWait) {
    return unimplemented("sceNetEpollWait");
}

EXPORT(int, sceNetEpollWaitCB) {
    return unimplemented("sceNetEpollWaitCB");
}

EXPORT(int, sceNetErrnoLoc) {
    return unimplemented("sceNetErrnoLoc");
}

EXPORT(int, sceNetEtherNtostr) {
    return unimplemented("sceNetEtherNtostr");
}

EXPORT(int, sceNetEtherStrton) {
    return unimplemented("sceNetEtherStrton");
}

EXPORT(int, sceNetGetMacAddress) {
    return unimplemented("sceNetGetMacAddress");
}

EXPORT(int, sceNetGetSockIdInfo) {
    return unimplemented("sceNetGetSockIdInfo");
}

EXPORT(int, sceNetGetSockInfo) {
    return unimplemented("sceNetGetSockInfo");
}

EXPORT(int, sceNetGetStatisticsInfo) {
    return unimplemented("sceNetGetStatisticsInfo");
}

EXPORT(int, sceNetGetpeername) {
    return unimplemented("sceNetGetpeername");
}

EXPORT(int, sceNetGetsockname, int s, SceNetSockaddr *name, unsigned int *namelen) {
    int res = get_socket_address(host.net, s, name, namelen);
    if (res < 0) {
        return RET_ERROR("sceNetGetsockname", translate_errorcode());
    } else {
        return res;
    }
}

EXPORT(int, sceNetGetsockopt) {
    return unimplemented("sceNetGetsockopt");
}

EXPORT(unsigned int, sceNetHtonl, unsigned int n) {
    return htonl(n);
}

EXPORT(int, sceNetHtonll) {
    return unimplemented("sceNetHtonll");
}

EXPORT(unsigned short int, sceNetHtons, unsigned short int n) {
    return htons(n);
}

EXPORT(Ptr<const char>, sceNetInetNtop, int af, const void *src, Ptr<char> dst, unsigned int size) {
    char *dst_ptr = dst.get(host.mem);
#ifdef WIN32
    const char *res = InetNtop(af, src, dst_ptr, size);
#else
    const char *res = inet_ntop(af, src, dst_ptr, size);
#endif
    if (res == nullptr) {
        RET_ERROR("sceNetInetNtop", 0x0);
        return Ptr<char>();
    }
    return dst;
}

EXPORT(int, sceNetInetPton, int af, const char *src, void *dst) {
#ifdef WIN32
    int res = InetPton(af, src, dst);
#else
    int res = inet_pton(af, src, dst);
#endif
    if (res < 0) {
        return RET_ERROR("sceNetInetPton", translate_errorcode());
    }
    return res;
}

EXPORT(int, sceNetInit, SceNetInitParam *param) {
    if (host.net.inited) {
        return RET_ERROR("sceNetInit", SCE_NET_ERROR_EINTERNAL);
    }
#ifdef WIN32
    WORD versionWanted = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(versionWanted, &wsaData);
#endif
    host.net.inited = true;
    return 0;
}

EXPORT(int, sceNetListen, int s, int backlog) {
    int res = listen_socket(host.net, s, backlog);
    if (res < 0) {
        return RET_ERROR("sceNetListen", translate_errorcode());
    } else {
        return res;
    }
}

EXPORT(unsigned int, sceNetNtohl, unsigned int n) {
    return ntohl(n);
}

EXPORT(int, sceNetNtohll) {
    return unimplemented("sceNetNtohll");
}

EXPORT(unsigned short int, sceNetNtohs, unsigned short int n) {
    return ntohs(n);
}

EXPORT(int, sceNetRecv, int s, void *buf, unsigned int len, int flags) {
    int res = recv_packet(host.net, s, buf, len, flags, nullptr, 0);
    if (res < 0) {
        return RET_ERROR("sceNetRecv", translate_errorcode());
    } else {
        return res;
    }
}

EXPORT(int, sceNetRecvfrom, int s, void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) {
    int res = recv_packet(host.net, s, buf, len, flags, from, fromlen);
    if (res < 0) {
        return RET_ERROR("sceNetRecvfrom", translate_errorcode());
    } else {
        return res;
    }
}

EXPORT(int, sceNetRecvmsg) {
    return unimplemented("sceNetRecvmsg");
}

EXPORT(int, sceNetResolverAbort) {
    return unimplemented("sceNetResolverAbort");
}

EXPORT(int, sceNetResolverCreate) {
    return unimplemented("sceNetResolverCreate");
}

EXPORT(int, sceNetResolverDestroy) {
    return unimplemented("sceNetResolverDestroy");
}

EXPORT(int, sceNetResolverGetError) {
    return unimplemented("sceNetResolverGetError");
}

EXPORT(int, sceNetResolverStartAton, int rid, const SceNetInAddr *addr, char *hostname, int len, int timeout, int retry, int flags) {
    struct hostent *resolved = gethostbyaddr((const char *)addr, len, AF_INET);
    strcpy(hostname, resolved->h_name);
    return 0;
}

EXPORT(int, sceNetResolverStartNtoa, int rid, const char *hostname, SceNetInAddr *addr, int timeout, int retry, int flags) {
    struct hostent *resolved = gethostbyname(hostname);
    memcpy(addr, resolved->h_addr, sizeof(uint32_t));
    return 0;
}

EXPORT(int, sceNetSend, int s, const void *msg, unsigned int len, int flags) {
    int res = send_packet(host.net, s, msg, len, flags, nullptr, 0);
    if (res < 0) {
        return RET_ERROR("sceNetSend", translate_errorcode());
    } else {
        return res;
    }
}

EXPORT(int, sceNetSendmsg) {
    return unimplemented("sceNetSendmsg");
}

EXPORT(int, sceNetSendto, int s, const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) {
    int res = send_packet(host.net, s, msg, len, flags, to, tolen);
    if (res < 0) {
        return RET_ERROR("sceNetSendto", translate_errorcode());
    } else {
        return res;
    }
}

EXPORT(int, sceNetSetDnsInfo) {
    return unimplemented("sceNetSetDnsInfo");
}

EXPORT(int, sceNetSetsockopt, int s, int level, int optname, const void *optval, unsigned int optlen) {
    int res = set_socket_options(host.net, s, level, optname, optval, optlen);
    if (res < 0) {
        return RET_ERROR("sceNetSetsockopt", translate_errorcode());
    } else {
        return res;
    }
}

EXPORT(int, sceNetShowIfconfig) {
    return unimplemented("sceNetShowIfconfig");
}

EXPORT(int, sceNetShowNetstat) {
    if (!host.net.inited) {
        return RET_ERROR("sceNetShowNetstat", SCE_NET_ERROR_ENOTINIT);
    }
    return 0;
}

EXPORT(int, sceNetShowRoute) {
    return unimplemented("sceNetShowRoute");
}

EXPORT(int, sceNetShutdown) {
    return unimplemented("sceNetShutdown");
}

EXPORT(int, sceNetSocket, const char *name, int domain, int type, int protocol) {
    if (type < SCE_NET_SOCK_STREAM || type > SCE_NET_SOCK_RAW) { // TODO: P2P type support
        return RET_ERROR("sceNetSocket", SCE_NET_ERROR_EINVAL);
    }
    int res = open_socket(host.net, domain, type, protocol);
    if (res < 0) {
        return RET_ERROR("sceNetSocket", translate_errorcode());
    } else {
        return res;
    }
}

EXPORT(int, sceNetSocketAbort) {
    return unimplemented("sceNetSocketAbort");
}

EXPORT(int, sceNetSocketClose, int s) {
    return close_socket(host.net, s);
}

EXPORT(int, sceNetTerm) {
    if (!host.net.inited) {
        return RET_ERROR("sceNetTerm", SCE_NET_ERROR_ENOTINIT);
    }
#ifdef WIN32
    WSACleanup();
#endif
    host.net.inited = false;
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
