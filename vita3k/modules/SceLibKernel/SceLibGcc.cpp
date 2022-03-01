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

#include "SceLibGcc.h"

EXPORT(int, _Unwind_Backtrace) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unwind_Complete) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unwind_DeleteException) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unwind_ForcedUnwind) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unwind_GetCFA) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unwind_GetDataRelBase) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unwind_GetLanguageSpecificData) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unwind_GetRegionStart) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unwind_GetTextRelBase) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unwind_RaiseException) {
    return UNIMPLEMENTED();
}

EXPORT(int, __Unwind_Resume) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unwind_Resume_or_Rethrow) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unwind_VRS_Get) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unwind_VRS_Pop) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unwind_VRS_Set) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_Unwind_Backtrace)
BRIDGE_IMPL(_Unwind_Complete)
BRIDGE_IMPL(_Unwind_DeleteException)
BRIDGE_IMPL(_Unwind_ForcedUnwind)
BRIDGE_IMPL(_Unwind_GetCFA)
BRIDGE_IMPL(_Unwind_GetDataRelBase)
BRIDGE_IMPL(_Unwind_GetLanguageSpecificData)
BRIDGE_IMPL(_Unwind_GetRegionStart)
BRIDGE_IMPL(_Unwind_GetTextRelBase)
BRIDGE_IMPL(_Unwind_RaiseException)
BRIDGE_IMPL(__Unwind_Resume)
BRIDGE_IMPL(_Unwind_Resume_or_Rethrow)
BRIDGE_IMPL(_Unwind_VRS_Get)
BRIDGE_IMPL(_Unwind_VRS_Pop)
BRIDGE_IMPL(_Unwind_VRS_Set)
