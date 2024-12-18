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

#include <chrono>

#include "adhoc/calloutSyncing.h"
#include "adhoc/state.h"
#include "adhoc/threads.h"

int SceNetAdhocMatchingCalloutSyncing::initializeCalloutThread(EmuEnvState &emuenv, SceUID thread_id, int id, int threadPriority, int threadStackSize, int threadCpuAffinityMask) {
    if (this->isInitialized)
        return SCE_NET_CALLOUT_ERROR_NOT_TERMINATED;

    this->shouldExit = false;
    this->calloutThread = std::thread(adhocMatchingCalloutThread, std::ref(emuenv), id);
    this->isInitialized = true;

    return SCE_NET_ADHOC_MATCHING_OK;
}

void SceNetAdhocMatchingCalloutSyncing::closeCalloutThread() {
    if (!this->isInitialized)
        return;

    this->shouldExit = true;
    this->condvar.notify_all();

    if (this->calloutThread.joinable())
        this->calloutThread.join();

    this->isInitialized = false;
}

int SceNetAdhocMatchingCalloutSyncing::addTimedFunction(SceNetAdhocMatchingCalloutFunction *calloutFunction, SceLong64 interval, int (*function)(void *), void *args) {
    if (!this->isInitialized)
        return SCE_NET_CALLOUT_ERROR_NOT_INITIALIZED;

    uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    calloutFunction->function = function;
    calloutFunction->args = args;
    calloutFunction->execAt = now + interval;

    std::lock_guard<std::mutex> guard(this->mutex);
    SceNetAdhocMatchingCalloutFunction *previous_entry = nullptr;
    SceNetAdhocMatchingCalloutFunction *entry = this->functionList;

    // Search for duplicates
    for (; entry != nullptr; entry = entry->next) {
        if (calloutFunction == entry) {
            return SCE_NET_CALLOUT_ERROR_DUPLICATED;
        }
    }

    // Order in ascending order
    for (entry = this->functionList; entry != nullptr; entry = entry->next) {
        if (calloutFunction->execAt > entry->execAt) {
            break;
        }
        previous_entry = entry;
    }

    // Add calloutFunction to list
    calloutFunction->next = entry;
    if (previous_entry != nullptr) {
        previous_entry->next = calloutFunction;
    } else {
        this->functionList = calloutFunction;
    }

    this->condvar.notify_one();
    return SCE_NET_CALLOUT_OK;
}

int SceNetAdhocMatchingCalloutSyncing::deleteTimedFunction(SceNetAdhocMatchingCalloutFunction *calloutFunction, bool *is_deleted) {
    if (!this->isInitialized)
        return SCE_NET_CALLOUT_ERROR_NOT_INITIALIZED;

    std::lock_guard<std::mutex> guard(this->mutex);
    SceNetAdhocMatchingCalloutFunction *entry = this->functionList;
    SceNetAdhocMatchingCalloutFunction *previous_entry = nullptr;

    for (; entry != nullptr; entry = entry->next) {
        if (calloutFunction == entry) {
            break;
        }
        previous_entry = entry;
    }

    if (entry == nullptr) {
        if (is_deleted != nullptr) {
            *is_deleted = false;
        }
        return SCE_NET_CALLOUT_OK;
    }

    if (previous_entry != nullptr) {
        previous_entry->next = entry->next;
    } else {
        this->functionList = entry->next;
    }

    if (is_deleted != nullptr) {
        *is_deleted = true;
    }

    return SCE_NET_CALLOUT_OK;
}

int SceNetAdhocMatchingCalloutSyncing::excecuteTimedFunctions() {
    mutex.lock();

    auto *entry = functionList;
    uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    while (entry != nullptr && entry->execAt < now) {
        mutex.unlock();
        entry->function(entry->args);
        mutex.lock();

        functionList = entry->next;
        entry = functionList;
    }

    if (shouldExit) {
        mutex.unlock();
        return 0;
    }

    uint64_t sleep_time = 0;
    if (entry != nullptr) {
        sleep_time = entry->execAt - now;
    }

    mutex.unlock();
    return sleep_time;
}

bool SceNetAdhocMatchingCalloutSyncing::isRunning() const {
    return !shouldExit;
}
