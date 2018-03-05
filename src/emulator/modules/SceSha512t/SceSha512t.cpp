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

#include "SceSha512t.h"

EXPORT(int, sceSha512tBlockInit) {
    return unimplemented("sceSha512tBlockInit");
}

EXPORT(int, sceSha512tBlockResult) {
    return unimplemented("sceSha512tBlockResult");
}

EXPORT(int, sceSha512tBlockUpdate) {
    return unimplemented("sceSha512tBlockUpdate");
}

EXPORT(int, sceSha512tDigest) {
    return unimplemented("sceSha512tDigest");
}

BRIDGE_IMPL(sceSha512tBlockInit)
BRIDGE_IMPL(sceSha512tBlockResult)
BRIDGE_IMPL(sceSha512tBlockUpdate)
BRIDGE_IMPL(sceSha512tDigest)
