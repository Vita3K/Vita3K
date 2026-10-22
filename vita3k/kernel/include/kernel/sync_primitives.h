// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <kernel/kernel_object.h>
#include <kernel/thread/thread_state.h>
#include <kernel/thread/wait_queue.h>
#include <kernel/types.h>
#include <util/byte_ring_buffer.h>

#include <expected>
#include <limits>
#include <span>
#include <string>
#include <variant>

class SyncPrimitive : public KernelObject {
public:
    // Returns the error for a name the guest can't use, or 0.
    static SceInt32 check_name(const char *name, SceUInt32 attr);

    const SceUInt32 attr;
    const std::string name;

protected:
    SyncPrimitive(SceUInt32 attr, const char *name);

    // Gives what the object holds to the waiters it can complete, in queue order. Runs with the lock
    // held, from each operation that gives the object something to hand out, and from wait_on() when
    // a waiter leaves without being given anything. An object with nothing to hand out keeps this
    // default.
    virtual void serve() {}

    // Ends a queued wait of thread that its deadline ended, before the waiters behind it are served.
    // Not for a wait whose deadline passed while it was out of the queue for its callbacks, which
    // ends without being queued again.
    virtual void timed_out(const ThreadStatePtr &) {}

    // Runs the thread's pending callbacks with the lock released, which a wait with callbacks does once
    // its checks passed and before it takes. The lock stays held when none would run. Returns
    // WAIT_DELETE if the object was deleted meanwhile.
    [[nodiscard]] WaitResult run_pending_callbacks(std::unique_lock<std::mutex> &lock, const ThreadStatePtr &thread, bool callbacks) {
        if (!callbacks || !thread->has_pending_callbacks())
            return SCE_KERNEL_OK;
        lock.unlock();
        WaitResult r = thread->run_callbacks();
        lock.lock();
        if (r && was_deleted())
            r = SCE_KERNEL_ERROR_WAIT_DELETE;
        return r;
    }

    // Blocks thread on target in queue until a waker gives it a result, the object is deleted, or
    // the guest timeout passes, and writes the time left back to the timeout. A waiter that leaves
    // without a waker, by its deadline, its thread's end or a callback, serves the waiters behind
    // it. The thread runs its callbacks out of the queue, then the waiter goes back in by priority.
    template <typename Entry>
    [[nodiscard]] WaitResult wait_on(WaitQueue<Entry> &queue, std::unique_lock<std::mutex> &lock, const ThreadStatePtr &thread, WaitTarget target, Entry entry, SceUInt32 *timeout) {
        typename WaitQueue<Entry>::Waiter waiter{ .thread = thread, .entry = std::move(entry) };
        const Deadline deadline = deadline_from(timeout);
        // A deadline already gone ends the wait before it is queued, so no waker can serve it and
        // nothing that ends a queued wait, like timed_out(), runs
        if (std::chrono::steady_clock::now() >= deadline) {
            writeback_timeout(timeout, deadline);
            return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
        }
        // The time left when the waiter left for its callbacks, kept if it gets what it waits for at
        // once after them
        std::optional<SceUInt32> time_left;
        WaitResult r;
        waiter.priority = thread->get_priority();
        queue.push(waiter);
        while (true) {
            lock.unlock();
            r = thread->wait(target, deadline);
            lock.lock();
            // A waker's result wins, it already handed over what the waiter waits for
            if (waiter.result)
                break;
            // Woken without a result, such as by a wake left over from an earlier wait, so it
            // waits on
            if (r && *r == SCE_KERNEL_OK)
                continue;
            queue.remove(waiter);
            if (r && *r == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
                timed_out(thread);
            serve();

            // The thread runs its callbacks out of the queue, then the waiter goes back in by its
            // priority unless they ended the wait
            if (r && *r == SCE_KERNEL_ERROR_NOTIFY_CALLBACK) {
                const SceUInt32 left = time_left_until(deadline);
                lock.unlock();
                r = thread->run_callbacks();
                lock.lock();
                if (r && was_deleted()) {
                    r = SCE_KERNEL_ERROR_WAIT_DELETE;
                } else if (r) {
                    // It gets what it waits for at once if no waiter ahead of it goes first
                    waiter.priority = thread->get_priority();
                    queue.push(waiter);
                    serve();
                    if (waiter.result) {
                        time_left = left;
                    } else if (std::chrono::steady_clock::now() >= deadline) {
                        // The deadline passed during the callbacks, so the wait ends without
                        // blocking again
                        queue.remove(waiter);
                        r = SCE_KERNEL_ERROR_WAIT_TIMEOUT;
                    } else {
                        continue;
                    }
                }
            }
            break;
        }
        if (waiter.result)
            r = *waiter.result;
        if (timeout && time_left)
            *timeout = *time_left;
        else
            writeback_timeout(timeout, deadline);
        return r;
    }
};

// The ID of the one process Vita3K runs
constexpr SceUID PROCESS_ID = 1;

// Returns the user data of a system event a thread caused, like a delete: its UID in the high word
// and the process ID in the low word.
constexpr SceUInt64 system_event_user_data(SceUID thread_id) {
    return (static_cast<SceUInt64>(thread_id) << 32) | static_cast<SceUInt32>(PROCESS_ID);
}

class SimpleEvent final : public WithUidClass<SyncPrimitive, UidClass::simple_event> {
public:
    const bool auto_reset;

    SimpleEvent(SceUInt32 attr, const char *name, SceUInt32 init_pattern);

    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 attr, SceUInt32 init_pattern);

    SceInt32 wait(const ThreadStatePtr &thread, SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool callbacks);
    SceInt32 poll(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data);
    SceInt32 set_or_pulse(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set);
    // Sets one of the system events, like the open event, with user_data for its waiters, or returns
    // UNKNOWN_EVENT_ID if it was deleted.
    SceInt32 set_system_event(SceUInt32 event, SceUInt64 user_data);
    SceInt32 clear(SceUInt32 clear_pattern);
    SceInt32 cancel(SceUInt32 *num_wait_threads);
    // Cancels the waits like cancel(), then replaces the whole pattern and the user data.
    SceInt32 cancel(SceUInt32 set_pattern, SceUInt64 user_data, SceUInt32 *num_wait_threads);
    // Sets result to the event's bit pattern, or returns UNKNOWN_EVENT_ID if it was deleted.
    SceInt32 get_pattern(SceUInt32 &result) const;
    // Fills info with what the simple event's info reports, or returns UNKNOWN_EVENT_ID if it was
    // deleted.
    SceInt32 info(SceKernelEventInfo &info) const;

private:
    struct WaitEntry {
        SceUInt32 pattern;
        SceUInt32 *result_pattern;
        SceUInt64 *user_data;
    };

    void on_delete(SceUID deleter) override;
    // Takes the event if wait_pattern matches, or returns SCE_KERNEL_ERROR_EVENT_COND. The lock must be held.
    SceInt32 try_take(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data);
    // Sets or pulses the bits with user_data and gives them to the waits they match. The lock must be
    // held.
    void fire(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set);
    // Gives the event's bits to the waits they match.
    void serve() override;

    // Guarded by the lock
    WaitQueue<WaitEntry> waiters;
    SceUInt32 pattern;
    SceUInt64 last_user_data = 0;
};

typedef std::shared_ptr<SimpleEvent> SimpleEventPtr;

class Timer final : public WithUidClass<SyncPrimitive, UidClass::timer> {
public:
    Timer(SceUInt32 attr, const char *name);

    SceInt32 set_event(SceUID type, SceKernelSysClock interval, SceInt32 repeats);
    SceInt32 wait(const ThreadStatePtr &thread, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool callbacks);
    SceInt32 poll(SceUInt32 *result_pattern, SceUInt64 *user_data);
    SceInt32 clear();
    SceInt32 start();
    SceInt32 stop();
    SceInt32 cancel(SceUInt32 *num_wait_threads);
    // Replaces the count and returns its old value, or the error.
    std::expected<uint64_t, SceInt32> set_time(uint64_t new_time);
    // Sets time to how much time the timer counted, or returns UNKNOWN_TIMER_ID if it was deleted.
    SceInt32 get_time(uint64_t &time) const;
    // Sets time to the clock time the count runs from, 0 if the timer never started, or returns
    // UNKNOWN_TIMER_ID if it was deleted.
    SceInt32 get_base(uint64_t &time) const;
    // Fills info with what the event info of the timer reports, or returns UNKNOWN_EVENT_ID if it was
    // deleted.
    SceInt32 event_info(SceKernelEventInfo &info) const;

private:
    struct WaitEntry {
        SceUInt32 *result_pattern;
    };

    void on_delete(SceUID deleter) override;
    void schedule_event();
    // Wakes the first waiter so it checks the event again
    void wake_first();
    // Moves next_event to the first event after current_time, or to never if the event doesn't repeat.
    void advance_next_event(uint64_t current_time);
    // Takes the event if it is set by current_time, or returns SCE_KERNEL_ERROR_EVENT_COND. The lock must be held.
    SceInt32 try_take(SceUInt32 *result_pattern, SceUInt64 *user_data, uint64_t current_time);
    // Returns how much time the timer counted. The lock must be held.
    uint64_t count() const;

    // Guarded by the lock
    // Only the first waiter waits for the next event, the others wait for their turn
    WaitQueue<WaitEntry> waiters;

    bool is_started = false;
    bool is_repeat = false;
    bool is_pulse = false;
    bool event_set = false;
    // The clock time the count runs from while the timer is started, 0 if it never started
    uint64_t base = 0;
    // The count while the timer is stopped
    uint64_t stopped_count = 0;
    uint64_t next_event = std::numeric_limits<uint64_t>::max();
    uint64_t event_interval = 0;
};

typedef std::shared_ptr<Timer> TimerPtr;

class Semaphore final : public WithUidClass<SyncPrimitive, UidClass::semaphore> {
public:
    const SceInt32 init_val;
    const SceInt32 max;

    Semaphore(SceUInt32 attr, const char *name, SceInt32 init_val, SceInt32 max_val);

    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 attr, SceInt32 init_val, SceInt32 max_val);

    SceInt32 wait(const ThreadStatePtr &thread, SceInt32 need_count, SceUInt32 *timeout, bool callbacks);
    SceInt32 poll(const ThreadStatePtr &thread, SceInt32 need_count);
    SceInt32 signal(SceInt32 count);
    SceInt32 cancel(SceInt32 set_count, SceUInt32 *num_wait_threads);
    // Fills info with what the semaphore's info reports, or returns UNKNOWN_SEMA_ID if it was deleted.
    SceInt32 info(SceKernelSemaInfo &info) const;

private:
    struct WaitEntry {
        SceInt32 need_count;
    };

    void on_delete(SceUID deleter) override;
    // Takes need_count if the count covers it and no waiter is queued ahead of thread. The lock
    // must be held.
    bool try_take(const ThreadStatePtr &thread, SceInt32 need_count);
    // Gives the count to the waits it covers, in queue order up to the first it doesn't.
    void serve() override;

    // Guarded by the lock
    WaitQueue<WaitEntry> waiters;
    SceInt32 val;
};

typedef std::shared_ptr<Semaphore> SemaphorePtr;

class Mutex : public SyncPrimitive {
public:
    const SceInt32 init_count;
    const Ptr<SceKernelLwMutexWork> workarea;

    Mutex(SceUInt32 attr, const char *name, SceInt32 init_count, ThreadStatePtr thread, Ptr<SceKernelLwMutexWork> workarea = {});
    // Returns the error for a name, attributes or count the caller can't use, or 0. Kernel callers may
    // use attributes games can't.
    static SceInt32 check_create(const char *name, SceUInt32 attr, SceInt32 init_count, bool kernel);

    SceInt32 acquire(MemState &mem, const ThreadStatePtr &thread, SceInt32 count, SceUInt32 *timeout, bool callbacks);
    // Same as acquire, but reports the wait as target, for a condition variable taking its mutex back
    SceInt32 acquire(MemState &mem, const ThreadStatePtr &thread, SceInt32 count, SceUInt32 *timeout, WaitTarget target);
    SceInt32 try_acquire(MemState &mem, const ThreadStatePtr &thread, SceInt32 count);
    SceInt32 release(MemState &mem, const ThreadStatePtr &thread, SceInt32 unlock_count);
    SceInt32 cancel(const ThreadStatePtr &thread, SceInt32 new_count, SceUInt32 *num_wait_threads);
    // Gives the mutex to its next waiter if thread, which is ending, owns it.
    void release_ended_owner(const ThreadState &thread);

protected:
    struct WaitEntry {
        SceInt32 lock_count;
    };

    // Guarded by the lock, read by the info of each kind of mutex
    WaitQueue<WaitEntry> waiters;
    SceInt32 lock_count;
    // The UID of the thread that holds it, or 0. A UID and not the thread, since a lightweight mutex
    // stays locked by an owner that ended and was deleted.
    SceUID owner;

private:
    void on_delete(SceUID deleter) override;
    bool lightweight() const { return get_uid_class() == UidClass::lw_mutex; }
    // Returns ILLEGAL_COUNT for a lock or unlock count the mutex can't take, or SCE_KERNEL_OK
    SceInt32 check_count(SceInt32 count) const;
    // Takes the mutex if it is free or owned by thread, or returns why it can't. The lock must be held.
    SceInt32 try_take(MemState &mem, const ThreadStatePtr &thread, SceInt32 count);
    // Gives a free mutex to the first waiter, if there is one.
    void serve() override;
    // Writes the owner and lock count to a lightweight mutex's work area, which the guest reads. The lock must be held.
    void update_workarea(MemState &mem);
};

class LwMutex final : public WithUidClass<Mutex, UidClass::lw_mutex> {
public:
    using WithUidClass::WithUidClass;

    // Fills info with what the lightweight mutex's info reports, or returns UNKNOWN_LW_MUTEX_ID if it
    // was deleted.
    SceInt32 info(SceKernelLwMutexInfo &info) const;
};

class HeavyMutex final : public WithUidClass<Mutex, UidClass::mutex> {
public:
    using WithUidClass::WithUidClass;

    // Fills info with what the mutex's info reports, or returns UNKNOWN_MUTEX_ID if it was deleted.
    SceInt32 info(SceKernelMutexInfo &info) const;
};

typedef std::shared_ptr<Mutex> MutexPtr;

class RWLock final : public WithUidClass<SyncPrimitive, UidClass::rw_lock> {
public:
    RWLock(SceUInt32 attr, const char *name);

    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 attr);

    SceInt32 acquire_read(const ThreadStatePtr &thread, SceUInt32 *timeout, bool callbacks);
    SceInt32 acquire_write(const ThreadStatePtr &thread, SceUInt32 *timeout, bool callbacks);
    SceInt32 try_acquire_read(const ThreadStatePtr &thread);
    SceInt32 try_acquire_write(const ThreadStatePtr &thread);
    SceInt32 release_read(const ThreadStatePtr &thread);
    SceInt32 release_write(const ThreadStatePtr &thread);
    SceInt32 cancel(const ThreadStatePtr &thread, SceUInt32 *num_read_wait_threads, SceUInt32 *num_write_wait_threads, SceInt32 flag);
    // Releases the write lock if thread, which is ending, holds it. A read lock stays held, like on
    // a Vita.
    void release_ended_writer(const ThreadState &thread);
    // Fills info with what the rwlock's info reports, or returns UNKNOWN_RW_LOCK_ID if it was deleted.
    SceInt32 info(SceKernelRWLockInfo &info) const;

private:
    struct WaitEntry {
        bool is_write;
    };

    void on_delete(SceUID deleter) override;
    SceInt32 acquire(const ThreadStatePtr &thread, bool is_write, SceUInt32 *timeout, bool callbacks);
    SceInt32 try_acquire(const ThreadStatePtr &thread, bool is_write);
    SceInt32 release(const ThreadStatePtr &thread, bool is_write);
    // Takes the lock if it is free for this kind of lock, or returns why it can't. The lock must be held.
    SceInt32 try_take(const ThreadStatePtr &thread, bool is_write);
    // Gives the lock to the waiters that get it next: every waiting reader while no writer holds or
    // waits for it, or else the first waiting writer once nobody holds it.
    void serve() override;

    // Guarded by the lock
    // The read locks of every reader, or the writer's count. Readers aren't tracked one by one, like
    // on a Vita, so any thread may read unlock while readers hold it.
    SceInt32 lock_count = 0;
    // The UID of the thread that holds the write lock, or 0
    SceUID write_owner = 0;
    WaitQueue<WaitEntry> waiters;
};

typedef std::shared_ptr<RWLock> RWLockPtr;

class EventFlag final : public WithUidClass<SyncPrimitive, UidClass::event_flag> {
public:
    // The pattern the event flag was created with, for its info
    const SceUInt32 init_pattern;

    EventFlag(SceUInt32 attr, const char *name, SceUInt32 init_pattern);

    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 attr);
    // Returns the error a poll or wait for pattern with wait_mode gets, or SCE_KERNEL_OK
    static SceInt32 check_wait(SceUInt32 pattern, SceUInt32 wait_mode);

    SceInt32 wait(const ThreadStatePtr &thread, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits, SceUInt32 *timeout, bool callbacks);
    SceInt32 poll(SceUID thread_id, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits);
    SceInt32 set(SceUInt32 pattern);
    SceInt32 clear(SceUInt32 pattern);
    SceInt32 cancel(SceUInt32 pattern, SceUInt32 *num_wait_threads);
    // Fills info with what the event flag's info reports, or returns UNKNOWN_EVF_ID if it was deleted.
    SceInt32 info(SceKernelEventFlagInfo &info) const;
    // Drops the holds of thread, which is ending.
    void release_ended_holder(const ThreadState &thread);

private:
    struct WaitEntry {
        SceUInt32 wait_mode;
        SceUInt32 pattern;
        SceUInt32 *out_bits;
    };

    void on_delete(SceUID deleter) override;
    // Returns whether another thread's wait holds the flag. The lock must be held.
    bool held_by_other(SceUID thread_id) const;
    // Takes the flags for the thread if pattern matches them, or returns why it can't. The lock must
    // be held.
    SceInt32 try_take(SceUID thread_id, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits);
    // Gives the flags to the waits they match, in queue order.
    void serve() override;
    // Gives back the hold of the wait that timed out.
    void timed_out(const ThreadStatePtr &thread) override;
    // Gives back one hold of the holder, the wait that took it ended. The lock must be held.
    void drop_hold();

    // Guarded by the lock
    WaitQueue<WaitEntry> waiters;
    SceUInt32 flags;
    // Without SCE_EVENT_WAITMULTIPLE, the thread whose waits hold the flag, which another thread can't
    // wait on or poll, and how many of its waits did. A wait that doesn't get the bits at once takes
    // a hold, and a set, a cancel or the delete that wakes it, or its timeout while queued, gives it
    // back. It stays while a callback has the waiter out of the queue. A wait that ends without
    // blocking or taking the bits, like one with a timeout of 0 or whose deadline passed during its
    // callbacks, keeps it until the thread ends or the flag is deleted.
    SceUID holder = 0;
    int holds = 0;
};

typedef std::shared_ptr<EventFlag> EventFlagPtr;

class Condvar : public SyncPrimitive {
public:
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

    const MutexPtr associated_mutex;

    Condvar(SceUInt32 attr, const char *name, MutexPtr associated_mutex);

    SceInt32 wait(MemState &mem, const ThreadStatePtr &thread, SceUInt32 *timeout, bool callbacks);
    SceInt32 signal(SignalTarget target);
    // Returns how many threads wait on the condition variable, 0 once it was deleted.
    SceUInt32 num_waiters() const;

protected:
    // Guarded by the lock, read by the info of a condition variable
    WaitQueue<std::monostate> waiters;

private:
    void on_delete(SceUID deleter) override;
    bool lightweight() const { return get_uid_class() == UidClass::lw_cond; }
};

class LwCond final : public WithUidClass<Condvar, UidClass::lw_cond> {
public:
    LwCond(SceUInt32 attr, const char *name, MutexPtr associated_mutex, Ptr<SceKernelLwCondWork> workarea);

    // Fills info with what the lightweight condition variable's info reports, or returns
    // UNKNOWN_LW_COND_ID if it was deleted.
    SceInt32 info(SceKernelLwCondInfo &info) const;

    const Ptr<SceKernelLwCondWork> workarea;
};

class HeavyCond final : public WithUidClass<Condvar, UidClass::cond> {
public:
    using WithUidClass::WithUidClass;

    // Fills info with what the condition variable's info reports, or returns UNKNOWN_COND_ID if it was
    // deleted.
    SceInt32 info(SceKernelCondInfo &info) const;
};

typedef std::shared_ptr<Condvar> CondvarPtr;

// One piece of a message pipe transfer, a buffer in guest memory
struct MsgPipeVector {
    Ptr<void> base;
    SceSize size;
};

class MsgPipe final : public WithUidClass<SyncPrimitive, UidClass::msg_pipe> {
public:
    MsgPipe(SceUInt32 attr, const char *name, std::size_t buf_size);

    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 type, SceUInt32 attr, SceSize buf_size);

    // Both move the bytes of the pieces in order, waiting until a FULL call moved all of them or an
    // ASAP call any. The size moved so far is kept in size, which may be null.
    SceInt32 receive(const ThreadStatePtr &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size, SceUInt32 *timeout, bool callbacks);
    SceInt32 send(const ThreadStatePtr &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size, SceUInt32 *timeout, bool callbacks);
    // Both move what they can at once, without waiting
    SceInt32 try_receive(const ThreadStatePtr &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size);
    SceInt32 try_send(const ThreadStatePtr &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size);
    SceInt32 cancel(SceUInt32 *num_send_wait_threads, SceUInt32 *num_receive_wait_threads);
    // Fills info with what the message pipe's info reports, or returns UNKNOWN_MSG_PIPE_ID if it was
    // deleted.
    SceInt32 info(SceKernelMppInfo &info) const;

private:
    // How far a send or a receive got, kept in its waiter so that whoever moves bytes carries it on
    struct Transfer {
        std::span<const MsgPipeVector> pieces;
        MemState *mem;
        // Where the caller gets the size moved, or null
        SceSize *size;
        SceSize total;
        SceSize left;
        bool full;
        // The piece the next byte goes to or comes from, and how far into it
        std::size_t piece = 0;
        SceSize offset = 0;

        Transfer(MemState &mem, std::span<const MsgPipeVector> pieces, SceSize *size, SceSize total, bool full);
        // Returns whether it moved what its mode asks for
        bool done() const;
        // Returns whether it moved part of its bytes, which keeps it at the head of its queue
        bool keeps_place() const;
        // Returns the guest bytes at its position, at most n and within one piece
        std::span<char> run(SceSize n);
        void advance(SceSize n);
        // Writes the size moved so far for the caller
        void report() const;
    };

    struct Sender : Transfer {
        using Transfer::Transfer;
    };

    struct Receiver : Transfer {
        // A DONT_REMOVE receive, which copies bytes without taking them
        bool peek;

        Receiver(MemState &mem, std::span<const MsgPipeVector> pieces, SceSize *size, SceSize total, bool full, bool peek);
    };

    void on_delete(SceUID deleter) override;
    // Carries on the first receiver's and the first sender's transfers until neither ends
    void serve() override;
    // Moves the sender's bytes to the waiting receivers, then puts the rest into the pipe. Returns
    // whether the sender is done.
    bool give(Sender &sender);
    // Moves bytes to the receiver from the pipe, then from the waiting senders, which put their rest
    // into the pipe. Returns whether the receiver is done.
    bool take(Receiver &receiver);
    // Puts as much of the sender's rest into the pipe as its mode allows
    void fill(Sender &sender);
    // Moves the pipe's bytes to the receiver, up to what it still wants
    void drain(Receiver &receiver);
    // Returns the bytes a receive can get at once, the pipe's and those the waiting senders still have
    SceSize available() const;
    // Copies n bytes from the sender to the receiver, and takes them from the sender unless the
    // receiver peeks
    static void copy(Sender &sender, Receiver &receiver, SceSize n);
    // Returns whether a new call goes ahead of the calls already waiting on its side
    template <typename Side>
    static bool goes_first(WaitQueue<Side> &side, int priority);

    // Guarded by the lock. At most one side waits, except right after a waiter came back from its
    // callbacks.
    WaitQueue<Sender> senders;
    WaitQueue<Receiver> receivers;
    ByteRingBuffer data_buffer;
};

typedef std::shared_ptr<MsgPipe> MsgPipePtr;
