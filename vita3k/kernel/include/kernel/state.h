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

#include <codec/state.h>
#include <cpu/functions.h>
#include <kernel/thread/thread_state.h>
#include <kernel/thread_data_queue.h>
#include <kernel/types.h>
#include <mem/ptr.h>
#include <rtc/rtc.h>
#include <util/pool.h>

#include <atomic>
#include <map>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

struct ThreadState;

struct SDL_Thread;

struct WatchMemory;

struct InitialFiber;

struct CodecEngineBlock;

typedef std::vector<InitialFiber> InitialFibers;

typedef std::shared_ptr<SceKernelMemBlockInfo> SceKernelMemBlockInfoPtr;
typedef std::map<SceUID, SceKernelMemBlockInfoPtr> Blocks;
typedef std::map<SceUID, CodecEngineBlock> CodecEngineBlocks;
typedef std::map<SceUID, Ptr<Ptr<void>>> SlotToAddress;
typedef std::map<SceUID, ThreadStatePtr> ThreadStatePtrs;
typedef std::shared_ptr<SDL_Thread> ThreadPtr;
typedef std::map<SceUID, ThreadPtr> ThreadPtrs;
typedef std::shared_ptr<SceKernelModuleInfo> SceKernelModuleInfoPtr;
typedef std::map<SceUID, SceKernelModuleInfoPtr> SceKernelModuleInfoPtrs;
typedef std::unordered_map<uint32_t, Address> ExportNids;
typedef std::map<Address, uint32_t> NidFromExport;
typedef std::map<Address, uint32_t> NotFoundVars;
typedef std::map<Address, WatchMemory> WatchMemoryAddrs;
typedef std::vector<ModuleRegion> ModuleRegions;
typedef Pool<CPUState> CPUPool;

struct MsgPipeData;

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
        // struct { }; // condvar
        struct { //msgpipe
            MsgPipeData *sending_data;
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

typedef std::unique_ptr<ThreadDataQueue<WaitingThreadData>> WaitingThreadQueuePtr;

// NOTE: uid is copied to sync primitives here for debugging,
//       not really needed since they are put in std::map's
struct SyncPrimitive {
    SceUID uid;

    uint32_t attr;

    std::mutex mutex;

    char name[KERNELOBJECT_MAX_NAME_LENGTH + 1];
};

struct Semaphore : SyncPrimitive {
    WaitingThreadQueuePtr waiting_threads;
    int max;
    int val;
};

typedef std::shared_ptr<Semaphore> SemaphorePtr;
typedef std::map<SceUID, SemaphorePtr> SemaphorePtrs;

struct Mutex : SyncPrimitive {
    int init_count;
    int lock_count;
    ThreadStatePtr owner;
    WaitingThreadQueuePtr waiting_threads;
    Ptr<SceKernelLwMutexWork> workarea;
};

typedef std::shared_ptr<Mutex> MutexPtr;
typedef std::map<SceUID, MutexPtr> MutexPtrs;

struct EventFlag : SyncPrimitive {
    WaitingThreadQueuePtr waiting_threads;
    int flags;
};

typedef std::shared_ptr<EventFlag> EventFlagPtr;
typedef std::map<SceUID, EventFlagPtr> EventFlagPtrs;

struct Condvar : SyncPrimitive {
    struct SignalTarget {
        enum class Type {
            Any, // signal any one waiting thread
            Specific, // signal a specific waiting thread (target_thread)
            All, // signal all waiting threads
        } type;

        SceUID thread_id; // for Type::One

        explicit SignalTarget(Type type)
            : type(type)
            , thread_id(0) {}
        SignalTarget(Type type, SceUID thread_id)
            : type(type)
            , thread_id(thread_id) {}
    };

    WaitingThreadQueuePtr waiting_threads;
    MutexPtr associated_mutex;
};
typedef std::shared_ptr<Condvar> CondvarPtr;
typedef std::map<SceUID, CondvarPtr> CondvarPtrs;

struct MsgPipeData {
    std::vector<char> data;
    SceUID thread_id;
    size_t read_size;
    bool waiting_sender;
    bool notify_at_empty;
};

// Unlimited buffer for now
struct MsgPipe : SyncPrimitive {
    std::mutex recv_mutex;
    WaitingThreadQueuePtr sender_threads;
    WaitingThreadQueuePtr reciever_threads;
    std::list<MsgPipeData> data_buffer;
};

typedef std::shared_ptr<MsgPipe> MsgPipePtr;
typedef std::map<SceUID, MsgPipePtr> MsgPipePtrs;

struct WaitingThreadState {
    std::string name; // for debugging
};

typedef std::map<SceUID, WaitingThreadState> KernelWaitingThreadStates;

typedef std::shared_ptr<DecoderState> DecoderPtr;
typedef std::map<SceUID, DecoderPtr> DecoderStates;

struct SceAvPlayerMemoryAllocator {
    uint32_t user_data;

    // All of these should be cast to SceAvPlayerAllocator or SceAvPlayerDeallocator types.
    Ptr<void> general_allocator;
    Ptr<void> general_deallocator;
    Ptr<void> texture_allocator;
    Ptr<void> texture_deallocator;
};

struct SceAvPlayerFileManager {
    uint32_t user_data;

    // Cast to SceAvPlayerOpenFile, SceAvPlayerCloseFile, SceAvPlayerReadFile and SceAvPlayerGetFileSize.
    Ptr<void> open_file;
    Ptr<void> close_file;
    Ptr<void> read_file;
    Ptr<void> file_size;
};

struct SceAvPlayerEventManager {
    uint32_t user_data;

    // Cast to SceAvPlayerEventCallback.
    Ptr<void> event_callback;
};

struct PlayerInfoState {
    PlayerState player;

    // Framebuffer count is defined in info. I'm being safe now and forcing it to 4 (even though its usually 2).
    constexpr static uint32_t RING_BUFFER_COUNT = 4;

    uint32_t video_buffer_ring_index = 0;
    uint32_t video_buffer_size = 0;
    std::array<Ptr<uint8_t>, RING_BUFFER_COUNT> video_buffer;

    uint32_t audio_buffer_ring_index = 0;
    uint32_t audio_buffer_size = 0;
    std::array<Ptr<uint8_t>, RING_BUFFER_COUNT> audio_buffer;

    bool do_loop = false;
    bool paused = false;

    uint64_t last_frame_time = 0;
    SceAvPlayerMemoryAllocator memory_allocator;
    SceAvPlayerFileManager file_manager;
    SceAvPlayerEventManager event_manager;
};

typedef std::shared_ptr<PlayerInfoState> PlayerPtr;
typedef std::map<SceUID, PlayerPtr> PlayerStates;

struct MJpegState {
    bool initialized = false;

    AVCodecContext *decoder{};
};

struct CodecEngineBlock {
    uint32_t size;
    int32_t vaddr;
};

struct TimerState {
    std::string name;

    enum class ThreadBehaviour {
        FIFO,
        PRIORITY,
    };

    enum class NotifyBehaviour {
        ALL,
        ONLY_WAKE
    };

    enum class ResetBehaviour {
        MANUAL,
        AUTOMATIC,
    };

    bool openable = false;
    ThreadBehaviour thread_behaviour = ThreadBehaviour::FIFO;
    NotifyBehaviour notify_behaviour = NotifyBehaviour::ALL;
    ResetBehaviour reset_behaviour = ResetBehaviour::MANUAL;

    bool is_started = false;
    bool repeats = false;
    uint64_t time = 0;
};

typedef std::shared_ptr<TimerState> TimerPtr;
typedef std::map<SceUID, TimerPtr> TimerStates;

using LoadedSysmodules = std::vector<SceSysmoduleModuleId>;

struct SceFiber;

struct InitialFiber {
    Address start;
    Address end;
    SceFiber *fiber;
};

struct WatchMemory {
    Address start;
    size_t size;
};

struct KernelState {
    std::mutex mutex;
    Blocks blocks;
    Blocks vm_blocks;
    CodecEngineBlocks codec_blocks;
    Ptr<const void> tls_address;
    unsigned int tls_psize;
    unsigned int tls_msize;
    SemaphorePtrs semaphores;
    CondvarPtrs condvars;
    CondvarPtrs lwcondvars;
    MutexPtrs mutexes;
    MutexPtrs lwmutexes; // also Mutexes for now
    EventFlagPtrs eventflags;
    MsgPipePtrs msgpipes;
    ThreadStatePtrs threads;
    ThreadPtrs running_threads;
    KernelWaitingThreadStates waiting_threads;
    SceKernelModuleInfoPtrs loaded_modules;
    LoadedSysmodules loaded_sysmodules;
    ExportNids export_nids;
    NidFromExport nid_from_export;
    NotFoundVars not_found_vars;
    WatchMemoryAddrs watch_memory_addrs;
    ModuleRegions module_regions;
    ExclusiveMonitorPtr exclusive_monitor;
    CPUPool cpu_pool;
    CPUBackend cpu_backend;
    CPUProtocolBase *cpu_protocol;

    InitialFibers initial_fibers;
    SceRtcTick start_tick;
    SceRtcTick base_tick;
    DecoderPtr mjpeg_state;
    DecoderStates decoders;
    PlayerStates players;
    TimerStates timers;
    Ptr<uint32_t> process_param;

    bool wait_for_debugger = false;
    bool watch_import_calls = false;
    bool watch_code = false;
    bool watch_memory = false;

    SceUID get_next_uid() {
        return next_uid++;
    }

private:
    std::atomic<SceUID> next_uid{ 0 };
};
