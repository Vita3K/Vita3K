// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <renderer/state.h>
#include <renderer/types.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct Config;
struct MemState;

namespace renderer::metal {

// Everything touching the Metal API lives behind this pointer (see src/metal/mtl_types.h),
// so this header can be included from plain C++ translation units.
struct MetalDevice;

struct MetalState : public renderer::State {
    std::unique_ptr<MetalDevice> device;
    MemState *mem = nullptr;
    std::string gpu_name;

    MetalState();
    ~MetalState() override;

    bool init() override;
    bool create(std::unique_ptr<renderer::State> &state, const Config &config);
    void late_init(const Config &cfg, const std::string_view game_id, MemState &mem) override;
    void cleanup() override;

    TextureCache *get_texture_cache() override;

    void render_frame(DisplayState &display, const GxmState &gxm, MemState &mem) override;
    void swap_window() override;
    std::vector<uint32_t> dump_frame(DisplayState &display, uint32_t &width, uint32_t &height) override;

    uint32_t get_features_mask() override;
    int get_supported_filters() override;
    void set_screen_filter(const std::string_view &filter) override;
    int get_max_anisotropic_filtering() override;
    void set_anisotropic_filtering(int anisotropic_filtering) override;
    int get_max_2d_texture_width() override;
    void set_async_compilation(bool enable) override;

    std::string_view get_gpu_name() override;
    uint32_t get_gpu_version() override;

    void precompile_shader(const ShadersHash &hash) override;
    void preclose_action() override;
};

} // namespace renderer::metal
