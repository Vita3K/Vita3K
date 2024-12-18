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

#include <cstring>
#include <thread>

#include "adhoc/callbacks.h"
#include "adhoc/matchingContext.h"
#include "adhoc/matchingTarget.h"
#include "adhoc/state.h"
#include "emuenv/state.h"
#include "kernel/state.h"
#include "net/types.h"
#include "util/lock_and_find.h"
#include "util/types.h"

#include "../SceNet/SceNet.h"

SceNetAdhocMatchingContext::~SceNetAdhocMatchingContext() {
    finalize();
}

int SceNetAdhocMatchingContext::initialize(SceNetAdhocMatchingMode mode, int maxnum, SceUShort16 port, int rxbuflen, unsigned int helloInterval, unsigned int keepaliveInterval, int retryCount, unsigned int rexmtInterval, Ptr<void> handlerAddr) {
    this->mode = mode;
    shouldExit = true;

    // Children have 2 peers max (parent and itself)
    this->maxnum = 2;
    if (this->mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT)
        this->maxnum = maxnum;

    this->port = port;
    this->rxbuflen = rxbuflen;
    this->rxbuf = new char[this->rxbuflen]; // Reserve space in adhoc

    if (this->rxbuf == nullptr) {
        return SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;
    }

    this->helloInterval = helloInterval;
    this->keepAliveInterval = keepaliveInterval;
    this->retryCount = retryCount;
    this->rexmtInterval = rexmtInterval;

    this->shouldHelloReqBeProcessed = false;
    this->helloOptionFlag = 1;
    this->targetList = nullptr;

    this->handler = handlerAddr.address();
    return SCE_NET_ADHOC_MATCHING_OK;
}

void SceNetAdhocMatchingContext::finalize() {
    shouldExit = true;
    auto *target = this->targetList;
    while (target != nullptr) {
        deleteAllTimedFunctions(*target);
        target->deleteOptMessage();
        target->deleteSendData();
        target->deleteRawPacket();
        auto *nextTarget = target->next;
        delete target;
        target = nextTarget;
    }
    this->targetList = nullptr;
    this->calloutSyncing.closeCalloutThread();
    closeEventHandler();
    if (isInputThreadInitialized && this->inputThread.joinable())
        this->inputThread.join();

    if (rxbuf != nullptr) {
        delete rxbuf;
    }
    if (helloMsg != nullptr) {
        delete helloMsg;
    }
    if (memberMsg != nullptr) {
        delete memberMsg;
    }
}

int SceNetAdhocMatchingContext::start(EmuEnvState &emuenv, SceUID thread_id, int threadPriority, int threadStackSize, int threadCpuAffinityMask, SceSize helloOptlen, char *helloOpt) {
    shouldExit = false;
    int result = initializeSendSocket(emuenv, thread_id);
    if (result != SCE_NET_ADHOC_MATCHING_OK)
        return result;

    result = initializeEventHandler(emuenv, thread_id, threadPriority, threadStackSize, threadCpuAffinityMask);
    if (result != SCE_NET_ADHOC_MATCHING_OK) {
        closeSendSocket(emuenv, thread_id);
        return result;
    }

    result = initializeInputThread(emuenv, thread_id, threadPriority, 0x1000, threadCpuAffinityMask);
    if (result != SCE_NET_ADHOC_MATCHING_OK) {
        closeEventHandler();
        closeSendSocket(emuenv, thread_id);
        return result;
    }

    result = getCalloutSyncing().initializeCalloutThread(emuenv, thread_id, getId(), threadPriority, 0x1000, threadCpuAffinityMask);
    if (result != SCE_NET_ADHOC_MATCHING_OK) {
        closeEventHandler();
        closeInputThread(emuenv, thread_id);
        closeSendSocket(emuenv, thread_id);
        return result;
    }

    if (getMode() == SCE_NET_ADHOC_MATCHING_MODE_PARENT || getMode() == SCE_NET_ADHOC_MATCHING_MODE_P2P) {
        result = setHelloOpt(helloOptlen, helloOpt);
        if (result != SCE_NET_ADHOC_MATCHING_OK) {
            getCalloutSyncing().closeCalloutThread();
            closeEventHandler();
            closeInputThread(emuenv, thread_id);
            closeSendSocket(emuenv, thread_id);
            return result;
        }

        addHelloTimedFunct(helloInterval);
    }

    createMembersList();

    status = SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_RUNNING;
    return SCE_NET_ADHOC_MATCHING_OK;
}

int SceNetAdhocMatchingContext::stop(EmuEnvState &emuenv, SceUID thread_id) {
    status = SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_STOPPING;

    // These 3 may take time because they wait for both threads to end
    calloutSyncing.closeCalloutThread();
    closeEventHandler();
    closeInputThread(emuenv, thread_id);

    emuenv.kernel.get_thread(input_thread_id)->exit_delete();
    emuenv.kernel.get_thread(event_thread_id)->exit_delete();

    if (mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT || mode == SCE_NET_ADHOC_MATCHING_MODE_P2P) {
        deleteHelloTimedFunction();
        deleteHelloMessage();
    }

    deleteAllTargets(emuenv, thread_id);
    deleteMemberList();
    closeSendSocket(emuenv, thread_id);

    status = SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_NOT_RUNNING;
    return SCE_NET_ADHOC_MATCHING_OK;
}

int SceNetAdhocMatchingContext::initializeInputThread(EmuEnvState &emuenv, SceUID thread_id, int threadPriority, int threadStackSize, int threadCpuAffinityMask) {
    int socket_uid = CALL_EXPORT(sceNetSocket, "SceNetAdhocMatchingRecv", AF_INET, SCE_NET_SOCK_DGRAM_P2P, SCE_NET_IPPROTO_IP);
    if (socket_uid < SCE_NET_ADHOC_MATCHING_OK)
        return socket_uid;

    this->recvSocket = socket_uid;

    const int flag = 1;
    int result = CALL_EXPORT(sceNetSetsockopt, this->recvSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_REUSEADDR, (const char *)&flag, sizeof(flag));
    if (result < SCE_NET_ADHOC_MATCHING_OK) {
        CALL_EXPORT(sceNetSocketClose, this->recvSocket);
        return result;
    }

    SceNetSockaddrIn recv_addr = {
        .sin_len = sizeof(SceNetSockaddrIn),
        .sin_family = AF_INET,
        .sin_port = htons(SCE_NET_ADHOC_DEFAULT_PORT),
        .sin_addr = htonl(INADDR_ANY),
        .sin_vport = htons(port),
    };

    auto bindResult = CALL_EXPORT(sceNetBind, this->recvSocket, (SceNetSockaddr *)&recv_addr, sizeof(SceNetSockaddrIn));

    if (bindResult < 0) {
        CALL_EXPORT(sceNetSocketClose, this->recvSocket);
        return bindResult;
    }

    const ThreadStatePtr input_thread = emuenv.kernel.create_thread(emuenv.mem, "SceAdhocMatchingInputThread", Ptr<void>(0), SCE_KERNEL_HIGHEST_PRIORITY_USER, SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT, SCE_KERNEL_STACK_SIZE_USER_DEFAULT, nullptr);
    this->input_thread_id = input_thread->id;

    this->inputThread = std::thread(adhocMatchingInputThread, std::ref(emuenv), this->input_thread_id, this->id);
    isInputThreadInitialized = true;
    return SCE_NET_ADHOC_MATCHING_OK;
}

void SceNetAdhocMatchingContext::closeInputThread(EmuEnvState &emuenv, SceUID thread_id) {
    if (!isInputThreadInitialized)
        return;

    shouldExit = true;
    CALL_EXPORT(sceNetSocketAbort, this->recvSocket);

    if (this->inputThread.joinable())
        this->inputThread.join();

    CALL_EXPORT(sceNetShutdown, this->recvSocket, 0);
    CALL_EXPORT(sceNetSocketClose, this->recvSocket);
    this->recvSocket = 0;
    isInputThreadInitialized = false;
}

int SceNetAdhocMatchingContext::initializeSendSocket(EmuEnvState &emuenv, SceUID thread_id) {
    SceNetInAddr ownAddr;
    CALL_EXPORT(sceNetCtlAdhocGetInAddr, &ownAddr);
    this->ownAddress = ownAddr.s_addr;

    int socket_uid = CALL_EXPORT(sceNetSocket, "SceNetAdhocMatchingSend", AF_INET, SCE_NET_SOCK_DGRAM_P2P, SCE_NET_IPPROTO_IP);
    if (socket_uid < SCE_NET_ADHOC_MATCHING_OK)
        return socket_uid;

    this->sendSocket = socket_uid;

    int result = SCE_NET_ADHOC_MATCHING_OK;
    int portOffset = this->mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT ? 1 : 2;
    do {
        SceNetSockaddrIn addr = {
            .sin_len = sizeof(SceNetSockaddrIn),
            .sin_family = AF_INET,
            .sin_port = htons(SCE_NET_ADHOC_DEFAULT_PORT),
            .sin_vport = htons(this->port + portOffset),
        };
        this->ownPort = htons(SCE_NET_ADHOC_DEFAULT_PORT) + htons(this->port + portOffset);
        int result = CALL_EXPORT(sceNetBind, this->sendSocket, (SceNetSockaddr *)&addr, sizeof(SceNetSockaddrIn));
        portOffset++;
    } while (result == SCE_NET_ERROR_EADDRINUSE && portOffset < 20);

    if (result < SCE_NET_ADHOC_MATCHING_OK) {
        CALL_EXPORT(sceNetShutdown, this->sendSocket, 0);
        CALL_EXPORT(sceNetSocketClose, this->sendSocket);
        return result;
    }

    const int flag = 1;
    result = CALL_EXPORT(sceNetSetsockopt, this->sendSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST, (const char *)&flag, sizeof(flag));
    if (result < SCE_NET_ADHOC_MATCHING_OK) {
        CALL_EXPORT(sceNetShutdown, this->sendSocket, 0);
        CALL_EXPORT(sceNetSocketClose, this->sendSocket);
        return result;
    }

    return SCE_NET_ADHOC_MATCHING_OK;
}

int SceNetAdhocMatchingContext::initializeEventHandler(EmuEnvState &emuenv, SceUID thread_id, int threadPriority, int threadStackSize, int threadCpuAffinityMask) {
#ifdef _WIN32
    auto pipeResult = _pipe(this->msgPipeUid, 0x1000, 0);
#else
    auto pipeResult = pipe(this->msgPipeUid);
#endif
    if (pipeResult < SCE_NET_ADHOC_MATCHING_OK) {
        return pipeResult;
    }
    const ThreadStatePtr event_thread = emuenv.kernel.create_thread(emuenv.mem, "SceAdhocMatchingEventThread", Ptr<void>(0), SCE_KERNEL_HIGHEST_PRIORITY_USER, SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT, SCE_KERNEL_STACK_SIZE_USER_DEFAULT, nullptr);
    this->event_thread_id = event_thread->id;

    this->eventThread = std::thread(adhocMatchingEventThread, std::ref(emuenv), this->event_thread_id, this->id);
    isEventThreadInitialized = true;

    return SCE_NET_ADHOC_MATCHING_OK;
}

void SceNetAdhocMatchingContext::closeEventHandler() {
    if (!isEventThreadInitialized)
        return;

    SceNetAdhocMatchingPipeMessage abortMsg{
        .type = SCE_NET_ADHOC_MATCHING_EVENT_ABORT,
    };
    write(getWritePipeUid(), &abortMsg, sizeof(SceNetAdhocMatchingPipeMessage));

    if (this->eventThread.joinable())
        this->eventThread.join();

    close(getWritePipeUid());
    close(getReadPipeUid());
    isEventThreadInitialized = false;
}

void SceNetAdhocMatchingContext::closeSendSocket(EmuEnvState &emuenv, SceUID thread_id) {
    CALL_EXPORT(sceNetSocketAbort, this->sendSocket);
    CALL_EXPORT(sceNetShutdown, this->sendSocket, 0);
    CALL_EXPORT(sceNetSocketClose, this->sendSocket);
}

SceNetAdhocMatchingContext *SceNetAdhocMatchingContext::getNext() {
    return this->next;
}

void SceNetAdhocMatchingContext::setNext(SceNetAdhocMatchingContext *next_context) {
    this->next = next_context;
}

SceUID SceNetAdhocMatchingContext::getId() const {
    return this->id;
}

void SceNetAdhocMatchingContext::setId(SceUID id) {
    this->id = id;
}

bool SceNetAdhocMatchingContext::isRunning() const {
    return !this->shouldExit;
}

SceUShort16 SceNetAdhocMatchingContext::getPort() const {
    return this->port;
}

SceNetAdhocMatchingContextStatus SceNetAdhocMatchingContext::getStatus() const {
    return this->status;
}

SceNetAdhocMatchingMode SceNetAdhocMatchingContext::getMode() const {
    return this->mode;
}

SceNetAdhocMatchingCalloutSyncing &SceNetAdhocMatchingContext::getCalloutSyncing() {
    return this->calloutSyncing;
}

int SceNetAdhocMatchingContext::getReadPipeUid() const {
    return this->msgPipeUid[0];
}

int SceNetAdhocMatchingContext::getWritePipeUid() const {
    return this->msgPipeUid[1];
}

void SceNetAdhocMatchingContext::processPacketFromTarget(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target) {
    SceNetAdhocMatchingMessageHeader packet{};
    const auto rawPacket = target.getRawPacket();
    packet.parse(rawPacket.data(), rawPacket.size());

    switch (this->mode) {
    case SCE_NET_ADHOC_MATCHING_MODE_PARENT:
        if (packet.type == SCE_NET_ADHOC_MATCHING_PACKET_TYPE_HELLO)
            return;
        if (packet.type == SCE_NET_ADHOC_MATCHING_PACKET_TYPE_MEMBER_LIST)
            return;
        break;
    case SCE_NET_ADHOC_MATCHING_MODE_CHILD:
        if (packet.type == SCE_NET_ADHOC_MATCHING_PACKET_TYPE_HELLO_ACK)
            return;
        if (packet.type == SCE_NET_ADHOC_MATCHING_PACKET_TYPE_MEMBER_LIST_ACK)
            return;
        break;
    case SCE_NET_ADHOC_MATCHING_MODE_P2P:
        if (packet.type == SCE_NET_ADHOC_MATCHING_PACKET_TYPE_MEMBER_LIST && isTargetAddressHigher(target))
            return;
        if (packet.type == SCE_NET_ADHOC_MATCHING_PACKET_TYPE_MEMBER_LIST_ACK)
            return;
        break;
    }

    int count = 0;
    if ((packet.type == SCE_NET_ADHOC_MATCHING_PACKET_TYPE_HELLO_ACK || packet.type == SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK3) && rawPacket.size() - target.getPacketLen() > 15) {
        memcpy(&count, rawPacket.data() + target.getPacketLen(), sizeof(count));
        count = ntohl(count);
        if (count != target.uid) {
            switch (target.status) {
            case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED:
                break;
            case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_2:
            case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES:
                setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
                deleteAllTimedFunctions(target);
                notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_CANCEL, target.addr);
                break;
            case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED:
                setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
                deleteAllTimedFunctions(target);
                notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_LEAVE, target.addr);
                break;
            }
        }
    }

    switch (packet.type) {
    case SCE_NET_ADHOC_MATCHING_PACKET_TYPE_HELLO:
        processHelloPacket(target);
        break;
    case SCE_NET_ADHOC_MATCHING_PACKET_TYPE_HELLO_ACK:
        processHelloAckPacket(emuenv, thread_id, target);
        break;
    case SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK3:
        switch (target.status) {
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED:
            sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL, target.getOpt());
            break;
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_2:
            setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
            deleteAllTimedFunctions(target);
            sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL, {});
            notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ERROR, target.addr);
            break;
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES:
            setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED);
            sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK4, {});
            addTargetTimeout(target);
            target.targetTimeout.retryCount = retryCount;
            notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ESTABLISHED, target.addr);
            break;
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES2:
            setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED);
            target.uid = count;
            sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK4, {});
            addTargetTimeout(target);
            target.targetTimeout.retryCount = retryCount;
            if (target.getPacketLen() - 4 < 1) {
                notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ACCEPT, target.addr);
            } else {
                notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ACCEPT, target.addr, target.getPacketLen() - 4, rawPacket.data() + 4);
            }
            notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ESTABLISHED, target.addr);
            break;
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED:
            sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK4, {});
            break;
        }
        break;
    case SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK4:
        switch (target.status) {
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED:
            sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL, target.getOpt());
            break;
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_2:
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES:
            setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
            deleteAllTimedFunctions(target);
            sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL, {});
            notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ERROR, target.addr);
            break;
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES2:
            setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED);
            addTargetTimeout(target);
            target.targetTimeout.retryCount = retryCount;
            notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ESTABLISHED, target.addr);
            break;
        }
        break;
    case SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL:
        switch (target.status) {
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED:
            break;
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_2:
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES:
            setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
            deleteAllTimedFunctions(target);
            if (target.getPacketLen() - 4 < 1) {
                notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_CANCEL, target.addr);
            } else {
                notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_CANCEL, target.addr, target.getPacketLen() - 4, rawPacket.data() + 4);
            }
            break;
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES2:
            setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
            deleteAllTimedFunctions(target);
            if (target.getPacketLen() - 4 < 1) {
                notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_DENY, target.addr);
            } else {
                notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_DENY, target.addr, target.getPacketLen() - 4, rawPacket.data() + 4);
            }
            break;
        case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED:
            setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
            deleteAllTimedFunctions(target);
            if (target.getPacketLen() - 4 < 1) {
                notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_LEAVE, target.addr);
            } else {
                notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_LEAVE, target.addr, target.getPacketLen() - 4, rawPacket.data() + 4);
            }
            break;
        }
        break;
    case SCE_NET_ADHOC_MATCHING_PACKET_TYPE_MEMBER_LIST:
        processMemberListPacket(emuenv, thread_id, target);
        return;
    case SCE_NET_ADHOC_MATCHING_PACKET_TYPE_MEMBER_LIST_ACK:
        processMemberListAckPacket(emuenv, thread_id, target);
        return;
    case SCE_NET_ADHOC_MATCHING_PACKET_TYPE_BYE:
        processByePacket(target);
        return;
    case SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK9:
        processUnk9Packet(target);
        return;
    case SCE_NET_ADHOC_MATCHING_PACKET_TYPE_DATA:
        processDataPacket(emuenv, thread_id, target);
        return;
    case SCE_NET_ADHOC_MATCHING_PACKET_TYPE_DATA_ACK:
        processDataAckPacket(emuenv, thread_id, target);
        return;
    }
}

int SceNetAdhocMatchingContext::processHelloPacket(SceNetAdhocMatchingTarget &target) {
    const SceSize targetCount = this->countTargetsWithStatusOrBetter(SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES);


    SceNetAdhocMatchingHelloMessage helloPacket;
    const auto rawPacket = target.getRawPacket();
    helloPacket.parse(rawPacket.data(), rawPacket.size());

    if (helloPacket.header.packetLength <= 7)
        return SCE_NET_ADHOC_MATCHING_OK;

    if (target.status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED) {
        target.unk_0c = helloPacket.helloInterval;
        target.keepAliveInterval = helloPacket.rexmtInterval;

        if (rawPacket.size() - target.getPacketLen() > 0xf) {
            target.unk_50 = helloPacket.unk_6c;
        }
    }

    if (targetCount + 1 >= maxnum)
        return SCE_NET_ADHOC_MATCHING_OK;

    this->notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_HELLO, target.addr, helloPacket.optBuffer.size(), helloPacket.optBuffer.data());
    return SCE_NET_ADHOC_MATCHING_OK;
}

int SceNetAdhocMatchingContext::processHelloAckPacket(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target) {
    auto targetCount = this->countTargetsWithStatusOrBetter(SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES);

    SceNetAdhocMatchingOptMessage helloAckPacket;
    const auto rawPacket = target.getRawPacket();
    helloAckPacket.parse(rawPacket.data(), rawPacket.size());

    switch (target.status) {
    case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED: {
        if (targetCount + 1 < this->maxnum) {
            setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_2);
            target.uid = helloAckPacket.targetCount;
            sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK9, {});
            int data_size = target.getPacketLen() - 4;
            if (data_size < 1) {
                this->notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_REQUEST, target.addr);
            } else {
                this->notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_REQUEST, target.addr, data_size, rawPacket.data() + 0x4);
            }
        } else {
            sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL, {});
        }
        break;
    }
    case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_2:
        if (targetCount + 1 < this->maxnum) {
            sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK9, {});
        } else {
            setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
            deleteAllTimedFunctions(target);
            sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL, {});
            this->notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_CANCEL, target.addr);
        }
        break;
    case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES:
        sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK3, target.getOpt());
        addRegisterTargetTimeout(target);
        break;
    case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES2: {
        setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES);
        target.uid = helloAckPacket.targetCount;
        sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK3, target.getOpt());
        addRegisterTargetTimeout(target);
        int data_size = target.getPacketLen() - 4;
        if (data_size < 1) {
            this->notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ACCEPT, target.addr);
        } else {
            this->notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ACCEPT, target.addr, data_size, rawPacket.data() + 0x4);
        }
        break;
    }
    case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED:
        setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
        deleteAllTimedFunctions(target);
        sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL, {});
        this->notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ERROR, target.addr);
    }
    return SCE_NET_ADHOC_MATCHING_OK;
}

int SceNetAdhocMatchingContext::processMemberListPacket(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target) {
    if (target.status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED) {
        sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL, target.getOpt());
        return SCE_NET_ADHOC_MATCHING_OK;
    }

    if (target.status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_2 || target.status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES2) {
        setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
        deleteAllTimedFunctions(target);
        sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL, {});
        notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ERROR, target.addr);
        return SCE_NET_ADHOC_MATCHING_OK;
    }

    SceNetAdhocMatchingMemberMessage *message = new SceNetAdhocMatchingMemberMessage();
    if (message == nullptr)
        return SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;

    const auto rawPacket = target.getRawPacket();
    message->parse(rawPacket.data(), rawPacket.size());

    if (mode == SCE_NET_ADHOC_MATCHING_MODE_CHILD) {
        deleteMemberList();
        memberMsg = message;
    }

    if (target.status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES) {
        setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED);
        addTargetTimeout(target);
        notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ESTABLISHED, target.addr);
    }

    sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_MEMBER_LIST_ACK, {});
    target.targetTimeout.retryCount = retryCount;
    return SCE_NET_ADHOC_MATCHING_OK;
}

int SceNetAdhocMatchingContext::processMemberListAckPacket(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target) {
    if (target.status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED) {
        target.targetTimeout.retryCount = retryCount;
        return SCE_NET_ADHOC_MATCHING_OK;
    }

    if (target.status != SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED) {
        setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
        deleteAllTimedFunctions(target);
        notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ERROR, target.addr);
    }

    sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL, target.getOpt());
    return SCE_NET_ADHOC_MATCHING_OK;
}

int SceNetAdhocMatchingContext::processByePacket(SceNetAdhocMatchingTarget &target) {
    if (target.status != SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED) {
        setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
        deleteAllTimedFunctions(target);
    }

    notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_BYE, target.addr);
    target.deleteTarget = true;

    return SCE_NET_ADHOC_MATCHING_OK;
}

int SceNetAdhocMatchingContext::processUnk9Packet(SceNetAdhocMatchingTarget &target) {
    if (target.status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES2)
        target.retryCount = retryCount;
    if (target.status != SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED)
        return SCE_NET_ADHOC_MATCHING_OK;

    SceNetAdhocMatchingDataMessage dataPacket;
    const auto rawPacket = target.getRawPacket();
    dataPacket.parse(rawPacket.data(), rawPacket.size());

    if (dataPacket.targetId != target.uid)
        return SCE_NET_ADHOC_MATCHING_OK;
    if (target.sendDataStatus != SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_BUSY)
        return SCE_NET_ADHOC_MATCHING_OK;
    if (dataPacket.packetId != target.sendDataCount)
        return SCE_NET_ADHOC_MATCHING_OK;

    target.setSendDataStatus(SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_READY);
    notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_DATA_ACK, target.addr);
    return SCE_NET_ADHOC_MATCHING_OK;
}

int SceNetAdhocMatchingContext::processDataPacket(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target) {
    if (target.status != SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED)
        return SCE_NET_ADHOC_MATCHING_OK;

    SceNetAdhocMatchingDataMessage dataPacket;
    const auto rawPacket = target.getRawPacket();
    dataPacket.parse(rawPacket.data(), rawPacket.size());

    if (dataPacket.targetId != target.uid)
        return SCE_NET_ADHOC_MATCHING_OK;
    if (target.recvDataCount <= dataPacket.packetId) {
        target.recvDataCount = dataPacket.packetId + 1;
        if (target.getPacketLen() - 0xc < 1) {
            notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_DATA, target.addr);
        } else {
            notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_DATA, target.addr, dataPacket.header.packetLength - 0x8, dataPacket.dataBuffer.data());
        }
    }
    sendDataMessageToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_DATA_ACK, {});

    return SCE_NET_ADHOC_MATCHING_OK;
}

int SceNetAdhocMatchingContext::processDataAckPacket(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target) {
    if (target.status != SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED)
        return SCE_NET_ADHOC_MATCHING_OK;

    SceNetAdhocMatchingDataMessage dataPacket;
    const auto rawPacket = target.getRawPacket();
    dataPacket.parse(rawPacket.data(), rawPacket.size());

    if (dataPacket.targetId != target.uid)
        return SCE_NET_ADHOC_MATCHING_OK;
    if (target.sendDataStatus != SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_BUSY)
        return SCE_NET_ADHOC_MATCHING_OK;
    if (dataPacket.packetId != target.sendDataCount)
        return SCE_NET_ADHOC_MATCHING_OK;

    target.setSendDataStatus(SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_READY);
    notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_DATA_ACK, target.addr);

    return SCE_NET_ADHOC_MATCHING_OK;
}

void SceNetAdhocMatchingContext::abortSendData(SceNetAdhocMatchingTarget &target) {
    deleteSendDataTimeout(target);
    target.setSendDataStatus(SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_READY);
}

int SceNetAdhocMatchingContext::cancelTargetWithOpt(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target, SceSize optLen, char *opt) {
    if (target.status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED) {
        return SCE_NET_ADHOC_MATCHING_OK;
    }

    deleteAllTimedFunctions(target);
    std::span<const char> optMsg(opt, optLen);
    sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL, optMsg);
    setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);

    if (target.setOptMessage(optLen, opt) < SCE_NET_ADHOC_MATCHING_OK)
        return SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;

    return SCE_NET_ADHOC_MATCHING_OK;
}

int SceNetAdhocMatchingContext::selectTarget(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target, SceSize optLen, char *opt) {
    auto membersCount = countTargetsWithStatusOrBetter(SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES);
    switch (target.status) {
    case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED:
        if (mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT)
            return SCE_NET_ADHOC_MATCHING_ERROR_TARGET_NOT_READY;
        if (membersCount + 1 >= maxnum)
            return SCE_NET_ADHOC_MATCHING_ERROR_EXCEED_MAXNUM;
        if (target.setOptMessage(optLen, opt) < SCE_NET_ADHOC_MATCHING_OK)
            return SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;
        target.targetCount++;
        if (target.targetCount == 0)
            target.targetCount = 1;

        sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_HELLO_ACK, target.getOpt());
        addRegisterTargetTimeout(target);
        setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES2);
        target.retryCount = retryCount;
        break;
    case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_2:
        if (membersCount + 1 >= maxnum)
            return SCE_NET_ADHOC_MATCHING_ERROR_EXCEED_MAXNUM;
        if (target.setOptMessage(optLen, opt) < SCE_NET_ADHOC_MATCHING_OK)
            return SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;
        target.targetCount++;
        if (target.targetCount == 0)
            target.targetCount = 1;

        sendOptDataToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK3, target.getOpt());
        addRegisterTargetTimeout(target);
        setTargetStatus(target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES2);
        target.retryCount = retryCount;
        break;
    case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES:
    case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES2:
        return SCE_NET_ADHOC_MATCHING_ERROR_REQUEST_IN_PROGRESS;
    case SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED:
        return SCE_NET_ADHOC_MATCHING_ERROR_ALREADY_ESTABLISHED;
    }

    return SCE_NET_ADHOC_MATCHING_OK;
}

int SceNetAdhocMatchingContext::sendData(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target, SceSize dataLen, char *data) {
    target.setSendData(dataLen, data);
    target.sendDataCount++;
    sendDataMessageToTarget(emuenv, thread_id, target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_DATA, target.getSendData());
    addSendDataTimeout(target);
    target.setSendDataStatus(SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_BUSY);
    return SCE_NET_ADHOC_MATCHING_OK;
}

void SceNetAdhocMatchingContext::handleEventMessage(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget *target) {
    target->incomingPacketMessage.isSheduled = false;
    if (target == nullptr)
        return;

    processPacketFromTarget(emuenv, thread_id, *target);
    target->deleteRawPacket();
}

void SceNetAdhocMatchingContext::handleEventRegistrationTimeout(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget *target) {
    target->targetTimeout.message.isSheduled = false;
    if (target == nullptr)
        return;

    if (target->status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES2) {
        if (target->unk_50 || target->retryCount-- > 0) {
            sendOptDataToTarget(emuenv, thread_id, *target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_HELLO_ACK, target->getOpt());
            addRegisterTargetTimeout(*target);
            return;
        }
        setTargetStatus(*target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
        sendOptDataToTarget(emuenv, thread_id, *target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL,{});
        notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_TIMEOUT, target->addr);
    }

    if (target->status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES) {
        if (target->retryCount-- > 0) {
            sendOptDataToTarget(emuenv, thread_id, *target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK3, target->getOpt());
            addRegisterTargetTimeout(*target);
            return;
        }
        setTargetStatus(*target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
        sendOptDataToTarget(emuenv, thread_id, *target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL, {});
        notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_TIMEOUT, target->addr);
    }
}

void SceNetAdhocMatchingContext::handleEventTargetTimeout(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget *target) {
    target->targetTimeout.message.isSheduled = false;
    if (target == nullptr)
        return;

    if (target->status != SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED) {
        return;
    }

    if (mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT || (mode == SCE_NET_ADHOC_MATCHING_MODE_P2P && isTargetAddressHigher(*target))) {
        sendMemberListToTarget(emuenv, thread_id, *target);
    }

    if (target->targetTimeout.retryCount-- > 0) {
        addTargetTimeout(*target);
        return;
    }

    setTargetStatus(*target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
    sendOptDataToTarget(emuenv, thread_id, *target, SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL, {});
    notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_TIMEOUT, target->addr);
}

void SceNetAdhocMatchingContext::handleEventHelloTimeout(EmuEnvState &emuenv, SceUID thread_id) {
    helloPipeMsg.isSheduled = false;

    const int num = countTargetsWithStatusOrBetter(SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES);
    if (num + 1 < maxnum)
        broadcastHello(emuenv, thread_id);

    addHelloTimedFunct(helloInterval);
}

void SceNetAdhocMatchingContext::handleEventDataTimeout(SceNetAdhocMatchingTarget *target) {
    target->sendDataTimeout.message.isSheduled = false;
    if (target == nullptr)
        return;

    if (target->sendDataStatus != SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_BUSY)
        return;

    if (target->sendDataTimeout.retryCount-- > 0)
        return;

    target->setSendDataStatus(SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_READY);
    notifyHandler(SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_DATA_TIMEOUT, target->addr);
}

void SceNetAdhocMatchingContext::handleIncommingPackage(SceNetInAddr addr, SceSize rawPacketLength, SceSize packetLength) {
    auto target = findTargetByAddr(addr);

    // No target found try to create one
    if (target == nullptr) {
        SceNetAdhocMatchingPacketType type = *(SceNetAdhocMatchingPacketType *)(rxbuf + 1);
        if (type == SCE_NET_ADHOC_MATCHING_PACKET_TYPE_HELLO_ACK && (mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT || mode == SCE_NET_ADHOC_MATCHING_MODE_P2P)) {
            target = newTarget(addr);
        }
        if (type == SCE_NET_ADHOC_MATCHING_PACKET_TYPE_HELLO && (mode == SCE_NET_ADHOC_MATCHING_MODE_CHILD || mode == SCE_NET_ADHOC_MATCHING_MODE_P2P)) {
            target = newTarget(addr);
        }
    }

    // No target available
    if (target == nullptr) {
        return;
    }

    if (!target->incomingPacketMessage.isSheduled) {
        target->setRawPacket(rawPacketLength, packetLength, this->rxbuf);
        target->keepAliveInterval = keepAliveInterval;

        target->incomingPacketMessage.type = SCE_NET_ADHOC_MATCHING_EVENT_PACKET;
        target->incomingPacketMessage.target = target;
        target->incomingPacketMessage.isSheduled = true;
        write(getWritePipeUid(), &target->incomingPacketMessage, sizeof(target->incomingPacketMessage));
    }
}

void SceNetAdhocMatchingContext::setTargetStatus(SceNetAdhocMatchingTarget &target, SceNetAdhocMatchingTargetStatus status) {
    target.setStatus(status);
    if (target.status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED || status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED) {
        createMembersList();
    }
}

SceNetAdhocMatchingTarget *SceNetAdhocMatchingContext::newTarget(SceNetInAddr addr) {
    auto *target = new SceNetAdhocMatchingTarget();

    if (target == nullptr) {
        return nullptr;
    }

    setTargetStatus(*target, SCE_NET_ADHOC_MATCHING_TARGET_STATUS_CANCELLED);
    target->addr.s_addr = addr.s_addr;
    target->setPipeUids(getReadPipeUid(), getWritePipeUid());

    // SceNetInternal_689B9D7D(&target->targetCount); <-- GetCurrentTargetCount?
    if (target->targetCount == 0) {
        target->targetCount = 1;
    }

    target->targetTimeout.isAckPending = false;
    if (target->sendDataStatus != SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_READY) {
        if (target->sendDataStatus == SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_BUSY) {
            target->deleteRawPacket();
        }
        target->sendDataStatus = SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_READY;
    }

    target->sendDataTimeout.isAckPending = false;
    target->next = targetList;
    targetList = target;

    return target;
}

SceNetAdhocMatchingTarget *SceNetAdhocMatchingContext::findTargetByAddr(SceNetInAddr addr) const {
    SceNetAdhocMatchingTarget *target = this->targetList;
    while (target != nullptr) {
        if (target->addr.s_addr == addr.s_addr && !target->deleteTarget)
            return target;
        target = target->next;
    }

    return nullptr;
};

void SceNetAdhocMatchingContext::getMemberList(SceNetAdhocMatchingTargetStatus status, SceNetInAddr *addrList, SceSize &addrListSize) const {
    auto *target = targetList;
    SceSize index = 0;

    for (; target != nullptr; target = target->next) {
        if (target->status < status) {
            continue;
        }
        if (addrListSize <= index) {
            break;
        }
        if (addrList != nullptr) {
            memcpy(&addrList[index], &target->addr, sizeof(SceNetInAddr));
        }
        index++;
    }

    addrListSize = index;
}

SceSize SceNetAdhocMatchingContext::countTargetsWithStatusOrBetter(SceNetAdhocMatchingTargetStatus status) const {
    SceSize i = 0;
    SceNetAdhocMatchingTarget *target;
    for (target = this->targetList; target != nullptr; target = target->next) {
        if (target->status >= status)
            i++;
    }
    return i;
}

bool SceNetAdhocMatchingContext::isTargetAddressHigher(SceNetAdhocMatchingTarget &target) const {
    return ownAddress < target.addr.s_addr;
}

void SceNetAdhocMatchingContext::deleteTarget(SceNetAdhocMatchingTarget *target) {
    SceNetAdhocMatchingTarget *previous = nullptr;
    auto *current = this->targetList;

    // Find and remove target from list
    while (true) {
        if (current == nullptr) {
            return;
        }

        if (current == target) {
            if (previous == nullptr) {
                this->targetList = current->next;
                break;
            }
            previous->next = current->next;
            break;
        }

        previous = current;
        current = current->next;
    }

    target->deleteOptMessage();
    target->deleteSendData();
    target->deleteRawPacket();

    delete target;
}

void SceNetAdhocMatchingContext::deleteAllTargets(EmuEnvState &emuenv, SceUID thread_id) {
    auto *target = this->targetList;
    while (target != nullptr) {
        deleteAllTimedFunctions(*target);
        target->deleteOptMessage();
        target->deleteSendData();
        target->deleteRawPacket();
        auto *nextTarget = target->next;
        delete target;
        target = nextTarget;
    }
    this->targetList = nullptr;
    broadcastBye(emuenv, thread_id);
}

int SceNetAdhocMatchingContext::createMembersList() {
    SceSize target_count = countTargetsWithStatusOrBetter(SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED);

    SceNetAdhocMatchingMemberMessage *message = new SceNetAdhocMatchingMemberMessage();

    if (message == nullptr)
        return SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;

    message->header = {
        .one = 1,
        .type = SCE_NET_ADHOC_MATCHING_PACKET_TYPE_MEMBER_LIST,
    };
    message->parent.s_addr = this->ownAddress;
    message->members.resize(target_count);
    getMemberList(SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED, message->members.data(), target_count);
    message->header.packetLength = static_cast<SceUShort16>(target_count + 1) * sizeof(SceNetInAddr);

    deleteMemberList();
    this->memberMsg = message;
    return SCE_NET_ADHOC_MATCHING_OK;
}

int SceNetAdhocMatchingContext::getMembers(SceSize &outMembersCount, SceNetAdhocMatchingMember *outMembers) const {
    if (this->memberMsg == nullptr) {
        outMembersCount = 0;
        return SCE_NET_ADHOC_MATCHING_OK;
    }

    if (outMembersCount > 0 && outMembers != nullptr) {
        memcpy(&outMembers[0], &this->memberMsg->parent, sizeof(SceNetAdhocMatchingMember));
    }

    SceSize count = 1;
    for (SceSize i = 0; i < this->memberMsg->members.size(); i++) {
        if (count >= outMembersCount) {
            break;
        }
        if (outMembers != nullptr) {
            memcpy(&outMembers[count], &this->memberMsg->members[i], sizeof(SceNetAdhocMatchingMember));
        }
        count++;
    }

    outMembersCount = count;
    return SCE_NET_ADHOC_MATCHING_OK;
}

void SceNetAdhocMatchingContext::deleteMemberList() {
    if (this->memberMsg == nullptr)
        return;

    delete this->memberMsg;
    this->memberMsg = nullptr;
}

int SceNetAdhocMatchingContext::getHelloOpt(SceSize &outOptLen, void *outOpt) const {
    if (this->helloMsg == nullptr) {
        outOptLen = 0;
        return SCE_NET_ADHOC_MATCHING_OK;
    }

    if (this->helloMsg->optBuffer.size() < outOptLen) {
        outOptLen = this->helloMsg->optBuffer.size();
    }

    if (outOpt != nullptr && 0 < outOptLen) {
        memcpy(outOpt, this->helloMsg, outOptLen);
    }

    return SCE_NET_ADHOC_MATCHING_OK;
};

int SceNetAdhocMatchingContext::setHelloOpt(SceSize optlen, void *opt) {
    auto *message = new SceNetAdhocMatchingHelloMessage();

    if (message == nullptr)
        return SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;

    message->header = {
        .one = 1,
        .type = SCE_NET_ADHOC_MATCHING_PACKET_TYPE_HELLO,
        .packetLength = static_cast<SceUShort16>(optlen + 8),
    };
    message->helloInterval = helloInterval;
    message->rexmtInterval = keepAliveInterval;
    message->unk_6c = true;
    message->padding = {};

    if (optlen > 0) {
        message->optBuffer.resize(optlen);
        memcpy(message->optBuffer.data(), opt, optlen);
    }

    deleteHelloMessage();
    this->helloMsg = message;
    return SCE_NET_ADHOC_MATCHING_OK;
};

void SceNetAdhocMatchingContext::deleteHelloMessage() {
    helloPipeMsg.isSheduled = false;
    if (this->helloMsg == nullptr)
        return;

    delete this->helloMsg;
    this->helloMsg = nullptr;
}

void SceNetAdhocMatchingContext::addHelloTimedFunct(uint64_t time_interval) {
    if (shouldHelloReqBeProcessed) {
        calloutSyncing.deleteTimedFunction(&helloTimedFunction);
        shouldHelloReqBeProcessed = false;
    }
    calloutSyncing.addTimedFunction(&helloTimedFunction, time_interval, &helloTimeoutCallback, this);
    shouldHelloReqBeProcessed = true;
}

void SceNetAdhocMatchingContext::addSendDataTimeout(SceNetAdhocMatchingTarget &target) {
    if (target.sendDataTimeout.isAckPending) {
        calloutSyncing.deleteTimedFunction(&target.sendDataTimeoutFunction);
        target.sendDataTimeout.isAckPending = false;
    }
    calloutSyncing.addTimedFunction(&target.sendDataTimeoutFunction, rexmtInterval, &sendDataTimeoutCallback, &target);
    target.sendDataTimeout.isAckPending = true;
}

void SceNetAdhocMatchingContext::addRegisterTargetTimeout(SceNetAdhocMatchingTarget &target) {
    if (target.targetTimeout.isAckPending) {
        calloutSyncing.deleteTimedFunction(&target.targetTimeoutFunction);
        target.targetTimeout.isAckPending = false;
    }
    calloutSyncing.addTimedFunction(&target.targetTimeoutFunction, rexmtInterval, &registerTargetTimeoutCallback, &target);
    target.targetTimeout.isAckPending = true;
}

void SceNetAdhocMatchingContext::addTargetTimeout(SceNetAdhocMatchingTarget &target) {
    int interval = keepAliveInterval;
    if (mode == SCE_NET_ADHOC_MATCHING_MODE_CHILD) {
        interval = target.keepAliveInterval;
    }

    if (target.targetTimeout.isAckPending) {
        calloutSyncing.deleteTimedFunction(&target.targetTimeoutFunction);
        target.targetTimeout.isAckPending = false;
    }
    calloutSyncing.addTimedFunction(&target.targetTimeoutFunction, interval, &targetTimeoutCallback, &target);
    target.targetTimeout.isAckPending = true;
}

void SceNetAdhocMatchingContext::deleteHelloTimedFunction() {
    if (!shouldHelloReqBeProcessed)
        return;

    calloutSyncing.deleteTimedFunction(&this->helloTimedFunction);
    shouldHelloReqBeProcessed = false;
}

void SceNetAdhocMatchingContext::deleteSendDataTimeout(SceNetAdhocMatchingTarget &target) {
    if (target.sendDataTimeout.isAckPending) {
        calloutSyncing.deleteTimedFunction(&target.sendDataTimeoutFunction);
        target.sendDataTimeout.isAckPending = false;
    }
}

void SceNetAdhocMatchingContext::deleteAllTimedFunctions(SceNetAdhocMatchingTarget &target) {
    if (target.sendDataTimeout.isAckPending) {
        calloutSyncing.deleteTimedFunction(&target.sendDataTimeoutFunction);
        target.sendDataTimeout.isAckPending = false;
    }
    if (target.targetTimeout.isAckPending) {
        calloutSyncing.deleteTimedFunction(&target.targetTimeoutFunction);
        target.targetTimeout.isAckPending = false;
    }
}

void SceNetAdhocMatchingContext::SendNotificationQueue(EmuEnvState &emuenv, SceUID thread_id) {
    if (!this->handler) {
        return;
    }

    while (!notificationQueue.empty()) {
        const auto notification = notificationQueue.front();

        Address vPeer = alloc(emuenv.mem, sizeof(SceNetInAddr), "adhocHandlerPeer");
        Address vOpt = 0;

        vPeer = alloc(emuenv.mem, sizeof(SceNetInAddr), "adhocHandlerPeer");
        memcpy(Ptr<char>(vPeer).get(emuenv.mem), &notification.peer, sizeof(SceNetInAddr));

        if (!notification.opt.empty()) {
            vOpt = alloc(emuenv.mem, notification.opt.size(), "adhocHandlerOpt");
            memcpy(Ptr<char>(vOpt).get(emuenv.mem), notification.opt.data(), notification.opt.size());
        }

        const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
        thread->run_adhoc_callback(this->handler, this->id, (uint32_t)notification.type, Ptr<char>(vPeer), notification.opt.size(), Ptr<char>(vOpt));

        free(emuenv.mem, vPeer); // free peer
        if (!notification.opt.empty())
            free(emuenv.mem, vOpt); // free opt
        notificationQueue.pop();
    }
}

void SceNetAdhocMatchingContext::notifyHandler(SceNetAdhocMatchingHandlerEventType type, SceNetInAddr peer, SceSize optLen, const void *opt) {
    HandlerNotification notification{
        .type = type,
        .peer = peer,
    };
    const SceNetAdhocMatchingPipeMessage notifyMsg{
        .type = SCE_NET_ADHOC_MATCHING_EVENT_NOTIFICATION,
    };

    notification.opt.resize(optLen);
    if (optLen > 0)
        memcpy(notification.opt.data(), opt, optLen);

    notificationQueue.push(notification);
    write(getWritePipeUid(), &notifyMsg, sizeof(SceNetAdhocMatchingPipeMessage));
}

uint32_t SceNetAdhocMatchingContext::getBroadcastAddr() {
    return INADDR_BROADCAST;
}

uint32_t SceNetAdhocMatchingContext::getTargetAddr(const SceNetAdhocMatchingTarget &target) {
    // Broadcast if two instances have the same address
    if (target.addr.s_addr == ownAddress)
        return getBroadcastAddr();
    return target.addr.s_addr;
}

int SceNetAdhocMatchingContext::sendMemberListToTarget(EmuEnvState &emuenv, SceUID thread_id, const SceNetAdhocMatchingTarget &target) {
    const int flags = 0x400; // 0x480 if sdk version < 0x1500000

    SceNetSockaddrIn addr = {
        .sin_len = sizeof(SceNetSockaddrIn),
        .sin_family = AF_INET,
        .sin_port = htons(SCE_NET_ADHOC_DEFAULT_PORT),
        .sin_addr = getTargetAddr(target),
        .sin_vport = htons(port),
    };

    auto result = CALL_EXPORT(sceNetSendto, this->sendSocket, this->memberMsg->serialize().data(), this->memberMsg->messageSize(), flags, (SceNetSockaddr *)&addr, sizeof(SceNetSockaddrIn));

    if (result == SCE_NET_ERROR_EAGAIN)
        result = SCE_NET_ADHOC_MATCHING_OK;

    return result;
}

int SceNetAdhocMatchingContext::sendDataMessageToTarget(EmuEnvState &emuenv, SceUID thread_id, const SceNetAdhocMatchingTarget &target, SceNetAdhocMatchingPacketType type, std::span<const char> data) {
    const int flags = 0x400; // 0x480 if sdk version < 0x1500000

    auto *msg = new SceNetAdhocMatchingDataMessage();

    msg->header = {
        .one = 1,
        .type = type,
        .packetLength = static_cast<SceUShort16>(data.size() + 8),
    };

    msg->targetId = target.targetCount;

    msg->packetId = target.sendDataCount;
    if (type == SCE_NET_ADHOC_MATCHING_PACKET_TYPE_DATA_ACK)
        msg->packetId = target.recvDataCount - 1;

    msg->dataBuffer.resize(data.size());
    memcpy(msg->dataBuffer.data(), data.data(), data.size());

    SceNetSockaddrIn addr = {
        .sin_len = sizeof(SceNetSockaddrIn),
        .sin_family = AF_INET,
        .sin_port = htons(SCE_NET_ADHOC_DEFAULT_PORT),
        .sin_addr = getTargetAddr(target),
        .sin_vport = htons(port),
    };

    auto result = CALL_EXPORT(sceNetSendto, this->sendSocket, msg->serialize().data(), msg->messageSize(), flags, (SceNetSockaddr *)&addr, sizeof(SceNetSockaddrIn));

    if (result == SCE_NET_ERROR_EAGAIN) {
        result = SCE_NET_ADHOC_MATCHING_OK;
    }

    return result;
}

int SceNetAdhocMatchingContext::sendOptDataToTarget(EmuEnvState &emuenv, SceUID thread_id, const SceNetAdhocMatchingTarget &target, SceNetAdhocMatchingPacketType type, std::span<const char> opt) {
    const int flags = 0x400; // 0x480 if sdk version < 0x1500000
    int headerSize = 4;

    auto *msg = new SceNetAdhocMatchingOptMessage();

    if (type == SCE_NET_ADHOC_MATCHING_PACKET_TYPE_HELLO_ACK) {
        headerSize = 0x14;
    }
    if (type == SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK3) {
        headerSize = 0x14;
    }

    msg->header = {
        .one = 1,
        .type = type,
        .packetLength = static_cast<SceUShort16>(opt.size()),
    };

    if (opt.size() > 0) {
        msg->dataBuffer.resize(opt.size());
        memcpy(msg->dataBuffer.data(), opt.data(), opt.size());
    }

    if (headerSize == 0x14) {
        msg->targetCount = target.targetCount;
        msg->padding = {};
    }

    SceNetSockaddrIn addr = {
        .sin_len = sizeof(SceNetSockaddrIn),
        .sin_family = AF_INET,
        .sin_port = htons(SCE_NET_ADHOC_DEFAULT_PORT),
        .sin_addr = getTargetAddr(target),
        .sin_vport = htons(port),
    };

    auto result = CALL_EXPORT(sceNetSendto, this->sendSocket, msg->serialize().data(), opt.size() + headerSize, flags, (SceNetSockaddr *)&addr, sizeof(SceNetSockaddrIn));
    if (result == SCE_NET_ERROR_EAGAIN) {
        result = SCE_NET_ADHOC_MATCHING_OK;
    }

    return result;
}

int SceNetAdhocMatchingContext::broadcastHello(EmuEnvState &emuenv, SceUID thread_id) {
    const int flags = 0x400; // 0x480 if sdk version < 0x1500000

    if (this->helloMsg == nullptr) {
        return SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG;
    }

    SceNetSockaddrIn addr = {
        .sin_len = sizeof(SceNetSockaddrIn),
        .sin_family = AF_INET,
        .sin_port = htons(SCE_NET_ADHOC_DEFAULT_PORT),
        .sin_addr = getBroadcastAddr(),
        .sin_vport = htons(this->port),
    };

    auto result = CALL_EXPORT(sceNetSendto, this->sendSocket, this->helloMsg->serialize().data(), this->helloMsg->messageSize(), flags, (SceNetSockaddr *)&addr, sizeof(SceNetSockaddrIn));
    if (result == SCE_NET_ERROR_EAGAIN)
        result = SCE_NET_ADHOC_MATCHING_OK;

    return result;
};

int SceNetAdhocMatchingContext::broadcastBye(EmuEnvState &emuenv, SceUID thread_id) {
    const int flags = 0x400; // 0x480 if sdk version < 0x1500000

    const SceNetAdhocMatchingMessageHeader byeMsg = {
        .one = 1,
        .type = SCE_NET_ADHOC_MATCHING_PACKET_TYPE_BYE,
        .packetLength = 0,
    };

    const SceNetSockaddrIn addr = {
        .sin_len = sizeof(SceNetSockaddrIn),
        .sin_family = AF_INET,
        .sin_port = htons(SCE_NET_ADHOC_DEFAULT_PORT),
        .sin_addr = getBroadcastAddr(),
        .sin_vport = htons(this->port),
    };

    auto result = CALL_EXPORT(sceNetSendto, this->sendSocket, &byeMsg, sizeof(SceNetAdhocMatchingMessageHeader), flags, (SceNetSockaddr *)&addr, sizeof(SceNetSockaddrIn));
    if (result == SCE_NET_ERROR_EAGAIN)
        result = SCE_NET_ADHOC_MATCHING_OK;

    return result;
}

int SceNetAdhocMatchingContext::broadcastAbort(EmuEnvState &emuenv, SceUID thread_id) {
    shouldExit = true;

    const SceNetAdhocMatchingMessageHeader abortMsg = {
        .one = 0,
        .type = SCE_NET_ADHOC_MATCHING_PACKET_TYPE_ABORT,
        .packetLength = 0,
    };

    const SceNetSockaddrIn addr = {
        .sin_len = sizeof(SceNetSockaddrIn),
        .sin_family = AF_INET,
        .sin_port = htons(SCE_NET_ADHOC_DEFAULT_PORT),
        .sin_addr = ownAddress,
        .sin_vport = htons(this->port),
    };

    auto result = CALL_EXPORT(sceNetSendto, this->sendSocket, &abortMsg, sizeof(SceNetAdhocMatchingMessageHeader), 0, (SceNetSockaddr *)&addr, sizeof(SceNetSockaddrIn));
    if (result == SCE_NET_ERROR_EAGAIN)
        result = SCE_NET_ADHOC_MATCHING_OK;

    return result;
}
