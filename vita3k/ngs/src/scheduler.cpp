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
    if (updater.has_value() && (updater.value() == std::this_thread::get_id())) {
        pending_deque.push_back(voice);
        return true;
    }

    lock.lock();
    const bool result = deque_voice_impl(voice);
    lock.unlock();

    return result;
}

bool VoiceScheduler::play(const MemState &mem, Voice *voice) {
    bool should_enqueue = (voice->state == ngs::VOICE_STATE_PAUSED || voice->state == ngs::VOICE_STATE_AVAILABLE);

    // Transition
    if (voice->state == ngs::VOICE_STATE_ACTIVE || voice->state == ngs::VOICE_STATE_PAUSED) {
        voice->transition(ngs::VOICE_STATE_ACTIVE);
    } else if (voice->state == ngs::VOICE_STATE_AVAILABLE) {
        voice->transition(ngs::VOICE_STATE_PENDING);
    } else {
        return false;
    }

    if (should_enqueue) {
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

        const std::lock_guard<std::mutex> guard(lock);
        queue.insert(queue.begin() + lowest_dest_pos, voice);
    }

    return true;
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

void VoiceScheduler::update(KernelState &kern, const MemState &mem, const SceUID thread_id) {
    const std::lock_guard<std::mutex> guard(lock);
    updater = std::this_thread::get_id();

    // Do a first routine to clear inputs from previous update session
    for (ngs::Voice *voice : queue) {
        voice->inputs.reset_inputs();
    }

    for (ngs::Voice *voice : queue) {
        // Modify the state, in peace....
        const std::lock_guard<std::mutex> guard(*voice->voice_lock);
        std::memset(voice->products, 0, sizeof(voice->products));

        voice->transition(ngs::VOICE_STATE_ACTIVE);

        for (std::size_t i = 0; i < voice->rack->modules.size(); i++) {
            if (voice->rack->modules[i]) {
                voice->rack->modules[i]->process(kern, mem, thread_id, voice->datas[i]);
            }
        }

        for (std::size_t i = 0; i < voice->rack->vdef->output_count(); i++) {
            if (voice->products[i].data)
                deliver_data(mem, voice, static_cast<std::uint8_t>(i), voice->products[i]);
        }

        voice->frame_count++;
    }

    for (ngs::Voice *nominee : pending_deque) {
        deque_voice_impl(nominee);
    }

    pending_deque.clear();
    updater.reset();
}

std::int32_t VoiceScheduler::get_position(Voice *v) {
    const std::lock_guard<std::mutex> guard(lock);
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
                    const std::lock_guard<std::mutex> guard(lock);
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
