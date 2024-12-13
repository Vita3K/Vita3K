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

#include <emuenv/window.h>
#include <util/fs.h>

#include <memory>
#include <set>
#include <string>

// forward declare everything used in EmuEnvState
namespace sfo {
struct SfoAppInfo;
}

namespace renderer {
enum class Backend : uint32_t;
struct State;
} // namespace renderer

namespace ngs {
struct State;
};

struct Config;
struct CPUProtocolBase;
struct MemState;
struct CtrlState;
struct TouchState;
struct KernelState;
struct AudioState;
struct GxmState;
struct IOState;
struct MotionState;
struct NetState;
struct NetCtlState;
struct NpState;
struct DisplayState;
struct DialogState;
struct Ime;
struct License;
struct RegMgrState;
struct SfoFile;
struct GDBState;
struct HTTPState;

typedef int32_t SceInt;
struct IVector2 {
    SceInt x;
    SceInt y;
};

typedef float SceFloat;
struct FVector2 {
    SceFloat x;
    SceFloat y;
};

typedef int SceUID;

using NIDSet = std::set<uint32_t>;

/**
 * @brief State of the emulated PlayStation Vita environment
 */
struct EmuEnvState {
    // declare this first as the unique_ptr need to be initialized before the references
private:
    std::unique_ptr<sfo::SfoAppInfo> _app_info;
    std::unique_ptr<Config> _cfg;
    std::unique_ptr<MemState> _mem;
    std::unique_ptr<CtrlState> _ctrl;
    std::unique_ptr<TouchState> _touch;
    std::unique_ptr<KernelState> _kernel;
    std::unique_ptr<AudioState> _audio;
    std::unique_ptr<GxmState> _gxm;
    std::unique_ptr<IOState> _io;
    std::unique_ptr<MotionState> _motion;
    std::unique_ptr<NetState> _net;
    std::unique_ptr<NetCtlState> _netctl;
    std::unique_ptr<ngs::State> _ngs;
    std::unique_ptr<NpState> _np;
    std::unique_ptr<DisplayState> _display;
    std::unique_ptr<DialogState> _common_dialog;
    std::unique_ptr<Ime> _ime;
    std::unique_ptr<License> _license;
    std::unique_ptr<RegMgrState> _regmgr;
    std::unique_ptr<SfoFile> _sfo_handle;
    std::unique_ptr<GDBState> _gdb;
    std::unique_ptr<HTTPState> _http;

public:
    // App info contained in its `param.sfo` file
    sfo::SfoAppInfo &app_info;
    std::string app_path{};
    std::string license_content_id{};
    std::string license_title_id{};
    std::string current_app_title{};
    fs::path base_path{};
    fs::path default_path{};
    fs::path config_path{};
    fs::path log_path{};
    fs::path cache_path{};
    fs::path pref_path{};
    fs::path static_assets_path{};
    fs::path shared_path{};
    fs::path patch_path{};
    bool load_exec{};
    std::string load_app_path{};
    std::string load_exec_argv{};
    std::string load_exec_path{};
    std::string self_name{};
    std::string self_path{};
    Config &cfg;
    std::unique_ptr<CPUProtocolBase> cpu_protocol{};
    SceUID main_thread_id{};
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
    renderer::Backend backend_renderer{};
    RendererPtr renderer{};
    IVector2 drawable_size = { 0, 0 };
    IVector2 window_size = { 0, 0 }; // Logical size of the window
    FVector2 logical_viewport_pos = { 0, 0 }; // Position of the logical viewport in the window. For ImGui
    FVector2 logical_viewport_size = { 0, 0 }; // Size of the logical viewport in the window. For ImGui
    FVector2 drawable_viewport_pos = { 0, 0 }; // Position of the drawable viewport in the window. For OpenGL/Vulkan
    FVector2 drawable_viewport_size = { 0, 0 }; // Size of the drawable viewport in the window. For OpenGL/Vulkan
    MemState &mem;
    CtrlState &ctrl;
    TouchState &touch;
    KernelState &kernel;
    AudioState &audio;
    GxmState &gxm;
    bool renderer_focused{};
    IOState &io;
    MotionState &motion;
    NetState &net;
    NetCtlState &netctl;
    ngs::State &ngs;
    NpState &np;
    DisplayState &display;
    DialogState &common_dialog;
    Ime &ime;
    License &license;
    RegMgrState &regmgr;
    SfoFile &sfo_handle;
    NIDSet missing_nids;
    float dpi_scale = 1.f;
    FVector2 gui_scale = { 1.f, 1.f };
    uint32_t res_width_dpi_scale = 0;
    uint32_t res_height_dpi_scale = 0;
    GDBState &gdb;
    HTTPState &http;

    EmuEnvState();
    // declaring a destructor is necessary to forward declare unique_ptrs
    ~EmuEnvState();

    // disable copy
    EmuEnvState(const EmuEnvState &) = delete;
    EmuEnvState &operator=(EmuEnvState const &) = delete;
};
