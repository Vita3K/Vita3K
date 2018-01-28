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

#include <SceSha224/exports.h>

EXPORT(int, sceSha224BlockInit) {
    return unimplemented("sceSha224BlockInit");
}

EXPORT(int, sceSha224BlockResult) {
    return unimplemented("sceSha224BlockResult");
}

EXPORT(int, sceSha224BlockUpdate) {
    return unimplemented("sceSha224BlockUpdate");
}

EXPORT(int, sceSha224Digest) {
    return unimplemented("sceSha224Digest");
}

BRIDGE_IMPL(sceSha224BlockInit)
BRIDGE_IMPL(sceSha224BlockResult)
BRIDGE_IMPL(sceSha224BlockUpdate)
BRIDGE_IMPL(sceSha224Digest)
