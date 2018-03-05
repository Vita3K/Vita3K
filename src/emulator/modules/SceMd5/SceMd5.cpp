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

#include "SceMd5.h"

EXPORT(int, sceMd5BlockInit) {
    return unimplemented("sceMd5BlockInit");
}

EXPORT(int, sceMd5BlockResult) {
    return unimplemented("sceMd5BlockResult");
}

EXPORT(int, sceMd5BlockUpdate) {
    return unimplemented("sceMd5BlockUpdate");
}

EXPORT(int, sceMd5Digest) {
    return unimplemented("sceMd5Digest");
}

BRIDGE_IMPL(sceMd5BlockInit)
BRIDGE_IMPL(sceMd5BlockResult)
BRIDGE_IMPL(sceMd5BlockUpdate)
BRIDGE_IMPL(sceMd5Digest)
