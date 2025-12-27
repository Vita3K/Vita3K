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

#include <../SceAppUtil/SceAppUtil.h>
#include <../SceNet/SceNet.h>
#include <../SceNpManager/SceNpManager.h>

#include <module/module.h>

#include <dialog/state.h>
#include <kernel/state.h>
#include <net/state.h>
#include <net/types.h>
#include <packages/sfo.h>
#include <rtc/rtc.h>
#include <util/lock_and_find.h>
#include <util/net_utils.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceNetCtl);

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

static void adhoc_thread(EmuEnvState &emuenv, int thread_id) {
    LOG_INFO("Adhoc thread started");
    constexpr uint16_t AUTH_RECV_VPORT = 0x8235;
    constexpr uint16_t AUTH_SEND_VPORT = 0x8236;

    const auto handle_error_and_disconnect = [&](int recv_id, int send_id = 0) {
        // Close the sockets if they are valid
        if (recv_id > 0)
            CALL_EXPORT(sceNetSocketClose, recv_id);
        if (send_id > 0)
            CALL_EXPORT(sceNetSocketClose, send_id);

        {
            // Set the adhoc state and event to disconnected and notify the condition variable
            std::lock_guard<std::mutex> lock(emuenv.netctl.mutex);
            emuenv.netctl.adhocState = SCE_NET_CTL_STATE_DISCONNECTED;
            emuenv.netctl.adhocEvent = SCE_NET_CTL_EVENT_TYPE_DISCONNECTED;
            emuenv.netctl.adhocCondVarReady = false;
            emuenv.netctl.adhocCondVar.notify_all();
        }

        // Set the common dialog status to finished with user canceled result
        if (emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING) {
            emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
            emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
        }

        LOG_ERROR("Adhoc thread: error occurred, closing sockets");
    };

    while (emuenv.netctl.adhocThreadRun.load()) {
        {
            std::unique_lock<std::mutex> lock(emuenv.netctl.mutex);
            emuenv.netctl.adhocCondVar.wait(lock, [&emuenv] {
                return emuenv.netctl.adhocCondVarReady || !emuenv.netctl.adhocThreadRun;
            });
            if (!emuenv.netctl.adhocThreadRun)
                break;
        }

        LOG_INFO("Adhoc thread: starting network loop");

        const SceNetSockaddrIn recvBind{
            .sin_len = sizeof(SceNetSockaddrIn),
            .sin_family = AF_INET,
            .sin_port = htons(SCE_NET_ADHOC_PORT),
            .sin_addr{ .s_addr = htonl(INADDR_ANY) },
            .sin_vport = htons(AUTH_RECV_VPORT),
        };

        const auto recv_id = CALL_EXPORT(sceNetSocket, "SceNetAdhocAuthRecv", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, SCE_NET_IPPROTO_IP);
        if (recv_id < 0) {
            LOG_ERROR("Failed to create adhoc recv socket: {}", log_hex(recv_id));
            handle_error_and_disconnect(recv_id);
            continue;
        }

        constexpr uint32_t timeout = 100000;
        int32_t res;
        if ((res = CALL_EXPORT(sceNetSetsockopt, recv_id, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &timeout, sizeof(timeout))) < 0) {
            LOG_ERROR("Failed to set recv timeout on adhoc socket: {}", log_hex(res));
            handle_error_and_disconnect(recv_id);
            continue;
        }

        if ((res = CALL_EXPORT(sceNetBind, recv_id, (SceNetSockaddr *)&recvBind, sizeof(recvBind))) < 0) {
            LOG_ERROR("Failed to bind adhoc recv socket: {}", log_hex(res));
            handle_error_and_disconnect(recv_id);
            continue;
        }

        const auto send_id = CALL_EXPORT(sceNetSocket, "SceNetAdhocAuthSend", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, SceNetProtocol(0));
        if (send_id < 0) {
            LOG_ERROR("Failed to create adhoc send socket: {}", log_hex(send_id));
            handle_error_and_disconnect(recv_id, send_id);
            continue;
        }

        const auto val = 1;
        if ((res = CALL_EXPORT(sceNetSetsockopt, send_id, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST, &val, sizeof(val))) < 0) {
            LOG_ERROR("Failed to set broadcast option on adhoc send socket: {}", log_hex(res));
            handle_error_and_disconnect(recv_id, send_id);
            continue;
        }

        SceNetSockaddrIn sendBind{
            .sin_len = sizeof(SceNetSockaddrIn),
            .sin_family = SCE_NET_AF_INET,
            .sin_port = htons(SCE_NET_ADHOC_PORT),
            .sin_addr{ .s_addr = htonl(INADDR_ANY) },
            .sin_vport = htons(AUTH_SEND_VPORT),
        };

        if ((res = CALL_EXPORT(sceNetBind, send_id, (SceNetSockaddr *)&sendBind, sizeof(sendBind))) < 0) {
            LOG_ERROR("Failed to bind adhoc send socket: {}", log_hex(res));
            handle_error_and_disconnect(recv_id, send_id);
            continue;
        }

        const auto snd_sock = lock_and_find(send_id, emuenv.net.socks, emuenv.netctl.mutex);
        if (!snd_sock) {
            LOG_ERROR("Failed to find send socket with ID {} in the list", send_id);
            handle_error_and_disconnect(recv_id, send_id);
            continue;
        }

        const auto rcv_sock = lock_and_find(recv_id, emuenv.net.socks, emuenv.netctl.mutex);
        if (!rcv_sock) {
            LOG_ERROR("Failed to find recv socket with ID {} in the list", recv_id);
            handle_error_and_disconnect(recv_id, send_id);
            continue;
        }

        if (emuenv.cfg.adhoc_addr != emuenv.net.current_addr_index) {
            // If address has changed, reinitialize the network address and broadcast address
            std::lock_guard<std::mutex> lock(emuenv.netctl.mutex);
            net_utils::init_address(emuenv.cfg.adhoc_addr, emuenv.net.netAddr, emuenv.net.broadcastAddr);
            emuenv.net.current_addr_index = emuenv.cfg.adhoc_addr;
        }

        const SceNetSockaddrIn to{
            .sin_len = sizeof(SceNetSockaddrIn),
            .sin_family = AF_INET,
            .sin_port = htons(SCE_NET_ADHOC_PORT),
            .sin_addr{ .s_addr = emuenv.net.broadcastAddr },
            .sin_vport = htons(AUTH_RECV_VPORT),
        };

        // Retrieve the app version from the SFO file
        std::string app_ver_string;
        int32_t app_ver = 0x00010000; // Default version 1.00
        if (sfo::get_data_by_key(app_ver_string, emuenv.sfo_handle, "APP_VER")) {
            // Parse version string like "1.05" to uint32_t as 0x00010005
            unsigned major = 0, minor = 0;
            if (sscanf(app_ver_string.c_str(), "%u.%u", &major, &minor) == 2)
                app_ver = (major << 16) | minor;
            LOG_DEBUG("Parsed app version: {}.{} to 0x{:08x}", major, minor, app_ver);
        }

        SceNetCtlAdhocPeerInfo selfInfo{
            .addr = { .s_addr = emuenv.net.netAddr },
            .lastRecv = 0,
            .appVer = app_ver,
            .isValidNpId = 1,
        };

        std::vector<int8_t> username(SCE_SYSTEM_PARAM_USERNAME_MAXSIZE);
        CALL_EXPORT(sceAppUtilSystemParamGetString, SCE_SYSTEM_PARAM_ID_USER_NAME, username.data(), sizeof(selfInfo.username));
        CALL_EXPORT(sceNpManagerGetNpId, &selfInfo.npId);
        std::strncpy(selfInfo.username, reinterpret_cast<const char *>(username.data()), sizeof(selfInfo.username) - 1);
        selfInfo.username[sizeof(selfInfo.username) - 1] = '\0';

        // Notify the common dialog that the adhoc connection is established
        emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;

        {
            // Set the adhoc event and state to indicate that the connection is established
            std::lock_guard<std::mutex> lock(emuenv.netctl.mutex);
            emuenv.netctl.adhocState = SCE_NET_CTL_STATE_IPOBTAINED;
            emuenv.netctl.adhocEvent = SCE_NET_CTL_EVENT_TYPE_IPOBTAINED;
        }

        // Timeout for peer inactivity set to 5 seconds, expressed in microseconds
        constexpr uint64_t TIMEOUT_USEC = 5'000'000;

        // Interval for sending our peer info to the network set to 1 second, expressed in microseconds
        constexpr uint64_t SEND_INTERVAL_USEC = 1'000'000;

        uint64_t lastSendTicks = rtc_get_ticks(emuenv.kernel.base_tick.tick) - emuenv.kernel.start_tick - SEND_INTERVAL_USEC;
        while (emuenv.netctl.adhocCondVarReady.load()) {
            const uint64_t currentTicks = rtc_get_ticks(emuenv.kernel.base_tick.tick) - emuenv.kernel.start_tick;
            emuenv.netctl.adhocPeers.erase(
                std::remove_if(
                    emuenv.netctl.adhocPeers.begin(),
                    emuenv.netctl.adhocPeers.end(),
                    [&](const SceNetCtlAdhocPeerInfo &peer) {
                        return currentTicks - peer.lastRecv > TIMEOUT_USEC;
                    }),
                emuenv.netctl.adhocPeers.end());

            // Send the self info to other peers every 1 second
            if ((currentTicks - lastSendTicks) >= SEND_INTERVAL_USEC) {
                const auto res = snd_sock->send_packet(&selfInfo, sizeof(selfInfo), 0, (SceNetSockaddr *)&to, sizeof(to));
                /*if (res < 0)
                    LOG_ERROR("Failed to send adhoc peer info: {}", log_hex(res));
                else
                    LOG_DEBUG("Sent adhoc peer info, res: {}", res);*/
                lastSendTicks = currentTicks;
            }

            // Wait for incoming packets
            SceNetSockaddrIn fromAddr{};
            auto fromlen = uint32_t(sizeof(fromAddr));
            SceNetCtlAdhocPeerInfo peerInfo{};
            //LOG_DEBUG("Waiting for adhoc peer info, recv_id: {}", recv_id);
            const auto res = rcv_sock->recv_packet(&peerInfo, sizeof(SceNetCtlAdhocPeerInfo), 0, (SceNetSockaddr *)&fromAddr, &fromlen);
            if ((res > 0) && (res == sizeof(SceNetCtlAdhocPeerInfo)) && (fromAddr.sin_addr.s_addr != emuenv.net.netAddr)) {
                auto peerIt = std::find_if(
                    emuenv.netctl.adhocPeers.begin(),
                    emuenv.netctl.adhocPeers.end(),
                    [&](const SceNetCtlAdhocPeerInfo &peer) {
                        return peer.addr.s_addr == fromAddr.sin_addr.s_addr;
                    });

                if (peerIt != emuenv.netctl.adhocPeers.end()) {
                    // If the peer is already in the list, update its lastRecv time
                     //LOG_DEBUG("Adhoc peer already present: user: {}, addr:{}, IP: {}, last recv: {}, size: {}",
                     //peerIt->username, peerIt->addr.s_addr, fromAddr.sin_addr.s_addr, peerIt->lastRecv, emuenv.netctl.adhocPeers.size());
                    peerIt->lastRecv = currentTicks;
                } else {
                    // Add the peer info to the list if the packet is a valid peer info packet
                    peerInfo.lastRecv = currentTicks;
                    //LOG_DEBUG("New adhoc peer: user: {}, addr:{}, IP: {}, last recv: {}, size: {}",
                    //    peerInfo.username, peerInfo.addr.s_addr, fromAddr.sin_addr.s_addr, peerInfo.lastRecv, emuenv.netctl.adhocPeers.size());
                    emuenv.netctl.adhocPeers.push_back(peerInfo);
                }
            } /* else if (res < 0)
                LOG_WARN("Failed to receive adhoc peer info: {}", log_hex(res));
            else if (fromAddr.sin_addr.s_addr == emuenv.net.netAddr)
                LOG_DEBUG("Received own adhoc info");*/
        }

        // Close the sockets when the loop ends
        CALL_EXPORT(sceNetSocketClose, recv_id);
        CALL_EXPORT(sceNetSocketClose, send_id);

        {
            // Notify the event and update the adhoc state
            std::lock_guard<std::mutex> lock(emuenv.netctl.mutex);
            emuenv.netctl.adhocState = SCE_NET_CTL_STATE_DISCONNECTED;
            emuenv.netctl.adhocEvent = SCE_NET_CTL_EVENT_TYPE_DISCONNECT_REQ_FINISHED;
        }

        LOG_INFO("Adhoc thread: stopping network loop");
    }

    LOG_INFO("Adhoc thread stopped");
}

EXPORT(int, sceNetCtlAdhocDisconnect) {
    TRACY_FUNC(sceNetCtlAdhocDisconnect);
    if (!emuenv.netctl.inited)
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);

    LOG_INFO("sceNetCtlAdhocDisconnect");

    const std::lock_guard<std::mutex> lock(emuenv.netctl.mutex);
    emuenv.netctl.adhocPeers.clear();
    emuenv.netctl.adhocCondVarReady = false;
    emuenv.netctl.adhocCondVar.notify_all();

    LOG_DEBUG("sceNetCtlAdhocDisconnect, adhocState: {}", (int)emuenv.netctl.adhocState);
    return STUBBED("Stub");
}

EXPORT(int, sceNetCtlAdhocGetInAddr, SceNetInAddr *inaddr) {
    TRACY_FUNC(sceNetCtlAdhocGetInAddr, inaddr);
    if (!emuenv.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!inaddr)
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);

    inaddr->s_addr = emuenv.net.netAddr;

    return 0;
}

EXPORT(int, sceNetCtlAdhocGetPeerList, SceSize *peerInfoNum, SceNetCtlAdhocPeerInfo *peerInfo) {
    TRACY_FUNC(sceNetCtlAdhocGetPeerList, peerInfoNum, peerInfo);
    if (!emuenv.netctl.inited)
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);

    if (!peerInfoNum)
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);

    std::lock_guard<std::mutex> lock(emuenv.netctl.mutex);
    if (peerInfo)
        memcpy(peerInfo, emuenv.netctl.adhocPeers.data(), emuenv.netctl.adhocPeers.size() * sizeof(*peerInfo));

    *peerInfoNum = emuenv.netctl.adhocPeers.size();
    LOG_DEBUG("sceNetCtlAdhocGetPeerList, peerInfoNum: {}", *peerInfoNum);
    return 0;
}

EXPORT(int, sceNetCtlAdhocGetResult, int eventType, int *errorCode) {
    TRACY_FUNC(sceNetCtlAdhocGetResult, eventType, errorCode);
    if (!emuenv.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!errorCode) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }
    LOG_INFO("sceNetCtlAdhocGetResult, eventType: {}", eventType);
    *errorCode = 0;
    return 0;
}

EXPORT(int, sceNetCtlAdhocGetState, int *state) {
    TRACY_FUNC(sceNetCtlAdhocGetState, state);
    if (!emuenv.netctl.inited)
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);

    if (!state)
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    LOG_INFO("sceNetCtlAdhocGetState, state: {}", (int)emuenv.netctl.adhocState);
    *state = emuenv.netctl.adhocState;
    return 0;
}

EXPORT(int, sceNetCtlAdhocRegisterCallback, Ptr<void> func, Ptr<void> arg, int *cid) {
    TRACY_FUNC(sceNetCtlAdhocRegisterCallback, func, arg, cid);
    if (!emuenv.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!func || !cid) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    const std::lock_guard<std::mutex> lock(emuenv.netctl.mutex);

    // Find the next available slot
    int next_id = 0;
    for (const auto &callback : emuenv.netctl.adhocCallbacks) {
        if (callback.pc == 0) {
            break;
        }
        next_id++;
    }

    if (next_id == 8) {
        return RET_ERROR(SCE_NET_CTL_ERROR_CALLBACK_MAX);
    }

    emuenv.netctl.adhocCallbacks[next_id].pc = func.address();
    emuenv.netctl.adhocCallbacks[next_id].arg = arg.address();
    *cid = next_id;
    return 0;
}

EXPORT(int, sceNetCtlAdhocUnregisterCallback, int cid) {
    TRACY_FUNC(sceNetCtlAdhocUnregisterCallback, cid);
    if (!emuenv.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if ((cid < 0) || (cid >= 8)) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ID);
    }

    const std::lock_guard<std::mutex> lock(emuenv.netctl.mutex);
    emuenv.netctl.adhocCallbacks[cid].pc = 0;
    emuenv.netctl.adhocCallbacks[cid].arg = 0;
    return 0;
}

EXPORT(int, sceNetCtlCheckCallback) {
    TRACY_FUNC(sceNetCtlCheckCallback);
    if (!emuenv.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);

    // TODO: Check if the network is connected
    if (emuenv.net.state != 1) {
        LOG_INFO("sceNetCtlCheckCallback, calling callbacks");
        for (auto &callback : emuenv.netctl.callbacks) {
            if (callback.pc != 0) {
                thread->run_callback(callback.pc, { SCE_NET_CTL_EVENT_TYPE_DISCONNECTED, callback.arg });
            }
        }
        emuenv.net.state = 1;
    }

    // Check if there are any adhoc events to notify
    if ((emuenv.netctl.adhocEvent != SCE_NET_CTL_EVENT_TYPE_NONE) && (emuenv.netctl.adhocEvent != emuenv.netctl.lastNotifiedAdhocEvent)) {
        LOG_INFO("sceNetCtlCheckCallback, calling adhoc callbacks, event: {}", (int)emuenv.netctl.adhocEvent);
        for (auto &callback : emuenv.netctl.adhocCallbacks) {
            if (callback.pc != 0)
                thread->run_callback(callback.pc, { emuenv.netctl.adhocEvent, callback.arg });
        }

        // Update the last notified adhoc event
        emuenv.netctl.lastNotifiedAdhocEvent = emuenv.netctl.adhocEvent;
    }

    return STUBBED("Stub");
}

EXPORT(int, sceNetCtlGetIfStat, int device, SceNetCtlIfStat *ifstat) {
    TRACY_FUNC(sceNetCtlGetIfStat, device, ifstat);
    if (!emuenv.netctl.inited) {
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
    TRACY_FUNC(sceNetCtlGetNatInfo, natinfo);
    if (!emuenv.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!natinfo) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    return UNIMPLEMENTED();
}

EXPORT(int, sceNetCtlGetPhoneMaxDownloadableSize, SceInt64 *maxDownloadableSize) {
    TRACY_FUNC(sceNetCtlGetPhoneMaxDownloadableSize, maxDownloadableSize);
    if (!emuenv.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!maxDownloadableSize) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    *maxDownloadableSize = 0x7fffffffffffffffLL; // Unlimited
    return STUBBED("maxDownloadableSize = Unlimited");
}

EXPORT(int, sceNetCtlInetGetInfo, int code, SceNetCtlInfo *info) {
    TRACY_FUNC(sceNetCtlInetGetInfo, code, info);
    if (!emuenv.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!info) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    const auto addr = net_utils::get_selected_assigned_addr(emuenv.cfg.adhoc_addr);

    switch (code) {
    case SCE_NETCTL_INFO_GET_DEVICE:
        info->device = 0; /*SCE_NET_CTL_DEVICE_WIRELESS*/
        // STUBBED("SCE_NETCTL_INFO_GET_DEVICE return SCE_NET_CTL_DEVICE_WIRELESS");
        break;
    case SCE_NETCTL_INFO_GET_RSSI_PERCENTAGE:
        info->rssi_percentage = 100;
        // STUBBED("code SCE_NETCTL_INFO_GET_RSSI_PERCENTAGE return 100%");
        break;
    case SCE_NETCTL_INFO_GET_IP_ADDRESS:
        std::strncpy(info->ip_address, addr.addr.c_str(), sizeof(info->ip_address) - 1);
        info->ip_address[sizeof(info->ip_address) - 1] = '\0';
        break;
    case SCE_NETCTL_INFO_GET_NETMASK:
        std::strncpy(info->netmask, addr.netMask.c_str(), sizeof(info->netmask) - 1);
        info->netmask[sizeof(info->netmask) - 1] = '\0';
        break;
    default:
        switch (code) {
        case SCE_NETCTL_INFO_GET_CNF_NAME:
            STUBBED("code SCE_NETCTL_INFO_GET_CNF_NAME not implemented");
            break;
        case SCE_NETCTL_INFO_GET_ETHER_ADDR:
            STUBBED("code SCE_NETCTL_INFO_GET_ETHER_ADDR not implemented");
            break;
        case SCE_NETCTL_INFO_GET_MTU:
            STUBBED("code SCE_NETCTL_INFO_GET_MTU not implemented");
            break;
        case SCE_NETCTL_INFO_GET_LINK:
            STUBBED("code SCE_NETCTL_INFO_GET_LINK not implemented");
            break;
        case SCE_NETCTL_INFO_GET_BSSID:
            STUBBED("code SCE_NETCTL_INFO_GET_BSSID not implemented");
            break;
        case SCE_NETCTL_INFO_GET_SSID:
            STUBBED("code SCE_NETCTL_INFO_GET_SSID not implemented");
            break;
        case SCE_NETCTL_INFO_GET_WIFI_SECURITY:
            STUBBED("code SCE_NETCTL_INFO_GET_WIFI_SECURITY not implemented");
            break;
        case SCE_NETCTL_INFO_GET_RSSI_DBM:
            STUBBED("code SCE_NETCTL_INFO_GET_RSSI_DBM not implemented");
            break;
        case SCE_NETCTL_INFO_GET_CHANNEL:
            STUBBED("code SCE_NETCTL_INFO_GET_CHANNEL not implemented");
            break;
        case SCE_NETCTL_INFO_GET_IP_CONFIG:
            STUBBED("code SCE_NETCTL_INFO_GET_IP_CONFIG not implemented");
            break;
        case SCE_NETCTL_INFO_GET_DHCP_HOSTNAME:
            STUBBED("code SCE_NETCTL_INFO_GET_DHCP_HOSTNAME not implemented");
            break;
        case SCE_NETCTL_INFO_GET_PPPOE_AUTH_NAME:
            STUBBED("code SCE_NETCTL_INFO_GET_PPPOE_AUTH_NAME not implemented");
            break;
        case SCE_NETCTL_INFO_GET_DEFAULT_ROUTE:
            STUBBED("code SCE_NETCTL_INFO_GET_DEFAULT_ROUTE not implemented");
            break;
        case SCE_NETCTL_INFO_GET_PRIMARY_DNS:
            STUBBED("code SCE_NETCTL_INFO_GET_PRIMARY_DNS not implemented");
            break;
        case SCE_NETCTL_INFO_GET_SECONDARY_DNS:
            STUBBED("code SCE_NETCTL_INFO_GET_SECONDARY_DNS not implemented");
            break;
        case SCE_NETCTL_INFO_GET_HTTP_PROXY_CONFIG:
            STUBBED("code SCE_NETCTL_INFO_GET_HTTP_PROXY_CONFIG not implemented");
            break;
        case SCE_NETCTL_INFO_GET_HTTP_PROXY_SERVER:
            STUBBED("code SCE_NETCTL_INFO_GET_HTTP_PROXY_SERVER not implemented");
            break;
        case SCE_NETCTL_INFO_GET_HTTP_PROXY_PORT:
            STUBBED("code SCE_NETCTL_INFO_GET_HTTP_PROXY_PORT not implemented");
            break;
        default:
            LOG_ERROR("Unknown code:{}", log_hex(code));
        }
    }
    return 0;
}

EXPORT(int, sceNetCtlInetGetResult, int eventType, int *errorCode) {
    TRACY_FUNC(sceNetCtlInetGetResult, eventType, errorCode);
    if (!emuenv.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!errorCode) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }
    LOG_INFO("sceNetCtlInetGetResult called with eventType: {}", eventType);
    *errorCode = 0;
    return 0;
}

EXPORT(int, sceNetCtlInetGetState, int *state) {
    TRACY_FUNC(sceNetCtlInetGetState, state);
    if (!emuenv.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!state) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    *state = SCE_NET_CTL_STATE_IPOBTAINED;
    return STUBBED("state = SCE_NETCTL_STATE_CONNECTED");
}

EXPORT(int, sceNetCtlInetRegisterCallback, Ptr<void> func, Ptr<void> arg, int *cid) {
    TRACY_FUNC(sceNetCtlInetRegisterCallback, func, arg, cid);
    if (!emuenv.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if (!func || !cid) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ADDR);
    }

    const std::lock_guard<std::mutex> lock(emuenv.netctl.mutex);

    // Find the next available slot
    int next_id = 0;
    for (const auto &callback : emuenv.netctl.callbacks) {
        if (callback.pc == 0) {
            break;
        }
        next_id++;
    }

    if (next_id == 8) {
        return RET_ERROR(SCE_NET_CTL_ERROR_CALLBACK_MAX);
    }

    emuenv.netctl.callbacks[next_id].pc = func.address();
    emuenv.netctl.callbacks[next_id].arg = arg.address();
    *cid = next_id;
    return 0;
}

EXPORT(int, sceNetCtlInetUnregisterCallback, int cid) {
    TRACY_FUNC(sceNetCtlInetUnregisterCallback, cid);
    if (!emuenv.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_INITIALIZED);
    }

    if ((cid < 0) || (cid >= 8)) {
        return RET_ERROR(SCE_NET_CTL_ERROR_INVALID_ID);
    }

    const std::lock_guard<std::mutex> lock(emuenv.netctl.mutex);
    emuenv.netctl.callbacks[cid].pc = 0;
    emuenv.netctl.callbacks[cid].arg = 0;

    return 0;
}

EXPORT(int, sceNetCtlInit) {
    TRACY_FUNC(sceNetCtlInit);
    if (emuenv.netctl.inited) {
        return RET_ERROR(SCE_NET_CTL_ERROR_NOT_TERMINATED);
    }
    LOG_INFO("sceNetCtlInit");
    const std::lock_guard<std::mutex> lock(emuenv.netctl.mutex);
    emuenv.netctl.adhocCallbacks.fill({ 0, 0 });
    emuenv.netctl.callbacks.fill({ 0, 0 });

    emuenv.netctl.inited = true;
    emuenv.netctl.adhocThreadRun = true;
    emuenv.netctl.adhocThread = std::thread(adhoc_thread, std::ref(emuenv), thread_id);

    return STUBBED("Stub");
}

EXPORT(void, sceNetCtlTerm) {
    TRACY_FUNC(sceNetCtlTerm);
    STUBBED("Stub");
    LOG_INFO("sceNetCtlTerm");
    emuenv.netctl.inited = false;

    {
        const std::lock_guard<std::mutex> lock(emuenv.netctl.mutex);
        emuenv.netctl.adhocThreadRun = false;
        emuenv.netctl.adhocCondVarReady = false;
        emuenv.netctl.adhocCondVar.notify_all();
    }

    if (emuenv.netctl.adhocThread.joinable())
        emuenv.netctl.adhocThread.join();

    emuenv.netctl.adhocState = SCE_NET_CTL_STATE_DISCONNECTED;
    emuenv.netctl.adhocEvent = SCE_NET_CTL_EVENT_TYPE_NONE;
    emuenv.netctl.lastNotifiedAdhocEvent = SCE_NET_CTL_EVENT_TYPE_NONE;
    emuenv.netctl.adhocPeers.clear();
}
