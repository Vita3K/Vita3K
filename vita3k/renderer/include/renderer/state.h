// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <features/state.h>
#include <renderer/commands.h>
#include <renderer/types.h>
#include <threads/queue.h>

#include <condition_variable>
#include <mutex>

struct SDL_Cursor;
struct SDL_Window;
struct DisplayState;
struct GxmState;

namespace renderer {
struct State {
    const char *base_path;
    const char *title_id;
    const char *self_name;

    Backend current_backend;
    FeatureState features;
    int res_multiplier;
    bool disable_surface_sync;

    Context *context;

    GXPPtrMap gxp_ptr_map;
    Queue<CommandList> command_buffer_queue;
    std::condition_variable command_finish_one;
    std::mutex command_finish_one_mutex;

    std::condition_variable notification_ready;
    std::mutex notification_mutex;

    std::vector<ShadersHash> shaders_cache_hashs;
    std::string shader_version;

    int last_scene_id = 0;

    uint32_t shaders_count_compiled = 0;
    uint32_t programs_count_pre_compiled = 0;

    bool should_display;

    virtual bool init(const char *base_path, const bool hashless_texture_cache) = 0;
    virtual void render_frame(const SceFVector2 &viewport_pos, const SceFVector2 &viewport_size, const DisplayState &display,
        const GxmState &gxm, MemState &mem)
        = 0;
    virtual void swap_window(SDL_Window *window) = 0;
    virtual void set_fxaa(bool enable_fxaa) = 0;
    virtual int get_max_anisotropic_filtering() = 0;
    virtual void set_anisotropic_filtering(int anisotropic_filtering) = 0;
    virtual bool map_memory(void *address, uint32_t size) {
        return true;
    }
    virtual void unmap_memory(void *address) {}
    virtual std::vector<std::string> get_gpu_list() {
        return { "Automatic" };
    }

    virtual void precompile_shader(const ShadersHash &hash) = 0;
    virtual void preclose_action() = 0;

    virtual ~State() = default;
};
} // namespace renderer
