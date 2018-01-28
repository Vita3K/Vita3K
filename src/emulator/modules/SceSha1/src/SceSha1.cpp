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

#include <SceSha1/exports.h>

EXPORT(int, sceSha1BlockInit) {
    return unimplemented("sceSha1BlockInit");
}

EXPORT(int, sceSha1BlockResult) {
    return unimplemented("sceSha1BlockResult");
}

EXPORT(int, sceSha1BlockUpdate) {
    return unimplemented("sceSha1BlockUpdate");
}

EXPORT(int, sceSha1Digest) {
    return unimplemented("sceSha1Digest");
}

BRIDGE_IMPL(sceSha1BlockInit)
BRIDGE_IMPL(sceSha1BlockResult)
BRIDGE_IMPL(sceSha1BlockUpdate)
BRIDGE_IMPL(sceSha1Digest)
