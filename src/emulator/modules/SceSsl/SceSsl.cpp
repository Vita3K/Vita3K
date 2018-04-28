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

#include "SceSsl.h"

EXPORT(int, sceSslFreeSslCertName) {
    return unimplemented(export_name);
}

EXPORT(int, sceSslGetIssuerName) {
    return unimplemented(export_name);
}

EXPORT(int, sceSslGetMemoryPoolStats) {
    return unimplemented(export_name);
}

EXPORT(int, sceSslGetNameEntryCount) {
    return unimplemented(export_name);
}

EXPORT(int, sceSslGetNameEntryInfo) {
    return unimplemented(export_name);
}

EXPORT(int, sceSslGetNotAfter) {
    return unimplemented(export_name);
}

EXPORT(int, sceSslGetNotBefore) {
    return unimplemented(export_name);
}

EXPORT(int, sceSslGetSerialNumber) {
    return unimplemented(export_name);
}

EXPORT(int, sceSslGetSubjectName) {
    return unimplemented(export_name);
}

EXPORT(int, sceSslInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceSslTerm) {
    return unimplemented(export_name);
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
