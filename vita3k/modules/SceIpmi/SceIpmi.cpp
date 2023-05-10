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

struct SceImpiStruct;

// SceIpmi_B282B430 returns a struct s such that *(*s+4) is a function
struct SceImpiStruct {
    Ptr<SceImpiStruct> itself;

    Ptr<void> func;
    // function that returns immediatly
    uint8_t func_content[2];
};

EXPORT(int, _ZN4IPMI6Client6createEPPS0_PKNS0_6ConfigEPvS6_, Ptr<SceImpiStruct> *s, void* config, void* user_data, void* memory) {
    STUBBED("stubbed");
    *s = alloc<SceImpiStruct>(emuenv.mem, export_name);
    SceImpiStruct *s_ptr = s->get(emuenv.mem);
    s_ptr->itself = *s;
    // function is THUMB
    s_ptr->func = (s->cast<uint8_t>() + offsetof(SceImpiStruct, func_content) + 1);
    // bx lr
    s_ptr->func_content[0] = 0x70;
    s_ptr->func_content[1] = 0x47;
    return 0;
}

BRIDGE_IMPL(_ZN4IPMI6Client6createEPPS0_PKNS0_6ConfigEPvS6_)
