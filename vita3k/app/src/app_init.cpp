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

#include <app/functions.h>

#include <audio/state.h>
#include <camera/state.h>
#include <compat/state.h>
#include <config/settings.h>
#include <config/state.h>
#include <config/version.h>
#include <ctrl/functions.h>
#include <ctrl/state.h>
#include <dialog/state.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <gxm/functions.h>
#include <gxm/state.h>
#include <http/state.h>
#include <ime/state.h>
#include <io/functions.h>
#include <kernel/state.h>
#include <lang/state.h>
#include <mem/functions.h>
#include <modules/module_parent.h>
#include <motion/state.h>
#include <net/state.h>
#include <ngs/state.h>
#include <np/functions.h>
#include <np/state.h>
#include <np/trophy/context.h>
#include <overlay/display_manager.h>
#include <overlay/input.h>
#include <overlay/trophy_notification.h>
#include <packages/sfo.h>
#include <regmgr/state.h>
#include <renderer/functions.h>
#include <renderer/state.h>
#include <renderer/texture_cache.h>
#include <touch/state.h>

#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>

#if USE_DISCORD
#include <app/discord.h>
#endif

#include <gdbstub/functions.h>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_gamepad.h>

#ifdef __ANDROID__
#include <SDL3/SDL_system.h>
#endif

#ifdef __linux__
#include <pwd.h>
#endif

#include <algorithm>
#include <fstream>

namespace app {

static float sdl_axis_to_float(int16_t axis, float mult) {
    const float unsigned_axis = static_cast<float>(axis - INT16_MIN);
    const float f = unsigned_axis / static_cast<float>(UINT16_MAX);
    return std::clamp(((f * 2.0f) - 1.0f) * mult, -1.0f, 1.0f);
}

static overlay::button_states poll_overlay_input(EmuEnvState &emuenv) {
    overlay::button_states states{};
    uint32_t buttons = 0;
    float axes[4] = {};

    std::lock_guard<std::mutex> lock(emuenv.ctrl.mutex);

    auto &kb = emuenv.ctrl.keyboard_state;
    buttons |= kb.buttons_ext;
    axes[0] += kb.axes[0];
    axes[1] += kb.axes[1];
    axes[2] += kb.axes[2];
    axes[3] += kb.axes[3];

    for (const auto &[_, controller] : emuenv.ctrl.controllers) {
        SDL_Gamepad *gp = controller.controller.get();
        for (const auto &binding : get_controller_bindings_ext(emuenv)) {
            if (SDL_GetGamepadButton(gp, binding.controller))
                buttons |= binding.button;
        }

        const auto &axis_binds = emuenv.cfg.controller_axis_binds;
        if (axis_binds.size() > 5) {
            if (SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis_binds[4])) > 0x3FFF)
                buttons |= SCE_CTRL_L2;
            if (SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis_binds[5])) > 0x3FFF)
                buttons |= SCE_CTRL_R2;
        }

        if (axis_binds.size() > 3) {
            const float mult = emuenv.cfg.controller_analog_multiplier;
            axes[0] += sdl_axis_to_float(SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis_binds[0])), mult);
            axes[1] += sdl_axis_to_float(SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis_binds[1])), mult);
            axes[2] += sdl_axis_to_float(SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis_binds[2])), mult);
            axes[3] += sdl_axis_to_float(SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis_binds[3])), mult);
        }
    }

    auto set = [&](uint32_t mask, overlay::pad_button btn) {
        if (buttons & mask)
            states[static_cast<size_t>(btn)] = true;
    };
    set(SCE_CTRL_UP, overlay::pad_button::dpad_up);
    set(SCE_CTRL_DOWN, overlay::pad_button::dpad_down);
    set(SCE_CTRL_LEFT, overlay::pad_button::dpad_left);
    set(SCE_CTRL_RIGHT, overlay::pad_button::dpad_right);
    set(SCE_CTRL_TRIANGLE, overlay::pad_button::triangle);
    set(SCE_CTRL_CIRCLE, overlay::pad_button::circle);
    set(SCE_CTRL_CROSS, overlay::pad_button::cross);
    set(SCE_CTRL_SQUARE, overlay::pad_button::square);
    set(SCE_CTRL_START, overlay::pad_button::start);
    set(SCE_CTRL_SELECT, overlay::pad_button::select);
    set(SCE_CTRL_L1, overlay::pad_button::L1);
    set(SCE_CTRL_R1, overlay::pad_button::R1);
    set(SCE_CTRL_L2, overlay::pad_button::L2);
    set(SCE_CTRL_R2, overlay::pad_button::R2);
    set(SCE_CTRL_L3, overlay::pad_button::L3);
    set(SCE_CTRL_R3, overlay::pad_button::R3);

    constexpr float threshold = 0.5f;
    if (axes[0] < -threshold)
        states[static_cast<size_t>(overlay::pad_button::ls_left)] = true;
    if (axes[0] > threshold)
        states[static_cast<size_t>(overlay::pad_button::ls_right)] = true;
    if (axes[1] < -threshold)
        states[static_cast<size_t>(overlay::pad_button::ls_up)] = true;
    if (axes[1] > threshold)
        states[static_cast<size_t>(overlay::pad_button::ls_down)] = true;
    if (axes[2] < -threshold)
        states[static_cast<size_t>(overlay::pad_button::rs_left)] = true;
    if (axes[2] > threshold)
        states[static_cast<size_t>(overlay::pad_button::rs_right)] = true;
    if (axes[3] < -threshold)
        states[static_cast<size_t>(overlay::pad_button::rs_up)] = true;
    if (axes[3] > threshold)
        states[static_cast<size_t>(overlay::pad_button::rs_down)] = true;

    return states;
}

static void set_backend_renderer(EmuEnvState &emuenv, const std::string &backend_renderer) {
#ifndef __APPLE__
    emuenv.backend_renderer = (string_utils::toupper(backend_renderer) == "OPENGL")
        ? renderer::Backend::OpenGL
        : renderer::Backend::Vulkan;
#else
    emuenv.backend_renderer = renderer::Backend::Vulkan;
#endif
}

static bool applies_to_active_profile(const EmuEnvState &emuenv, const std::string &scope_app_path) {
    if (!scope_app_path.empty())
        return scope_app_path == emuenv.io.app_path;

    return emuenv.io.app_path.empty()
        || !config::has_custom_config(emuenv.config_path, emuenv.io.app_path);
}

static Config::CurrentConfig get_effective_current_config(const Config &cfg, const fs::path &config_path, const std::string &app_path) {
    Config effective_cfg;
    effective_cfg = cfg;
    config::set_current_config(effective_cfg, config_path, app_path);
    return effective_cfg.current_config;
}

static Config::CurrentConfig get_runtime_current_config_after_save(
    const Config::CurrentConfig &previous_current,
    const Config::CurrentConfig &effective_current,
    const std::vector<config::RestartRequiredSetting> &restart_required_settings) {
    Config::CurrentConfig runtime_current = effective_current;

    for (const auto setting : restart_required_settings) {
        switch (setting) {
        case config::RestartRequiredSetting::CpuOpt:
            runtime_current.cpu_opt = previous_current.cpu_opt;
            break;
        case config::RestartRequiredSetting::BackendRenderer:
            runtime_current.backend_renderer = previous_current.backend_renderer;
            break;
        case config::RestartRequiredSetting::GraphicsDevice:
            runtime_current.gpu_idx = previous_current.gpu_idx;
            break;
#ifdef __ANDROID__
        case config::RestartRequiredSetting::CustomDriver:
            runtime_current.custom_driver_name = previous_current.custom_driver_name;
            break;
#endif
        case config::RestartRequiredSetting::HighAccuracy:
            runtime_current.high_accuracy = previous_current.high_accuracy;
            break;
        case config::RestartRequiredSetting::ResolutionMultiplier:
            runtime_current.resolution_multiplier = previous_current.resolution_multiplier;
            break;
        case config::RestartRequiredSetting::MemoryMapping:
            runtime_current.memory_mapping = previous_current.memory_mapping;
            break;
        case config::RestartRequiredSetting::AudioBackend:
            runtime_current.audio_backend = previous_current.audio_backend;
            break;
        case config::RestartRequiredSetting::ValidationLayer:
            runtime_current.validation_layer = previous_current.validation_layer;
            break;
        }
    }

    return runtime_current;
}

void set_current_config(EmuEnvState &emuenv, const std::string &app_path) {
    config::set_current_config(emuenv.cfg, emuenv.config_path, app_path);
    set_backend_renderer(emuenv, emuenv.cfg.current_config.backend_renderer);
    emuenv.audio.set_global_volume(emuenv.cfg.current_config.audio_volume / 100.f);
    lang::set_locale(emuenv.cfg.current_config.sys_lang);
}

// Initializes paths to their respective defaults, to be changed later by settings or CLI
// Returns true if in portable mode, false otherwise
bool init_paths(Root &root_paths) {
    bool portable = false;
#ifdef __ANDROID__
    fs::path internal_storage_path = fs::path(SDL_GetAndroidExternalStoragePath()) / "";
    fs::path vita_storage_path = internal_storage_path / "vita/";

    // On Android, static assets are bundled inside the APK and accessed via SDL_IOFromFile.
    root_paths.set_static_assets_path({});

    root_paths.set_vita_fs_path(vita_storage_path);
    root_paths.set_log_path(internal_storage_path);
    root_paths.set_config_path(internal_storage_path);
    root_paths.set_shared_path(internal_storage_path);
    root_paths.set_cache_path(internal_storage_path / "cache" / "");
    root_paths.set_patch_path(internal_storage_path / "patch" / "");
#else
    auto sdl_exe_path = SDL_GetBasePath();
    auto exe_path = fs_utils::utf8_to_path(sdl_exe_path);

    root_paths.set_static_assets_path(exe_path);

#ifdef _WIN32
    auto portable_path = exe_path / "portable" / "";
#elif defined(__APPLE__)
    // On Apple platforms, exe_path is "Contents/Resources/" inside the app bundle.
    // An extra parent_path is apparently needed because of the trailing slash.
    auto portable_path = exe_path.parent_path().parent_path().parent_path().parent_path() / "portable" / "";
#elif defined(__linux__)
    fs::path portable_path = "";
    auto APPIMAGE = getenv("APPIMAGE"); // Used in AppImage
    if (APPIMAGE) {
        fs::path appimage_path = fs::path(APPIMAGE).remove_filename() / "";
        portable_path = appimage_path / "portable" / "";
    } else {
        portable_path = exe_path / "portable" / "";
    }
#endif

    if (fs::is_directory(portable_path)) {
        portable = true;
        // If a portable directory exists, use it for everything else.
        // Note that vita_fs_path should not be the same as the other paths.
        root_paths.set_vita_fs_path(portable_path / "fs" / "");
        root_paths.set_log_path(portable_path);
        root_paths.set_config_path(portable_path);
        root_paths.set_shared_path(portable_path);
        root_paths.set_cache_path(portable_path / "cache" / "");
        root_paths.set_patch_path(portable_path / "patch" / "");
    } else {
        // SDL_GetPrefPath is deferred as it creates the directory.
        // When using a portable directory, it is not needed.
        auto sdl_vita_fs_path = SDL_GetPrefPath(org_name, app_name);
        auto vita_fs_path = fs_utils::utf8_to_path(sdl_vita_fs_path);
        SDL_free(sdl_vita_fs_path);

#if defined(__APPLE__)
        // Store other data in the user-wide path. Otherwise we may end up dumping
        // files into the "/Applications/" install directory or the app bundle.
        // This will typically be "~/Library/Application Support/Vita3K/Vita3K/".
        // Check for config.yml first, though, to maintain backwards compatibility,
        // even though storing user data inside the app bundle is not a good idea.
        auto existing_config = exe_path / "config.yml";
        if (!fs::exists(existing_config)) {
            exe_path = vita_fs_path;
        }

        // vita_fs_path should not be the same as the other paths.
        // For backwards compatibility, though, check if ux0 exists first.
        auto existing_ux0 = vita_fs_path / "ux0";
        if (!fs::is_directory(existing_ux0)) {
            vita_fs_path = vita_fs_path / "fs" / "";
        }
#endif

        root_paths.set_vita_fs_path(vita_fs_path);
        root_paths.set_log_path(exe_path);
        root_paths.set_config_path(exe_path);
        root_paths.set_shared_path(exe_path);
        root_paths.set_cache_path(exe_path / "cache" / "");
        root_paths.set_patch_path(exe_path / "patch" / "");

#if defined(__linux__)
        // XDG Data Dirs.
        char home_path[PATH_MAX] = {};
        auto env_home = getenv("HOME");
        if (env_home != NULL)
            strncpy(home_path, env_home, PATH_MAX - 1);
        else {
            struct passwd *pw = getpwuid(getuid());
            if (pw) {
                strncpy(home_path, pw->pw_dir, PATH_MAX - 1);
            } else {
                LOG_CRITICAL("Failed to get home directory path");
            }
        }

        auto XDG_DATA_HOME = getenv("XDG_DATA_HOME");
        auto XDG_CACHE_HOME = getenv("XDG_CACHE_HOME");
        auto XDG_CONFIG_HOME = getenv("XDG_CONFIG_HOME");
        auto APPDIR = getenv("APPDIR"); // Used by AppImage

        // Config and game-specific configs
        if (XDG_CONFIG_HOME != NULL)
            root_paths.set_config_path(fs::path(XDG_CONFIG_HOME) / app_name / "");
        else if (home_path[0] != '\0')
            root_paths.set_config_path(fs::path(home_path) / ".config" / app_name / "");

        // Logs, cache and dumps
        if (XDG_CACHE_HOME != NULL) {
            root_paths.set_cache_path(fs::path(XDG_CACHE_HOME) / app_name / "");
            root_paths.set_log_path(fs::path(XDG_CACHE_HOME) / app_name / "");
        } else if (home_path[0] != '\0') {
            root_paths.set_cache_path(fs::path(home_path) / ".cache" / app_name / "");
            root_paths.set_log_path(fs::path(home_path) / ".cache" / app_name / "");
        }

        const constexpr char *static_asset_paths[] = {
            "/usr/local/share/Vita3K",
            "/usr/share/Vita3K",
        };

        // Check both normal case and all lowercase paths
        for (const auto &path : static_asset_paths) {
            if (fs::exists(path)) {
                root_paths.set_static_assets_path(path);
                break;
            }
            if (fs::exists(string_utils::tolower(path))) {
                root_paths.set_static_assets_path(string_utils::tolower(path));
                break;
            }
        }

        // Only really used in standalone (rare) or development
        if (fs::exists(exe_path / "shaders-builtin"))
            root_paths.set_static_assets_path(exe_path);

        // AppImage root
        if (APPDIR != NULL && fs::exists(fs::path(APPDIR) / "usr/share/Vita3K"))
            root_paths.set_static_assets_path(fs::path(APPDIR) / "usr/share/Vita3K");

        // shared path
        if (XDG_DATA_HOME != NULL)
            root_paths.set_shared_path(fs::path(XDG_DATA_HOME) / app_name / "");
        else if (home_path[0] != '\0')
            root_paths.set_shared_path(fs::path(home_path) / ".local/share" / app_name / "");

        // These default to being in shared path
        root_paths.set_vita_fs_path(root_paths.get_shared_path() / app_name / "");
        root_paths.set_patch_path(root_paths.get_shared_path() / "patch" / "");
#endif
    }
#endif

    // Create paths for safety
    fs::create_directories(root_paths.get_config_path());
    fs::create_directories(root_paths.get_cache_path());
    fs::create_directories(root_paths.get_log_path() / "shaderlog");
    fs::create_directories(root_paths.get_log_path() / "texturelog");
    fs::create_directories(root_paths.get_patch_path());

    const auto gui_configs_source_path = root_paths.get_static_assets_path() / "data" / "gui-configs";
    if (fs::is_directory(gui_configs_source_path)) {
        const auto gui_configs_destination_path = root_paths.get_config_path() / "gui-configs";
        if (!fs_utils::copy_directory_contents(gui_configs_source_path, gui_configs_destination_path, fs::copy_options::overwrite_existing))
            LOG_WARN("Failed to copy GUI configs from {} to {}", gui_configs_source_path, gui_configs_destination_path);
    }
    return portable;
}

bool init(EmuEnvState &state, Config &cfg, const Root &root_paths) {
    state.cfg = std::move(cfg);

    state.default_path = root_paths.get_vita_fs_path();
    state.log_path = root_paths.get_log_path();
    state.config_path = root_paths.get_config_path();
    state.cache_path = root_paths.get_cache_path();
    state.shared_path = root_paths.get_shared_path();
    state.static_assets_path = root_paths.get_static_assets_path();
    state.patch_path = root_paths.get_patch_path();

    // If configuration does not provide a VitaFS path, use SDL's default
    if (state.cfg.vita_fs_path == root_paths.get_vita_fs_path() || state.cfg.vita_fs_path.empty()) {
        state.vita_fs_path = root_paths.get_vita_fs_path();
    } else {
        auto last_char = state.cfg.vita_fs_path.back();
        if (last_char != fs::path::preferred_separator && last_char != '/')
            state.cfg.vita_fs_path += fs::path::preferred_separator;
        state.vita_fs_path = state.cfg.get_vita_fs_path();
    }

    set_current_config(state, state.cfg.run_app_path.has_value() ? *state.cfg.run_app_path : "");

    // Here any path can be used since they are all the same in windows/macos
    LOG_INFO("Base path: {}", state.static_assets_path);
#if defined(__linux__) && !defined(__ANDROID__)
    LOG_INFO("Static assets path: {}", state.static_assets_path);
    LOG_INFO("Shared path: {}", state.shared_path);
    LOG_INFO("Log path: {}", state.log_path);
    LOG_INFO("User config path: {}", state.config_path);
    LOG_INFO("User cache path: {}", state.cache_path);
#endif
    LOG_INFO("VitaFS path: {}", state.vita_fs_path);

    if (!init(state.io, state.cache_path, state.log_path, state.vita_fs_path, state.cfg.console)) {
        LOG_ERROR("Failed to initialize file system for the emulator!");
        return false;
    }

#if USE_DISCORD
    if (state.cfg.discord_rich_presence && discordrpc::init()) {
        discordrpc::update_presence();
    }
#endif

    state.motion.init();

    return true;
}

void shutdown_app_runtime(EmuEnvState &state) {
    state.audio.stop_all_ports();

    gxm::shutdown(state);

    state.net.abort_all();
    state.http.shutdown_connections();

    state.kernel.process_exit();

    state.motion.reset_runtime();

    state.audio.deinit();

    state.renderer->preclose_action();
    renderer::stop_render_thread(*state.renderer);
    gxm::destroy_all_contexts(state, true);
    gxm::destroy_all_render_targets(state, true);
    state.gxm.deinit();
    state.overlay_manager.reset();

    state.display.deinit();

    state.netctl.deinit();

    ngs::deinit(state.ngs, state.mem);

    // trophy (maybe namespace this?)
    deinit(state.np);

    state.http.deinit();

    state.net.deinit();

    io_deinit(state.io);

    state.camera.deinit();

    state.common_dialog.deinit();

    state.ime.deinit();

    state.touch.reset_runtime();

    state.sfo_handle.header = {};
    state.sfo_handle.entries.clear();

    state.license_content_id.clear();
    state.license_title_id.clear();

    state.app_info = {};

    state.regmgr.system_dreg.clear();
    state.regmgr.system_dreg_path.clear();
    state.regmgr.reg_category_template.clear();
    state.regmgr.reg_template.clear();

    state.kernel.deinit(state.mem);

    state.renderer->cleanup();
    state.renderer.reset();

    deinit_mem(state.mem);
}

void reset_app_state(EmuEnvState &state) {
    app::reset_perf_metrics(state);

    state.app_path.clear();
    state.current_app_title.clear();
    state.self_name.clear();
    state.self_path.clear();
    state.main_thread_id = 0;
    state.drop_inputs = false;
    state.missing_nids.clear();
    state.clear_app_launch_request();

    state.ctrl.reset_runtime();
    set_current_config(state, "");

#if USE_DISCORD
    if (state.cfg.discord_rich_presence)
        discordrpc::update_presence();
#endif

    // re-init
    init_libraries(state);

    if (!init(state.io, state.cache_path, state.log_path, state.vita_fs_path, state.cfg.console)) {
        LOG_ERROR("Failed to re-initialize file system after deinit!");
    }
}

bool late_init(EmuEnvState &state) {
    // note: mem is not initialized yet but that's not an issue
    // the renderer is not using it yet, just storing it for later uses
    state.renderer->late_init(state.cfg, state.app_path, state.mem);

    const bool need_page_table = state.renderer->mapping_method == MappingMethod::PageTable || state.renderer->mapping_method == MappingMethod::NativeBuffer;
    if (!init(state.mem, need_page_table)) {
        LOG_ERROR("Failed to initialize memory for emulator state!");
        return false;
    }

    if (!state.audio.init(state.cfg.current_config.audio_backend)) {
        LOG_WARN("Failed to initialize audio! Audio will not work.");
    }

    if (!ngs::init(state.ngs, state.mem)) {
        LOG_ERROR("Failed to initialize ngs.");
        return false;
    }

    return true;
}

void apply_renderer_config(EmuEnvState &emuenv) {
    auto &r = *emuenv.renderer;
    const auto &cc = emuenv.cfg.current_config;

    r.res_multiplier = cc.resolution_multiplier;
    r.set_vsync_state(cc.v_sync);
    r.set_surface_sync_state(cc.disable_surface_sync);
    r.set_screen_filter(cc.screen_filter);
    r.set_anisotropic_filtering(cc.anisotropic_filtering);
    r.set_stretch_display(cc.stretch_the_display_area);
    r.stretch_hd_pixel_perfect(cc.fullscreen_hd_res_pixel_perfect);
    r.set_async_compilation(cc.async_pipeline_compilation);
    r.get_texture_cache()->set_replacement_state(cc.import_textures, cc.export_textures, cc.export_as_png);
#ifdef __ANDROID__
    if (r.support_custom_drivers())
        r.set_turbo_mode(emuenv.cfg.turbo_mode);
#endif
    emuenv.display.fps_hack = cc.fps_hack;

    if (!emuenv.overlay_manager)
        emuenv.overlay_manager = std::make_unique<overlay::display_manager>();
    r.overlay_manager = emuenv.overlay_manager.get();
    emuenv.overlay_manager->set_flip_request_callback([&r]() {
        r.async_flip_requested.store(true, std::memory_order_relaxed);
        r.command_buffer_queue.wake();
    });
    r.common_dialog = &emuenv.common_dialog;
    r.sys_date_format = cc.sys_date_format;
    r.sys_lang = cc.sys_lang;
    r.sys_button = cc.sys_button;

    r.show_compile_shaders = emuenv.cfg.show_compile_shaders;
    app::sync_perf_overlay_config(emuenv);

    // overlay input callbacks
    emuenv.overlay_manager->set_input_callbacks(
        [&emuenv]() -> overlay::button_states {
            return poll_overlay_input(emuenv);
        },
        [&emuenv](bool intercepted) {
            auto &ctrl = emuenv.ctrl;
            if (intercepted) {
                ctrl.overlay_input_intercepted.store(true, std::memory_order_relaxed);
            } else {
                ctrl.overlay_input_intercepted.store(false, std::memory_order_relaxed);
            }
        },
        [&emuenv]() -> overlay::overlay_touch_state {
            auto &mouse = emuenv.ctrl.overlay_mouse;
            overlay::overlay_touch_state state;
            state.x = mouse.x.load(std::memory_order_relaxed);
            state.y = mouse.y.load(std::memory_order_relaxed);
            state.pressed = mouse.pressed.load(std::memory_order_relaxed);
            return state;
        });

    emuenv.np.trophy_state.add_trophy_unlock_callback([&emuenv](NpTrophyUnlockCallbackData &data) {
        if (!emuenv.overlay_manager)
            return;
        auto notif = emuenv.overlay_manager->get<overlay::trophy_notification>();
        if (!notif)
            notif = emuenv.overlay_manager->create<overlay::trophy_notification>();

        std::vector<uint8_t> grade_icon_buf;
        const char *grade_name = nullptr;
        switch (data.trophy_kind) {
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM: grade_name = "platinum"; break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD: grade_name = "gold"; break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER: grade_name = "silver"; break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE: grade_name = "bronze"; break;
        default: break;
        }
        if (grade_name) {
            const auto icon_path = emuenv.static_assets_path / "icons" / (std::string(grade_name) + ".png");
            std::ifstream file(icon_path.c_str(), std::ios::binary | std::ios::ate);
            if (file) {
                const auto size = file.tellg();
                file.seekg(0);
                grade_icon_buf.resize(static_cast<size_t>(size));
                file.read(reinterpret_cast<char *>(grade_icon_buf.data()), size);
            }
        }

        notif->enqueue(data.trophy_name, data.icon_buf, grade_icon_buf);
    });
}

void apply_runtime_settings(EmuEnvState &emuenv) {
    const auto &cc = emuenv.cfg.current_config;

    emuenv.audio.set_global_volume(cc.audio_volume / 100.f);
    lang::set_locale(cc.sys_lang);
    emuenv.compat.log_compat_warn = emuenv.cfg.log_compat_warn;

    if (!emuenv.renderer)
        return;

    auto &r = *emuenv.renderer;
    r.set_vsync_state(cc.v_sync);
    r.set_surface_sync_state(cc.disable_surface_sync);
    r.set_screen_filter(cc.screen_filter);
    r.set_anisotropic_filtering(cc.anisotropic_filtering);
    r.set_stretch_display(cc.stretch_the_display_area);
    r.stretch_hd_pixel_perfect(cc.fullscreen_hd_res_pixel_perfect);
    r.set_async_compilation(cc.async_pipeline_compilation);
    r.get_texture_cache()->set_replacement_state(cc.import_textures, cc.export_textures, cc.export_as_png);
#ifdef __ANDROID__
    if (r.support_custom_drivers())
        r.set_turbo_mode(emuenv.cfg.turbo_mode);
#endif
    emuenv.display.fps_hack = cc.fps_hack;
    r.sys_date_format = cc.sys_date_format;
    r.sys_lang = cc.sys_lang;
    r.sys_button = cc.sys_button;
    r.show_compile_shaders = emuenv.cfg.show_compile_shaders;
    app::sync_perf_overlay_config(emuenv);
}

SettingsCommitResult commit_settings(EmuEnvState &emuenv, const Config &desired_cfg, const std::string &scope_app_path) {
    SettingsCommitResult result;
    const bool scope_is_custom = !scope_app_path.empty();
    const bool active_profile = applies_to_active_profile(emuenv, scope_app_path);
    const bool running_game = !emuenv.io.app_path.empty();
    const std::string active_app_path = running_game ? emuenv.io.app_path : std::string();

    result.affected_running_game = running_game && active_profile;
    result.active_source_is_custom = scope_is_custom
        || (!active_app_path.empty() && config::has_custom_config(emuenv.config_path, active_app_path));

    const auto previous_current = emuenv.cfg.current_config;

    if (scope_is_custom) {
        const bool had_custom_config = config::has_custom_config(emuenv.config_path, scope_app_path);

        Config persisted_cfg;
        persisted_cfg = emuenv.cfg;
        persisted_cfg.current_config = desired_cfg.current_config;
        config::save_current_config(persisted_cfg, emuenv.config_path, scope_app_path, true);

        result.custom_config_created = !had_custom_config;
        result.active_source_is_custom = true;

        if (active_profile) {
            const auto effective_current = get_effective_current_config(persisted_cfg, emuenv.config_path, active_app_path);
            if (result.affected_running_game)
                result.restart_required_settings = config::get_restart_required_settings(previous_current, effective_current);
            emuenv.cfg.current_config = result.affected_running_game
                ? get_runtime_current_config_after_save(previous_current, effective_current, result.restart_required_settings)
                : effective_current;
            apply_runtime_settings(emuenv);
            result.runtime_settings_applied = true;
        }

        return result;
    }

    Config persisted_cfg;
    persisted_cfg = emuenv.cfg;
    persisted_cfg = desired_cfg;
    persisted_cfg.current_config = desired_cfg.current_config;
    config::save_current_config(persisted_cfg, emuenv.config_path, {}, false);
    emuenv.cfg = persisted_cfg;

    if (!active_profile) {
        emuenv.cfg.current_config = previous_current;
        result.active_source_is_custom = true;
        return result;
    }

    result.active_source_is_custom = false;
    const auto effective_current = get_effective_current_config(emuenv.cfg, emuenv.config_path, active_app_path);
    if (result.affected_running_game)
        result.restart_required_settings = config::get_restart_required_settings(previous_current, effective_current);
    emuenv.cfg.current_config = result.affected_running_game
        ? get_runtime_current_config_after_save(previous_current, effective_current, result.restart_required_settings)
        : effective_current;
    apply_runtime_settings(emuenv);
    result.runtime_settings_applied = true;
    return result;
}

SettingsCommitResult delete_custom_settings(EmuEnvState &emuenv, const std::string &scope_app_path) {
    SettingsCommitResult result;
    if (scope_app_path.empty())
        return result;

    if (!config::delete_custom_config(emuenv.config_path, scope_app_path))
        return result;

    if (emuenv.io.app_path != scope_app_path)
        return result;

    Config desired_cfg;
    desired_cfg = emuenv.cfg;
    config::set_current_config(desired_cfg, emuenv.config_path, {});
    return commit_settings(emuenv, desired_cfg, {});
}

// need to remove this function completely, waiting for gdb refactor
void destroy(EmuEnvState &emuenv) {
    if (emuenv.cfg.gdbstub)
        server_close(emuenv);
}

} // namespace app
