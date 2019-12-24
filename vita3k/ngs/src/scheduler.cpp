#include <ngs/scheduler.h>
#include <ngs/system.h>

#include <algorithm>

namespace emu::ngs {
    bool VoiceScheduler::deque_voice(Voice *voice) {
        const std::lock_guard<std::mutex> guard(lock);
        auto voice_in = std::find(queue.begin(), queue.end(), voice);

        if (voice_in == queue.end()) {
            return false;
        }

        queue.erase(voice_in);
        return true;
    }

    bool VoiceScheduler::play(const MemState &mem, Voice *voice) {
        bool should_enqueue = (voice->state == emu::ngs::VOICE_STATE_PAUSED || voice->state == emu::ngs::VOICE_STATE_AVAILABLE);

        // Transition
        if (voice->state == emu::ngs::VOICE_STATE_ACTIVE || voice->state == emu::ngs::VOICE_STATE_PAUSED) {
            voice->state = emu::ngs::VOICE_STATE_ACTIVE;
        } else if (voice->state == emu::ngs::VOICE_STATE_AVAILABLE) {
            voice->state = emu::ngs::VOICE_STATE_PENDING;
        } else {
            return false;
        }

        if (should_enqueue) {
            std::int32_t lowest_dest_pos = queue.size();

            // Check its dependencies position
            for (const auto &patch: voice->outputs) {
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

            const std::lock_guard<std::mutex> guard(lock);
            queue.insert(queue.begin() + lowest_dest_pos, voice);
        }

        return true;
    }

    bool VoiceScheduler::pause(Voice *voice) {
        if (voice->state == emu::ngs::VOICE_STATE_AVAILABLE || voice->state == emu::ngs::VOICE_STATE_PAUSED) {
            return true;
        }

        if (voice->state == emu::ngs::VOICE_STATE_ACTIVE || voice->state == emu::ngs::VOICE_STATE_PENDING) {
            voice->state = emu::ngs::VOICE_STATE_PAUSED;

            // Remove from the list
            deque_voice(voice);

            return true;
        }

        return false;
    }

    bool VoiceScheduler::stop(Voice *voice) {
        if (voice->state == emu::ngs::VOICE_STATE_AVAILABLE || voice->state == emu::ngs::VOICE_STATE_FINALIZING) {
            return false;
        }

        voice->state = emu::ngs::VOICE_STATE_AVAILABLE;
        deque_voice(voice);

        return true;
    }

    void VoiceScheduler::update(const MemState &mem) {
        const std::lock_guard<std::mutex> guard(lock);

        for (emu::ngs::Voice *voice: queue) {
            voice->state = emu::ngs::VOICE_STATE_ACTIVE;
            voice->rack->module->process(mem, voice);
        }
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

        // Check all dependencies
        for (const auto &patch: source->outputs) {
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
}