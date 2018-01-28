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

#include <SceIme/exports.h>

EXPORT(int, sceImeClose) {
    return unimplemented("sceImeClose");
}

EXPORT(int, sceImeOpen) {
    return unimplemented("sceImeOpen");
}

EXPORT(int, sceImeSetCaret) {
    return unimplemented("sceImeSetCaret");
}

EXPORT(int, sceImeSetPreeditGeometry) {
    return unimplemented("sceImeSetPreeditGeometry");
}

EXPORT(int, sceImeSetText) {
    return unimplemented("sceImeSetText");
}

EXPORT(int, sceImeUpdate) {
    return unimplemented("sceImeUpdate");
}

BRIDGE_IMPL(sceImeClose)
BRIDGE_IMPL(sceImeOpen)
BRIDGE_IMPL(sceImeSetCaret)
BRIDGE_IMPL(sceImeSetPreeditGeometry)
BRIDGE_IMPL(sceImeSetText)
BRIDGE_IMPL(sceImeUpdate)
