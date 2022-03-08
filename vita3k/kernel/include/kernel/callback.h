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

#include <kernel/types.h>
#include <mutex>

#define SCE_UID_INVALID_UID (SceUID)(0xFFFFFFFF)

struct ThreadState;
typedef std::shared_ptr<ThreadState> ThreadStatePtr;

struct Callback {
    /**
     * @brief Creates a Callback object
     *
     * @param thread Thread object that created this callback
     * @param name Name of the callback
     * @param cb_func Pointer to the callback function
     * @param pCommon User-provided parameter
     */
    Callback(SceUID thread_id, ThreadStatePtr thread, std::string &name, Ptr<SceKernelCallbackFunction> cb_func, Ptr<void> pCommon)
        : thread_id(thread_id)
        , thread(thread)
        , name(name)
        , cb_func(cb_func)
        , userdata(pCommon) {}

    /**
     * @return UID of the thread that created and owns this callback
     */
    SceUID get_owner_thread_id() { return this->thread_id; }

    /**
     * @return Name of the callback
     */
    const std::string &get_name() { return this->name; }

    /**
     * @return Callback function
     */
    Ptr<SceKernelCallbackFunction> get_callback_function() const { return this->cb_func; }

    /**
     * @return UID of the event that notified the callback last
     */
    SceUID get_notifier_id();

    /**
     * @return notifyArg from last time callback was notified
     */
    SceInt32 get_notify_arg();

    /**
     * @return User-provided common argument
     */
    Ptr<void> get_user_common_ptr() const { return this->userdata; }

    /**
     * @brief Notify this callback
     * @param notifier_id UID of the notifying event
     * @param notify_arg User-specified notification argument
     */
    void notify(SceUID notifier_id, SceInt32 notify_arg);

    /**
     * @brief Notify this callback from an event, without notification argument
     * @param notifier_id UID of the event that notifies this callback
     */
    void event_notify(SceUID notifier_id);

    /**
     * @brief Notify this callback directly (not from event)
     * @param notify_arg User-specified notification argument
     */
    void direct_notify(SceInt32 notify_arg);

    /**
     * @brief Cancels every notification sent to this callback
     */
    void cancel();

    /**
     * @return true if the callback can be executed, false otherwise
     */
    bool is_executable();

    /**
     * @return Number of times callback has been notified since last execution
     */
    uint32_t get_num_notifications();

    /**
     * @brief Runs callback in the context of creator thread
     * @return true if the callback is executed and requests to be deleted (CallbackFunction returned non-zero)
     * @return false otherwise
     * @note Calling this method when Callback.executable() == false returns false and does nothing
     */
    bool execute();

private:
    void reset();
    bool is_notified() const;
    std::mutex _mutex;

    const SceUID thread_id; // UID of the thread that created this callback
    const ThreadStatePtr thread; // Thread object that created this callback
    const std::string name; // Name of the callback
    const Ptr<SceKernelCallbackFunction> cb_func; // Function to execute when the callback should run
    const Ptr<void> userdata; // User-provided data - passed as pCommon

    uint32_t num_notifications = 0; // Number of times this callback has been notified - reset every time it is run
    SceInt32 notification_arg = 0; // User-specified argument passed by sceKernelNotifyCallback
    SceUID notifier_id = SCE_UID_INVALID_UID; // UID of the last event that notified this thread - SCE_UID_INVALID_UID if not an event
};

typedef std::shared_ptr<Callback> CallbackPtr;
