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

#include <SceLibMp4Recorder/exports.h>

EXPORT(int, sceMp4RecAddAudioSample) {
    return unimplemented("sceMp4RecAddAudioSample");
}

EXPORT(int, sceMp4RecAddVideoSample) {
    return unimplemented("sceMp4RecAddVideoSample");
}

EXPORT(int, sceMp4RecCreateRecorder) {
    return unimplemented("sceMp4RecCreateRecorder");
}

EXPORT(int, sceMp4RecCsc) {
    return unimplemented("sceMp4RecCsc");
}

EXPORT(int, sceMp4RecDeleteRecorder) {
    return unimplemented("sceMp4RecDeleteRecorder");
}

EXPORT(int, sceMp4RecInit) {
    return unimplemented("sceMp4RecInit");
}

EXPORT(int, sceMp4RecQueryPhysicalMemSize) {
    return unimplemented("sceMp4RecQueryPhysicalMemSize");
}

EXPORT(int, sceMp4RecRecorderEnd) {
    return unimplemented("sceMp4RecRecorderEnd");
}

EXPORT(int, sceMp4RecRecorderInit) {
    return unimplemented("sceMp4RecRecorderInit");
}

EXPORT(int, sceMp4RecTerm) {
    return unimplemented("sceMp4RecTerm");
}

BRIDGE_IMPL(sceMp4RecAddAudioSample)
BRIDGE_IMPL(sceMp4RecAddVideoSample)
BRIDGE_IMPL(sceMp4RecCreateRecorder)
BRIDGE_IMPL(sceMp4RecCsc)
BRIDGE_IMPL(sceMp4RecDeleteRecorder)
BRIDGE_IMPL(sceMp4RecInit)
BRIDGE_IMPL(sceMp4RecQueryPhysicalMemSize)
BRIDGE_IMPL(sceMp4RecRecorderEnd)
BRIDGE_IMPL(sceMp4RecRecorderInit)
BRIDGE_IMPL(sceMp4RecTerm)
