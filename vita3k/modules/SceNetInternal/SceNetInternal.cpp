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

#include "SceNetInternal.h"

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

EXPORT(int, sceNetInternal_EE1985D9) {
    TRACY_FUNC(sceNetInternal_EE1985D9);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNetInternal_DF815D48) {
    TRACY_FUNC(SceNetInternal_DF815D48);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNetInternal_333F9CB3) {
    TRACY_FUNC(SceNetInternal_333F9CB3);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNetInternal_694F8996) {
    TRACY_FUNC(SceNetInternal_694F8996);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetInternal_6A9F6F6C) {
    TRACY_FUNC(sceNetInternal_6A9F6F6C);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNetInternal_8157DE3E) {
    TRACY_FUNC(SceNetInternal_8157DE3E);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNetInternal_E03F6A77) {
    TRACY_FUNC(SceNetInternal_E03F6A77);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNetInternal_EAC33599) {
    TRACY_FUNC(SceNetInternal_EAC33599);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNetInternal_EDAA3453) {
    TRACY_FUNC(SceNetInternal_EDAA3453);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNetInternal_235DE96C) {
    TRACY_FUNC(SceNetInternal_235DE96C);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNetInternal_04E6136D) {
    TRACY_FUNC(SceNetInternal_04E6136D);
    return UNIMPLEMENTED();
}

EXPORT(int, SceNetInternal_A9F2277C) {
    TRACY_FUNC(SceNetInternal_A9F2277C);
    return 1;
}

BRIDGE_IMPL(sceNetInternalIcmConnect)
BRIDGE_IMPL(sceNetInternalInetPton)
BRIDGE_IMPL(sceNetInternal_EE1985D9)
BRIDGE_IMPL(SceNetInternal_DF815D48)
BRIDGE_IMPL(SceNetInternal_333F9CB3)
BRIDGE_IMPL(SceNetInternal_694F8996)
BRIDGE_IMPL(SceNetInternal_8157DE3E)
BRIDGE_IMPL(SceNetInternal_E03F6A77)
BRIDGE_IMPL(SceNetInternal_EAC33599)
BRIDGE_IMPL(SceNetInternal_EDAA3453)
BRIDGE_IMPL(SceNetInternal_235DE96C)
BRIDGE_IMPL(SceNetInternal_04E6136D)
BRIDGE_IMPL(SceNetInternal_A9F2277C)
