#pragma once

#include <vector>

namespace emu::ngs {
    struct Voice;

    struct VoiceScheduler {
        std::vector<Voice*> queue;

    protected:
        bool deque_voice(Voice *voice);

    public:
        bool play(Voice *voice);
        bool pause(Voice *voice);
        bool stop(Voice *voice);
    };
}