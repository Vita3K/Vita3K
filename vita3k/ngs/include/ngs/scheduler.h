#pragma once

#include <mem/ptr.h>
#include <vector>

struct MemState;

namespace ngs {
    struct PatchSetupInfo;
    struct Voice;
    struct Patch;

    struct VoiceScheduler {
        std::vector<Voice*> queue;
        std::mutex lock;

    protected:
        bool deque_voice(Voice *voice);

        /**
         * \brief Sort the voice queue so that dependencies between them are respected.
         */
        bool resort_to_respect_dependencies(const MemState &mem, Voice *source);

        std::int32_t get_position(Voice *v);

    public:
        bool play(const MemState &mem, Voice *voice);
        bool pause(Voice *voice);
        bool stop(Voice *voice);

        void update(const MemState &mem);

        Ptr<Patch> patch(const MemState &mem, PatchSetupInfo *info);
    };
}