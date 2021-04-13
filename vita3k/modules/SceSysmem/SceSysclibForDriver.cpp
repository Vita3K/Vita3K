// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include "SceSysclibForDriver.h"

EXPORT(int, __aeabi_idiv) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_lcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_ldivmod) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_lmul) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_uidiv) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_uidivmod) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_ulcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, __memcpy_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, __memmove_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, __memset_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, __kstack_chk_fail) {
    return UNIMPLEMENTED();
}

EXPORT(int, __strncat_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, __strncpy_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, look_ctype_table) {
    return UNIMPLEMENTED();
}

EXPORT(int, kmemchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, kmemcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, kmemcpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, kmemmove) {
    return UNIMPLEMENTED();
}

EXPORT(int, kmemset) {
    return UNIMPLEMENTED();
}

EXPORT(int, rshift) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksnprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, strlcat) {
    return UNIMPLEMENTED();
}

EXPORT(int, strlcpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrlen) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrncat) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrncmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrncpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, strnlen) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrrchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrstr) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrtol) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrtoll) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrtoul) {
    return UNIMPLEMENTED();
}

EXPORT(int, ktolower) {
    return UNIMPLEMENTED();
}

EXPORT(int, ktoupper) {
    return UNIMPLEMENTED();
}

EXPORT(int, kvsnprintf) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(__aeabi_idiv)
BRIDGE_IMPL(__aeabi_lcmp)
BRIDGE_IMPL(__aeabi_ldivmod)
BRIDGE_IMPL(__aeabi_lmul)
BRIDGE_IMPL(__aeabi_uidiv)
BRIDGE_IMPL(__aeabi_uidivmod)
BRIDGE_IMPL(__aeabi_ulcmp)
BRIDGE_IMPL(__memcpy_chk)
BRIDGE_IMPL(__memmove_chk)
BRIDGE_IMPL(__memset_chk)
BRIDGE_IMPL(__kstack_chk_fail)
BRIDGE_IMPL(__strncat_chk)
BRIDGE_IMPL(__strncpy_chk)
BRIDGE_IMPL(look_ctype_table)
BRIDGE_IMPL(kmemchr)
BRIDGE_IMPL(kmemcmp)
BRIDGE_IMPL(kmemcpy)
BRIDGE_IMPL(kmemmove)
BRIDGE_IMPL(kmemset)
BRIDGE_IMPL(rshift)
BRIDGE_IMPL(ksnprintf)
BRIDGE_IMPL(kstrchr)
BRIDGE_IMPL(kstrcmp)
BRIDGE_IMPL(strlcat)
BRIDGE_IMPL(strlcpy)
BRIDGE_IMPL(kstrlen)
BRIDGE_IMPL(kstrncat)
BRIDGE_IMPL(kstrncmp)
BRIDGE_IMPL(kstrncpy)
BRIDGE_IMPL(strnlen)
BRIDGE_IMPL(kstrrchr)
BRIDGE_IMPL(kstrstr)
BRIDGE_IMPL(kstrtol)
BRIDGE_IMPL(kstrtoll)
BRIDGE_IMPL(kstrtoul)
BRIDGE_IMPL(ktolower)
BRIDGE_IMPL(ktoupper)
BRIDGE_IMPL(kvsnprintf)
