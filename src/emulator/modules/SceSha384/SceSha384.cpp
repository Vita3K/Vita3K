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

#include "SceSha384.h"

EXPORT(int, sceSha384BlockInit) {
    return unimplemented("sceSha384BlockInit");
}

EXPORT(int, sceSha384BlockResult) {
    return unimplemented("sceSha384BlockResult");
}

EXPORT(int, sceSha384BlockUpdate) {
    return unimplemented("sceSha384BlockUpdate");
}

EXPORT(int, sceSha384Digest) {
    return unimplemented("sceSha384Digest");
}

BRIDGE_IMPL(sceSha384BlockInit)
BRIDGE_IMPL(sceSha384BlockResult)
BRIDGE_IMPL(sceSha384BlockUpdate)
BRIDGE_IMPL(sceSha384Digest)
