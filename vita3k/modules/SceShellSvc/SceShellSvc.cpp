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

#include <module/module.h>
#include <modules/module_parent.h>

#include <util/tracy.h>

TRACY_MODULE_NAME(SceShellSvc)

EXPORT(int, sceShellSvcGetSvcObj) {
    TRACY_FUNC(sceShellSvcGetSvcObj);
    static Ptr<Address> svc_client;
    if (!svc_client) {
        svc_client = Ptr<Address>(alloc(emuenv.mem, sizeof(int), "ShellSvc"));
        *svc_client.get(emuenv.mem) = get_client_vtable(emuenv.mem).address();
    }

    STUBBED("STUBBED");
    return svc_client.address();
}
