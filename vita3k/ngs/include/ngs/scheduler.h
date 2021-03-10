#pragma once

#include <util/types.h>

#include <mem/ptr.h>

#include <thread>

#include <optional>
#include <vector>

struct MemState;
struct KernelState;

namespace ngs {
struct PatchSetupInfo;
struct Voice;
struct Patch;

struct VoiceScheduler {
    std::vector<Voice *> queue;
    std::vector<Voice *> pending_deque;

    std::mutex lock;
    std::optional<std::thread::id> updater;

protected:
    bool deque_voice(Voice *voice);
    bool deque_voice_impl(Voice *voice);

    bool resort_to_respect_dependencies(const MemState &mem, Voice *source);

    std::int32_t get_position(Voice *v);

public:
    bool play(const MemState &mem, Voice *voice);
    bool pause(Voice *voice);
    bool resume(const MemState &mem, Voice *voice);
    bool stop(Voice *voice);

    void update(KernelState &kern, const MemState &mem, const SceUID thread_id);

    Ptr<Patch> patch(const MemState &mem, PatchSetupInfo *info);
};
} // namespace ngs
