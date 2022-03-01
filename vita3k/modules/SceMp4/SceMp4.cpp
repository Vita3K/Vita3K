// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include "SceMp4.h"

EXPORT(int, sceMp4CloseFile) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4EnableStream) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4GetNextUnit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4GetNextUnit2Ref) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4GetNextUnit3Ref) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4GetNextUnitData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4GetStreamInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4JumpPTS) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4OpenFile) {
    STUBBED("Fake Size");

    return 13;
}

EXPORT(int, sceMp4PTSToTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4ReleaseBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4Reset) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4StartFileStreaming) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4TimeToPTS) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceMp4CloseFile)
BRIDGE_IMPL(sceMp4EnableStream)
BRIDGE_IMPL(sceMp4GetNextUnit)
BRIDGE_IMPL(sceMp4GetNextUnit2Ref)
BRIDGE_IMPL(sceMp4GetNextUnit3Ref)
BRIDGE_IMPL(sceMp4GetNextUnitData)
BRIDGE_IMPL(sceMp4GetStreamInfo)
BRIDGE_IMPL(sceMp4JumpPTS)
BRIDGE_IMPL(sceMp4OpenFile)
BRIDGE_IMPL(sceMp4PTSToTime)
BRIDGE_IMPL(sceMp4ReleaseBuffer)
BRIDGE_IMPL(sceMp4Reset)
BRIDGE_IMPL(sceMp4StartFileStreaming)
BRIDGE_IMPL(sceMp4TimeToPTS)
