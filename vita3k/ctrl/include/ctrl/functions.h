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

#include <ctrl/state.h>
#include <host/state.h>

SceCtrlExternalInputMode get_type_of_controller(const CtrlState &state, int idx);
int peek_data(HostState &host, int port, SceCtrlData *&pad_data, int count, bool negative, bool from_ext_function);
int peek_data(HostState &host, int port, SceCtrlData2 *&pad_data, int count, bool negative, bool from_ext_function);
void refresh_controllers(CtrlState &state);
