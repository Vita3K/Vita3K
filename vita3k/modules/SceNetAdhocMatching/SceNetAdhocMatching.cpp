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

#include <adhoc/matchingContext.h>
#include <adhoc/matchingTarget.h>
#include <adhoc/state.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceNetAdhocMatching);

EXPORT(int, sceNetAdhocMatchingAbortSendData, int id, SceNetInAddr *addr) {
    TRACY_FUNC(sceNetAdhocMatchingAbortSendData, id, addr);
    if (!emuenv.adhoc.is_initialized)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED);

    if (addr == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG);

    std::lock_guard guard(emuenv.adhoc.getMutex());
    SceNetAdhocMatchingContext *ctx = emuenv.adhoc.findMatchingContextById(id);

    if (ctx == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID);

    if (ctx->getStatus() != SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_RUNNING)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING);

    auto *target = ctx->findTargetByAddr(*addr);

    if (target == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_UNKNOWN_TARGET);

    ctx->abortSendData(*target);

    return SCE_NET_ADHOC_MATCHING_OK;
}

EXPORT(int, sceNetAdhocMatchingCancelTargetWithOpt, int id, SceNetInAddr *addr, int optLen, char *opt) {
    TRACY_FUNC(sceNetAdhocMatchingCancelTargetWithOpt, id, addr, optLen, opt);
    if (!emuenv.adhoc.is_initialized)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED);

    if (addr == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG);

    std::lock_guard guard(emuenv.adhoc.getMutex());
    SceNetAdhocMatchingContext *ctx = emuenv.adhoc.findMatchingContextById(id);

    if (ctx == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID);

    if (ctx->getStatus() != SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_RUNNING)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING);

    auto *foundTarget = ctx->findTargetByAddr(*addr);
    if (foundTarget == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_UNKNOWN_TARGET);

    if (optLen < 0 || SCE_NET_ADHOC_MATCHING_MAXOPTLEN < optLen)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN);

    if (0 < optLen && opt == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN);

    const int result = ctx->cancelTargetWithOpt(emuenv, thread_id, *foundTarget, optLen, opt);
    if (result < SCE_NET_ADHOC_MATCHING_OK)
        return RET_ERROR(result);

    return SCE_NET_ADHOC_MATCHING_OK;
}

EXPORT(int, sceNetAdhocMatchingCancelTarget, int id, SceNetInAddr *addr) {
    TRACY_FUNC(sceNetAdhocMatchingCancelTarget, id, addr);
    return CALL_EXPORT(sceNetAdhocMatchingCancelTargetWithOpt, id, addr, 0, nullptr);
}

EXPORT(int, sceNetAdhocMatchingCreate, SceNetAdhocMatchingMode mode, int maxnum, SceUShort16 port, int rxbuflen, unsigned int helloInterval, unsigned int keepaliveInterval, int retryCount, unsigned int rexmtInterval, Ptr<void> handlerAddr) {
    TRACY_FUNC(sceNetAdhocMatchingCreate, mode, maxnum, port, rxbuflen, helloInterval, keepaliveInterval, retryCount, rexmtInterval, handlerAddr);
    if (!emuenv.adhoc.is_initialized)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED);

    std::lock_guard guard(emuenv.adhoc.getMutex());

    if (mode < SCE_NET_ADHOC_MATCHING_MODE_PARENT || mode >= SCE_NET_ADHOC_MATCHING_MODE_MAX)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_MODE);

    if (maxnum < 2 || maxnum > 16)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_MAXNUM);

    if (rxbuflen < maxnum * 4 + 4U)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_RXBUF_TOO_SHORT);

    if ((mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT || mode == SCE_NET_ADHOC_MATCHING_MODE_P2P) && (helloInterval == 0 || keepaliveInterval == 0))
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG);

    if (retryCount < 0 || rexmtInterval == 0)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG);

    const auto id = emuenv.adhoc.createMatchingContext(port);

    if (id < SCE_NET_ADHOC_MATCHING_OK)
        return RET_ERROR(id);

    const auto ctx = emuenv.adhoc.findMatchingContextById(id);
    const int result = ctx->initialize(mode, maxnum, port, rxbuflen, helloInterval, keepaliveInterval, retryCount, rexmtInterval, handlerAddr);

    if (result < SCE_NET_ADHOC_MATCHING_OK) {
        emuenv.adhoc.deleteMatchingContext(ctx);
        return RET_ERROR(result);
    }

    return ctx->getId();
}

EXPORT(int, sceNetAdhocMatchingStop, int id) {
    TRACY_FUNC(sceNetAdhocMatchingStop, id);
    if (!emuenv.adhoc.is_initialized)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED);

    std::lock_guard guard(emuenv.adhoc.getMutex());
    SceNetAdhocMatchingContext *ctx = emuenv.adhoc.findMatchingContextById(id);

    if (ctx == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID);

    // The context is already stopping or its stopped already, nothing to do
    if (ctx->getStatus() != SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_RUNNING)
        return SCE_NET_ADHOC_MATCHING_OK;

    return ctx->stop(emuenv, thread_id);
}

EXPORT(int, sceNetAdhocMatchingDelete, int id) {
    TRACY_FUNC(sceNetAdhocMatchingDelete, id);
    if (!emuenv.adhoc.is_initialized)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED);

    std::lock_guard guard(emuenv.adhoc.getMutex());
    SceNetAdhocMatchingContext *ctx = emuenv.adhoc.findMatchingContextById(id);

    if (ctx == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID);

    if (ctx->getStatus() != SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_NOT_RUNNING)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_IS_RUNNING);

    emuenv.adhoc.deleteMatchingContext(ctx);

    return SCE_NET_ADHOC_MATCHING_OK;
}

EXPORT(int, sceNetAdhocMatchingGetHelloOpt, int id, SceSize *optlen, void *opt) {
    TRACY_FUNC(sceNetAdhocMatchingGetHelloOpt, id, optlen, opt);

    if (!emuenv.adhoc.is_initialized)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED);

    if (optlen == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG);

    std::lock_guard guard(emuenv.adhoc.getMutex());
    SceNetAdhocMatchingContext *ctx = emuenv.adhoc.findMatchingContextById(id);

    if (ctx == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID);

    if (ctx->getMode() == SCE_NET_ADHOC_MATCHING_MODE_CHILD)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_MODE);

    if (ctx->getStatus() != SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_RUNNING)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_NOT_RUNNING);

    return ctx->getHelloOpt(*optlen, opt);
}

EXPORT(int, sceNetAdhocMatchingGetMembers, int id, unsigned int *membersCount, SceNetAdhocMatchingMember *members) {
    TRACY_FUNC(sceNetAdhocMatchingGetMembers, id, membersCount, members);
    if (!emuenv.adhoc.is_initialized)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED);

    if (membersCount == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG);

    std::lock_guard guard(emuenv.adhoc.getMutex());
    SceNetAdhocMatchingContext *ctx = emuenv.adhoc.findMatchingContextById(id);

    if (ctx == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID);

    if (ctx->getStatus() != SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_RUNNING)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING);

    return ctx->getMembers(*membersCount, members);
}

EXPORT(int, sceNetAdhocMatchingSelectTarget, int id, SceNetInAddr *addr, int optLen, char *opt) {
    TRACY_FUNC(sceNetAdhocMatchingSelectTarget, id, addr, optLen, opt);
    if (!emuenv.adhoc.is_initialized)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED);

    if (addr == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG);

    std::lock_guard guard(emuenv.adhoc.getMutex());
    SceNetAdhocMatchingContext *ctx = emuenv.adhoc.findMatchingContextById(id);

    if (ctx == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID);

    if (ctx->getStatus() != SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_RUNNING)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING);

    auto foundTarget = ctx->findTargetByAddr(*addr);
    if (foundTarget == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_UNKNOWN_TARGET);

    if (optLen < 0 || SCE_NET_ADHOC_MATCHING_MAXOPTLEN < optLen)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN);

    if (0 < optLen && opt == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN);

    const int result = ctx->selectTarget(emuenv, thread_id, *foundTarget, optLen, opt);
    if (result < SCE_NET_ADHOC_MATCHING_OK)
        return RET_ERROR(result);

    return SCE_NET_ADHOC_MATCHING_OK;
}

EXPORT(int, sceNetAdhocMatchingSendData, int id, SceNetInAddr *addr, int dataLen, char *data) {
    TRACY_FUNC(sceNetAdhocMatchingSendData, id, addr, dataLen, data);
    if (!emuenv.adhoc.is_initialized)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED);

    if (addr == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG);

    std::lock_guard guard(emuenv.adhoc.getMutex());
    SceNetAdhocMatchingContext *ctx = emuenv.adhoc.findMatchingContextById(id);

    if (ctx == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID);

    auto *target = ctx->findTargetByAddr(*addr);

    if (target == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_UNKNOWN_TARGET);

    if (dataLen < 1 || dataLen > SCE_NET_ADHOC_MATCHING_MAXDATALEN)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_DATALEN);

    if (data == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG);

    if (target->status != SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_ESTABLISHED);

    if (target->sendDataStatus != SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_READY)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_DATA_BUSY);

    const int result = ctx->sendData(emuenv, thread_id, *target, dataLen, data);
    if (result < SCE_NET_ADHOC_MATCHING_OK)
        return RET_ERROR(result);

    return SCE_NET_ADHOC_MATCHING_OK;
}

EXPORT(int, sceNetAdhocMatchingSetHelloOpt, int id, int optlen, void *opt) {
    TRACY_FUNC(sceNetAdhocMatchingSetHelloOpt, id, optlen, opt);
    if (!emuenv.adhoc.is_initialized)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED);

    std::lock_guard guard(emuenv.adhoc.getMutex());
    SceNetAdhocMatchingContext *ctx = emuenv.adhoc.findMatchingContextById(id);

    if (ctx == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID);

    if (ctx->getMode() == SCE_NET_ADHOC_MATCHING_MODE_CHILD)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_MODE);

    if (ctx->getStatus() != SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_RUNNING)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING);

    if (optlen < 0 || SCE_NET_ADHOC_MATCHING_MAXHELLOOPTLEN < optlen)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN);

    if (0 < optlen && opt == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN);

    return ctx->setHelloOpt(optlen, opt);
}

EXPORT(int, sceNetAdhocMatchingStart, int id, int threadPriority, int threadStackSize, int threadCpuAffinityMask, int helloOptlen, char *helloOpt) {
    TRACY_FUNC(sceNetAdhocMatchingStart, id, threadPriority, threadStackSize, threadCpuAffinityMask, helloOptlen, helloOpt);
    if (!emuenv.adhoc.is_initialized)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED);

    // TODO: Define MAXOPTLEN MAXDATALEN MAXHELLOOPTLEN size limits based on sdk version

    std::lock_guard guard(emuenv.adhoc.getMutex());
    SceNetAdhocMatchingContext *ctx = emuenv.adhoc.findMatchingContextById(id);

    if (ctx == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID);

    if (ctx->getStatus() != SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_NOT_RUNNING)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_IS_RUNNING);

    if (helloOptlen < 0 || helloOptlen > SCE_NET_ADHOC_MATCHING_MAXHELLOOPTLEN)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN);

    if (helloOptlen > 0 && helloOpt == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN);

    if (threadPriority == 0)
        threadPriority = 0x10000100;
    if (threadStackSize == 0)
        threadPriority = 0x4000;

    const int result = ctx->start(emuenv, thread_id, threadPriority, threadStackSize, threadCpuAffinityMask, helloOptlen, helloOpt);
    if (result < SCE_NET_ADHOC_MATCHING_OK)
        return RET_ERROR(result);

    return SCE_NET_ADHOC_MATCHING_OK;
}

EXPORT(int, sceNetAdhocMatchingInit, SceSize poolsize, void *poolptr) {
    TRACY_FUNC(sceNetAdhocMatchingInit, poolsize, poolptr);
    if (emuenv.adhoc.is_initialized)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_ALREADY_INITIALIZED);

    if (poolptr == nullptr)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG);

    int result = emuenv.adhoc.initializeMutex();
    if (result != SCE_NET_ADHOC_MATCHING_OK)
        return RET_ERROR(result);

    if (poolsize == 0) {
        poolsize = 0x20000;
    }

    result = emuenv.adhoc.createMSpace(poolsize, poolptr);
    if (result != SCE_NET_ADHOC_MATCHING_OK) {
        emuenv.adhoc.deleteMutex();
        return RET_ERROR(result);
    }

    result = emuenv.adhoc.initializeMatchingContextList();
    if (result != SCE_NET_ADHOC_MATCHING_OK) {
        emuenv.adhoc.deleteMSpace();
        emuenv.adhoc.deleteMutex();
        return RET_ERROR(result);
    }

    emuenv.adhoc.is_initialized = true;

    return SCE_NET_ADHOC_MATCHING_OK;
}

EXPORT(int, sceNetAdhocMatchingTerm) {
    TRACY_FUNC(sceNetAdhocMatchingTerm);
    if (!emuenv.adhoc.is_initialized)
        return RET_ERROR(SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED);

    for (int i = 0; i < SCE_NET_ADHOC_MATCHING_MAXNUM; i++) {
        // This call will use the mutex
        CALL_EXPORT(sceNetAdhocMatchingStop, i);

        // We aren't guarded by a mutex. We need to check every iteration
        if (!emuenv.adhoc.is_initialized)
            continue;

        std::lock_guard guard(emuenv.adhoc.getMutex());
        auto ctx = emuenv.adhoc.findMatchingContextById(i);
        if (ctx == nullptr)
            continue;
        if (ctx->getStatus() != SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_NOT_RUNNING)
            continue;

        emuenv.adhoc.deleteMatchingContext(ctx);
    }

    int result = emuenv.adhoc.isAnyMatchingContextRunning();
    if (result != SCE_NET_ADHOC_MATCHING_OK) {
        return RET_ERROR(result);
    }

    emuenv.adhoc.deleteAllMatchingContext();
    emuenv.adhoc.deleteMSpace();
    emuenv.adhoc.deleteMutex();
    emuenv.adhoc.is_initialized = false;
    return SCE_NET_ADHOC_MATCHING_OK;
}
