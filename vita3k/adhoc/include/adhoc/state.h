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

#include <mutex>

#include "mem/util.h"
#include "module/module.h"
#include "util/types.h"

#define SCE_NET_ADHOC_DEFAULT_PORT 0xe4a
#define SCE_NET_ADHOC_MATCHING_MAXNUM 16
#define SCE_NET_ADHOC_MATCHING_MAXOPTLEN 9196
#define SCE_NET_ADHOC_MATCHING_MAXDATALEN 9204
#define SCE_NET_ADHOC_MATCHING_MAXHELLOOPTLEN 1426

struct SceNetInAddr;
DECL_EXPORT(SceInt32, sceNetCtlAdhocGetInAddr, SceNetInAddr *inaddr);

enum SceNetAdhocMatchingErrorCode {
    SCE_NET_ADHOC_MATCHING_OK = 0x0,
    SCE_NET_ADHOC_MATCHING_ERROR_INVALID_MODE = 0x80413101,
    SCE_NET_ADHOC_MATCHING_ERROR_INVALID_PORT = 0x80413102,
    SCE_NET_ADHOC_MATCHING_ERROR_INVALID_MAXNUM = 0x80413103,
    SCE_NET_ADHOC_MATCHING_ERROR_RXBUF_TOO_SHORT = 0x80413104,
    SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN = 0x80413105,
    SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG = 0x80413106,
    SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID = 0x80413107,
    SCE_NET_ADHOC_MATCHING_ERROR_ID_NOT_AVAIL = 0x80413108,
    SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE = 0x80413109,
    SCE_NET_ADHOC_MATCHING_ERROR_IS_RUNNING = 0x8041310a,
    SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING = 0x8041310b,
    SCE_NET_ADHOC_MATCHING_ERROR_UNKNOWN_TARGET = 0x8041310c,
    SCE_NET_ADHOC_MATCHING_ERROR_TARGET_NOT_READY = 0x8041310d,
    SCE_NET_ADHOC_MATCHING_ERROR_EXCEED_MAXNUM = 0x8041310e,
    SCE_NET_ADHOC_MATCHING_ERROR_REQUEST_IN_PROGRESS = 0x8041310f,
    SCE_NET_ADHOC_MATCHING_ERROR_ALREADY_ESTABLISHED = 0x80413110,
    SCE_NET_ADHOC_MATCHING_ERROR_BUSY = 0x80413111,
    SCE_NET_ADHOC_MATCHING_ERROR_ALREADY_INITIALIZED = 0x80413112,
    SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED = 0x80413113,
    SCE_NET_ADHOC_MATCHING_ERROR_PORT_IN_USE = 0x80413114,
    SCE_NET_ADHOC_MATCHING_ERROR_STACKSIZE_TOO_SHORT = 0x80413115,
    SCE_NET_ADHOC_MATCHING_ERROR_INVALID_DATALEN = 0x80413116,
    SCE_NET_ADHOC_MATCHING_ERROR_NOT_ESTABLISHED = 0x80413117,
    SCE_NET_ADHOC_MATCHING_ERROR_DATA_BUSY = 0x80413118,
    SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ALIGNMENT = 0x80413119
};

enum SceNetAdhocMatchingMode : uint8_t {
    SCE_NET_ADHOC_MATCHING_MODE_NONE = 0,
    SCE_NET_ADHOC_MATCHING_MODE_PARENT = 1,
    SCE_NET_ADHOC_MATCHING_MODE_CHILD = 2,
    SCE_NET_ADHOC_MATCHING_MODE_P2P = 3,
    SCE_NET_ADHOC_MATCHING_MODE_MAX = 4,
};

enum SceNetAdhocMatchingHandlerEventType {
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_NONE = 0,
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_HELLO = 1,
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_REQUEST = 2,
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_LEAVE = 3,
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_DENY = 4,
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_CANCEL = 5,
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ACCEPT = 6,
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ESTABLISHED = 7,
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_TIMEOUT = 8,
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_ERROR = 9,
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_BYE = 10,
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_DATA = 11,
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_DATA_ACK = 12,
    SCE_NET_ADHOC_MATCHING_HANDLER_EVENT_DATA_TIMEOUT = 13
};

enum SceNetAdhocMatchingEvent : uint32_t {
    SCE_NET_ADHOC_MATCHING_EVENT_ABORT = 0,
    SCE_NET_ADHOC_MATCHING_EVENT_PACKET = 1,
    SCE_NET_ADHOC_MATCHING_EVENT_REGISTRATION_TIMEOUT = 2,
    SCE_NET_ADHOC_MATCHING_EVENT_TARGET_TIMEOUT = 3,
    SCE_NET_ADHOC_MATCHING_EVENT_HELLO_TIMEOUT = 4,
    SCE_NET_ADHOC_MATCHING_EVENT_DATA_TIMEOUT = 5,
    SCE_NET_ADHOC_MATCHING_EVENT_NOTIFICATION = 6,
};

struct SceNetAdhocHandlerArguments {
    uint32_t id;
    uint32_t event;
    Address peer;
    uint32_t optlen;
    Address opt;
};
static_assert(sizeof(SceNetAdhocHandlerArguments) == 0x14, "SceNetAdhocHandlerArguments is an invalid size");

class SceNetAdhocMatchingContext;
class SceNetAdhocMatchingTarget;

struct SceNetAdhocMatchingPipeMessage {
    SceNetAdhocMatchingEvent type;
    SceNetAdhocMatchingTarget *target;
    bool isSheduled;
};
typedef Address SceNetAdhocMatchingHandler;

class AdhocState {
public:
    ~AdhocState();

    int initializeMutex();
    int deleteMutex();
    std::mutex &getMutex();

    int createMSpace(SceSize poolsize, void *poolptr);
    int deleteMSpace();

    int initializeMatchingContextList();
    int isAnyMatchingContextRunning();
    SceNetAdhocMatchingContext *findMatchingContextById(SceUID id);
    int createMatchingContext(SceUShort16 port);
    void deleteMatchingContext(SceNetAdhocMatchingContext *ctx);
    void deleteAllMatchingContext();

public: // Globals
    bool is_initialized = false;

private:
    bool is_mutex_initialized = false;

    std::mutex mutex;
    SceNetAdhocMatchingContext *contextList = NULL;
    SceUID matchingCtxCount = 1;
};
