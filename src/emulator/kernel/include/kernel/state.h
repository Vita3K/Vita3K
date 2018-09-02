// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <kernel/thread/thread_state.h>
#include <kernel/types.h>
#include <mem/ptr.h>

#include <psp2/rtc.h>
#include <psp2/sysmodule.h>
#include <psp2/types.h>

#include <atomic>
#include <map>
#include <mutex>
#include <queue>
#include <vector>

struct ThreadState;

struct SDL_Thread;

typedef std::map<SceUID, Ptr<void>> Blocks;
typedef std::map<SceUID, Ptr<Ptr<void>>> SlotToAddress;
typedef std::map<SceUID, SlotToAddress> ThreadToSlotToAddress;
typedef std::shared_ptr<ThreadState> ThreadStatePtr;
typedef std::map<SceUID, ThreadStatePtr> ThreadStatePtrs;
typedef std::shared_ptr<SDL_Thread> ThreadPtr;
typedef std::map<SceUID, ThreadPtr> ThreadPtrs;
typedef std::shared_ptr<emu::SceKernelModuleInfo> SceKernelModuleInfoPtr;
typedef std::map<SceUID, SceKernelModuleInfoPtr> SceKernelModuleInfoPtrs;
typedef std::map<uint32_t, Address> ExportNids;

struct WaitingThreadData {
    ThreadStatePtr thread;
    int32_t priority;

    // additional fields for each primitive
    union {
        struct { // mutex
            int32_t lock_count;
        };
        struct { // semaphore
            int32_t signal;
        };
        struct { // event flags
            int32_t wait;
            int32_t flags;
        };
        struct { // condvar
        };
    };

    bool operator>(const WaitingThreadData &rhs) const {
        return priority > rhs.priority;
    }

    bool operator==(const WaitingThreadData &rhs) const {
        return thread == rhs.thread;
    }

    bool operator==(const ThreadStatePtr &rhs) const {
        return thread == rhs;
    }
};

class WaitingThreadQueue : public std::priority_queue<WaitingThreadData, std::vector<WaitingThreadData>, std::greater<WaitingThreadData>> {
public:
    auto begin() const { return this->c.begin(); }
    auto end() const { return this->c.end(); }
    bool remove(const WaitingThreadData &value) {
        auto it = std::find(this->c.begin(), this->c.end(), value);
        if (it != this->c.end()) {
            this->c.erase(it);
            std::make_heap(this->c.begin(), this->c.end(), this->comp);
            return true;
        } else {
            return false;
        }
    }
    auto find(const WaitingThreadData &value) {
        return std::find(this->c.begin(), this->c.end(), value);
    }
    auto find(const ThreadStatePtr &value) {
        return std::find(this->c.begin(), this->c.end(), value);
    }
};

// NOTE: uid is copied to sync primitives here for debugging,
//       not really needed since they are put in std::map's

struct Semaphore {
    SceUID uid;
    int val;
    int max;
    uint32_t attr;
    std::mutex mutex;
    WaitingThreadQueue waiting_threads;
    char name[KERNELOBJECT_MAX_NAME_LENGTH + 1];
};
typedef std::shared_ptr<Semaphore> SemaphorePtr;
typedef std::map<SceUID, SemaphorePtr> SemaphorePtrs;

struct Mutex {
    SceUID uid;
    int lock_count;
    uint32_t attr;
    std::mutex mutex;
    WaitingThreadQueue waiting_threads;
    ThreadStatePtr owner;
    char name[KERNELOBJECT_MAX_NAME_LENGTH + 1];
};

struct EventFlag {
    SceUID uid;
    int flags;
    uint32_t attr;
    std::mutex mutex;
    WaitingThreadQueue waiting_threads;
    char name[KERNELOBJECT_MAX_NAME_LENGTH + 1];
};

typedef std::shared_ptr<Semaphore> SemaphorePtr;
typedef std::map<SceUID, SemaphorePtr> SemaphorePtrs;
typedef std::shared_ptr<Mutex> MutexPtr;
typedef std::map<SceUID, MutexPtr> MutexPtrs;
typedef std::shared_ptr<EventFlag> EventFlagPtr;
typedef std::map<SceUID, EventFlagPtr> EventFlagPtrs;

struct Condvar {
    struct SignalTarget {
        enum class Type {
            Any, // signal any one waiting thread
            Specific, // signal a specific waiting thread (target_thread)
            All, // signal all waiting threads
        } type;

        SceUID thread_id; // for Type::One

        SignalTarget(Type type)
            : type(type)
            , thread_id(0) {}
        SignalTarget(Type type, SceUID thread_id)
            : type(type)
            , thread_id(thread_id) {}
    };

    SceUID uid;
    uint32_t attr;
    MutexPtr associated_mutex;
    std::mutex mutex;
    WaitingThreadQueue waiting_threads;
    char name[KERNELOBJECT_MAX_NAME_LENGTH + 1];
};
typedef std::shared_ptr<Condvar> CondvarPtr;
typedef std::map<SceUID, CondvarPtr> CondvarPtrs;

namespace emu {
typedef Ptr<int(SceSize args, Ptr<void> argp)> SceKernelThreadEntry;
}

struct WaitingThreadState {
    std::string name; // for debugging
};

typedef std::map<SceUID, WaitingThreadState> KernelWaitingThreadStates;

using LoadedSysmodules = std::vector<SceSysmoduleModuleId>;

struct KernelState {
    std::mutex mutex;
    Blocks blocks;
    ThreadToSlotToAddress tls;
    SemaphorePtrs semaphores;
    CondvarPtrs condvars;
    CondvarPtrs lwcondvars;
    MutexPtrs mutexes;
    MutexPtrs lwmutexes; // also Mutexes for now
    EventFlagPtrs eventflags;
    ThreadStatePtrs threads;
    ThreadPtrs running_threads;
    KernelWaitingThreadStates waiting_threads;
    SceKernelModuleInfoPtrs loaded_modules;
    LoadedSysmodules loaded_sysmodules;
    ExportNids export_nids;
    SceRtcTick base_tick;
    Ptr<uint32_t> process_param;

    SceUID get_next_uid() {
        return next_uid++;
    }

private:
    std::atomic<SceUID> next_uid{ 0 };
};
