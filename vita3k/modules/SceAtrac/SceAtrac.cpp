// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <module/module.h>

#include "../SceAudiodec/SceAudiodecUser.h"

#include <util/tracy.h>

TRACY_MODULE_NAME(SceAtrac);

#define SCE_ATRAC_ALIGNMENT_SIZE 0x100U
#define SCE_ATRAC_TYPE_AT9 0x2003U
#define SCE_ATRAC_AT9_MAX_TOTAL_CH 16
#define SCE_ATRAC_WORD_LENGTH_16BITS 16

enum {
    SCE_ATRAC_ERROR_INVALID_POINTER = 0x80630000,
    SCE_ATRAC_ERROR_INVALID_SIZE = 0x80630001,
    SCE_ATRAC_ERROR_INVALID_WORD_LENGTH = 0x80630002,
    SCE_ATRAC_ERROR_INVALID_TYPE = 0x80630003,
    SCE_ATRAC_ERROR_INVALID_TOTAL_CH = 0x80630004,
    SCE_ATRAC_ERROR_INVALID_ALIGNMENT = 0x80630005,
    SCE_ATRAC_ERROR_ALREADY_CREATED = 0x80630006,
    SCE_ATRAC_ERROR_NOT_CREATED = 0x80630007,
    SCE_ATRAC_ERROR_SHORTAGE_OF_CH = 0x80630008,
    SCE_ATRAC_ERROR_UNSUPPORTED_DATA = 0x80630009,
    SCE_ATRAC_ERROR_INVALID_DATA = 0x8063000A,
    SCE_ATRAC_ERROR_READ_SIZE_IS_TOO_SMALL = 0x8063000B,
    SCE_ATRAC_ERROR_INVALID_HANDLE = 0x8063000C,
    SCE_ATRAC_ERROR_READ_SIZE_OVER_BUFFER = 0x8063000D,
    SCE_ATRAC_ERROR_MAIN_BUFFER_SIZE_IS_TOO_SMALL = 0x8063000E,
    SCE_ATRAC_ERROR_SUB_BUFFER_SIZE_IS_TOO_SMALL = 0x8063000F,
    SCE_ATRAC_ERROR_DATA_SHORTAGE_IN_BUFFER = 0x80630010,
    SCE_ATRAC_ERROR_ALL_DATA_WAS_DECODED = 0x80630011,
    SCE_ATRAC_ERROR_INVALID_MAX_OUTPUT_SAMPLES = 0x80630012,
    SCE_ATRAC_ERROR_ADDED_DATA_IS_TOO_BIG = 0x80630013,
    SCE_ATRAC_ERROR_NEED_SUB_BUFFER = 0x80630014,
    SCE_ATRAC_ERROR_INVALID_SAMPLE = 0x80630015,
    SCE_ATRAC_ERROR_NO_NEED_SUB_BUFFER = 0x80630016,
    SCE_ATRAC_ERROR_INVALID_LOOP_STATUS = 0x80630017,
    SCE_ATRAC_ERROR_REMAIN_VALID_HANDLE = 0x80630018,
    SCE_ATRAC_ERROR_INVALID_LOOP_NUM = 0x80630030
};

struct SceAtracDecoderGroup {
    SceUInt32 size;
    SceUInt32 wordLength;
    SceUInt32 totalCh;
};

struct SceAtracContentInfo {
    SceUInt32 size;
    SceUInt32 atracType;
    SceUInt32 channel;
    SceUInt32 samplingRate;
    SceInt32 endSample;
    SceInt32 loopStartSample;
    SceInt32 loopEndSample;
    SceUInt32 bitRate;
    SceUInt32 fixedEncBlockSize;
    SceUInt32 fixedEncBlockSample;
    SceUInt32 frameSample;
    SceUInt32 loopBlockOffset;
    SceUInt32 loopBlockSize;
};

struct SceAtracStreamInfo {
    SceUInt32 size;
    Ptr<SceUChar8> pWritePosition;
    SceUInt32 readPosition;
    SceUInt32 writableSize;
};

EXPORT(SceInt32, sceAtracAddStreamData, SceInt32 atracHandle, SceUInt32 addSize) {
    TRACY_FUNC(sceAtracAddStreamData, atracHandle, addSize);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracCreateDecoderGroup, SceUInt32 atracType, const SceAtracDecoderGroup *decoderGroup, Ptr<void> pvWorkMem, SceInt32 initAudiodecFlag) {
    TRACY_FUNC(sceAtracCreateDecoderGroup, atracType, decoderGroup, pvWorkMem, initAudiodecFlag);
    if (!decoderGroup) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    if (decoderGroup->size != sizeof(SceAtracDecoderGroup)) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_SIZE);
    }

    if (decoderGroup->wordLength != SCE_ATRAC_WORD_LENGTH_16BITS) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_WORD_LENGTH);
    }

    if (!pvWorkMem) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    if ((pvWorkMem.address() & (SCE_ATRAC_ALIGNMENT_SIZE - 1)) != 0) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_ALIGNMENT);
    }

    if (atracType != SCE_ATRAC_TYPE_AT9) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_TYPE);
    }

    if ((decoderGroup->totalCh == 0) || (decoderGroup->totalCh > SCE_ATRAC_AT9_MAX_TOTAL_CH)) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_TOTAL_CH);
    }

    if (initAudiodecFlag) {
        SceAudiodecInitParam audiodecInitParam = {};
        audiodecInitParam.size = sizeof(audiodecInitParam.at9);
        audiodecInitParam.at9.totalCh = decoderGroup->totalCh;

        SceInt32 res = CALL_EXPORT(sceAudiodecInitLibrary, SCE_AUDIODEC_TYPE_AT9, &audiodecInitParam);
        if (res < 0) {
            return res;
        }
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracDecode, SceInt32 atracHandle, Ptr<void> pOutputBuffer, Ptr<SceUInt32> pOutputSamples, Ptr<SceUInt32> pDecoderStatus) {
    TRACY_FUNC(sceAtracDecode, atracHandle, pOutputBuffer, pOutputSamples, pDecoderStatus);
    if (!pOutputBuffer || !pOutputSamples || !pDecoderStatus) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    if ((pOutputBuffer.address() & 1) != 0) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_ALIGNMENT);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracDeleteDecoderGroup, SceUInt32 atracType, SceInt32 termAudiodecFlag) {
    TRACY_FUNC(sceAtracDeleteDecoderGroup, atracType, termAudiodecFlag);
    if (atracType != SCE_ATRAC_TYPE_AT9) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_TYPE);
    }

    if (termAudiodecFlag) {
        SceInt32 res = CALL_EXPORT(sceAudiodecTermLibrary, SCE_AUDIODEC_TYPE_AT9);
        if (res < 0) {
            return res;
        }
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracGetContentInfo, SceInt32 atracHandle, SceAtracContentInfo *contentInfo) {
    TRACY_FUNC(sceAtracGetContentInfo, atracHandle, contentInfo);
    if (!contentInfo) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    if (contentInfo->size != sizeof(SceAtracContentInfo)) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_SIZE);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracGetDecoderGroupInfo, SceUInt32 atracType, SceAtracDecoderGroup *createdDecoder, SceAtracDecoderGroup *availableDecoder) {
    TRACY_FUNC(sceAtracGetDecoderGroupInfo, atracType, createdDecoder, availableDecoder);
    if (!createdDecoder || !availableDecoder) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    if (createdDecoder->size != sizeof(SceAtracDecoderGroup) || availableDecoder->size != sizeof(SceAtracDecoderGroup)) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    if (atracType != SCE_ATRAC_TYPE_AT9) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_TYPE);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracGetDecoderStatus, SceInt32 atracHandle, Ptr<SceUInt32> pDecoderStatus) {
    TRACY_FUNC(sceAtracGetDecoderStatus, atracHandle, pDecoderStatus);
    if (!pDecoderStatus) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracGetInternalError, SceInt32 atracHandle, Ptr<SceInt32> pInternalError) {
    TRACY_FUNC(sceAtracGetInternalError, atracHandle, pInternalError);
    if (!pInternalError) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracGetLoopInfo, SceInt32 atracHandle, Ptr<SceInt32> pLoopNum, Ptr<SceUInt32> pLoopStatus) {
    TRACY_FUNC(sceAtracGetLoopInfo, atracHandle, pLoopNum, pLoopStatus);
    if (!pLoopNum || !pLoopStatus) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracGetNextOutputPosition, SceInt32 atracHandle, Ptr<SceUInt32> pNextOutputSample) {
    TRACY_FUNC(sceAtracGetNextOutputPosition, atracHandle, pNextOutputSample);
    if (!pNextOutputSample) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracGetOutputSamples, SceInt32 atracHandle, Ptr<SceUInt32> pOutputSamples) {
    TRACY_FUNC(sceAtracGetOutputSamples, atracHandle, pOutputSamples);
    if (!pOutputSamples) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracGetOutputableSamples, SceInt32 atracHandle, Ptr<SceLong64> pOutputableSamples) {
    TRACY_FUNC(sceAtracGetOutputableSamples, atracHandle, pOutputableSamples);
    if (!pOutputableSamples) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracGetRemainSamples, SceInt32 atracHandle, Ptr<SceLong64> pRemainSamples) {
    TRACY_FUNC(sceAtracGetRemainSamples, atracHandle, pRemainSamples);
    if (!pRemainSamples) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracGetStreamInfo, SceInt32 atracHandle, SceAtracStreamInfo *streamInfo) {
    TRACY_FUNC(sceAtracGetStreamInfo, atracHandle, streamInfo);
    if (!streamInfo) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    if (streamInfo->size != sizeof(SceAtracStreamInfo)) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_SIZE);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracGetSubBufferInfo, SceInt32 atracHandle, Ptr<SceUInt32> pReadPosition, Ptr<SceUInt32> pMinSubBufferSize, Ptr<SceUInt32> pDataSize) {
    TRACY_FUNC(sceAtracGetSubBufferInfo, atracHandle, pReadPosition, pMinSubBufferSize, pDataSize);
    if (!pReadPosition || !pMinSubBufferSize || !pDataSize) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracGetVacantSize, SceInt32 atracHandle, Ptr<SceUInt32> pVacantSize) {
    TRACY_FUNC(sceAtracGetVacantSize, atracHandle, pVacantSize);
    if (!pVacantSize) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracIsSubBufferNeeded, SceInt32 atracHandle) {
    TRACY_FUNC(sceAtracIsSubBufferNeeded, atracHandle);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracQueryDecoderGroupMemSize, SceUInt32 atracType, const SceAtracDecoderGroup *decoderGroup) {
    TRACY_FUNC(sceAtracQueryDecoderGroupMemSize, atracType, decoderGroup);
    if (!decoderGroup) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    if (decoderGroup->size != sizeof(SceAtracDecoderGroup)) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_SIZE);
    }

    if (decoderGroup->wordLength != SCE_ATRAC_WORD_LENGTH_16BITS) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_WORD_LENGTH);
    }

    if (atracType != SCE_ATRAC_TYPE_AT9) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_TYPE);
    }

    if ((decoderGroup->totalCh == 0) || (decoderGroup->totalCh > SCE_ATRAC_AT9_MAX_TOTAL_CH)) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_TOTAL_CH);
    }

    return decoderGroup->totalCh << 10;
}

EXPORT(SceInt32, sceAtracReleaseHandle, SceInt32 atracHandle) {
    TRACY_FUNC(sceAtracReleaseHandle, atracHandle);
    if ((atracHandle >> 16) != SCE_ATRAC_TYPE_AT9) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_HANDLE);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracResetNextOutputPosition, SceInt32 atracHandle, SceUInt32 resetSample) {
    TRACY_FUNC(sceAtracResetNextOutputPosition, atracHandle, resetSample);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracSetDataAndAcquireHandle, Ptr<SceUChar8> pucBuffer, SceUInt32 uiReadSize, SceUInt32 uiBufferSize) {
    TRACY_FUNC(sceAtracSetDataAndAcquireHandle, pucBuffer, uiReadSize, uiBufferSize);
    if (!pucBuffer) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_POINTER);
    }

    if (((pucBuffer.address() & (SCE_ATRAC_ALIGNMENT_SIZE - 1)) != 0) || ((uiBufferSize & (SCE_ATRAC_ALIGNMENT_SIZE - 1)) != 0)) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_ALIGNMENT);
    }

    if (uiBufferSize < uiReadSize) {
        return RET_ERROR(SCE_ATRAC_ERROR_READ_SIZE_OVER_BUFFER);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracSetLoopNum, SceInt32 atracHandle, SceInt32 loopNum) {
    TRACY_FUNC(sceAtracSetLoopNum, atracHandle, loopNum);
    if (loopNum < -1) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_LOOP_NUM);
    }

    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracSetOutputSamples, SceInt32 atracHandle, SceUInt32 outputSamples) {
    TRACY_FUNC(sceAtracSetOutputSamples, atracHandle, outputSamples);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAtracSetSubBuffer, SceInt32 atracHandle, Ptr<SceUChar8> pSubBuffer, SceUInt32 subBufferSize) {
    TRACY_FUNC(sceAtracSetSubBuffer, atracHandle, pSubBuffer, subBufferSize);
    if (((pSubBuffer.address() & (SCE_ATRAC_ALIGNMENT_SIZE - 1)) != 0) || ((subBufferSize & (SCE_ATRAC_ALIGNMENT_SIZE - 1)) != 0)) {
        return RET_ERROR(SCE_ATRAC_ERROR_INVALID_ALIGNMENT);
    }

    return UNIMPLEMENTED();
}
