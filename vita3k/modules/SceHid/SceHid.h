// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

BRIDGE_DECL(sceHidConsumerControlEnumerate)
BRIDGE_DECL(sceHidConsumerControlRead)
BRIDGE_DECL(sceHidConsumerControlRegisterEnumHintCallback)
BRIDGE_DECL(sceHidConsumerControlRegisterReadHintCallback)
BRIDGE_DECL(sceHidConsumerControlUnregisterEnumHintCallback)
BRIDGE_DECL(sceHidConsumerControlUnregisterReadHintCallback)
BRIDGE_DECL(sceHidControllerEnumerate)
BRIDGE_DECL(sceHidControllerRead)
BRIDGE_DECL(sceHidControllerRegisterEnumHintCallback)
BRIDGE_DECL(sceHidControllerRegisterReadHintCallback)
BRIDGE_DECL(sceHidControllerUnregisterEnumHintCallback)
BRIDGE_DECL(sceHidControllerUnregisterReadHintCallback)
BRIDGE_DECL(sceHidKeyboardClear)
BRIDGE_DECL(sceHidKeyboardEnumerate)
BRIDGE_DECL(sceHidKeyboardGetIntercept)
BRIDGE_DECL(sceHidKeyboardPeek)
BRIDGE_DECL(sceHidKeyboardRead)
BRIDGE_DECL(sceHidKeyboardRegisterEnumHintCallback)
BRIDGE_DECL(sceHidKeyboardRegisterReadHintCallback)
BRIDGE_DECL(sceHidKeyboardSetIntercept)
BRIDGE_DECL(sceHidKeyboardUnregisterEnumHintCallback)
BRIDGE_DECL(sceHidKeyboardUnregisterReadHintCallback)
BRIDGE_DECL(sceHidMouseEnumerate)
BRIDGE_DECL(sceHidMouseRead)
BRIDGE_DECL(sceHidMouseRegisterEnumHintCallback)
BRIDGE_DECL(sceHidMouseRegisterReadHintCallback)
BRIDGE_DECL(sceHidMouseUnregisterEnumHintCallback)
BRIDGE_DECL(sceHidMouseUnregisterReadHintCallback)
