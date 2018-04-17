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

#include <kernel/types.h>
#include <mem/ptr.h>

#include <psp2/kernel/modulemgr.h>
#include <psp2/rtc.h>
#include <psp2/types.h>

#include <map>
#include <mutex>
#include <vector>

struct ThreadState;

struct SDL_semaphore;
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

struct Semaphore {
    int val;
    int max;
    std::mutex mutex;
    std::vector<ThreadStatePtr> locked;
    std::string name;
};

struct Mutex {
    int lock_count;
    std::mutex mutex;
    std::vector<ThreadStatePtr> locked;
    std::string name;
};

typedef std::shared_ptr<Semaphore> SemaphorePtr;
typedef std::map<SceUID, SemaphorePtr> SemaphorePtrs;
typedef std::shared_ptr<Mutex> MutexPtr;
typedef std::map<SceUID, MutexPtr> MutexPtrs;

namespace emu {
    typedef Ptr<int(SceSize args, Ptr<void> argp)> SceKernelThreadEntry;
}

struct WaitingThreadState {
    std::string name;
};

typedef std::map<SceUID, WaitingThreadState> WaitingThreadStates;

struct KernelState {
    std::mutex mutex;
    Blocks blocks;
    SceUID next_uid = 0;
    ThreadToSlotToAddress tls;
    SemaphorePtrs semaphores;
    MutexPtrs mutexes;
    ThreadStatePtrs threads;
    ThreadPtrs running_threads;
    WaitingThreadStates waiting_threads;
    SceKernelModuleInfoPtrs loaded_modules;
    ExportNids export_nids;
    SceRtcTick base_tick;
    Ptr<uint32_t> process_param;
};
