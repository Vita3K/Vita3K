// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include "SceIpmi.h"

#include "modules/module_parent.h"

static Ptr<void> client_vtable = Ptr<void>();

EXPORT(int, _ZN4IPMI6Client10disconnectEv) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client11getUserDataEv) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client12tryGetResultEjPiPvPmm) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client12tryGetResultEjjPiPNS_10BufferInfoEj) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client13pollEventFlagEjjjPj) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client13waitEventFlagEjjjPjS1_) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client16invokeSyncMethodEjPKNS_8DataInfoEjPiPNS_10BufferInfoEj) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client16invokeSyncMethodEjPKvjPiPvPjj) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client17invokeAsyncMethodEjPKNS_8DataInfoEjPjPKNS0_12EventNotifeeE) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client17invokeAsyncMethodEjPKvjPiPKNS0_12EventNotifeeE) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client19terminateConnectionEv) {
    return UNIMPLEMENTED();
}

// IPMI::Client::Config::estimateClientMemorySize()
EXPORT(int, _ZN4IPMI6Client6Config24estimateClientMemorySizeEv) {
    STUBBED("stubbed");
    return 0x100;
}

// IPMI::Client::create(IPMI::Client**, IPMI::Client::Config const*, void*, void*)
EXPORT(int, _ZN4IPMI6Client6createEPPS0_PKNS0_6ConfigEPvS6_, Ptr<void> *client, void const *config, Ptr<void> user_data, Ptr<void> client_memory) {
    if (!client_vtable) {
        // initialize the vtable
        client_vtable = create_vtable({
                                          0x101C93F8, // destroy
                                          0xA22C3E01, // connect
                                          0xEC73331C, // disconnect
                                          0xD484D36D, // terminateConnection
                                          0x28BD5F19, // invokeSyncMethod
                                          0x73C72FBB, // invokeSyncMethod
                                          0xAFD10F3B, // invokeAsyncMethod
                                          0x387AFA3F, // invokeAsyncMethod
                                          0xF8C2B8BA, // tryGetResult
                                          0x4EBB01A2, // tryGetResult
                                          0x8FF23C3C, // pollEventFlag
                                          0x45C32034, // waitEventFlag
                                          0x004F48ED, // getUserData
                                          0xA3E650B0, // getMsg
                                          0x60EFADE7, // tryGetMsg
                                          0xA5AA193C, // ~Client
                                      },
            emuenv.mem);
    }

    *client_memory.cast<Ptr<void>>().get(emuenv.mem) = client_vtable;
    *client = client_memory;
    return 0;
}

EXPORT(int, _ZN4IPMI6Client6getMsgEjPvPjjS2_) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client7connectEPKvjPi, void* client, void const* params, SceSize params_size, SceInt32* error) {
    *error = 0;
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client7destroyEv) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client9tryGetMsgEjPvPmm) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6ClientD1Ev) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_ZN4IPMI6Client10disconnectEv)
BRIDGE_IMPL(_ZN4IPMI6Client11getUserDataEv)
BRIDGE_IMPL(_ZN4IPMI6Client12tryGetResultEjPiPvPmm)
BRIDGE_IMPL(_ZN4IPMI6Client12tryGetResultEjjPiPNS_10BufferInfoEj)
BRIDGE_IMPL(_ZN4IPMI6Client13pollEventFlagEjjjPj)
BRIDGE_IMPL(_ZN4IPMI6Client13waitEventFlagEjjjPjS1_)
BRIDGE_IMPL(_ZN4IPMI6Client16invokeSyncMethodEjPKNS_8DataInfoEjPiPNS_10BufferInfoEj)
BRIDGE_IMPL(_ZN4IPMI6Client16invokeSyncMethodEjPKvjPiPvPjj)
BRIDGE_IMPL(_ZN4IPMI6Client17invokeAsyncMethodEjPKNS_8DataInfoEjPjPKNS0_12EventNotifeeE)
BRIDGE_IMPL(_ZN4IPMI6Client17invokeAsyncMethodEjPKvjPiPKNS0_12EventNotifeeE)
BRIDGE_IMPL(_ZN4IPMI6Client19terminateConnectionEv)
BRIDGE_IMPL(_ZN4IPMI6Client6Config24estimateClientMemorySizeEv)
BRIDGE_IMPL(_ZN4IPMI6Client6createEPPS0_PKNS0_6ConfigEPvS6_)
BRIDGE_IMPL(_ZN4IPMI6Client6getMsgEjPvPjjS2_)
BRIDGE_IMPL(_ZN4IPMI6Client7connectEPKvjPi)
BRIDGE_IMPL(_ZN4IPMI6Client7destroyEv)
BRIDGE_IMPL(_ZN4IPMI6Client9tryGetMsgEjPvPmm)
BRIDGE_IMPL(_ZN4IPMI6ClientD1Ev)
