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

#pragma once

#include <module/module.h>

BRIDGE_DECL(__aeabi_idiv)
BRIDGE_DECL(__aeabi_lcmp)
BRIDGE_DECL(__aeabi_ldivmod)
BRIDGE_DECL(__aeabi_lmul)
BRIDGE_DECL(__aeabi_uidiv)
BRIDGE_DECL(__aeabi_uidivmod)
BRIDGE_DECL(__aeabi_ulcmp)
BRIDGE_DECL(__memcpy_chk)
BRIDGE_DECL(__memmove_chk)
BRIDGE_DECL(__memset_chk)
BRIDGE_DECL(__kstack_chk_fail)
BRIDGE_DECL(__strncat_chk)
BRIDGE_DECL(__strncpy_chk)
BRIDGE_DECL(look_ctype_table)
BRIDGE_DECL(kmemchr)
BRIDGE_DECL(kmemcmp)
BRIDGE_DECL(kmemcpy)
BRIDGE_DECL(kmemmove)
BRIDGE_DECL(kmemset)
BRIDGE_DECL(rshift)
BRIDGE_DECL(ksnprintf)
BRIDGE_DECL(kstrchr)
BRIDGE_DECL(kstrcmp)
BRIDGE_DECL(strlcat)
BRIDGE_DECL(strlcpy)
BRIDGE_DECL(kstrlen)
BRIDGE_DECL(kstrncat)
BRIDGE_DECL(kstrncmp)
BRIDGE_DECL(kstrncpy)
BRIDGE_DECL(strnlen)
BRIDGE_DECL(kstrrchr)
BRIDGE_DECL(kstrstr)
BRIDGE_DECL(kstrtol)
BRIDGE_DECL(kstrtoll)
BRIDGE_DECL(kstrtoul)
BRIDGE_DECL(ktolower)
BRIDGE_DECL(ktoupper)
BRIDGE_DECL(kvsnprintf)
