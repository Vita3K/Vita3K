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

#include <audio/state.h>
#include <config/state.h>
#include <ctrl/state.h>
#include <dialog/state.h>
#include <display/state.h>
#include <gxm/state.h>
#include <host/sfo.h>
#include <host/window.h>
#include <ime/state.h>
#include <io/state.h>
#include <kernel/state.h>
#include <net/state.h>
#include <ngs/state.h>
#include <nids/types.h>
#include <np/state.h>
#include <renderer/state.h>
#include <touch/state.h>

// The GDB Stub requires winsock.h on windows (included in above headers). Keep it here to prevent build errors.
#include <gdbstub/state.h>

#include <atomic>
#include <memory>
#include <string>

struct HostState {
    std::string app_version;
    std::string app_category;
    std::string app_content_id;
    std::string app_addcont;
    std::string app_savedata;
    std::string app_parental_level;
    std::string app_short_title;
    std::string app_title;
    std::string app_title_id;
    std::string app_path;
    int32_t app_sku_flag;
    std::string license_content_id;
    std::string license_title_id;
    std::string current_app_title;
    std::string base_path;
    std::string default_path;
    std::wstring pref_path;
    bool load_exec = false;
    std::string load_app_path;
    std::string load_exec_argv;
    std::string load_exec_path;
    std::string self_path;
    Config cfg;
    std::unique_ptr<CPUProtocolBase> cpu_protocol;
    SceUID main_thread_id;
    size_t frame_count = 0;
    uint32_t sdl_ticks = 0;
    uint32_t fps = 0;
    uint32_t avg_fps = 0;
    uint32_t min_fps = 0;
    uint32_t max_fps = 0;
    float fps_values[20] = {};
    uint32_t current_fps_offset = 0;
    uint32_t ms_per_frame = 0;
    WindowPtr window = WindowPtr(nullptr, nullptr);
    renderer::Backend backend_renderer;
    RendererPtr renderer;
    SceIVector2 drawable_size = { 0, 0 };
    SceFVector2 viewport_pos = { 0, 0 };
    SceFVector2 viewport_size = { 0, 0 };
    MemState mem;
    CtrlState ctrl;
    TouchState touch;
    KernelState kernel;
    AudioState audio;
    GxmState gxm;
    bool renderer_focused;
    IOState io;
    NetState net;
    NetCtlState netctl;
    ngs::State ngs;
    NpState np;
    DisplayState display;
    DialogState common_dialog;
    Ime ime;
    SfoFile sfo_handle;
    NIDSet missing_nids;
    float dpi_scale = 1.0f;
    float res_width_dpi_scale;
    float res_height_dpi_scale;
    GDBState gdb;
};
