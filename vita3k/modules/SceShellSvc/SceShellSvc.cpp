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

#include "SceShellSvc.h"

#include "modules/module_parent.h"

static Ptr<void> client_vtable = Ptr<void>();
static Ptr<int> svc_client;
EXPORT(int, sceShellSvcGetSvcObj) {
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

    if (!svc_client) {
        svc_client = Ptr<int>(alloc(emuenv.mem, sizeof(int), "ShellSvc"));
        *svc_client.get(emuenv.mem) = client_vtable.address();
    }

    STUBBED("STUBBED");
    return svc_client.address();
}

EXPORT(int, SceShellSvc_7CBF442B) {
    return UNIMPLEMENTED();
}

EXPORT(int, SceShellSvc_D943CE15) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceShellSvcGetSvcObj)
BRIDGE_IMPL(SceShellSvc_7CBF442B)
BRIDGE_IMPL(SceShellSvc_D943CE15)
