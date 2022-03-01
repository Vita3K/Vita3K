// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

struct MemState;
struct Context;
struct Config;
struct FeatureState;

namespace renderer {

struct State;
struct Command;
struct CommandHelper;

#define COMMAND(name)                                                                                \
    void cmd_##name(renderer::State &renderer, MemState &mem, Config &config, CommandHelper &helper, \
        const FeatureState &features, Context *render_context, const char *base_path, const char *title_id)

#define COMMAND_SET_STATE(name)                                                                                \
    void cmd_set_state_##name(renderer::State &renderer, MemState &mem, Config &config, CommandHelper &helper, \
        Context *render_context, const char *base_path, const char *title_id)

COMMAND_SET_STATE(region_clip);
COMMAND_SET_STATE(program);
COMMAND_SET_STATE(viewport);
COMMAND_SET_STATE(depth_bias);
COMMAND_SET_STATE(depth_func);
COMMAND_SET_STATE(depth_write_enable);
COMMAND_SET_STATE(polygon_mode);
COMMAND_SET_STATE(point_line_width);
COMMAND_SET_STATE(stencil_func);
COMMAND_SET_STATE(fragment_texture);

// State set
COMMAND(handle_set_state);

// Creation
COMMAND(handle_create_context);
COMMAND(handle_create_render_target);
COMMAND(handle_destroy_render_target);

// Scene
COMMAND(handle_set_context);
COMMAND(handle_sync_surface_data);

COMMAND(handle_draw);

// Sync
COMMAND(handle_nop);
COMMAND(handle_signal_sync_object);
COMMAND(handle_notification);

} // namespace renderer