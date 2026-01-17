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

#include "../SceNet/SceNet.h"

#include <util/tracy.h>
TRACY_MODULE_NAME(SceNetInternal);

EXPORT(int, sceNetInternalInetPton, int af, const char *src, void *dst) {
    return CALL_EXPORT(sceNetInetPton, af, src, dst);
}

EXPORT(int, sceNetInternalIcmConnect, int sid, int flags) {
    TRACY_FUNC(sceNetInternalIcmConnect, sid, flags);
    // call sceNetSyscallIcmConnect(sid, flags)
    return UNIMPLEMENTED();
}

EXPORT(Ptr<const char>, SceNetInternal_F3917021, int unk) {
    TRACY_FUNC(SceNetInternal_F3917021);
    LOG_DEBUG("SceNetInternal_F3917021, {}", unk);
    Ptr<char> rep = Ptr<char>(alloc(emuenv.mem, 0x10, "ip_field"));
    strcpy(rep.get(emuenv.mem), "127.0.0.1");
    return rep;
}

EXPORT(int, SceNetInternal_689B9D7D, uint32_t *actual_interval) {
    TRACY_FUNC(SceNetInternal_689B9D7D);
    LOG_DEBUG("actual interval: {}", *actual_interval);
    return 0;
}
