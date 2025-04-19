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

#include "adhoc/callbacks.h"
#include "adhoc/matchingContext.h"
#include "adhoc/matchingTarget.h"
#include "adhoc/state.h"
#include "adhoc/threads.h"

int helloTimeoutCallback(void *args) {
    SceNetAdhocMatchingContext *ctx = (SceNetAdhocMatchingContext *)args;

    if (!ctx->helloPipeMsg.isSheduled) {
        ctx->helloPipeMsg.target = 0;
        ctx->helloPipeMsg.isSheduled = true;
        ctx->helloPipeMsg.type = SCE_NET_ADHOC_MATCHING_EVENT_HELLO_TIMEOUT;
        write(ctx->getWritePipeUid(), &ctx->helloPipeMsg, sizeof(SceNetAdhocMatchingPipeMessage));
    }
    ctx->shouldHelloReqBeProcessed = false;
    return SCE_NET_ADHOC_MATCHING_OK;
}

int registerTargetTimeoutCallback(void *args) {
    SceNetAdhocMatchingTarget *target = (SceNetAdhocMatchingTarget *)args;

    if (!target->targetTimeout.message.isSheduled) {
        target->targetTimeout.message = {
            .type = SCE_NET_ADHOC_MATCHING_EVENT_REGISTRATION_TIMEOUT,
            .target = target,
            .isSheduled = true,
        };
        write(target->getWritePipeUid(), &target->targetTimeout.message, sizeof(SceNetAdhocMatchingPipeMessage));
    }
    target->targetTimeout.isAckPending = false;
    return SCE_NET_ADHOC_MATCHING_OK;
}

int targetTimeoutCallback(void *args) {
    SceNetAdhocMatchingTarget *target = (SceNetAdhocMatchingTarget *)args;

    if (!target->targetTimeout.message.isSheduled) {
        target->targetTimeout.message = {
            .type = SCE_NET_ADHOC_MATCHING_EVENT_TARGET_TIMEOUT,
            .target = target,
            .isSheduled = true,
        };
        write(target->getWritePipeUid(), &target->targetTimeout.message, sizeof(SceNetAdhocMatchingPipeMessage));
    }
    target->targetTimeout.isAckPending = false;
    return SCE_NET_ADHOC_MATCHING_OK;
}

int sendDataTimeoutCallback(void *args) {
    SceNetAdhocMatchingTarget *target = (SceNetAdhocMatchingTarget *)args;

    if (!target->sendDataTimeout.message.isSheduled) {
        target->sendDataTimeout.message = {
            .type = SCE_NET_ADHOC_MATCHING_EVENT_DATA_TIMEOUT,
            .target = target,
            .isSheduled = true,
        };
        write(target->getWritePipeUid(), &target->sendDataTimeout.message, sizeof(SceNetAdhocMatchingPipeMessage));
    }
    target->sendDataTimeout.isAckPending = false;
    return SCE_NET_ADHOC_MATCHING_OK;
}
