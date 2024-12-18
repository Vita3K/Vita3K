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

#include <condition_variable>
#include <mutex>
#include <thread>

#include "util/types.h"

struct EmuEnvState;

enum SceNetCalloutErrorCode {
    SCE_NET_CALLOUT_OK = 0x0,
    SCE_NET_CALLOUT_ERROR_NOT_INITIALIZED = 0x80558001,
    SCE_NET_CALLOUT_ERROR_NOT_TERMINATED = 0x80558002,
    SCE_NET_CALLOUT_ERROR_DUPLICATED = 0x80558006,
};

struct SceNetAdhocMatchingCalloutFunction {
    SceNetAdhocMatchingCalloutFunction *next;
    uint64_t execAt;
    int (*function)(void *);
    void *args;
};

class SceNetAdhocMatchingCalloutSyncing {
public:
    int initializeCalloutThread(EmuEnvState &emuenv, SceUID thread_id, SceUID id, int threadPriority, int threadStackSize, int threadCpuAffinityMask);
    void closeCalloutThread();

    int addTimedFunction(SceNetAdhocMatchingCalloutFunction *calloutFunction, SceLong64 interval, int (*function)(void *), void *args);
    int deleteTimedFunction(SceNetAdhocMatchingCalloutFunction *calloutFunction, bool *is_deleted = nullptr);
    int excecuteTimedFunctions();

    bool isRunning() const;

public:
    SceNetAdhocMatchingCalloutFunction *functionList;

private:
    std::thread calloutThread;
    std::mutex mutex;
    std::condition_variable condvar;
    bool isInitialized;
    bool shouldExit;
};
