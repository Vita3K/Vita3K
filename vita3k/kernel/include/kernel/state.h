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

#include <rtc/rtc.h>

#include <atomic>
#include <map>
#include <mutex>
#include <queue>
#include <vector>

struct ThreadState;

struct SDL_Thread;

struct WatchMemory;

typedef std::shared_ptr<SceKernelMemBlockInfo> SceKernelMemBlockInfoPtr;
typedef std::map<SceUID, SceKernelMemBlockInfoPtr> Blocks;
typedef std::map<SceUID, Ptr<Ptr<void>>> SlotToAddress;
typedef std::map<SceUID, SlotToAddress> ThreadToSlotToAddress;
typedef std::shared_ptr<ThreadState> ThreadStatePtr;
typedef std::map<SceUID, ThreadStatePtr> ThreadStatePtrs;
typedef std::shared_ptr<SDL_Thread> ThreadPtr;
typedef std::map<SceUID, ThreadPtr> ThreadPtrs;
typedef std::shared_ptr<SceKernelModuleInfo> SceKernelModuleInfoPtr;
typedef std::map<SceUID, SceKernelModuleInfoPtr> SceKernelModuleInfoPtrs;
typedef std::map<uint32_t, Address> ExportNids;
typedef std::map<Address, uint32_t> NidFromExport;
typedef std::map<Address, uint32_t> NotFoundVars;
typedef std::map<Address, WatchMemory> WatchMemoryAddrs;

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
struct SyncPrimitive {
    SceUID uid;

    uint32_t attr;

    std::mutex mutex;
    WaitingThreadQueue waiting_threads;

    char name[KERNELOBJECT_MAX_NAME_LENGTH + 1];
};

struct Semaphore : SyncPrimitive {
    int max;
    int val;
};

typedef std::shared_ptr<Semaphore> SemaphorePtr;
typedef std::map<SceUID, SemaphorePtr> SemaphorePtrs;

struct Mutex : SyncPrimitive {
    int lock_count;
    ThreadStatePtr owner;
};

typedef std::shared_ptr<Mutex> MutexPtr;
typedef std::map<SceUID, MutexPtr> MutexPtrs;

struct EventFlag : SyncPrimitive {
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

    MutexPtr associated_mutex;
};
typedef std::shared_ptr<Condvar> CondvarPtr;
typedef std::map<SceUID, CondvarPtr> CondvarPtrs;

struct WaitingThreadState {
    std::string name; // for debugging
};

typedef std::map<SceUID, WaitingThreadState> KernelWaitingThreadStates;

struct AVPacket;
struct AVCodecContext;
struct AVFormatContext;
struct AVCodecParserContext;

struct VideoDecoderState {
    AVCodecContext *context{};
    AVCodecParserContext *parser{};

    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t frame_ref_count;
};

typedef std::shared_ptr<VideoDecoderState> DecoderPtr;
typedef std::map<SceUID, DecoderPtr> DecoderStates;

struct PlayerState {
    Ptr<void> general_allocator;
    Ptr<void> general_deallocator;
    Ptr<void> texture_allocator;
    Ptr<void> texture_deallocator;

    std::string video_playing;
    std::queue<std::string> videos_queue;

    AVFormatContext *format{};
    AVCodecContext *video_context{};
    AVCodecContext *audio_context{};
    int32_t video_stream_id = 0;
    int32_t audio_stream_id = 0;

    std::queue<AVPacket *> audio_packets;
    std::queue<AVPacket *> video_packets;

    // A ring buffer is used to lower the chances of a race condition between rendering and decoding thread.
    constexpr static uint32_t RING_BUFFER_COUNT = 4;

    uint32_t video_buffer_ring_index = 0;
    uint32_t video_buffer_size = 0;
    Ptr<uint8_t> video_buffer[RING_BUFFER_COUNT];

    uint32_t audio_buffer_ring_index = 0;
    uint32_t audio_buffer_size = 0;
    Ptr<uint8_t> audio_buffer[RING_BUFFER_COUNT];

    uint64_t time_of_last_frame = 0;

    uint64_t last_timestamp = 0;
    uint32_t last_channels = 0;
    uint32_t last_sample_rate = 0;
    uint32_t last_sample_count = 0;

    bool do_loop = false;
    bool paused = false;
};

typedef std::shared_ptr<PlayerState> PlayerPtr;
typedef std::map<SceUID, PlayerPtr> PlayerStates;

struct MJpegState {
    bool initialized = false;

    AVCodecContext *decoder{};
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

struct WatchMemory {
    Address start;
    size_t size;
};

struct KernelState {
    std::mutex mutex;
    Blocks blocks;
    Blocks vm_blocks;
    ThreadToSlotToAddress tls;
    Ptr<const void> tls_address;
    unsigned int tls_psize;
    unsigned int tls_msize;
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
    NidFromExport nid_from_export;
    NotFoundVars not_found_vars;
    WatchMemoryAddrs watch_memory_addrs;

    SceRtcTick start_tick;
    SceRtcTick base_tick;
    DecoderStates decoders;
    PlayerStates players;
    MJpegState mjpeg_state;
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
