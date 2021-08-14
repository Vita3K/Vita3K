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

#include "SceError.h"

EXPORT(int, _sceErrorGetExternalString) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceErrorHistoryClearError) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceErrorHistoryGetError) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceErrorHistoryPostError) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceErrorHistorySetDefaultFormat) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceErrorHistoryUpdateSequenceInfo) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_sceErrorGetExternalString)
BRIDGE_IMPL(_sceErrorHistoryClearError)
BRIDGE_IMPL(_sceErrorHistoryGetError)
BRIDGE_IMPL(_sceErrorHistoryPostError)
BRIDGE_IMPL(_sceErrorHistorySetDefaultFormat)
BRIDGE_IMPL(_sceErrorHistoryUpdateSequenceInfo)
