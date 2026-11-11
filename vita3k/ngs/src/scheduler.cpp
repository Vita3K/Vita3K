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

#include <ngs/modules/envelope.h>
#include <ngs/scheduler.h>
#include <ngs/system.h>

#include <cpu/state.h>
#include <kernel/state.h>
#include <kernel/thread/thread_state.h>

#include <algorithm>
#include <cstring>
#include <util/vector_utils.h>

namespace ngs {

bool VoiceScheduler::deque_voice(Voice *voice) {
    const std::lock_guard<std::recursive_mutex> guard(mutex);

    return vector_utils::erase_first(queue, voice);
}

bool VoiceScheduler::relock_voice_after_callback(Voice *voice, std::unique_lock<std::mutex> &voice_lock) {
    if (released_voices_during_update.contains(voice)) {
        voice_lock.release();
        return false;
    }
    voice_lock.lock();
    return true;
}

void VoiceScheduler::deque_insert(const MemState &mem, Voice *voice) {
    const std::lock_guard<std::recursive_mutex> guard(mutex);
    int32_t lowest_dest_pos = queue.size();

    // Check its dependencies position
    for (auto &patches : voice->patches) {
        for (const auto patch : patches) {
            if (!patch) {
                continue;
            }

            Voice *dest = patch.get(mem)->dest.get(mem);
            if (!dest) {
                continue;
            }
            const int32_t pos = get_position(dest);

            if (pos == -1) {
                continue;
            }

            lowest_dest_pos = std::min<int32_t>(lowest_dest_pos, pos);
        }
    }

    queue.insert(queue.begin() + lowest_dest_pos, voice);
}

bool VoiceScheduler::play(const MemState &mem, Voice *voice) {
    if (voice->state != VOICE_STATE_AVAILABLE)
        return false;

    // Transition
    voice->transition(mem, VOICE_STATE_ACTIVE);

    // Should Enqueue
    if (!voice->is_paused)
        deque_insert(mem, voice);

    return true;
}

bool VoiceScheduler::pause(const MemState &mem, Voice *voice) {
    if (!voice->is_paused) {
        voice->is_paused = true;

        // Remove from the list
        if (voice->state == VOICE_STATE_ACTIVE || voice->state == VOICE_STATE_FINALIZING)
            deque_voice(voice);
        return true;
    }

    return false;
}

bool VoiceScheduler::resume(const MemState &mem, Voice *voice) {
    if (!voice->is_paused) {
        return false;
    }

    voice->is_paused = false;

    if (voice->state == VOICE_STATE_ACTIVE || voice->state == VOICE_STATE_FINALIZING)
        deque_insert(mem, voice);

    return true;
}

bool VoiceScheduler::stop(const MemState &mem, Voice *voice) {
    if (voice->state != VOICE_STATE_ACTIVE && voice->state != VOICE_STATE_FINALIZING)
        return false;

    voice->transition(mem, VOICE_STATE_AVAILABLE);
    if (!voice->is_paused)
        deque_voice(voice);

    voice->is_paused = false;
    voice->is_keyed_off = false;

    return true;
}

bool VoiceScheduler::off(const MemState &mem, Voice *voice) {
    const std::lock_guard<std::recursive_mutex> guard(mutex);
    if (voice->state != VOICE_STATE_ACTIVE)
        return false;

    voice->is_keyed_off = true;
    voice->transition(mem, VOICE_STATE_FINALIZING);

    return true;
}

bool VoiceScheduler::update(KernelState &kern, const MemState &mem, const SceUID thread_id) {
    std::unique_lock<std::recursive_mutex> scheduler_lock(mutex);
    if (is_updating && updating_thread_id == thread_id)
        return false;
    while (is_updating) {
        scheduler_lock.unlock();
        {
            ThreadHostWait host_wait(kern.get_thread(thread_id));
            scheduler_lock.lock();
            if (is_updating)
                condvar.wait(scheduler_lock);
            scheduler_lock.unlock();
        }
        scheduler_lock.lock();
    }
    is_updating = true;
    updating_thread_id = thread_id;
    released_voices_during_update.clear();

    // make a copy of the queue, this way we have no issue if it is modified in a callback
    std::vector<ngs::Voice *> queue_copy = queue;

    // Do a first routine to clear inputs from previous update session
    for (ngs::Voice *voice : queue_copy) {
        voice->inputs.reset_inputs();
    }

    for (ngs::Voice *voice : queue_copy) {
        if (released_voices_during_update.contains(voice))
            continue;

        // Modify the state, in peace....
        std::unique_lock<std::mutex> voice_lock(*voice->voice_mutex);
        memset(voice->products, 0, sizeof(voice->products));

        const bool has_key_off_handler = [&] {
            for (size_t i = 0; i < voice->rack->modules.size(); ++i) {
                const auto &module = voice->rack->modules[i];
                if (module && !voice->datas[i].is_bypassed && module->module_id() == (SCE_NGS_ENVELOPE_PARAMS_STRUCT_ID & 0xffff))
                    return true;
            }
            return false;
        }();
        const bool finish_on_key_off = voice->is_keyed_off && !has_key_off_handler;
        bool finished = finish_on_key_off;
        bool finished_triggers_callback = true;
        uint32_t finished_module = 0;

        bool released = false;
        for (size_t i = 0; i < voice->rack->modules.size(); i++) {
            if (voice->rack->modules[i] && !finish_on_key_off) {
                processing_rack = voice->rack;
                processing_module_index = i;
                const uint32_t module_id = voice->rack->modules[i]->module_id();
                const bool module_finished = voice->rack->modules[i]->process(kern, mem, thread_id, voice->datas[i], scheduler_lock, voice_lock);
                processing_rack = nullptr;
                processing_module_keepalive.reset();
                if (released_voices_during_update.contains(voice)) {
                    voice_lock.release();
                    released = true;
                    break;
                }
                if (module_finished) {
                    finished = true;
                    finished_module = module_id;
                    finished_triggers_callback = voice->rack->modules[i]->finished_triggers_callback(voice->datas[i]);
                }
            }
        }
        if (released)
            continue;
        if (finished) {
            if (!finished_triggers_callback) {
                stop(mem, voice);
            } else {
                if (voice->is_keyed_off)
                    finished_module = 0;
                voice->is_keyed_off = true;
                voice->transition(mem, VOICE_STATE_FINALIZING);
                if (voice->finished_callback) {
                    voice_lock.unlock();
                    scheduler_lock.unlock();
                    voice->invoke_callback(kern, mem, thread_id, voice->finished_callback, voice->finished_callback_user_data, finished_module);
                    scheduler_lock.lock();
                    if (released_voices_during_update.contains(voice))
                        continue;
                    voice_lock.lock();
                }
                voice->is_keyed_off = false;

                stop(mem, voice);
            }
        }

        for (size_t i = 0; i < voice->rack->vdef->output_count; i++) {
            if (voice->products[i].data)
                deliver_data(mem, queue, voice, static_cast<uint8_t>(i), voice->products[i]);
        }

        voice->frame_count++;
    }

    while (!operations_pending.empty()) {
        OperationPending op = operations_pending.front();
        operations_pending.pop();

        switch (op.type) {
        case PendingType::ReleaseRack:
            release_rack(*op.release_data.state, mem, op.system, op.release_data.rack);
            pending_rack_releases.erase(op.release_data.rack);
            {
                const ThreadStatePtr thread = kern.get_thread(thread_id);
                const Address callback_info_addr = thread->cpu->stack_alloc(sizeof(SceNgsCallbackInfo));
                auto *info = Ptr<SceNgsCallbackInfo>(callback_info_addr).get(mem);
                *info = {};
                info->rack_handle = Ptr<void>(op.release_data.rack, mem);
                thread->call_guest(op.release_data.callback, { callback_info_addr });
                thread->cpu->stack_free(sizeof(SceNgsCallbackInfo));
            }
            break;
        }
    }

    is_updating = false;
    updating_thread_id = -1;
    condvar.notify_all();
    return true;
}

int32_t VoiceScheduler::get_position(Voice *v) {
    // we assume the scheduler lock is being held when calling this function
    return vector_utils::find_index(queue, v);
}

bool VoiceScheduler::resort_to_respect_dependencies(const MemState &mem, Voice *source) {
    // this function is called by patch, which already acquired the scheduler mutex

    // Get my position
    int32_t position = get_position(source);

    if (position == -1) {
        return false;
    }

    // Check all dependencies, could be optimized- @sunho suggested dfs topological sort
    for (size_t i = 0; i < source->patches.size(); i++) {
        for (const auto &patch : source->patches[i]) {
            if (!patch || !patch.get(mem)->is_active()) {
                continue;
            }

            Voice *dest = patch.get(mem)->dest.get(mem);
            if (!dest) {
                continue;
            }
            const int32_t dest_pos = get_position(dest);

            if (dest_pos == -1) {
                // Maybe not scheduled yet. Continue
                continue;
            }

            if (dest_pos < position) {
                // Switch to the end. Resort dependencies for this one that just got sorted too.
                std::rotate(queue.begin() + dest_pos, queue.begin() + dest_pos + 1, queue.end());
                resort_to_respect_dependencies(mem, dest);
                position = get_position(source);
            }
        }
    }

    return true;
}

Ptr<Patch> VoiceScheduler::patch(const MemState &mem, SceNgsPatchSetupInfo *info) {
    const std::lock_guard<std::recursive_mutex> guard(mutex);
    // First, check if these two voices are scheduled yet
    Voice *source = info->source.get(mem);
    Voice *dest = info->dest.get(mem);

    Ptr<Patch> patch = source->patch(mem, info->source_output_index, info->source_output_subindex, info->dest_input_index, info->source, info->dest);

    if (!patch) {
        return patch;
    }

    const int32_t source_pos = get_position(source);
    const int32_t dest_pos = get_position(dest);

    if (source_pos == -1 || dest_pos == -1) {
        // Later
        return patch;
    }

    resort_to_respect_dependencies(mem, source);
    return patch;
}
} // namespace ngs
