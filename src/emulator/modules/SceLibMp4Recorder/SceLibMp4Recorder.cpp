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

#include "SceLibMp4Recorder.h"

EXPORT(int, sceMp4RecAddAudioSample) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4RecAddVideoSample) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4RecCreateRecorder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4RecCsc) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4RecDeleteRecorder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4RecInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4RecQueryPhysicalMemSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4RecRecorderEnd) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4RecRecorderInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMp4RecTerm) {
    return UNIMPLEMENTED();
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
