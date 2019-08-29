#pragma once

#include <features/state.h>
#include <renderer/commands.h>
#include <renderer/types.h>
#include <threads/queue.h>

#include <condition_variable>
#include <mutex>

struct SDL_Cursor;

namespace renderer {
// Used instead of including imgui.h for ImGuiMouseCursor_COUNT.
constexpr uint32_t cursor_count = 8;

struct State {
    SDL_Window *window;
    Backend current_backend;
    FeatureState features;

    GXPPtrMap gxp_ptr_map;
    Queue<CommandList> command_buffer_queue;
    std::condition_variable command_finish_one;
    std::mutex command_finish_one_mutex;

    std::uint32_t scene_processed_since_last_frame = 0;
    std::uint32_t average_scene_per_frame = 1;

    // This should be moved somewhere else since it is not renderer related.
    struct {
        uint64_t time = 0;
        bool mouse_pressed[3] = { false, false, false };
        SDL_Cursor *mouse_cursors[cursor_count] = { nullptr };

        bool init = false;
    } gui;

    virtual ~State() = default;
};
} // namespace renderer
