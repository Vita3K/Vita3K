#include <ngs/scheduler.h>
#include <ngs/system.h>

#include <algorithm>

namespace emu::ngs {
    bool VoiceScheduler::deque_voice(Voice *voice) {
        auto voice_in = std::lower_bound(queue.begin(), queue.end(), voice, [](const Voice *lhs, const Voice *rhs) {
            return reinterpret_cast<std::uint64_t>(lhs) < reinterpret_cast<std::uint64_t>(rhs);
        });

        if (voice_in == queue.end() || *voice_in != voice) {
            return false;
        }

        queue.erase(voice_in);
        return true;
    }

    bool VoiceScheduler::play(Voice *voice) {
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
            queue.push_back(voice);
            std::sort(queue.begin(), queue.end(), [](const Voice *lhs, const Voice *rhs) {
                return reinterpret_cast<std::uint64_t>(lhs) < reinterpret_cast<std::uint64_t>(rhs);
            });
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
}