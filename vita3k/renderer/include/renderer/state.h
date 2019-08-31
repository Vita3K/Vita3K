#pragma once

#include <features/state.h>
#include <renderer/commands.h>
#include <renderer/types.h>
#include <threads/queue.h>

#include <condition_variable>
#include <mutex>

struct SDL_Cursor;

namespace renderer {
struct State {
    Backend current_backend;
    FeatureState features;

    GXPPtrMap gxp_ptr_map;
    Queue<CommandList> command_buffer_queue;
    std::condition_variable command_finish_one;
    std::mutex command_finish_one_mutex;

    std::uint32_t scene_processed_since_last_frame = 0;
    std::uint32_t average_scene_per_frame = 1;

    virtual ~State() = default;
};
} // namespace renderer
