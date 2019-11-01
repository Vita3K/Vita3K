#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <mem/ptr.h>
#include <ngs/common.h>
#include <ngs/mempool.h>

struct MemState;

namespace emu::ngs {
    struct VoiceDefinition;
    struct System;

    struct State: public MempoolObject {
        std::array<Ptr<VoiceDefinition>, TOTAL_BUSS_TYPES> definitions;
        std::vector<System*> systems;
    };
    
    bool init(State &ngs, MemState &mem);
}