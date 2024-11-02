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

#include <module/module.h>

#include "modules/module_parent.h"

#include <util/tracy.h>

TRACY_MODULE_NAME(SceIpmi);

static Ptr<void> client_vtable = Ptr<void>();

EXPORT(int, _ZN4IPMI6Client10disconnectEv) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client11getUserDataEv) {
    return UNIMPLEMENTED();
}

struct BufferInfo {
    Ptr<void> pBuffer;
    SceSize bufferSize;
    SceSize bufferWrittenSize; // size written by method
};

EXPORT(int, _ZN4IPMI6Client12tryGetResultEjPiPvPmm, unsigned int a1, int *a2, void *a3, unsigned long *a4, unsigned long a5) {
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
    TRACY_FUNC(_ZN4IPMI6Client6Config24estimateClientMemorySizeEv);
    STUBBED("stubbed");
    return 0x100;
}

// IPMI::Client::create(IPMI::Client**, IPMI::Client::Config const*, void*, void*)
EXPORT(int, _ZN4IPMI6Client6createEPPS0_PKNS0_6ConfigEPvS6_, Ptr<void> *client, void const *config, Ptr<void> user_data, Ptr<void> client_memory) {
    TRACY_FUNC(_ZN4IPMI6Client6createEPPS0_PKNS0_6ConfigEPvS6_, client, config, user_data, client_memory);
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
    STUBBED("Stubed");
    return 0;
}

EXPORT(int, _ZN4IPMI6Client6getMsgEjPvPjjS2_) {
    return UNIMPLEMENTED();
}

EXPORT(int, _ZN4IPMI6Client7connectEPKvjPi, void *client, void const *params, SceSize params_size, SceInt32 *error) {
    TRACY_FUNC(_ZN4IPMI6Client7connectEPKvjPi, client, error);
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

EXPORT(int, SceIpmi_296D44D4) {
    return UNIMPLEMENTED();
}

EXPORT(int, SceIpmi_BC3A3031) {
    return UNIMPLEMENTED();
}

EXPORT(int, SceIpmi_2C6DB642) {
    return UNIMPLEMENTED();
}

EXPORT(int, SceIpmi_006EFA9D) {
    return UNIMPLEMENTED();
}
