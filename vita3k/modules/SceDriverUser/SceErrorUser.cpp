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

#include <../SceError/SceError.h>

TRACY_MODULE_NAME(SceErrorUser);

EXPORT(SceInt32, sceErrorGetExternalString, char *result, int err) {
    TRACY_FUNC(sceErrorGetExternalString, result, err);
    return CALL_EXPORT(_sceErrorGetExternalString, result, err);
}

EXPORT(int, sceErrorHistoryClearError) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceErrorHistoryGetError) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceErrorHistoryPostError) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceErrorHistorySetDefaultFormat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceErrorHistoryUpdateSequenceInfo) {
    return UNIMPLEMENTED();
}
