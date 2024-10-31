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

#pragma once

#include <enum.h>

BETTER_ENUM(VitaIoDevice, int,
    addcont0 = 0,
    app0,
    cache0,
    host0,
    grw0,
    imc0,
    music0,
    os0,
    pd0,
    photo0,
    sa0,
    savedata0,
    savedata1,
    sd0,
    tm0,
    tty0,
    tty1,
    tty2,
    tty3,
    ud0,
    uma0,
    ur0,
    ux0,
    vd0,
    video0,
    vs0,
    xmc0,
    _INVALID = -1)
