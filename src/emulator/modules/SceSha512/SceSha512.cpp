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

#include "SceSha512.h"

EXPORT(int, sceSha512BlockInit) {
    return unimplemented("sceSha512BlockInit");
}

EXPORT(int, sceSha512BlockResult) {
    return unimplemented("sceSha512BlockResult");
}

EXPORT(int, sceSha512BlockUpdate) {
    return unimplemented("sceSha512BlockUpdate");
}

EXPORT(int, sceSha512Digest) {
    return unimplemented("sceSha512Digest");
}

BRIDGE_IMPL(sceSha512BlockInit)
BRIDGE_IMPL(sceSha512BlockResult)
BRIDGE_IMPL(sceSha512BlockUpdate)
BRIDGE_IMPL(sceSha512Digest)
