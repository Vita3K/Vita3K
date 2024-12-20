// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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
#include <string_view>

struct SDL_Window;
struct DisplayState;
struct GxmState;
struct Config;

namespace renderer {

class TextureCache;

enum struct Filter : int {
    NEAREST = 1 << 0,
    BILINEAR = 1 << 1,
    BICUBIC = 1 << 2,
    FXAA = 1 << 3,
    FSR = 1 << 4
};

struct State {
    fs::path cache_path;
    fs::path log_path;
    fs::path shared_path;
    fs::path static_assets;
    fs::path shaders_path;
    fs::path shaders_log_path;

    Backend current_backend;
    FeatureState features;
    float res_multiplier;
    bool disable_surface_sync;
    bool stretch_the_display_area;

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

    // on Vulkan, this is actually the number of pipelines compiled
    uint32_t shaders_count_compiled = 0;
    uint32_t programs_count_pre_compiled = 0;

    bool should_display;

    bool need_page_table = false;

    virtual bool init() = 0;
    virtual void late_init(const Config &cfg, const std::string_view game_id, MemState &mem) = 0;

    virtual TextureCache *get_texture_cache() = 0;

    virtual void render_frame(const SceFVector2 &viewport_pos, const SceFVector2 &viewport_size, DisplayState &display,
        const GxmState &gxm, MemState &mem)
        = 0;
    virtual void swap_window(SDL_Window *window) = 0;
    // perform a screenshot of the (upscaled) frame to be rendered and return it in a vector in its rgba8 format
    virtual std::vector<uint32_t> dump_frame(DisplayState &display, uint32_t &width, uint32_t &height) = 0;
    // return a mask of the features which can influence the compiled shaders
    virtual uint32_t get_features_mask() {
        return 0;
    }
    // return a bitmask with the Filter enum values of the supported enum filters
    virtual int get_supported_filters() = 0;
    virtual void set_screen_filter(const std::string_view &filter) = 0;
    virtual int get_max_anisotropic_filtering() = 0;
    virtual void set_anisotropic_filtering(int anisotropic_filtering) = 0;
    virtual int get_max_2d_texture_width() = 0;
    virtual void set_async_compilation(bool enable) {}
    void set_surface_sync_state(bool disable) {
        disable_surface_sync = disable;
    }
    void set_stretch_display(bool enable) {
        stretch_the_display_area = enable;
    }
    virtual bool map_memory(MemState &mem, Ptr<void> address, uint32_t size) {
        return true;
    }
    virtual void unmap_memory(MemState &mem, Ptr<void> address) {}
    virtual std::vector<std::string> get_gpu_list() {
        return { "Automatic" };
    }

    virtual std::string_view get_gpu_name() = 0;

    virtual void precompile_shader(const ShadersHash &hash) = 0;
    virtual void preclose_action() = 0;

    virtual ~State() = default;

    fs::path texture_folder() const {
        return shared_path / "textures";
    }

    void init_paths(const Root &root_paths) {
        cache_path = root_paths.get_cache_path();
        log_path = root_paths.get_log_path();
        shared_path = root_paths.get_shared_path();
        static_assets = root_paths.get_static_assets_path();
    }

    void set_app(const char *title_id, const char *self_name) {
        shaders_path = cache_path / "shaders" / title_id / self_name;
        shaders_log_path = log_path / "shaderlog" / title_id / self_name;
    }
};
} // namespace renderer
