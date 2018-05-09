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

#include "SceHandwriting.h"

EXPORT(int, sceHandwritingGetBufferSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHandwritingInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHandwritingRecognize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHandwritingRegisterDelete) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHandwritingRegisterGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHandwritingRegisterInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHandwritingRegisterInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHandwritingRegisterSet) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHandwritingRegisterTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHandwritingSetMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHandwritingTerm) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceHandwritingGetBufferSize)
BRIDGE_IMPL(sceHandwritingInit)
BRIDGE_IMPL(sceHandwritingRecognize)
BRIDGE_IMPL(sceHandwritingRegisterDelete)
BRIDGE_IMPL(sceHandwritingRegisterGetResult)
BRIDGE_IMPL(sceHandwritingRegisterInfo)
BRIDGE_IMPL(sceHandwritingRegisterInit)
BRIDGE_IMPL(sceHandwritingRegisterSet)
BRIDGE_IMPL(sceHandwritingRegisterTerm)
BRIDGE_IMPL(sceHandwritingSetMode)
BRIDGE_IMPL(sceHandwritingTerm)
