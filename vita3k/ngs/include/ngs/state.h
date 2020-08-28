#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <mem/mempool.h>
#include <mem/ptr.h>
#include <ngs/common.h>

struct MemState;

namespace ngs {
struct VoiceDefinition;
struct System;

struct State : public MempoolObject {
    std::map<BussType, Ptr<VoiceDefinition>> definitions;
    std::vector<System *> systems;
};

bool init(State &ngs, MemState &mem);
} // namespace ngs