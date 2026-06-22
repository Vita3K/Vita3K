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

#include <emuenv/window.h>
#include <util/fs.h>

#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

// forward declare everything used in EmuEnvState
namespace sfo {
struct SfoAppInfo;
}

namespace renderer {
enum class Backend : uint32_t;
struct State;
struct VulkanDeviceInfo;
} // namespace renderer

namespace ngs {
struct State;
};

struct Config;
struct CompatState;
struct MemState;
struct CtrlState;
struct TouchState;
struct KernelState;
struct AppState;
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
struct CameraState;

namespace overlay {
class display_manager;
}

typedef float SceFloat;
struct FVector2 {
    SceFloat x;
    SceFloat y;
};

typedef int SceUID;

using NIDSet = std::set<uint32_t>;

#include <emuenv/app_launch_request.h>

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
    std::unique_ptr<AppState> _app;
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
    std::unique_ptr<CameraState> _camera;
    std::unique_ptr<CompatState> _compat;
    mutable std::mutex _launch_request_mutex;
    std::optional<AppLaunchRequest> _pending_launch_request;

public:
    // App info contained in its `param.sfo` file
    sfo::SfoAppInfo &app_info;
    std::string app_path{};
    std::string license_content_id{};
    std::string license_title_id{};
    std::string current_app_title{};
    fs::path default_path{};
    fs::path config_path{}; // Path for config files
    fs::path log_path{}; // Path for log file
    fs::path cache_path{}; // Path for cache files (shaders, elf/texture dumps, and compat cache)
    fs::path vita_fs_path{}; // Path for VitaFS
    fs::path static_assets_path{}; // Path for static assets (shaders, icons, etc)
    fs::path shared_path{}; // Path for files (UI themes, textures, etc)
    fs::path patch_path{}; // Path for patch files
    std::string self_name{};
    std::string self_path{};
    Config &cfg;
    SceUID main_thread_id{};
    size_t frame_count = 0;
    uint32_t fps = 0;
    uint32_t avg_fps = 0;
    uint32_t min_fps = 0;
    uint32_t max_fps = 0;
    float fps_values[20] = {};
    uint32_t current_fps_offset = 0;
    uint32_t ms_per_frame = 0;
    renderer::Backend backend_renderer{};
    RendererPtr renderer{};
    std::unique_ptr<renderer::VulkanDeviceInfo> vulkan_device_info;
    bool drop_inputs{};
    MemState &mem;
    CtrlState &ctrl;
    TouchState &touch;
    KernelState &kernel;
    AppState &app;
    AudioState &audio;
    GxmState &gxm;
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
    float system_dpi_scale = 1.f;
    float manual_dpi_scale = 1.f;
    FVector2 gui_scale = { 1.f, 1.f };
    GDBState &gdb;
    HTTPState &http;
    CameraState &camera;
    CompatState &compat;
    int max_font_level = 0;
    int current_font_level = 0;

    std::unique_ptr<overlay::display_manager> overlay_manager;

    void post_app_launch_request(AppLaunchRequest request) {
        std::scoped_lock lock(_launch_request_mutex);
        _pending_launch_request = std::move(request);
    }

    std::optional<AppLaunchRequest> take_app_launch_request() {
        std::scoped_lock lock(_launch_request_mutex);
        if (!_pending_launch_request)
            return std::nullopt;

        auto request = std::move(_pending_launch_request);
        _pending_launch_request.reset();
        return request;
    }

    void clear_app_launch_request() {
        std::scoped_lock lock(_launch_request_mutex);
        _pending_launch_request.reset();
    }

    Root get_root_paths() const {
        Root r;
        r.set_vita_fs_path(vita_fs_path);
        r.set_patch_path(patch_path);
        r.set_log_path(log_path);
        r.set_config_path(config_path);
        r.set_shared_path(shared_path);
        r.set_cache_path(cache_path);
        r.set_static_assets_path(static_assets_path);
        return r;
    }

    EmuEnvState();
    // declaring a destructor is necessary to forward declare unique_ptrs
    ~EmuEnvState();

    // disable copy
    EmuEnvState(const EmuEnvState &) = delete;
    EmuEnvState &operator=(EmuEnvState const &) = delete;
};
