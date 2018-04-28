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
    return unimplemented(export_name);
}

EXPORT(int, sceHandwritingInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceHandwritingRecognize) {
    return unimplemented(export_name);
}

EXPORT(int, sceHandwritingRegisterDelete) {
    return unimplemented(export_name);
}

EXPORT(int, sceHandwritingRegisterGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceHandwritingRegisterInfo) {
    return unimplemented(export_name);
}

EXPORT(int, sceHandwritingRegisterInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceHandwritingRegisterSet) {
    return unimplemented(export_name);
}

EXPORT(int, sceHandwritingRegisterTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceHandwritingSetMode) {
    return unimplemented(export_name);
}

EXPORT(int, sceHandwritingTerm) {
    return unimplemented(export_name);
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
