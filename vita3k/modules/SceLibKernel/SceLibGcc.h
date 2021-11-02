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

BRIDGE_DECL(_Unwind_Backtrace)
BRIDGE_DECL(_Unwind_Complete)
BRIDGE_DECL(_Unwind_DeleteException)
BRIDGE_DECL(_Unwind_ForcedUnwind)
BRIDGE_DECL(_Unwind_GetCFA)
BRIDGE_DECL(_Unwind_GetDataRelBase)
BRIDGE_DECL(_Unwind_GetLanguageSpecificData)
BRIDGE_DECL(_Unwind_GetRegionStart)
BRIDGE_DECL(_Unwind_GetTextRelBase)
BRIDGE_DECL(_Unwind_RaiseException)
BRIDGE_DECL(__Unwind_Resume)
BRIDGE_DECL(_Unwind_Resume_or_Rethrow)
BRIDGE_DECL(_Unwind_VRS_Get)
BRIDGE_DECL(_Unwind_VRS_Pop)
BRIDGE_DECL(_Unwind_VRS_Set)
