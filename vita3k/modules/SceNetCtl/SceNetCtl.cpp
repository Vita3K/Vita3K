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

#include <util/lock_and_find.h>

#define SCE_NETCTL_INFO_SSID_LEN_MAX 32
#define SCE_NETCTL_INFO_CONFIG_NAME_LEN_MAX 64

enum {
    SCE_NET_CTL_ERROR_NOT_INITIALIZED = 0x80412101,
    SCE_NET_CTL_ERROR_NOT_TERMINATED = 0x80412102,
    SCE_NET_CTL_ERROR_CALLBACK_MAX = 0x80412103,
    SCE_NET_CTL_ERROR_ID_NOT_FOUND = 0x80412104,
    SCE_NET_CTL_ERROR_INVALID_ID = 0x80412105,
    SCE_NET_CTL_ERROR_INVALID_CODE = 0x80412106,
    SCE_NET_CTL_ERROR_INVALID_ADDR = 0x80412107,
    SCE_NET_CTL_ERROR_NOT_CONNECTED = 0x80412108,
    SCE_NET_CTL_ERROR_NOT_AVAIL = 0x80412109,
    SCE_NET_CTL_ERROR_AUTO_CONNECT_DISABLED = 0x8041210a,
    SCE_NET_CTL_ERROR_AUTO_CONNECT_FAILED = 0x8041210b,
    SCE_NET_CTL_ERROR_NO_SUITABLE_SETTING_FOR_AUTO_CONNECT = 0x8041210c,
    SCE_NET_CTL_ERROR_DISCONNECTED_FOR_ADHOC_USE = 0x8041210d,
    SCE_NET_CTL_ERROR_DISCONNECT_REQ = 0x8041210e,
    SCE_NET_CTL_ERROR_INVALID_TYPE = 0x8041210f,
    SCE_NET_CTL_ERROR_AUTO_DISCONNECT = 0x80412110,
    SCE_NET_CTL_ERROR_INVALID_SIZE = 0x80412111,
    SCE_NET_CTL_ERROR_FLIGHT_MODE_ENABLED = 0x80412112,
    SCE_NET_CTL_ERROR_WIFI_DISABLED = 0x80412113,
    SCE_NET_CTL_ERROR_WIFI_IN_ADHOC_USE = 0x80412114,
    SCE_NET_CTL_ERROR_ETHERNET_PLUGOUT = 0x80412115,
    SCE_NET_CTL_ERROR_WIFI_DEAUTHED = 0x80412116,
    SCE_NET_CTL_ERROR_WIFI_BEACON_LOST = 0x80412117,
    SCE_NET_CTL_ERROR_DISCONNECTED_FOR_SUSPEND = 0x80412118,
    SCE_NET_CTL_ERROR_COMMUNICATION_ID_NOT_EXIST = 0x80412119,
    SCE_NET_CTL_ERROR_ADHOC_ALREADY_CONNECTED = 0x8041211a,
    SCE_NET_CTL_ERROR_DHCP_TIMEOUT = 0x8041211b,
    SCE_NET_CTL_ERROR_PPPOE_TIMEOUT = 0x8041211c,
    SCE_NET_CTL_ERROR_INSUFFICIENT_MEMORY = 0x8041211d,
    SCE_NET_CTL_ERROR_PSP_ADHOC_JOIN_TIMEOUT = 0x8041211e,
    SCE_NET_CTL_ERROR_UNKNOWN_DEVICE = 0x80412188
};

enum SceNetCtlState {
    SCE_NETCTL_STATE_DISCONNECTED,
    SCE_NETCTL_STATE_CONNECTING,
    SCE_NETCTL_STATE_FINALIZING,
    SCE_NETCTL_STATE_CONNECTED
};

enum SceNetCtlInfoType {
    SCE_NETCTL_INFO_GET_CNF_NAME = 1,
    SCE_NETCTL_INFO_GET_DEVICE,
    SCE_NETCTL_INFO_GET_ETHER_ADDR,
    SCE_NETCTL_INFO_GET_MTU,
    SCE_NETCTL_INFO_GET_LINK,
    SCE_NETCTL_INFO_GET_BSSID,
    SCE_NETCTL_INFO_GET_SSID,
    SCE_NETCTL_INFO_GET_WIFI_SECURITY,
    SCE_NETCTL_INFO_GET_RSSI_DBM,
    SCE_NETCTL_INFO_GET_RSSI_PERCENTAGE,
    SCE_NETCTL_INFO_GET_CHANNEL,
    SCE_NETCTL_INFO_GET_IP_CONFIG,
    SCE_NETCTL_INFO_GET_DHCP_HOSTNAME,
    SCE_NETCTL_INFO_GET_PPPOE_AUTH_NAME,
    SCE_NETCTL_INFO_GET_IP_ADDRESS,
    SCE_NETCTL_INFO_GET_NETMASK,
    SCE_NETCTL_INFO_GET_DEFAULT_ROUTE,
    SCE_NETCTL_INFO_GET_PRIMARY_DNS,
    SCE_NETCTL_INFO_GET_SECONDARY_DNS,
    SCE_NETCTL_INFO_GET_HTTP_PROXY_CONFIG,
    SCE_NETCTL_INFO_GET_HTTP_PROXY_SERVER,
    SCE_NETCTL_INFO_GET_HTTP_PROXY_PORT,
};

typedef union SceNetCtlInfo {
    char cnf_name[SCE_NETCTL_INFO_CONFIG_NAME_LEN_MAX + 1];
    unsigned int device;
    SceNetEtherAddr ether_addr;
    unsigned int mtu;
    unsigned int link;
    SceNetEtherAddr bssid;
    char ssid[SCE_NETCTL_INFO_SSID_LEN_MAX + 1];
    unsigned int wifi_security;
    unsigned int rssi_dbm;
    unsigned int rssi_percentage;
    unsigned int channel;
    unsigned int ip_config;
    char dhcp_hostname[256];
    char pppoe_auth_name[128];
    char ip_address[16];
    char netmask[16];
    char default_route[16];
    char primary_dns[16];
    char secondary_dns[16];
    unsigned int http_proxy_config;
    char http_proxy_server[256];
    unsigned int http_proxy_port;
} SceNetCtlInfo;

struct SceNetCtlNatInfo {
    SceSize size;
    int stun_status;
    int nat_type;
    SceNetInAddr mapped_addr;
};

struct SceNetCtlIfStat {
    SceSize size;
    SceUInt32 totalSec;
    SceUInt64 txBytes;
    SceUInt64 rxBytes;
    SceRtcTick resetTick;
    SceUInt32 reserved[8];
};

struct SceNetCtlAdhocPeerInfo;

EXPORT(int, sceNetCtlAdhocDisconnect) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceNetCtlAdhocGetInAddr, SceNetInAddr *inaddr) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!inaddr) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceNetCtlAdhocGetPeerList, SceSize *peerInfoNum, SceNetCtlAdhocPeerInfo *peerInfo) {
    if (!peerInfoNum) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceNetCtlAdhocGetResult, int eventType, int *errorCode) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!errorCode) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    *errorCode = SCE_NET_CTL_ERROR_FLIGHT_MODE_ENABLED;
    return STUBBED("errorCode = SCE_NET_CTL_ERROR_FLIGHT_MODE_ENABLED");
}

EXPORT(int, sceNetCtlAdhocGetState, int *state) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!state) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    *state = SCE_NETCTL_STATE_DISCONNECTED;
    return STUBBED("state = SCE_NETCTL_STATE_DISCONNECTED");
}

EXPORT(int, sceNetCtlAdhocRegisterCallback, Ptr<void> func, Ptr<void> arg, int *cid) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!func || !cid) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    const std::lock_guard<std::mutex> lock(host.netctl.mutex);

    // Find the next available slot
    int next_id = 0;
    for (const auto &callback : host.netctl.adhocCallbacks) {
        if (callback.pc == NULL) {
            break;
        }
        next_id++;
    }

    if (next_id == 8) {
        return RET_ERROR(SCE_NET_CTL_ERROR_CALLBACK_MAX);
    }

    host.netctl.adhocCallbacks[next_id].pc = func.address();
    host.netctl.adhocCallbacks[next_id].arg = arg.address();
    *cid = next_id;
    return 0;
}

EXPORT(int, sceNetCtlAdhocUnregisterCallback, int cid) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if ((cid < 0) || (cid >= 8)) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ID);
    }

    const std::lock_guard<std::mutex> lock(host.netctl.mutex);
    host.netctl.adhocCallbacks[cid].pc = NULL;
    host.netctl.adhocCallbacks[cid].arg = NULL;
    return 0;
}

EXPORT(int, sceNetCtlCheckCallback) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (host.net.state == 1) {
        return 0;
    }

    host.net.state = 1;

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    // TODO: Limit the number of callbacks called to 5
    // TODO: Check in which order the callbacks are executed

    for (auto &callback : host.netctl.callbacks) {
        if (callback.pc != NULL) {
            thread->request_callback(callback.pc, { 1, callback.arg });
        }
    }

    for (auto &callback : host.netctl.adhocCallbacks) {
        if (callback.pc != NULL) {
            thread->request_callback(callback.pc, { 1, callback.arg });
        }
    }

    return STUBBED("Stub");
}

EXPORT(int, sceNetCtlGetIfStat, int device, SceNetCtlIfStat *ifstat) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!ifstat) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    if (ifstat->size != sizeof(SceNetCtlIfStat)) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_SIZE);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceNetCtlGetNatInfo, SceNetCtlNatInfo *natinfo) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!natinfo) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceNetCtlGetPhoneMaxDownloadableSize, SceInt64 *maxDownloadableSize) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!maxDownloadableSize) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    *maxDownloadableSize = 0x7fffffffffffffffLL; // Unlimited
    return STUBBED("maxDownloadableSize = Unlimited");
}

EXPORT(int, sceNetCtlInetGetInfo, int code, SceNetCtlInfo *info) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!info) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

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
                strcpy(info->ip_address, addr);
                break;
            }
        }
        break;
    }
    return 0;
}

EXPORT(int, sceNetCtlInetGetResult, int eventType, int *errorCode) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!errorCode) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    *errorCode = SCE_NET_CTL_ERROR_FLIGHT_MODE_ENABLED;
    return STUBBED("errorCode = SCE_NET_CTL_ERROR_FLIGHT_MODE_ENABLED");
}

EXPORT(int, sceNetCtlInetGetState, int *state) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!state) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    *state = SCE_NETCTL_STATE_DISCONNECTED;
    return STUBBED("state = SCE_NETCTL_STATE_DISCONNECTED");
}

EXPORT(int, sceNetCtlInetRegisterCallback, Ptr<void> func, Ptr<void> arg, int *cid) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!func || !cid) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    const std::lock_guard<std::mutex> lock(host.netctl.mutex);

    // Find the next available slot
    int next_id = 0;
    for (const auto &callback : host.netctl.callbacks) {
        if (callback.pc == NULL) {
            break;
        }
        next_id++;
    }

    if (next_id == 8) {
        return RET_ERROR(SCE_NET_CTL_ERROR_CALLBACK_MAX);
    }

    host.netctl.callbacks[next_id].pc = func.address();
    host.netctl.callbacks[next_id].arg = arg.address();
    *cid = next_id;
    return 0;
}

EXPORT(int, sceNetCtlInetUnregisterCallback, int cid) {
    if (!host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if ((cid < 0) || (cid >= 8)) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ID);
    }

    const std::lock_guard<std::mutex> lock(host.netctl.mutex);
    host.netctl.callbacks[cid].pc = NULL;
    host.netctl.callbacks[cid].arg = NULL;
    return 0;
}

EXPORT(int, sceNetCtlInit) {
    if (host.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_TERMINATED);
    }

    const std::lock_guard<std::mutex> lock(host.netctl.mutex);
    host.netctl.adhocCallbacks.fill({ NULL, NULL });
    host.netctl.callbacks.fill({ NULL, NULL });

    host.netctl.inited = true;
    return STUBBED("Stub");
}

EXPORT(void, sceNetCtlTerm) {
    STUBBED("Stub");
    host.netctl.inited = false;
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
