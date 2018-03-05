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

#include "SceSha0.h"

EXPORT(int, sceSha0BlockInit) {
    return unimplemented("sceSha0BlockInit");
}

EXPORT(int, sceSha0BlockResult) {
    return unimplemented("sceSha0BlockResult");
}

EXPORT(int, sceSha0BlockUpdate) {
    return unimplemented("sceSha0BlockUpdate");
}

EXPORT(int, sceSha0Digest) {
    return unimplemented("sceSha0Digest");
}

BRIDGE_IMPL(sceSha0BlockInit)
BRIDGE_IMPL(sceSha0BlockResult)
BRIDGE_IMPL(sceSha0BlockUpdate)
BRIDGE_IMPL(sceSha0Digest)
