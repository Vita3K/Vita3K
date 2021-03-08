#include <chrono>
#include <gxm/types.h>
#include <renderer/commands.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include "driver_functions.h"
#include <renderer/gl/functions.h>

#include <renderer/functions.h>
#include <util/log.h>

namespace renderer {
COMMAND(handle_nop) {
    // Signal back to client
    int code_to_finish = helper.pop<int>();
    complete_command(renderer, helper, code_to_finish);
}

COMMAND(handle_signal_sync_object) {
    SceGxmSyncObject *sync = helper.pop<Ptr<SceGxmSyncObject>>().get(mem);
    renderer::subject_done(sync, renderer::SyncObjectSubject::Fragment);
}

COMMAND(handle_notification) {
    SceGxmNotification *nof = helper.pop<Ptr<SceGxmNotification>>().get(mem);
    [[maybe_unused]] const bool is_vertex = helper.pop<bool>();

    volatile std::uint32_t *val = nof->address.get(mem);
    *val = nof->value;
}

// Client side function
void finish(State &state, Context &context) {
    // Wait for the code
    wait_for_status(state, &context.render_finish_status);
}

int wait_for_status(State &state, int *result_code) {
    if (*result_code != CommandErrorCodePending) {
        // Signaled, return
        return *result_code;
    }

    // Wait for it to get signaled
    std::unique_lock<std::mutex> finish_mutex(state.command_finish_one_mutex);
    state.command_finish_one.wait(finish_mutex, [&]() { return *result_code != CommandErrorCodePending; });

    return *result_code;
}

void wishlist(SceGxmSyncObject *sync_object, const SyncObjectSubject subjects) {
    {
        const std::lock_guard<std::mutex> mutex_guard(sync_object->lock);

        if (sync_object->done & subjects) {
            return;
        }
    }

    std::unique_lock<std::mutex> finish_mutex(sync_object->lock);
    sync_object->cond.wait(finish_mutex, [&]() { return sync_object->done & subjects; });
}

void subject_done(SceGxmSyncObject *sync_object, const SyncObjectSubject subjects) {
    {
        const std::lock_guard<std::mutex> mutex_guard(sync_object->lock);
        sync_object->done |= subjects;
    }

    sync_object->cond.notify_all();
}

void subject_in_progress(SceGxmSyncObject *sync_object, const SyncObjectSubject subjects) {
    const std::lock_guard<std::mutex> mutex_guard(sync_object->lock);
    sync_object->done &= ~subjects;
}

void submit_command_list(State &state, renderer::Context *context, CommandList &command_list) {
    command_list.context = context;
    state.command_buffer_queue.push(std::move(command_list));
}
} // namespace renderer
