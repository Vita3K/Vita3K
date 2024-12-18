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

#include "adhoc/matchingTarget.h"

void SceNetAdhocMatchingTarget::setSendDataStatus(SceNetAdhocMatchingSendDataStatus status) {
    if (this->sendDataStatus == status)
        return;
    if (this->sendDataStatus == SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_BUSY && status == SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_READY)
        deleteSendData();
    this->sendDataStatus = status;
}

void SceNetAdhocMatchingTarget::setStatus(SceNetAdhocMatchingTargetStatus status) {
    if (this->status == status) {
        return;
    }

    bool is_target_in_progress = this->status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES || this->status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES2;
    bool is_status_not_in_progress = status != SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES2 && status != SCE_NET_ADHOC_MATCHING_TARGET_STATUS_INPROGRES2;

    if (is_target_in_progress && is_status_not_in_progress) {
        deleteOptMessage();
    }

    if (this->status != SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED && status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED) {
        this->sendDataCount = 0;
        this->recvDataCount = 0;
    }

    if (this->status == SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED && status != SCE_NET_ADHOC_MATCHING_TARGET_STATUS_ESTABLISHED && this->sendDataStatus != SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_READY) {
        if (this->sendDataStatus == SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_BUSY)
            deleteSendData();
        this->sendDataStatus = SCE_NET_ADHOC_MATCHING_CONTEXT_SEND_DATA_STATUS_READY;
    }

    this->status = status;
}

void SceNetAdhocMatchingTarget::setPipeUids(int read, int write) {
    this->msgPipeUid[0] = read;
    this->msgPipeUid[1] = write;
}

int SceNetAdhocMatchingTarget::getReadPipeUid() const {
    return this->msgPipeUid[0];
}

int SceNetAdhocMatchingTarget::getWritePipeUid() const {
    return this->msgPipeUid[1];
}

int SceNetAdhocMatchingTarget::setOptMessage(SceSize length, const char *data) {
    this->opt.resize(length);
    if (length != 0)
        memcpy(this->opt.data(), data, length);

    return SCE_NET_ADHOC_MATCHING_OK;
}

std::span<const char> SceNetAdhocMatchingTarget::getOpt() const {
    return opt;
}

void SceNetAdhocMatchingTarget::deleteOptMessage() {
    opt.clear();
}

int SceNetAdhocMatchingTarget::setRawPacket(SceSize rawLength, SceSize length, const char *data) {
    this->packet.resize(rawLength);
    if (rawLength == 0) {
        this->packetLength = 0;
        return SCE_NET_ADHOC_MATCHING_OK;
    }

    memcpy(this->packet.data(), data, rawLength);
    this->packetLength = length + sizeof(SceNetAdhocMatchingMessageHeader);

    return SCE_NET_ADHOC_MATCHING_OK;
}

SceSize SceNetAdhocMatchingTarget::getPacketLen() const {
    return this->packetLength;
}

std::span<const char> SceNetAdhocMatchingTarget::getRawPacket() const {
    return this->packet;
}

void SceNetAdhocMatchingTarget::deleteRawPacket() {
    this->packetLength = 0;
    this->packet.clear();
}

int SceNetAdhocMatchingTarget::setSendData(SceSize length, const char *data) {
    this->sendData.resize(length);
    if (length != 0)
        memcpy(this->sendData.data(), data, length);

    return SCE_NET_ADHOC_MATCHING_OK;
}

std::span<const char> SceNetAdhocMatchingTarget::getSendData() const {
    return this->sendData;
}

void SceNetAdhocMatchingTarget::deleteSendData() {
    this->sendData.clear();
}
