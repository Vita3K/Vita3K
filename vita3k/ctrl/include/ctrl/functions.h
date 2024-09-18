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

#pragma once

#include <ctrl/state.h>
#include <emuenv/state.h>

#include <array>

std::array<ControllerBinding, 15> get_controller_bindings_ext(EmuEnvState &emuenv);
SceCtrlExternalInputMode get_type_of_controller(const int idx);
int ctrl_get(const SceUID thread_id, EmuEnvState &emuenv, int port, SceCtrlData2 *pData, SceUInt32 count, bool negative, bool is_peek, bool is_v2, bool from_ext);
void refresh_controllers(CtrlState &state, EmuEnvState &emuenv);
