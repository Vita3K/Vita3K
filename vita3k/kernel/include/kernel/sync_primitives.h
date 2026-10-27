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

    // Wakes the waiters that can now get what they wait for, in queue order. Called with the lock
    // held when the object gets something new to give, and by wait_loop() when a waiter leaves the
    // queue without being woken.
    virtual void serve() {}

    // Ends a queued wait of thread that its deadline ended, while it is still queued and before the
    // waiters behind it are served. Not called if the deadline passed before the wait was queued.
    virtual void timed_out(const ThreadStatePtr &) {}

    // When wait_loop() runs the callbacks notified during the wait, before it returns
    enum class CallbacksCheck {
        // Unless attempt succeeded
        after_wait,
        // Also when attempt succeeded
        after_take_or_wait,
        // Only when a delete ended the sleep
        on_delete,
    };

    // Runs the thread's pending callbacks if callbacks is set, releasing the lock while they run.
    // Returns WAIT_DELETE if the object was deleted meanwhile.
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

    // Waits on queue. Each round runs the pending callbacks, calls attempt, then sleeps until woken,
    // and a callback notify starts a new round. attempt returns the result once the thread got what
    // it waits for, or nullopt to keep waiting. It is passed the result of the last sleep
    // (SCE_KERNEL_OK the first time). timeout is updated after every sleep. callbacks_check decides
    // whether callbacks notified during the wait run before it returns.
    template <typename Entry, typename Attempt>
    [[nodiscard]] WaitResult wait_loop(WaitQueue<Entry> &queue, std::unique_lock<std::mutex> &lock, const ThreadStatePtr &thread, WaitTarget target, Entry entry, SceUInt32 *timeout, CallbacksCheck callbacks_check, Attempt attempt) {
        typename WaitQueue<Entry>::Waiter waiter{ .thread = thread, .entry = std::move(entry) };
        const Deadline deadline = deadline_from(timeout);
        // The result of the last sleep, then the final result
        WaitResult result = SCE_KERNEL_OK;
        while (true) {
            if (const WaitResult r = run_pending_callbacks(lock, thread, target.callbacks); !r || *r != SCE_KERNEL_OK)
                return r;
            if (const std::optional<SceInt32> taken = attempt(waiter.entry, *result)) {
                if (callbacks_check != CallbacksCheck::after_take_or_wait)
                    return *taken;
                result = *taken;
                break;
            }
            // Timed out before being queued, so timed_out() isn't called
            if (std::chrono::steady_clock::now() >= deadline) {
                if (timeout)
                    *timeout = 0;
                result = SCE_KERNEL_ERROR_WAIT_TIMEOUT;
                break;
            }
            // A callback is already pending, so start a new round without sleeping
            if (target.callbacks && thread->has_pending_callbacks()) {
                result = SCE_KERNEL_ERROR_NOTIFY_CALLBACK;
                continue;
            }
            waiter.priority = thread->get_priority();
            queue.push(waiter);
            // Ignore stray wakeups, like one left over from an earlier wait
            do {
                lock.unlock();
                result = thread->wait(target, deadline);
                lock.lock();
            } while (!waiter.result && result && *result == SCE_KERNEL_OK);
            if (timeout)
                *timeout = time_left_until(deadline);
            // A waker's result wins, it already handed over what the waiter waits for
            if (waiter.result) {
                result = *waiter.result;
                break;
            }
            if (result && *result == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
                timed_out(thread);
            queue.remove(waiter);
            serve();
            if (!result || *result != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
                break;
        }
        if (!result || (callbacks_check == CallbacksCheck::on_delete && *result != SCE_KERNEL_ERROR_WAIT_DELETE))
            return result;
        // Run the callbacks notified during the wait. Only the thread exiting changes the result.
        if (const WaitResult ran = run_pending_callbacks(lock, thread, target.callbacks); !ran)
            return ran;
        return result;
    }
};

// The ID of the one process Vita3K runs
constexpr SceUID PROCESS_ID = 1;

// Returns the user data of a system event a thread caused, like a delete: its UID in the high word
// and the process ID in the low word.
constexpr SceUInt64 system_event_user_data(SceUID thread_id) {
    return (static_cast<SceUInt64>(thread_id) << 32) | static_cast<SceUInt32>(PROCESS_ID);
}

// The event part of simple events and timers: a bit pattern, the user data of the last set, and the
// waits for bits of the pattern.
class Event : public SyncPrimitive {
public:
    const bool auto_reset;

    SceInt32 poll(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data);
    SceInt32 clear(SceUInt32 clear_pattern);
    virtual SceInt32 cancel(SceUInt32 *num_wait_threads);
    // Cancels the waits like cancel(), then replaces the whole pattern and the user data.
    SceInt32 cancel(SceUInt32 set_pattern, SceUInt64 user_data, SceUInt32 *num_wait_threads);
    // Sets result to the event's bit pattern, or returns UNKNOWN_EVENT_ID if it was deleted.
    SceInt32 get_pattern(SceUInt32 &result);
    // Fills info with what the event's info reports, or returns UNKNOWN_EVENT_ID if it was deleted.
    SceInt32 info(SceKernelEventInfo &info);

protected:
    Event(SceUInt32 attr, const char *name, SceUInt32 init_pattern, SceUInt64 init_user_data);

    struct WaitEntry {
        SceUInt32 pattern;
        SceUInt32 *result_pattern;
        SceUInt64 *user_data;
    };

    void on_delete(SceUID deleter) override;
    // Brings the event up to date before an event operation looks at it. Called with the lock held.
    // Only a timer has anything to do.
    virtual void catch_up() {}
    // Takes the event if wait_pattern matches, or returns SCE_KERNEL_ERROR_EVENT_COND. The lock must be held.
    SceInt32 try_take(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data);
    // Returns the guest's result of a wait, and gives a wait that a delete or a cancel ended the error
    // event.
    SceInt32 wait_result(const WaitResult &r, SceUInt32 *result_pattern);
    // Sets or pulses the bits with user_data and gives them to the waits they match. The lock must be
    // held.
    void fire(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set);
    // Gives the event's bits to the waits they match.
    void serve() override;

    // Guarded by the lock
    WaitQueue<WaitEntry> waiters;
    SceUInt32 pattern;
    SceUInt64 last_user_data;
};

class SimpleEvent final : public WithUidClass<Event, UidClass::simple_event> {
public:
    SimpleEvent(SceUInt32 attr, const char *name, SceUInt32 init_pattern, SceUID creator);

    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 attr, SceUInt32 init_pattern);

    SceInt32 wait(const ThreadStatePtr &thread, SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool callbacks);
    SceInt32 set_or_pulse(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set);
    // Sets one of the system events, like the open event, with user_data for its waiters, or returns
    // UNKNOWN_EVENT_ID if it was deleted.
    SceInt32 set_system_event(SceUInt32 event, SceUInt64 user_data);
};

using SimpleEventPtr = std::shared_ptr<SimpleEvent>;

using EventPtr = std::shared_ptr<Event>;

class Timer final : public WithUidClass<Event, UidClass::timer> {
public:
    Timer(SceUInt32 attr, const char *name, SceUID creator);

    SceInt32 set_event(SceUID type, SceKernelSysClock interval, SceInt32 repeats);
    SceInt32 wait(const ThreadStatePtr &thread, SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool callbacks);
    SceInt32 start();
    SceInt32 stop();
    SceInt32 cancel(SceUInt32 *num_wait_threads) override;
    // Replaces the count and returns its old value, or the error.
    std::expected<uint64_t, SceInt32> set_time(uint64_t new_time);
    // Sets time to how much time the timer counted, or returns UNKNOWN_TIMER_ID if it was deleted.
    SceInt32 get_time(uint64_t &time) const;
    // Sets time to the clock time the count runs from, 0 while the timer is stopped, or returns
    // UNKNOWN_TIMER_ID if it was deleted.
    SceInt32 get_base(uint64_t &time) const;
    // Sets time to how much time the armed event has left, or returns EVENT_NOT_SET with no event
    // armed or UNKNOWN_TIMER_ID if it was deleted.
    SceInt32 remaining_time(uint64_t &time);
    // Fills info with what the timer's info reports, with the clock time the count runs from as the
    // base of a running timer, or returns UNKNOWN_TIMER_ID if it was deleted.
    SceInt32 timer_info(SceKernelTimerInfo &info);

private:
    // Fires the event if it is due: sets or pulses SCE_KERNEL_EVENT_TIMER, and queues a repeating
    // event again an interval after it was due.
    void catch_up() override;
    // Queues the event to be due at the clock time deadline, and wakes the first waiter so it sees
    // the new time. The lock must be held.
    void queue_event(uint64_t deadline);
    // Gives the event's bits to the waits they match, and wakes the first waiter left.
    void serve() override;
    // Returns when the next event is due, on the clock of wait deadlines. The lock must be held.
    Deadline event_deadline() const;
    // Returns how much time the timer counted. The lock must be held.
    uint64_t count() const;

    // Guarded by the lock
    bool is_started = false;
    // An event is set, and a fired one shot event stays armed
    bool armed = false;
    bool is_repeat = false;
    bool is_pulse = false;
    // The clock time the count runs from while the timer is started, 0 if it never started
    uint64_t base = 0;
    // The count while the timer is stopped
    uint64_t stopped_count = 0;
    // The count the armed event is due at
    uint64_t schedule = 0;
    // The clock time the next event is due, the maximum while none is queued
    uint64_t next_event = std::numeric_limits<uint64_t>::max();
    uint64_t event_interval = 0;
};

using TimerPtr = std::shared_ptr<Timer>;

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

using SemaphorePtr = std::shared_ptr<Semaphore>;

class Mutex : public SyncPrimitive {
public:
    const SceInt32 init_count;
    const Ptr<SceKernelLwMutexWork> workarea;

    Mutex(SceUInt32 attr, const char *name, SceInt32 init_count, ThreadStatePtr thread, Ptr<SceKernelLwMutexWork> workarea = {});
    // Returns the error for a name, attributes or count the caller can't use, or 0. Kernel callers may
    // use attributes games can't.
    static SceInt32 check_create(const char *name, SceUInt32 attr, SceInt32 init_count, bool kernel);

    SceInt32 acquire(MemState &mem, const ThreadStatePtr &thread, SceInt32 count, SceUInt32 *timeout, bool callbacks);
    // Relocks the mutex after a condition variable wait. No timeout, and target is the reported wait.
    SceInt32 relock(MemState &mem, const ThreadStatePtr &thread, WaitTarget target);
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
    // Takes the mutex if nobody owns it and, for a heavy mutex, no waiter is ahead of thread, or
    // returns nothing. The lock must be held.
    std::optional<SceInt32> take_if_free(MemState &mem, const ThreadStatePtr &thread, SceInt32 count);
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

using MutexPtr = std::shared_ptr<Mutex>;

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

using RWLockPtr = std::shared_ptr<RWLock>;

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
    // Completes wait if the flags match its pattern: writes its out bits, clears the flags its mode
    // asks for and releases its hold. Returns false if they don't match. The lock must be held.
    bool take(const WaitEntry &wait);
    // Gives the flags to the waits they match, in queue order.
    void serve() override;
    // Gives the wait that timed out the flag's bits and gives back its hold.
    void timed_out(const ThreadStatePtr &thread) override;
    // Gives back one hold of the holder, the wait that took it ended. The lock must be held.
    void drop_hold();

    // Guarded by the lock
    WaitQueue<WaitEntry> waiters;
    SceUInt32 flags;
    // Without SCE_EVENT_WAITMULTIPLE, the thread whose waits hold the flag and how many waits hold
    // it. Other threads can't wait on or poll a held flag. A wait releases its hold when it gets the
    // bits, when a cancel or delete wakes it, or when it times out in the queue. A wait that ends
    // before sleeping without getting the bits, like one with a timeout of 0, keeps its hold until
    // the thread ends or the flag is deleted.
    SceUID holder = 0;
    int holds = 0;
};

using EventFlagPtr = std::shared_ptr<EventFlag>;

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

using CondvarPtr = std::shared_ptr<Condvar>;

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

using MsgPipePtr = std::shared_ptr<MsgPipe>;
