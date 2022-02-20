// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <cpu/common.h>
#include <kernel/thread/thread_data_queue.h>
#include <kernel/types.h>
#include <util/byte_ring_buffer.h>

struct KernelState;

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
        struct { // msgpipe
            SceSize request_size;
        } mp;
    };

    bool operator<(const WaitingThreadData &rhs) const {
        return priority < rhs.priority;
    }

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
    // FIXME turn this into SimpleEvent!
    SceUID uid;

    uint32_t attr;

    std::mutex mutex;

    char name[KERNELOBJECT_MAX_NAME_LENGTH + 1];

    virtual ~SyncPrimitive() = default;
};

struct Semaphore : SyncPrimitive {
    WaitingThreadQueuePtr waiting_threads;
    int max;
    int val;

    ~Semaphore() override = default;
};

typedef std::shared_ptr<Semaphore> SemaphorePtr;
typedef std::map<SceUID, SemaphorePtr> SemaphorePtrs;

struct Mutex : SyncPrimitive {
    int init_count;
    int lock_count;
    ThreadStatePtr owner;
    WaitingThreadQueuePtr waiting_threads;
    Ptr<SceKernelLwMutexWork> workarea;

    ~Mutex() override = default;
};

typedef std::shared_ptr<Mutex> MutexPtr;
typedef std::map<SceUID, MutexPtr> MutexPtrs;

struct EventFlag : SyncPrimitive {
    WaitingThreadQueuePtr waiting_threads;
    int flags;

    ~EventFlag() override = default;
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

    ~Condvar() override = default;
};
typedef std::shared_ptr<Condvar> CondvarPtr;
typedef std::map<SceUID, CondvarPtr> CondvarPtrs;

struct MsgPipe : SyncPrimitive {
    MsgPipe(std::size_t bufSize)
        : data_buffer(bufSize) {}

    WaitingThreadQueuePtr senders;
    WaitingThreadQueuePtr receivers;
    ByteRingBuffer data_buffer;

    bool beingDeleted = false;
    std::atomic<std::size_t> remainingThreads = 0;

    ~MsgPipe() override = default;
};

typedef std::shared_ptr<MsgPipe> MsgPipePtr;
typedef std::map<SceUID, MsgPipePtr> MsgPipePtrs;

enum class SyncWeight {
    Light, // lightweight
    Heavy // 'heavy'weight
};

// Mutex
SceUID mutex_create(SceUID *uid_out, KernelState &kernel, MemState &mem, const char *export_name, const char *name, SceUID thread_id, SceUInt attr, int init_count, Ptr<SceKernelLwMutexWork> workarea, SyncWeight weight);
int mutex_lock(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID mutexid, int lock_count, unsigned int *timeout, SyncWeight weight);
int mutex_try_lock(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID mutexid, int lock_count, SyncWeight weight);
int mutex_unlock(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutexid, int unlock_count, SyncWeight weight);
int mutex_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutexid, SyncWeight weight);
MutexPtr mutex_get(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutexid, SyncWeight weight);

// Semaphore
SceUID semaphore_create(KernelState &kernel, const char *export_name, const char *name, SceUID thread_id, SceUInt attr, int initVal, int maxVal);
int semaphore_wait(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID semaid, SceInt32 signal, SceUInt *timeout);
int semaphore_signal(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID semaid, int signal);
int semaphore_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID semaid);

// Condition Variable
SceUID condvar_create(SceUID *uid_out, KernelState &kernel, const char *export_name, const char *name, SceUID thread_id, SceUInt attr, SceUID assoc_mutexid, SyncWeight weight);
int condvar_wait(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID semaid, SceUInt *timeout, SyncWeight weight);
int condvar_signal(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID condid, Condvar::SignalTarget signal_target, SyncWeight weight);
int condvar_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutexid, SyncWeight weight);

// Event Flag
SceUID eventflag_clear(KernelState &kernel, const char *export_name, SceUID evfId, SceUInt32 bitPattern);
SceUID eventflag_create(KernelState &kernel, const char *export_name, SceUID thread_id, const char *pName, SceUInt32 attr, SceUInt32 initPattern);
SceInt32 eventflag_wait(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, SceUInt32 *pResultPat, SceUInt32 *pTimeout);
int eventflag_poll(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID eventflagid, unsigned int flags, unsigned int wait, unsigned int *outBits);
SceInt32 eventflag_set(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID evfId, SceUInt32 bitPattern);
int eventflag_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID event_id);

// Message Pipe
SceUID msgpipe_create(KernelState &kernel, const char *export_name, const char *name, SceUID thread_id, SceUInt attr, SceSize bufSize);
SceUID msgpipe_find(KernelState &kernel, const char *export_name, const char *name);
int msgpipe_recv(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID msgpipe_id, SceUInt32 wait_mode, char *recv_buf, SceSize msg_size, SceUInt32 *timeout);
int msgpipe_send(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID msgpipe_id, SceUInt32 wait_mode, char *send_buf, SceSize msg_size, SceUInt32 *timeout);
SceUID msgpipe_delete(KernelState &kernel, const char *export_name, const char *name, SceUID thread_id, SceUID msgpipe_id);
