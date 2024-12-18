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

#pragma once

#include <cassert>
#include <queue>
#include <span>
#include <vector>

#include "adhoc/calloutSyncing.h"
#include "adhoc/state.h"
#include "net/state.h"
#include "util/types.h"

struct EmuEnvState;
class SceNetAdhocMatchingTarget;
enum SceNetAdhocMatchingTargetStatus : uint32_t;

enum SceNetAdhocMatchingPacketType : uint8_t {
    SCE_NET_ADHOC_MATCHING_PACKET_TYPE_ABORT = 0,
    SCE_NET_ADHOC_MATCHING_PACKET_TYPE_HELLO = 1,
    SCE_NET_ADHOC_MATCHING_PACKET_TYPE_HELLO_ACK = 2,
    SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK3 = 3,
    SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK4 = 4,
    SCE_NET_ADHOC_MATCHING_PACKET_TYPE_CANCEL = 5,
    SCE_NET_ADHOC_MATCHING_PACKET_TYPE_MEMBER_LIST = 6,
    SCE_NET_ADHOC_MATCHING_PACKET_TYPE_MEMBER_LIST_ACK = 7,
    SCE_NET_ADHOC_MATCHING_PACKET_TYPE_BYE = 8,
    SCE_NET_ADHOC_MATCHING_PACKET_TYPE_UNK9 = 9,
    SCE_NET_ADHOC_MATCHING_PACKET_TYPE_DATA = 10,
    SCE_NET_ADHOC_MATCHING_PACKET_TYPE_DATA_ACK = 11
};

enum SceNetAdhocMatchingContextStatus {
    SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_NOT_RUNNING = 0,
    SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_STOPPING = 1,
    SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_RUNNING = 2,
};

typedef SceNetInAddr SceNetAdhocMatchingMember;

struct SceNetAdhocMatchingMessageHeader {
    uint8_t one; //! ALWAYS 1
    SceNetAdhocMatchingPacketType type;
    SceUShort16 packetLength;

    std::vector<char> serialize() const {
        std::vector<char> data(sizeof(SceNetAdhocMatchingMessageHeader));
        const SceUShort16 nPacketLength = htons(packetLength);

        memcpy(data.data(), &one, sizeof(uint8_t));
        memcpy(data.data() + 0x1, &type, sizeof(SceNetAdhocMatchingPacketType));
        memcpy(data.data() + 0x2, &nPacketLength, sizeof(SceUShort16));
        return data;
    }

    void parse(const char *data, SceSize dataLen) {
        assert(dataLen >= sizeof(SceNetAdhocMatchingMessageHeader));

        memcpy(&one, data, sizeof(uint8_t));
        memcpy(&type, data + 0x1, sizeof(SceNetAdhocMatchingPacketType));
        memcpy(&packetLength, data + 0x2, sizeof(SceUShort16));
        packetLength = htons(packetLength);
    }
};
static_assert(sizeof(SceNetAdhocMatchingMessageHeader) == 0x4, "SceNetAdhocMatchingMessageHeader is an invalid size");

struct SceNetAdhocMatchingDataMessage {
    SceNetAdhocMatchingMessageHeader header;
    SceUID targetId;
    SceUID packetId;
    std::vector<char> dataBuffer;

    std::size_t messageSize() const {
        return sizeof(header) + dataBuffer.size() + 0x8;
    }

    std::vector<char> serialize() const {
        std::vector<char> data = header.serialize();
        data.resize(messageSize());

        const SceUID ntargetId = htonl(targetId);
        const int nPacketId = htonl(packetId);
        memcpy(data.data() + sizeof(header), &ntargetId, sizeof(SceUID));
        memcpy(data.data() + sizeof(header) + 0x4, &nPacketId, sizeof(SceUID));
        memcpy(data.data() + sizeof(header) + 0x8, dataBuffer.data(), dataBuffer.size());
        return data;
    }

    void parse(const char *data, SceSize dataLen) {
        assert(dataLen >= sizeof(header) + 0x8);
        dataBuffer.resize(dataLen - sizeof(header) - 0x8);

        header.parse(data, dataLen);
        memcpy(dataBuffer.data(), data + sizeof(header), dataBuffer.size());
        memcpy(&targetId, data + sizeof(header), sizeof(SceUID));
        memcpy(&packetId, data + sizeof(header) + 0x4, sizeof(SceUID));
        memcpy(dataBuffer.data(), data + sizeof(header) + 0x8, dataBuffer.size());
        targetId = htonl(targetId);
        packetId = htonl(packetId);
    }
};

struct SceNetAdhocMatchingOptMessage {
    SceNetAdhocMatchingMessageHeader header;
    std::vector<char> dataBuffer;
    int targetCount;
    std::array<char, 0xc> padding;

    std::size_t messageSize() const {
        return sizeof(header) + dataBuffer.size() + 0x10;
    }

    std::vector<char> serialize() const {
        std::vector<char> data = header.serialize();
        data.resize(messageSize());

        const int nTargetCount = htonl(targetCount);
        memcpy(data.data() + sizeof(header), dataBuffer.data(), dataBuffer.size());
        memcpy(data.data() + sizeof(header) + dataBuffer.size(), &nTargetCount, sizeof(int));
        memcpy(data.data() + sizeof(header) + dataBuffer.size() + 0x4, padding.data(), padding.size());
        return data;
    }

    void parse(const char *data, SceSize dataLen) {
        assert(dataLen >= sizeof(header));
        dataBuffer.resize(dataLen - sizeof(header) - 0x10);

        header.parse(data, dataLen);
        memcpy(dataBuffer.data(), data + sizeof(header), dataBuffer.size());
        memcpy(&targetCount, data + sizeof(header) + dataBuffer.size(), sizeof(int));
        memcpy(padding.data(), data + sizeof(header) + dataBuffer.size() + 0x4, padding.size());
        targetCount = htonl(targetCount);
    }
};

struct SceNetAdhocMatchingHelloMessage {
    SceNetAdhocMatchingMessageHeader header;
    int helloInterval;
    int rexmtInterval;
    std::vector<char> optBuffer;
    bool unk_6c;
    std::array<char, 0xf> padding;

    std::size_t messageSize() const {
        return sizeof(header) + optBuffer.size() + 0x18;
    }

    std::vector<char> serialize() const {
        std::vector<char> data = header.serialize();
        data.resize(messageSize());

        const int nhelloInterval = htonl(helloInterval);
        const int nrexmtInterval = htonl(rexmtInterval);
        memcpy(data.data() + sizeof(header), &nhelloInterval, sizeof(int));
        memcpy(data.data() + sizeof(header) + 0x4, &nrexmtInterval, sizeof(int));
        memcpy(data.data() + sizeof(header) + 0x8, optBuffer.data(), optBuffer.size());
        memcpy(data.data() + sizeof(header) + 0x8 + optBuffer.size(), &unk_6c, sizeof(bool));
        memcpy(data.data() + sizeof(header) + 0x9 + optBuffer.size(), padding.data(), sizeof(0xf));
        return data;
    }

    void parse(const char *data, SceSize dataLen) {
        assert(dataLen >= sizeof(header) + 0x8);
        optBuffer.resize(dataLen - sizeof(header) - 0x18);

        header.parse(data, dataLen);
        memcpy(&helloInterval, data + sizeof(header), sizeof(int));
        memcpy(&rexmtInterval, data + sizeof(header) + 0x4, sizeof(int));
        memcpy(optBuffer.data(), data + sizeof(header) + 0x8, optBuffer.size());
        memcpy(&unk_6c, data + sizeof(header) + 0x8 + optBuffer.size(), sizeof(bool));
        memcpy(padding.data(), data + sizeof(header) + 0x9 + optBuffer.size(), sizeof(0xf));
        helloInterval = htonl(helloInterval);
        rexmtInterval = htonl(rexmtInterval);
    }
};

struct SceNetAdhocMatchingMemberMessage {
    SceNetAdhocMatchingMessageHeader header;
    SceNetInAddr parent;
    std::vector<SceNetInAddr> members;

    std::size_t messageSize() const {
        const std::size_t memberSize = members.size() * sizeof(SceNetInAddr);
        return sizeof(header) + sizeof(parent) + memberSize;
    }

    std::vector<char> serialize() const {
        std::vector<char> data = header.serialize();
        data.resize(messageSize());
        memcpy(data.data() + sizeof(header), &parent, sizeof(SceNetInAddr));
        memcpy(data.data() + sizeof(header) + sizeof(parent), members.data(), members.size() * sizeof(SceNetInAddr));
        return data;
    }

    void parse(const char *data, SceSize dataLen) {
        assert(dataLen >= sizeof(header) + sizeof(parent));
        assert(dataLen % 4 == 0);

        const std::size_t entries = (dataLen / sizeof(SceNetInAddr)) - 2;
        members.resize(entries);
        header.parse(data, dataLen);
        memcpy(&parent, data + sizeof(header), sizeof(SceNetInAddr));
        memcpy(members.data(), data + sizeof(header) + sizeof(parent), entries * sizeof(SceNetInAddr));
    }
};

struct HandlerNotification {
    SceNetAdhocMatchingHandlerEventType type;
    SceNetInAddr peer;
    std::vector<char> opt;
};

class SceNetAdhocMatchingContext {
public:
    ~SceNetAdhocMatchingContext();

    int initialize(SceNetAdhocMatchingMode mode, int maxnum, SceUShort16 port, int rxbuflen, unsigned int helloInterval, unsigned int keepaliveInterval, int retryCount, unsigned int rexmtInterval, Ptr<void> handlerAddr);
    void finalize();

    int start(EmuEnvState &emuenv, SceUID thread_id, int threadPriority, int threadStackSize, int threadCpuAffinityMask, SceSize helloOptLen, char *helloOpt);
    int stop(EmuEnvState &emuenv, SceUID thread_id);

    int initializeInputThread(EmuEnvState &emuenv, SceUID thread_id, int threadPriority, int threadStackSize, int threadCpuAffinityMask);
    void closeInputThread(EmuEnvState &emuenv, SceUID thread_id);

    int initializeEventHandler(EmuEnvState &emuenv, SceUID thread_id, int threadPriority, int threadStackSize, int threadCpuAffinityMask);
    void closeEventHandler();

    int initializeSendSocket(EmuEnvState &emuenv, SceUID thread_id);
    void closeSendSocket(EmuEnvState &emuenv, SceUID thread_id);

    SceNetAdhocMatchingContext *getNext();
    void setNext(SceNetAdhocMatchingContext *next_context);

    SceUID getId() const;
    void setId(SceUID id);

    bool isRunning() const;
    SceUShort16 getPort() const;
    SceNetAdhocMatchingContextStatus getStatus() const;
    SceNetAdhocMatchingMode getMode() const;
    SceNetAdhocMatchingCalloutSyncing &getCalloutSyncing();
    SceNetAdhocMatchingTarget *findTargetByAddr(SceNetInAddr addr) const;
    int getReadPipeUid() const;
    int getWritePipeUid() const;

    int getMembers(SceSize &outMembersCount, SceNetAdhocMatchingMember *outMembers) const;
    int getHelloOpt(SceSize &outOptlen, void *outOpt) const;
    int setHelloOpt(SceSize optlen, void *opt);
    void deleteTarget(SceNetAdhocMatchingTarget *target);

    void abortSendData(SceNetAdhocMatchingTarget &target);
    int cancelTargetWithOpt(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target, SceSize optLen, char *opt);
    int selectTarget(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target, SceSize optLen, char *opt);
    int sendData(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target, SceSize dateLen, char *data);

    void handleEventMessage(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget *target);
    void handleEventRegistrationTimeout(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget *target);
    void handleEventTargetTimeout(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget *target);
    void handleEventHelloTimeout(EmuEnvState &emuenv, SceUID thread_id);
    void handleEventDataTimeout(SceNetAdhocMatchingTarget *target);
    void handleIncommingPackage(SceNetInAddr addr, SceSize rawPacketLen, SceSize packetLength);

    void SendNotificationQueue(EmuEnvState &emuenv, SceUID thread_id);
    int broadcastAbort(EmuEnvState &emuenv, SceUID thread_id);

public:
    SceNetAdhocMatchingPipeMessage helloPipeMsg{};
    bool shouldHelloReqBeProcessed{};

    int sendSocket{};
    int recvSocket{};

    int rxbuflen{};
    char *rxbuf{ nullptr };

    uint32_t ownAddress{};
    uint16_t ownPort{};

private:
    // Packet data processing
    void processPacketFromTarget(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target);
    int processHelloPacket(SceNetAdhocMatchingTarget &target);
    int processHelloAckPacket(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target);
    int processMemberListPacket(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target);
    int processMemberListAckPacket(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target);
    int processByePacket(SceNetAdhocMatchingTarget &target);
    int processUnk9Packet(SceNetAdhocMatchingTarget &target);
    int processDataPacket(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target);
    int processDataAckPacket(EmuEnvState &emuenv, SceUID thread_id, SceNetAdhocMatchingTarget &target);

    // Target
    void setTargetStatus(SceNetAdhocMatchingTarget &target, SceNetAdhocMatchingTargetStatus status);
    SceNetAdhocMatchingTarget *newTarget(SceNetInAddr addr);
    void getMemberList(SceNetAdhocMatchingTargetStatus status, SceNetInAddr *addrList, SceSize &addrListSize) const;
    SceSize countTargetsWithStatusOrBetter(SceNetAdhocMatchingTargetStatus status) const;
    bool isTargetAddressHigher(SceNetAdhocMatchingTarget &target) const;
    void deleteAllTargets(EmuEnvState &emuenv, SceUID thread_id);

    // Member list message
    int createMembersList();
    void deleteMemberList();

    // Hello optional data
    void deleteHelloMessage();

    // Timed functions
    void addHelloTimedFunct(uint64_t time_interval);
    void addSendDataTimeout(SceNetAdhocMatchingTarget &target);
    void addRegisterTargetTimeout(SceNetAdhocMatchingTarget &target);
    void addTargetTimeout(SceNetAdhocMatchingTarget &target);
    void deleteHelloTimedFunction();
    void deleteSendDataTimeout(SceNetAdhocMatchingTarget &target);
    void deleteAllTimedFunctions(SceNetAdhocMatchingTarget &target);

    void notifyHandler(SceNetAdhocMatchingHandlerEventType type, SceNetInAddr peer, SceSize optLen = 0, const void *opt = nullptr);

    uint32_t getBroadcastAddr();
    uint32_t getTargetAddr(const SceNetAdhocMatchingTarget &target);

    int sendMemberListToTarget(EmuEnvState &emuenv, SceUID thread_id, const SceNetAdhocMatchingTarget &target);
    int sendDataMessageToTarget(EmuEnvState &emuenv, SceUID thread_id, const SceNetAdhocMatchingTarget &target, SceNetAdhocMatchingPacketType type, std::span<const char> data);
    int sendOptDataToTarget(EmuEnvState &emuenv, SceUID thread_id, const SceNetAdhocMatchingTarget &target, SceNetAdhocMatchingPacketType type, std::span<const char> opt);

    int broadcastHello(EmuEnvState &emuenv, SceUID thread_id);
    int broadcastBye(EmuEnvState &emuenv, SceUID thread_id);

    SceNetAdhocMatchingContext *next{ nullptr };
    SceUID id{};
    SceNetAdhocMatchingContextStatus status{ SCE_NET_ADHOC_MATCHING_CONTEXT_STATUS_NOT_RUNNING };
    SceNetAdhocMatchingMode mode{ SCE_NET_ADHOC_MATCHING_MODE_NONE };
    int maxnum{};
    SceUShort16 port{};

    unsigned int helloInterval{};
    unsigned int keepAliveInterval{};
    unsigned int retryCount{};
    unsigned int rexmtInterval{};

    SceNetAdhocMatchingHandler handler{};
    std::queue<HandlerNotification> notificationQueue{};

    bool shouldExit{ true };
    bool isEventThreadInitialized{};
    bool isInputThreadInitialized{};
    std::thread eventThread{};
    std::thread inputThread{};
    SceUID event_thread_id{};
    SceUID input_thread_id{};

    int msgPipeUid[2]{}; // 0 = read, 1 = write

    SceNetAdhocMatchingHelloMessage *helloMsg{ nullptr };
    SceNetAdhocMatchingMemberMessage *memberMsg{ nullptr };

    int helloOptionFlag{};

    SceNetAdhocMatchingTarget *targetList{ nullptr };

    SceNetAdhocMatchingCalloutSyncing calloutSyncing{};
    SceNetAdhocMatchingCalloutFunction helloTimedFunction{};
};
