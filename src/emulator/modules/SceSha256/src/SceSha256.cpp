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

#include <SceSha256/exports.h>

EXPORT(int, sceSha256BlockInit) {
    return unimplemented("sceSha256BlockInit");
}

EXPORT(int, sceSha256BlockResult) {
    return unimplemented("sceSha256BlockResult");
}

EXPORT(int, sceSha256BlockUpdate) {
    return unimplemented("sceSha256BlockUpdate");
}

EXPORT(int, sceSha256Digest) {
    return unimplemented("sceSha256Digest");
}

BRIDGE_IMPL(sceSha256BlockInit)
BRIDGE_IMPL(sceSha256BlockResult)
BRIDGE_IMPL(sceSha256BlockUpdate)
BRIDGE_IMPL(sceSha256Digest)
