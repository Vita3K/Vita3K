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

struct SceMp4FileManager {
    uint32_t user_data = 0;

    // Cast to SceMp4OpenFile, SceMp4CloseFile, SceMp4ReadFile and SceMp4GetFileSize.
    Ptr<void> open_file;
    Ptr<void> close_file;
    Ptr<void> read_file;
    Ptr<void> file_size;
};

enum class MediaType {
    VIDEO,
    AUDIO,
};

struct SceMp4StreamInfo {
    MediaType stream_type;
    SceInt32 codec_type; // (must be 1 - for stream type 0 and 2 or 4 for stream type 1)
};

EXPORT(SceInt32, sceMp4CloseFile, SceUID mp4_handler) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceMp4EnableStream, SceUID mp4_handler, int32_t stream_no, bool state) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceMp4GetNextUnit, SceUID mp4_handler, uint32_t param_2) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceMp4GetNextUnit2Ref, SceUID mp4_handler, uint32_t *param_2) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceMp4GetNextUnit3Ref, SceUID mp4_handler, uint32_t *param_2) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceMp4GetNextUnitData, SceUID mp4_handler, void *param_2) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceMp4GetStreamInfo, SceUID mp4_handler, int32_t stream_no, SceMp4StreamInfo *stream_info) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceMp4JumpPTS) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceMp4OpenFile, Ptr<const char> path, SceMp4FileManager *file_manager, int32_t param_3, int32_t memory_base, int32_t capacity, int32_t param_6, int32_t log_level) {
    STUBBED("Fake Handle");

    return 13;
}

EXPORT(SceInt64, sceMp4PTSToTime, SceInt64 pts) {
    return UNIMPLEMENTED();
}

EXPORT(void, sceMp4ReleaseBuffer, SceUID mp4_handler, uint32_t *buffer) {
    buffer[0] = 0;
    buffer[1] = 0;
    buffer[2] = 0;
}

EXPORT(SceInt32, sceMp4Reset, SceUID mp4_handler) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceMp4StartFileStreaming, SceUID mp4_handler, uint32_t *streams_count, SceInt64 *video_length) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt64, sceMp4TimeToPTS, SceInt64 Time) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceMp4GetStreamInfo, SceUID mp4_handle, uint32_t stream_no, SceMp4StreamInfo *stream_info) {
    return CALL_EXPORT(sceMp4GetStreamInfo, mp4_handle, stream_no, stream_info);
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
BRIDGE_IMPL(_sceMp4GetStreamInfo)
