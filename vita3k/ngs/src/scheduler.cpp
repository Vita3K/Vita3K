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

#include <ngs/scheduler.h>
#include <ngs/system.h>

#include <algorithm>
#include <cstring>

namespace ngs {
bool VoiceScheduler::deque_voice_impl(Voice *voice) {
    auto voice_in = std::find(queue.begin(), queue.end(), voice);

    if (voice_in == queue.end()) {
        return false;
    }

    queue.erase(voice_in);
    return true;
}

bool VoiceScheduler::deque_voice(Voice *voice) {
    const std::lock_guard<std::recursive_mutex> guard(mutex);

    const bool result = deque_voice_impl(voice);

    return result;
}

bool VoiceScheduler::play(const MemState &mem, Voice *voice) {
    // Should Enqueue
    if ((voice->state == ngs::VOICE_STATE_AVAILABLE) || (voice->state == ngs::VOICE_STATE_PAUSED)) {
        // Transition
        voice->transition(ngs::VOICE_STATE_ACTIVE);

        std::int32_t lowest_dest_pos = static_cast<std::int32_t>(queue.size());

        // Check its dependencies position
        for (std::uint8_t i = 0; i < voice->patches.size(); i++) {
            for (const auto &patch : voice->patches[i]) {
                if (!patch) {
                    continue;
                }

                Voice *dest = patch.get(mem)->dest;
                const std::int32_t pos = get_position(dest);

                if (pos == -1) {
                    continue;
                }

                lowest_dest_pos = std::min<std::int32_t>(lowest_dest_pos, pos);
            }
        }

        const std::lock_guard<std::recursive_mutex> guard(mutex);
        queue.insert(queue.begin() + lowest_dest_pos, voice);

        return true;
    } else if (voice->state == ngs::VOICE_STATE_ACTIVE)
        return true;

    return false;
}

bool VoiceScheduler::pause(Voice *voice) {
    if (voice->state == ngs::VOICE_STATE_AVAILABLE || voice->state == ngs::VOICE_STATE_PAUSED) {
        return true;
    }

    if (voice->state == ngs::VOICE_STATE_ACTIVE || voice->state == ngs::VOICE_STATE_PENDING) {
        voice->transition(ngs::VOICE_STATE_PAUSED);

        // Remove from the list
        deque_voice(voice);

        return true;
    }

    return false;
}

bool VoiceScheduler::resume(const MemState &mem, Voice *voice) {
    if (voice->state != ngs::VOICE_STATE_PAUSED) {
        return false;
    }

    return play(mem, voice);
}

bool VoiceScheduler::stop(Voice *voice) {
    if (voice->state == ngs::VOICE_STATE_AVAILABLE || voice->state == ngs::VOICE_STATE_FINALIZING) {
        return false;
    }

    voice->transition(ngs::VOICE_STATE_AVAILABLE);
    deque_voice(voice);

    return true;
}

bool VoiceScheduler::off(Voice *voice) {
    if (voice->state == ngs::VOICE_STATE_KEY_OFF || voice->state == ngs::VOICE_STATE_FINALIZING) {
        return false;
    }

    voice->transition(ngs::VOICE_STATE_KEY_OFF);

    return true;
}

void VoiceScheduler::update(KernelState &kern, const MemState &mem, const SceUID thread_id) {
    std::unique_lock<std::recursive_mutex> scheduler_lock(mutex);
    updater = thread_id;

    // make a copy of the queue, this way we have no issue if it is modified in a callbck
    std::vector<ngs::Voice *> queue_copy = queue;

    // Do a first routine to clear inputs from previous update session
    for (ngs::Voice *voice : queue_copy) {
        voice->inputs.reset_inputs();
    }

    for (ngs::Voice *voice : queue_copy) {
        // Modify the state, in peace....
        std::unique_lock<std::mutex> voice_lock(*voice->voice_mutex);
        std::memset(voice->products, 0, sizeof(voice->products));

        const bool is_key_off = voice->state == ngs::VOICE_STATE_KEY_OFF;
        bool finished = false;
        for (std::size_t i = 0; i < voice->rack->modules.size(); i++) {
            if (voice->rack->modules[i]) {
                finished |= voice->rack->modules[i]->process(kern, mem, thread_id, voice->datas[i], scheduler_lock, voice_lock);
            }
        }
        if (is_key_off || finished)
            stop(voice);

        for (std::size_t i = 0; i < voice->rack->vdef->output_count(); i++) {
            if (voice->products[i].data)
                deliver_data(mem, voice, static_cast<std::uint8_t>(i), voice->products[i]);
        }

        voice->frame_count++;
    }

    updater = 0; // clear value (set to invalid thread id)
}

std::int32_t VoiceScheduler::get_position(Voice *v) {
    const std::lock_guard<std::recursive_mutex> guard(mutex);
    auto result = std::find(queue.begin(), queue.end(), v);

    if (result != queue.end()) {
        return static_cast<std::int32_t>(std::distance(queue.begin(), result));
    }

    return -1;
}

bool VoiceScheduler::resort_to_respect_dependencies(const MemState &mem, Voice *source) {
    // Get my position
    std::int32_t position = get_position(source);

    if (position == -1) {
        return false;
    }

    // Check all dependencies, could be optimized- @sunho suggested dfs topological sort
    for (size_t i = 0; i < source->patches.size(); i++) {
        for (const auto &patch : source->patches[i]) {
            if (!patch) {
                continue;
            }

            Voice *dest = patch.get(mem)->dest;
            const std::int32_t dest_pos = get_position(dest);

            if (position == -1) {
                // Maybe not scheduled yet. Continue
                continue;
            }

            if (dest_pos < position) {
                // Switch to the end. Resort dependencies for this one that just got sorted too.
                {
                    const std::lock_guard<std::recursive_mutex> guard(mutex);
                    std::rotate(queue.begin() + dest_pos, queue.begin() + dest_pos + 1, queue.end());
                }

                resort_to_respect_dependencies(mem, dest);
                position = get_position(source);
            }
        }
    }

    return true;
}

Ptr<Patch> VoiceScheduler::patch(const MemState &mem, PatchSetupInfo *info) {
    // First, check if these two voices are scheduled yet
    Voice *source = info->source.get(mem);
    Voice *dest = info->dest.get(mem);

    Ptr<Patch> patch = source->patch(mem, info->source_output_index, info->source_output_subindex, info->dest_input_index, dest);

    if (!patch) {
        return patch;
    }

    const std::int32_t source_pos = get_position(source);
    const std::int32_t dest_pos = get_position(dest);

    if (source_pos == -1 || dest_pos == -1) {
        // Later
        return patch;
    }

    resort_to_respect_dependencies(mem, source);
    return patch;
}
} // namespace ngs
