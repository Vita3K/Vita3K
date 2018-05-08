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

#include "SceNetCtl.h"

#ifdef WIN32
#undef s_addr
#define s_addr s_addr
#endif
#include <kernel/thread_functions.h>
#include <psp2/net/netctl.h>

EXPORT(int, sceNetCtlAdhocDisconnect) {
    return unimplemented("sceNetCtlAdhocDisconnect");
}

EXPORT(int, sceNetCtlAdhocGetInAddr) {
    return unimplemented("sceNetCtlAdhocGetInAddr");
}

EXPORT(int, sceNetCtlAdhocGetPeerList) {
    return unimplemented("sceNetCtlAdhocGetPeerList");
}

EXPORT(int, sceNetCtlAdhocGetResult) {
    return unimplemented("sceNetCtlAdhocGetResult");
}

EXPORT(int, sceNetCtlAdhocGetState) {
    return unimplemented("sceNetCtlAdhocGetState");
}

EXPORT(int, sceNetCtlAdhocRegisterCallback) {
    return unimplemented("sceNetCtlAdhocRegisterCallback");
}

EXPORT(int, sceNetCtlAdhocUnregisterCallback) {
    return unimplemented("sceNetCtlAdhocUnregisterCallback");
}

EXPORT(int, sceNetCtlCheckCallback) {
    if (host.net.state == 0)
        return 0;

    host.net.state = 0;

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    for (auto &callback : host.net.cbs) {
        Ptr<void> argp = Ptr<void>(callback.second.data);
        run_on_current(*thread, Ptr<void>(callback.second.pc), host.net.state, argp);
    }
    unimplemented("sceNetCtlGetIfStat - Stub");
    return 0;
}

EXPORT(int, sceNetCtlGetIfStat) {
    return unimplemented("sceNetCtlGetIfStat");
}

EXPORT(int, sceNetCtlGetNatInfo) {
    return unimplemented("sceNetCtlGetNatInfo");
}

EXPORT(int, sceNetCtlGetPhoneMaxDownloadableSize) {
    return unimplemented("sceNetCtlGetPhoneMaxDownloadableSize");
}

EXPORT(int, sceNetCtlInetGetInfo, int code, SceNetCtlInfo *info) {
    switch (code) {
    case SCE_NETCTL_INFO_GET_IP_ADDRESS:
        char devname[80];
        gethostname(devname, 80);
        struct hostent *resolved = gethostbyname(devname);
        for (int i = 0; resolved->h_addr_list[i] != nullptr; ++i) {
            struct in_addr addrIn;
            memcpy(&addrIn, resolved->h_addr_list[i], sizeof(uint32_t));
            char *addr = inet_ntoa(addrIn);
            if (strcmp(addr, "127.0.0.1") != 0) {
                strlcpy(info->ip_address, addr);
                break;
            }
        }
        break;
    }
    return 0;
}

EXPORT(int, sceNetCtlInetGetResult) {
    return unimplemented("sceNetCtlInetGetResult");
}

EXPORT(int, sceNetCtlInetGetState, uint32_t *state) {
    *state = SCE_NETCTL_STATE_DISCONNECTED;
    unimplemented("sceNetCtlInetGetState => SCE_NETCTL_STATE_DISCONNECTED");
    return 0;
}

EXPORT(int, sceNetCtlInetRegisterCallback, Ptr<void> callback, Ptr<void> data, uint32_t *cid) {
    const std::lock_guard<std::mutex> lock(host.kernel.mutex);
    *cid = host.kernel.next_uid++;
    emu::SceNetCtlCallback sceNetCtlCallback;
    sceNetCtlCallback.pc = callback.address();
    sceNetCtlCallback.data = data.address();
    host.net.cbs.emplace(*cid, sceNetCtlCallback);
    return 0;
}

EXPORT(int, sceNetCtlInetUnregisterCallback) {
    return unimplemented("sceNetCtlInetUnregisterCallback");
}

EXPORT(int, sceNetCtlInit) {
    unimplemented("sceNetCtlInit - Stub");
    return 0;
}

EXPORT(int, sceNetCtlTerm) {
    return unimplemented("sceNetCtlTerm");
}

BRIDGE_IMPL(sceNetCtlAdhocDisconnect)
BRIDGE_IMPL(sceNetCtlAdhocGetInAddr)
BRIDGE_IMPL(sceNetCtlAdhocGetPeerList)
BRIDGE_IMPL(sceNetCtlAdhocGetResult)
BRIDGE_IMPL(sceNetCtlAdhocGetState)
BRIDGE_IMPL(sceNetCtlAdhocRegisterCallback)
BRIDGE_IMPL(sceNetCtlAdhocUnregisterCallback)
BRIDGE_IMPL(sceNetCtlCheckCallback)
BRIDGE_IMPL(sceNetCtlGetIfStat)
BRIDGE_IMPL(sceNetCtlGetNatInfo)
BRIDGE_IMPL(sceNetCtlGetPhoneMaxDownloadableSize)
BRIDGE_IMPL(sceNetCtlInetGetInfo)
BRIDGE_IMPL(sceNetCtlInetGetResult)
BRIDGE_IMPL(sceNetCtlInetGetState)
BRIDGE_IMPL(sceNetCtlInetRegisterCallback)
BRIDGE_IMPL(sceNetCtlInetUnregisterCallback)
BRIDGE_IMPL(sceNetCtlInit)
BRIDGE_IMPL(sceNetCtlTerm)
