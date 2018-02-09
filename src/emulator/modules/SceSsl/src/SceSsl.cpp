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

#include <SceSsl/exports.h>

EXPORT(int, sceSslFreeSslCertName) {
    return unimplemented("sceSslFreeSslCertName");
}

EXPORT(int, sceSslGetIssuerName) {
    return unimplemented("sceSslGetIssuerName");
}

EXPORT(int, sceSslGetMemoryPoolStats) {
    return unimplemented("sceSslGetMemoryPoolStats");
}

EXPORT(int, sceSslGetNameEntryCount) {
    return unimplemented("sceSslGetNameEntryCount");
}

EXPORT(int, sceSslGetNameEntryInfo) {
    return unimplemented("sceSslGetNameEntryInfo");
}

EXPORT(int, sceSslGetNotAfter) {
    return unimplemented("sceSslGetNotAfter");
}

EXPORT(int, sceSslGetNotBefore) {
    return unimplemented("sceSslGetNotBefore");
}

EXPORT(int, sceSslGetSerialNumber) {
    return unimplemented("sceSslGetSerialNumber");
}

EXPORT(int, sceSslGetSubjectName) {
    return unimplemented("sceSslGetSubjectName");
}

EXPORT(int, sceSslInit) {
    return unimplemented("sceSslInit");
}

EXPORT(int, sceSslTerm) {
    return unimplemented("sceSslTerm");
}

BRIDGE_IMPL(sceSslFreeSslCertName)
BRIDGE_IMPL(sceSslGetIssuerName)
BRIDGE_IMPL(sceSslGetMemoryPoolStats)
BRIDGE_IMPL(sceSslGetNameEntryCount)
BRIDGE_IMPL(sceSslGetNameEntryInfo)
BRIDGE_IMPL(sceSslGetNotAfter)
BRIDGE_IMPL(sceSslGetNotBefore)
BRIDGE_IMPL(sceSslGetSerialNumber)
BRIDGE_IMPL(sceSslGetSubjectName)
BRIDGE_IMPL(sceSslInit)
BRIDGE_IMPL(sceSslTerm)
