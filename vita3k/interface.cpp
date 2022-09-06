// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include "interface.h"

#include "module/load_module.h"

#include <app/functions.h>
#include <config/state.h>
#include <ctime>
#include <ctrl/functions.h>
#include <ctrl/state.h>
#include <dialog/state.h>
#include <display/functions.h>
#include <display/state.h>
#include <gui/functions.h>
#include <gxm/state.h>
#include <host/dialog/filesystem.h>
#include <io/functions.h>
#include <io/vfs.h>
#include <kernel/state.h>
#include <packages/functions.h>
#include <packages/license.h>
#include <packages/pkg.h>
#include <packages/sfo.h>
#include <renderer/state.h>
#include <renderer/texture_cache.h>

#include <modules/module_parent.h>
#include <motion/event_handler.h>
#include <string>
#include <touch/functions.h>
#include <util/log.h>
#include <util/string_utils.h>
#include <util/vector_utils.h>

#include <gui/imgui_impl_sdl.h>

#include <regex>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_system.h>
#include <SDL3/SDL_video.h>

#include <fmt/chrono.h>
#include <stb_image_write.h>

#include <gdbstub/functions.h>

#if USE_DISCORD
#include <app/discord.h>
#endif

static size_t write_to_buffer(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n) {
    vfs::FileBuffer *const buffer = static_cast<vfs::FileBuffer *>(pOpaque);
    assert(file_ofs == buffer->size());
    const uint8_t *const first = static_cast<const uint8_t *>(pBuf);
    const uint8_t *const last = &first[n];
    buffer->insert(buffer->end(), first, last);

    return n;
}

static const char *miniz_get_error(const ZipPtr &zip) {
    return mz_zip_get_error_string(mz_zip_get_last_error(zip.get()));
}

static void set_theme_name(EmuEnvState &emuenv, vfs::FileBuffer &buf) {
    emuenv.app_info.app_title = gui::get_theme_title_from_buffer(buf);
    emuenv.app_info.app_title_id = string_utils::remove_special_chars(emuenv.app_info.app_title);
    const auto nospace = std::remove_if(emuenv.app_info.app_title_id.begin(), emuenv.app_info.app_title_id.end(), isspace);
    emuenv.app_info.app_title_id.erase(nospace, emuenv.app_info.app_title_id.end());
    emuenv.app_info.app_category = "theme";
    emuenv.app_info.app_title += " (Theme)";
}

static bool is_nonpdrm(EmuEnvState &emuenv, const fs::path &output_path) {
    const auto app_license_path{ emuenv.pref_path / "ux0/license" / emuenv.app_info.app_title_id / fmt::format("{}.rif", emuenv.app_info.app_content_id) };
    const auto is_patch_found_app_license = (emuenv.app_info.app_category == "gp") && fs::exists(app_license_path);
    if (fs::exists(output_path / "sce_sys/package/work.bin") || is_patch_found_app_license) {
        fs::path licpath = is_patch_found_app_license ? app_license_path : output_path / "sce_sys/package/work.bin";
        LOG_INFO("Decrypt layer: {}", output_path);
        if (!decrypt_install_nonpdrm(emuenv, licpath, output_path)) {
            LOG_ERROR("NoNpDrm installation failed, deleting data!");
            fs::remove_all(output_path);
            return false;
        }
        return true;
    }

    return false;
}

static bool set_content_path(EmuEnvState &emuenv, const bool is_theme, fs::path &dest_path) {
    const auto app_path = dest_path / "app" / emuenv.app_info.app_title_id;

    if (emuenv.app_info.app_category == "ac") {
        if (is_theme) {
            dest_path /= fs::path("theme") / emuenv.app_info.app_content_id;
            emuenv.app_info.app_title += " (Theme)";
        } else {
            emuenv.app_info.app_content_id = emuenv.app_info.app_content_id.substr(20);
            dest_path /= fs::path("addcont") / emuenv.app_info.app_title_id / emuenv.app_info.app_content_id;
            emuenv.app_info.app_title += " (DLC)";
        }
    } else if (emuenv.app_info.app_category.find("gp") != std::string::npos) {
        if (!fs::exists(app_path) || fs::is_empty(app_path)) {
            LOG_ERROR("Install app before patch");
            return false;
        }
        dest_path /= fs::path("patch") / emuenv.app_info.app_title_id;
        emuenv.app_info.app_title += " (Patch)";
    } else {
        dest_path = app_path;
        emuenv.app_info.app_title += " (App)";
    }

    return true;
}

static bool install_archive_content(EmuEnvState &emuenv, GuiState *gui, const ZipPtr &zip, const std::string &content_path, const std::function<void(ArchiveContents)> &progress_callback) {
    std::string sfo_path = "sce_sys/param.sfo";
    std::string theme_path = "theme.xml";
    vfs::FileBuffer buffer, theme;

    const auto is_theme = mz_zip_reader_extract_file_to_callback(zip.get(), (content_path + theme_path).c_str(), &write_to_buffer, &theme, 0);

    auto output_path{ emuenv.pref_path / "ux0" };
    if (mz_zip_reader_extract_file_to_callback(zip.get(), (content_path + sfo_path).c_str(), &write_to_buffer, &buffer, 0)) {
        sfo::get_param_info(emuenv.app_info, buffer, emuenv.cfg.sys_lang);
        if (!set_content_path(emuenv, is_theme, output_path))
            return false;
    } else if (is_theme) {
        set_theme_name(emuenv, theme);
        output_path /= fs::path("theme") / emuenv.app_info.app_title_id;
    } else {
        LOG_CRITICAL("miniz error: {} extracting file: {}", miniz_get_error(zip), sfo_path);
        return false;
    }

    const auto created = fs::create_directories(output_path);
    if (!created) {
        if (!gui || gui->file_menu.archive_install_dialog) {
            fs::remove_all(output_path);
        } else if (!gui->file_menu.archive_install_dialog) {
            gui::GenericDialogState status = gui::UNK_STATE;

            while (handle_events(emuenv, *gui) && (status == gui::UNK_STATE)) {
                gui::draw_begin(*gui, emuenv);
                gui::draw_ui(*gui, emuenv);
                gui::draw_reinstall_dialog(&status, *gui, emuenv);
                gui::draw_end(*gui);
                emuenv.renderer->swap_window(emuenv.window.get());
            }
            switch (status) {
            case gui::CANCEL_STATE:
                LOG_INFO("{} already installed, {}", emuenv.app_info.app_title_id, emuenv.app_info.app_category.find("gd") != std::string::npos ? "launching application..." : "Open home");
                return true;
            case gui::CONFIRM_STATE:
                fs::remove_all(output_path);
                break;
            case gui::UNK_STATE:
                exit(0);
            default: break;
            }
        }
    }

    float file_progress = 0;
    float decrypt_progress = 0;

    const auto update_progress = [&]() {
        if (progress_callback)
            progress_callback({ {}, {}, { file_progress * 0.7f + decrypt_progress * 0.3f } });
    };

    mz_uint num_files = mz_zip_reader_get_num_files(zip.get());
    for (mz_uint i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat)) {
            continue;
        }
        const std::string m_filename = file_stat.m_filename;
        if (m_filename.find(content_path) != std::string::npos) {
            file_progress = static_cast<float>(i) / num_files * 100.0f;
            update_progress();

            std::string replace_filename = m_filename.substr(content_path.size());
            const fs::path file_output = (output_path / fs_utils::utf8_to_path(replace_filename)).generic_path();
            if (mz_zip_reader_is_file_a_directory(zip.get(), i)) {
                fs::create_directories(file_output);
            } else {
                fs::create_directories(file_output.parent_path());
                LOG_INFO("Extracting {}", file_output);
                mz_zip_reader_extract_to_file(zip.get(), i, fs_utils::path_to_utf8(file_output).c_str(), 0);
            }
        }
    }

    if (fs::exists(output_path / "sce_sys/package/") && emuenv.app_info.app_title_id.starts_with("PCS")) {
        update_progress();
        if (is_nonpdrm(emuenv, output_path))
            decrypt_progress = 100.f;
        else
            return false;
    }
    if (!copy_path(output_path, emuenv.pref_path, emuenv.app_info.app_title_id, emuenv.app_info.app_category))
        return false;

    update_progress();

    LOG_INFO("{} [{}] installed successfully!", emuenv.app_info.app_title, emuenv.app_info.app_title_id);

    if (!gui->file_menu.archive_install_dialog && (emuenv.app_info.app_category != "theme")) {
        gui::update_notice_info(*gui, emuenv, "content");
        if ((emuenv.app_info.app_category.find("gd") != std::string::npos) || (emuenv.app_info.app_category.find("gp") != std::string::npos)) {
            gui::init_user_app(*gui, emuenv, emuenv.app_info.app_title_id);
            gui::save_apps_cache(*gui, emuenv);
        }
    }

    return true;
}

static std::vector<std::string> get_archive_contents_path(const ZipPtr &zip) {
    mz_uint num_files = mz_zip_reader_get_num_files(zip.get());
    std::vector<std::string> content_path;
    std::string sfo_path = "sce_sys/param.sfo";
    std::string theme_path = "theme.xml";

    for (mz_uint i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat))
            continue;

        std::string m_filename = std::string(file_stat.m_filename);
        if (m_filename.find("sce_module/steroid.suprx") != std::string::npos) {
            LOG_CRITICAL("A Vitamin dump was detected, aborting installation...");
#ifdef __ANDROID__
            SDL_ShowAndroidToast("Vitamin dumps are not supported!", 1, -1, 0, 0);
#endif
            content_path.clear();
            break;
        }

        const auto is_content = (m_filename.find(sfo_path) != std::string::npos) || (m_filename.find(theme_path) != std::string::npos);
        if (is_content) {
            const auto content_type = (m_filename.find(sfo_path) != std::string::npos) ? sfo_path : theme_path;
            m_filename.erase(m_filename.find(content_type));
            vector_utils::push_if_not_exists(content_path, m_filename);
        }
    }

    return content_path;
}

std::vector<ContentInfo> install_archive(EmuEnvState &emuenv, GuiState *gui, const fs::path &archive_path, const std::function<void(ArchiveContents)> &progress_callback) {
    FILE *vpk_fp = host::dialog::filesystem::resolve_host_handle(archive_path);

    if (!vpk_fp) {
        LOG_CRITICAL("Failed to load archive file in path: {}", archive_path.generic_path().string());
        return {};
    }
    const ZipPtr zip(new mz_zip_archive, delete_zip);
    std::memset(zip.get(), 0, sizeof(*zip));

    if (!mz_zip_reader_init_cfile(zip.get(), vpk_fp, 0, 0)) {
        LOG_CRITICAL("miniz error reading archive: {}", miniz_get_error(zip));
        fclose(vpk_fp);
        return {};
    }

    const auto content_path = get_archive_contents_path(zip);
    if (content_path.empty()) {
        fclose(vpk_fp);
        return {};
    }

    const auto count = static_cast<float>(content_path.size());
    float current = 0.f;
    const auto update_progress = [&]() {
        if (progress_callback)
            progress_callback({ count, current, {} });
    };
    update_progress();

    std::vector<ContentInfo> content_installed{};
    for (auto &path : content_path) {
        current++;
        update_progress();
        bool state = install_archive_content(emuenv, gui, zip, path, progress_callback);
        // Can't use emplace_back due to Clang 15 for macos
        content_installed.push_back({ emuenv.app_info.app_title, emuenv.app_info.app_title_id, emuenv.app_info.app_category, emuenv.app_info.app_content_id, path, state });
    }

    fclose(vpk_fp);
    return content_installed;
}

static std::vector<fs::path> get_contents_path(const fs::path &path) {
    std::vector<fs::path> contents_path;

    for (const auto &p : fs::recursive_directory_iterator(path)) {
        auto filename = p.path().filename();
        const auto is_content = (filename == "param.sfo") || (filename == "theme.xml");
        if (is_content) {
            auto parent_path = p.path().parent_path();
            const auto content_path = (filename == "param.sfo") ? parent_path.parent_path() : parent_path;
            vector_utils::push_if_not_exists(contents_path, content_path);
        }
    }

    return contents_path;
}

static bool install_content(EmuEnvState &emuenv, GuiState *gui, const fs::path &content_path) {
    const auto sfo_path{ content_path / "sce_sys/param.sfo" };
    const auto theme_path{ content_path / "theme.xml" };
    vfs::FileBuffer buffer;

    const auto is_theme = fs::exists(theme_path);
    auto dst_path{ emuenv.pref_path / "ux0" };
    if (fs_utils::read_data(sfo_path, buffer)) {
        sfo::get_param_info(emuenv.app_info, buffer, emuenv.cfg.sys_lang);
        if (!set_content_path(emuenv, is_theme, dst_path))
            return false;

        if (exists(dst_path))
            fs::remove_all(dst_path);

    } else if (fs_utils::read_data(theme_path, buffer)) {
        set_theme_name(emuenv, buffer);
        dst_path /= fs::path("theme") / fs_utils::utf8_to_path(emuenv.app_info.app_title_id);
    } else {
        LOG_ERROR("Param.sfo file is missing in path", sfo_path);
        return false;
    }

    if (!copy_directories(content_path, dst_path)) {
        LOG_ERROR("Failed to copy directory to: {}", dst_path);
        return false;
    }

    if (fs::exists(dst_path / "sce_sys/package/") && !is_nonpdrm(emuenv, dst_path))
        return false;

    if (!copy_path(dst_path, emuenv.pref_path, emuenv.app_info.app_title_id, emuenv.app_info.app_category))
        return false;

    LOG_INFO("{} [{}] installed successfully!", emuenv.app_info.app_title, emuenv.app_info.app_title_id);

    if ((emuenv.app_info.app_category.find("gd") != std::string::npos) || (emuenv.app_info.app_category.find("gp") != std::string::npos)) {
        gui::init_user_app(*gui, emuenv, emuenv.app_info.app_title_id);
        gui::save_apps_cache(*gui, emuenv);
    }

    if (emuenv.app_info.app_category != "theme")
        gui::update_notice_info(*gui, emuenv, "content");

    return true;
}

uint32_t install_contents(EmuEnvState &emuenv, GuiState *gui, const fs::path &path) {
    const auto src_path = get_contents_path(path);

    LOG_WARN_IF(src_path.empty(), "No found any content compatible on this path: {}", path);

    uint32_t installed = 0;
    for (const auto &src : src_path) {
        if (install_content(emuenv, gui, src))
            ++installed;
    }

    if (installed) {
        gui::save_apps_cache(*gui, emuenv);
        LOG_INFO("Successfully installed {} content!", installed);
    }

    return installed;
}

static ExitCode load_app_impl(SceUID &main_module_id, EmuEnvState &emuenv) {
    const auto call_import = [&emuenv](CPUState &cpu, uint32_t nid, SceUID thread_id) {
        ::call_import(emuenv, cpu, nid, thread_id);
    };
    if (!emuenv.kernel.init(emuenv.mem, call_import, emuenv.kernel.cpu_opt)) {
        LOG_WARN("Failed to init kernel!");
        return KernelInitFailed;
    }

    if (emuenv.cfg.archive_log) {
        const fs::path log_directory{ emuenv.log_path / "logs" };
        fs::create_directory(log_directory);
        const auto log_path{ log_directory / fs_utils::utf8_to_path(emuenv.io.title_id + " - [" + string_utils::remove_special_chars(emuenv.current_app_title) + "].log") };
        if (logging::add_sink(log_path) != Success)
            return InitConfigFailed;
        logging::set_level(static_cast<spdlog::level::level_enum>(emuenv.cfg.log_level));
    }

    LOG_INFO("CPU Optimisation state: {}", emuenv.cfg.current_config.cpu_opt);
    LOG_INFO("ngs state: {}", emuenv.cfg.current_config.ngs_enable);
    LOG_INFO("Resolution multiplier: {}", emuenv.cfg.resolution_multiplier);

    // Set controller overlay state
    if (emuenv.cfg.enable_gamepad_overlay) {
        gui::set_controller_overlay_state(gui::get_overlay_display_mask(emuenv.cfg));
        refresh_controllers(emuenv.ctrl, emuenv);
    }

    if (emuenv.ctrl.controllers_num) {
        LOG_INFO("{} Controllers Connected", emuenv.ctrl.controllers_num);
        for (auto controller_it = emuenv.ctrl.controllers.begin(); controller_it != emuenv.ctrl.controllers.end(); ++controller_it) {
            LOG_INFO("Controller {}: {}", controller_it->second.port, controller_it->second.name);
        }
        if (emuenv.ctrl.has_motion_support)
            LOG_INFO("Controller has motion support");
    }
    constexpr std::array modules_mode_names{ "Automatic", "Auto & Manual", "Manual" };
    LOG_INFO("modules mode: {}", modules_mode_names.at(emuenv.cfg.current_config.modules_mode));
    if ((emuenv.cfg.current_config.modules_mode != ModulesMode::AUTOMATIC) && !emuenv.cfg.current_config.lle_modules.empty()) {
        std::string modules;
        for (const auto &mod : emuenv.cfg.current_config.lle_modules) {
            modules += mod + ",";
        }
        modules.pop_back();
        LOG_INFO("lle-modules: {}", modules);
    }

    LOG_INFO("Title: {}", emuenv.current_app_title);
    LOG_INFO("Serial: {}", emuenv.io.title_id);
    LOG_INFO("Version: {}", emuenv.app_info.app_version);
    LOG_INFO("Category: {}", emuenv.app_info.app_category);

    init_device_paths(emuenv.io);
    init_savedata_app_path(emuenv.io, emuenv.pref_path);

    // Load param.sfo
    vfs::FileBuffer param_sfo;
    if (vfs::read_app_file(param_sfo, emuenv.pref_path, emuenv.io.app_path, "sce_sys/param.sfo"))
        sfo::load(emuenv.sfo_handle, param_sfo);

    init_exported_vars(emuenv);

    // Load main executable
    emuenv.self_path = !emuenv.cfg.self_path.empty() ? emuenv.cfg.self_path : EBOOT_PATH;
    main_module_id = load_module(emuenv, "app0:" + emuenv.self_path);
    if (main_module_id >= 0) {
        const auto module = emuenv.kernel.loaded_modules[main_module_id];
        LOG_INFO("Main executable {} ({}) loaded", module->info.module_name, emuenv.self_path);
    } else
        return FileNotFound;
    // Set self name from self path, can contain folder, get file name only
    emuenv.self_name = fs::path(emuenv.self_path).filename().string();

    // get list of preload modules
    SceUInt32 process_preload_disabled = 0;
    auto process_param = emuenv.kernel.process_param.get(emuenv.mem);
    if (process_param) {
        auto preload_disabled_ptr = Ptr<SceUInt32>(process_param->process_preload_disabled);
        if (preload_disabled_ptr) {
            process_preload_disabled = *preload_disabled_ptr.get(emuenv.mem);
        }
    }
    const auto module_app_path{ emuenv.pref_path / "ux0/app" / emuenv.io.app_path / "sce_module" };

    std::vector<std::string> lib_load_list = {};
    // todo: check if module is imported
    auto add_preload_module = [&](uint32_t code, SceSysmoduleModuleId module_id, const std::string &name, bool load_from_app) {
        if ((process_preload_disabled & code) == 0) {
            if (is_lle_module(name, emuenv)) {
                const auto module_name_file = fmt::format("{}.suprx", name);
                if (load_from_app && fs::exists(module_app_path / module_name_file))
                    lib_load_list.emplace_back(fmt::format("app0:sce_module/{}", module_name_file));
                else if (fs::exists(emuenv.pref_path / "vs0/sys/external" / module_name_file))
                    lib_load_list.emplace_back(fmt::format("vs0:sys/external/{}", module_name_file));
            }

            if (module_id != SCE_SYSMODULE_INVALID)
                emuenv.kernel.loaded_sysmodules[module_id] = {};
        }
    };
    add_preload_module(0x00010000, SCE_SYSMODULE_INVALID, "libc", true);
    add_preload_module(0x00020000, SCE_SYSMODULE_DBG, "libdbg", false);
    add_preload_module(0x00080000, SCE_SYSMODULE_INVALID, "libshellsvc", false);
    add_preload_module(0x00100000, SCE_SYSMODULE_INVALID, "libcdlg", false);
    add_preload_module(0x00200000, SCE_SYSMODULE_FIOS2, "libfios2", true);
    add_preload_module(0x00400000, SCE_SYSMODULE_APPUTIL, "apputil", false);
    add_preload_module(0x00800000, SCE_SYSMODULE_INVALID, "libSceFt2", false);
    add_preload_module(0x01000000, SCE_SYSMODULE_INVALID, "libpvf", false);
    add_preload_module(0x02000000, SCE_SYSMODULE_PERF, "libperf", false); // if DEVELOPMENT_MODE dipsw is set

    for (const auto &module_path : lib_load_list) {
        auto res = load_module(emuenv, module_path);
        if (res < 0)
            return FileNotFound;
    }
    return Success;
}

static void switch_full_screen(EmuEnvState &emuenv) {
    emuenv.display.fullscreen = !emuenv.display.fullscreen;
    emuenv.renderer->set_fullscreen(emuenv.display.fullscreen);

    SDL_SetWindowFullscreen(emuenv.window.get(), emuenv.display.fullscreen.load());

    // Refresh Viewport Size
    app::update_viewport(emuenv);
}

static void toggle_texture_replacement(EmuEnvState &emuenv) {
    emuenv.cfg.current_config.import_textures = !emuenv.cfg.current_config.import_textures;
    emuenv.renderer->get_texture_cache()->set_replacement_state(emuenv.cfg.current_config.import_textures, emuenv.cfg.current_config.export_textures, emuenv.cfg.current_config.export_as_png);
}

static std::vector<uint32_t> get_current_app_frame(EmuEnvState &emuenv, uint32_t &width, uint32_t &height) {
    // Dump the current frame from the emulator display
    std::vector<uint32_t> frame = emuenv.renderer->dump_frame(emuenv.display, width, height);
    if (frame.empty() || (frame.size() != (width * height))) {
        return {};
    }

    // Force alpha channel to 255 (fully opaque) for every pixel
    for (uint32_t &pixel : frame) {
        pixel |= 0xFF000000;
    }

    return frame;
}

static void update_live_area_last_app_frame(EmuEnvState &emuenv, GuiState &gui) {
    uint32_t width, height;

    // Capture the current frame of the app
    auto frame = get_current_app_frame(emuenv, width, height);
    if (frame.empty()) {
        LOG_ERROR("Failed to dump current app frame for live area");
        return;
    }

    // Create and assign a new texture with the captured frame
    gui.live_area_last_app_frame = ImGui_Texture(gui.imgui_state.get(), frame.data(), width, height);
}

static void take_screenshot(EmuEnvState &emuenv) {
    if (emuenv.cfg.screenshot_format == None)
        return;

    if (emuenv.io.title_id.empty()) {
        LOG_ERROR("Trying to take a screenshot while not ingame");
        return;
    }

    uint32_t width, height;
    auto frame = get_current_app_frame(emuenv, width, height);
    if (frame.empty()) {
        LOG_ERROR("Failed to take screenshot");
        return;
    }

    const fs::path save_folder = emuenv.shared_path / "screenshots" / fmt::format("{}", string_utils::remove_special_chars(emuenv.current_app_title));
    fs::create_directories(save_folder);

    auto t = std::time(nullptr);
    struct tm localtime;
#ifdef _WIN32
    localtime_s(&localtime, &t);
#else
    localtime_r(&t, &localtime);
#endif

    const auto img_format = emuenv.cfg.screenshot_format == JPEG ? ".jpg" : ".png";
    const fs::path save_file = save_folder / fmt::format("{}_{:%Y-%m-%d-%H%M%OS}{}", string_utils::remove_special_chars(emuenv.current_app_title), localtime, img_format);
    constexpr int quality = 85; // google recommended value
    if (emuenv.cfg.screenshot_format == JPEG) {
        if (stbi_write_jpg(fs_utils::path_to_utf8(save_file).c_str(), width, height, 4, frame.data(), quality) == 1)
            LOG_INFO("Successfully saved screenshot to {}", save_file);
        else
            LOG_INFO("Failed to save screenshot");
    } else {
        if (stbi_write_png(fs_utils::path_to_utf8(save_file).c_str(), width, height, 4, frame.data(), width * 4) == 1)
            LOG_INFO("Successfully saved screenshot to {}", save_file);
        else
            LOG_INFO("Failed to save screenshot");
    }
}

bool handle_events(EmuEnvState &emuenv, GuiState &gui) {
    const auto allow_switch_state = !emuenv.io.title_id.empty() && !gui.vita_area.app_close && !gui.vita_area.home_screen && !gui.vita_area.user_management && !gui.configuration_menu.custom_settings_dialog && !gui.configuration_menu.settings_dialog && !gui.controls_menu.controls_dialog && gui::get_sys_apps_state(gui);

    const auto ui_navigation = [&emuenv, &gui, allow_switch_state](const uint32_t sce_ctrl_btn) {
        if (gui.vita_area.app_close) {
            const auto cancel = [&gui]() {
                gui.vita_area.app_close = false;
            };
            const auto confirm = [&gui, &emuenv]() {
                const auto app_path = gui.vita_area.live_area_screen ? gui.live_area_current_open_apps_list[gui.live_area_app_current_open] : emuenv.app_path;
                gui::close_and_run_new_app(emuenv, app_path);
            };
            switch (sce_ctrl_btn) {
            case SCE_CTRL_CIRCLE:
                if (emuenv.cfg.sys_button == 1)
                    cancel();
                else
                    confirm();
                break;
            case SCE_CTRL_CROSS:
                if (emuenv.cfg.sys_button == 1)
                    confirm();
                else
                    cancel();
                break;
            default: break;
            }
        } else if (gui.vita_area.user_management)
            gui::browse_users_management(gui, emuenv, sce_ctrl_btn);
        else if (gui.vita_area.manual)
            gui::browse_pages_manual(gui, emuenv, sce_ctrl_btn);
        else if (gui.vita_area.home_screen)
            gui::browse_home_apps_list(gui, emuenv, sce_ctrl_btn);
        else if (gui.vita_area.live_area_screen)
            gui::browse_live_area_apps_list(gui, emuenv, sce_ctrl_btn);
        else if (emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING) {
            switch (emuenv.common_dialog.type) {
            case SAVEDATA_DIALOG:
                gui::browse_save_data_dialog(gui, emuenv, sce_ctrl_btn);
                break;

            default: break;
            }
        }

        switch (sce_ctrl_btn) {
        case SCE_CTRL_CROSS:
        case SCE_CTRL_CIRCLE:
            gui.is_key_locked = true;
            if (gui.vita_area.start_screen) {
                gui.vita_area.start_screen = false;
                gui.vita_area.home_screen = true;
                if (emuenv.cfg.show_info_bar)
                    gui.vita_area.information_bar = true;
            }
            break;
        case SCE_CTRL_PSBUTTON:
            gui.is_key_locked = true;
            if (allow_switch_state) {
                // Show/Hide Live Area during app running
                const auto live_area_app_index = gui::get_live_area_current_open_apps_list_index(gui, emuenv.io.app_path);
                if (live_area_app_index == gui.live_area_current_open_apps_list.end())
                    gui::open_live_area(gui, emuenv, emuenv.io.app_path);
                else {
                    // If current live area app open is not the current app running, set it as current
                    if ((gui.live_area_app_current_open < 0) || (gui.live_area_current_open_apps_list[gui.live_area_app_current_open] != emuenv.io.app_path))
                        gui.live_area_app_current_open = static_cast<int32_t>(std::distance(live_area_app_index, gui.live_area_current_open_apps_list.end()) - 1);

                    // Switch Live Area state
                    gui.vita_area.information_bar = !gui.vita_area.information_bar;
                    gui.vita_area.live_area_screen = !gui.vita_area.live_area_screen;
                }

                // Update the last app frame for live area
                if (gui.vita_area.live_area_screen)
                    update_live_area_last_app_frame(emuenv, gui);

                app::switch_state(emuenv, !emuenv.kernel.is_threads_paused());
                gui::switch_bgm_state(!emuenv.kernel.is_threads_paused());

            } else if (!gui::get_sys_apps_state(gui))
                gui::close_system_app(gui, emuenv);
            break;
        default: break;
        }
    };

    // Check if any settings or controls dialog is open and drop inputs on this case
    emuenv.drop_inputs = gui.configuration_menu.settings_dialog || gui.configuration_menu.custom_settings_dialog || gui.controls_menu.controllers_dialog || gui.controls_menu.controls_dialog;

    // A set to store the last pressed buttons to prevent duplicate inputs from the controller.
    std::set<uint32_t> last_buttons;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSdl_ProcessEvent(gui.imgui_state.get(), &event);
        switch (event.type) {
        case SDL_EVENT_QUIT:
            gui::destroy_bgm_player();
            if (!emuenv.io.app_path.empty())
                gui::update_time_app_used(gui, emuenv, emuenv.io.app_path);
            emuenv.kernel.exit_delete_all_threads();
            emuenv.gxm.display_queue.abort();
            emuenv.display.abort = true;
            if (emuenv.display.vblank_thread) {
                emuenv.display.vblank_thread->join();
            }
            return false;

        case SDL_EVENT_KEY_DOWN: {
            const auto get_sce_ctrl_btn_from_scancode = [&emuenv](const SDL_Scancode scancode) {
                if (scancode == emuenv.cfg.keyboard_button_up)
                    return SCE_CTRL_UP;
                else if (scancode == emuenv.cfg.keyboard_button_right)
                    return SCE_CTRL_RIGHT;
                else if (scancode == emuenv.cfg.keyboard_button_down)
                    return SCE_CTRL_DOWN;
                else if (scancode == emuenv.cfg.keyboard_button_left)
                    return SCE_CTRL_LEFT;
                else if (scancode == emuenv.cfg.keyboard_button_l1)
                    return SCE_CTRL_L1;
                else if (scancode == emuenv.cfg.keyboard_button_r1)
                    return SCE_CTRL_R1;
                else if (scancode == emuenv.cfg.keyboard_button_triangle)
                    return SCE_CTRL_TRIANGLE;
                else if (scancode == emuenv.cfg.keyboard_button_circle)
                    return SCE_CTRL_CIRCLE;
                else if (scancode == emuenv.cfg.keyboard_button_cross)
                    return SCE_CTRL_CROSS;
                else if (scancode == emuenv.cfg.keyboard_button_psbutton)
                    return SCE_CTRL_PSBUTTON;
                else
                    return static_cast<SceCtrlButtons>(0);
            };

            // Get Sce Ctrl button from key
            auto sce_ctrl_btn = get_sce_ctrl_btn_from_scancode(event.key.scancode);

            if (gui.is_capturing_keys && event.key.scancode) {
                gui.is_key_capture_dropped = false;
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    LOG_ERROR("Key is reserved!");
                    gui.captured_key = gui.old_captured_key;
                    gui.is_key_capture_dropped = true;
                } else {
                    gui.captured_key = static_cast<int>(event.key.scancode);
                }
                gui.is_capturing_keys = false;
            }

            if (ImGui::GetIO().WantTextInput || gui.is_key_locked || emuenv.drop_inputs)
                continue;
#ifdef __ANDROID__
            if (event.key.scancode == SDL_SCANCODE_AC_BACK)
                sce_ctrl_btn = SCE_CTRL_PSBUTTON;
#else
            // toggle gui state
            if (allow_switch_state && (event.key.scancode == emuenv.cfg.keyboard_gui_toggle_gui))
                emuenv.display.imgui_render = !emuenv.display.imgui_render;
            if (event.key.scancode == emuenv.cfg.keyboard_gui_toggle_touch && !gui.is_key_capture_dropped)
                toggle_touchscreen();
            if (event.key.scancode == emuenv.cfg.keyboard_gui_fullscreen && !gui.is_key_capture_dropped)
                switch_full_screen(emuenv);
            if (event.key.scancode == emuenv.cfg.keyboard_toggle_texture_replacement && !gui.is_key_capture_dropped)
                toggle_texture_replacement(emuenv);
            if (event.key.scancode == emuenv.cfg.keyboard_take_screenshot && !gui.is_key_capture_dropped)
                take_screenshot(emuenv);
#endif

            if (sce_ctrl_btn != 0) {
                if (last_buttons.contains(sce_ctrl_btn)) {
                    continue;
                }
                last_buttons.insert(sce_ctrl_btn);
                ui_navigation(sce_ctrl_btn);
            }

            break;
        }
        case SDL_EVENT_KEY_UP:
            gui.is_key_locked = false;
            break;

        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_WHEEL:
            gui.is_nav_button = false;
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (!emuenv.kernel.is_threads_paused() && (event.gbutton.button == SDL_GAMEPAD_BUTTON_TOUCHPAD))
                toggle_touchscreen();

            if (ImGui::GetIO().WantTextInput || emuenv.drop_inputs)
                continue;

            for (const auto &binding : get_controller_bindings_ext(emuenv)) {
                if (event.gbutton.button == binding.controller) {
                    if (last_buttons.contains(binding.button)) {
                        continue;
                    }
                    last_buttons.insert(binding.button);
                    ui_navigation(binding.button);

                    break;
                }
            }
            break;

        case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
            handle_touchpad_event(event.gtouchpad);
            break;
        case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
            handle_motion_event(emuenv, event.gsensor.sensor, event.gsensor);
            break;
        case SDL_EVENT_SENSOR_UPDATE:
            handle_motion_event(emuenv, SDL_GetSensorTypeForID(event.sensor.which), event.sensor);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            refresh_controllers(emuenv.ctrl, emuenv);
            break;

        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            app::update_viewport(emuenv);
            break;

        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
            handle_touch_event(event.tfinger);
            break;
        case SDL_EVENT_DROP_FILE: {
            const auto drop_file = fs_utils::utf8_to_path(event.drop.data);
            const auto extension = string_utils::tolower(drop_file.extension().string());
            if (extension == ".pup") {
                const std::string fw_version = install_pup(emuenv.pref_path, drop_file);
                if (!fw_version.empty()) {
                    LOG_INFO("Firmware {} installed successfully!", fw_version);
                    gui::get_modules_list(gui, emuenv);
                    if (emuenv.cfg.initial_setup)
                        gui::init_theme(gui, emuenv, gui.users[emuenv.cfg.user_id].theme_id);
                }
            } else if ((extension == ".vpk") || (extension == ".zip"))
                install_archive(emuenv, &gui, drop_file);
            else if ((extension == ".rif") || (drop_file.filename() == "work.bin"))
                copy_license(emuenv, drop_file);
            else if (fs::is_directory(drop_file))
                install_contents(emuenv, &gui, drop_file);
            else if (drop_file.filename() == "theme.xml")
                install_content(emuenv, &gui, drop_file.parent_path());
            else
                LOG_ERROR("File dropped: [{}] is not supported.", drop_file.filename());
            break;
        }
        }
    }

    return true;
}

ExitCode load_app(int32_t &main_module_id, EmuEnvState &emuenv) {
    if (load_app_impl(main_module_id, emuenv) != Success) {
        std::string message = fmt::format(fmt::runtime(emuenv.common_dialog.lang.message["load_app_failed"]), emuenv.pref_path / "ux0/app" / emuenv.io.app_path / emuenv.self_path);
        app::error_dialog(message, emuenv.window.get());
        return ModuleLoadFailed;
    }

    if (!emuenv.cfg.show_gui)
        emuenv.display.imgui_render = false;

    if (emuenv.cfg.gdbstub) {
        emuenv.kernel.debugger.wait_for_debugger = true;
        server_open(emuenv);
    }

#if USE_DISCORD
    if (emuenv.cfg.discord_rich_presence)
        discordrpc::update_presence(emuenv.io.title_id, emuenv.current_app_title);
#endif

    return Success;
}

static std::vector<std::string> split(const std::string &input, const std::string &regex) {
    std::regex re(regex);
    std::sregex_token_iterator
        first{ input.begin(), input.end(), re, -1 },
        last;
    return { first, last };
}

ExitCode run_app(EmuEnvState &emuenv, int32_t main_module_id) {
    auto entry_point = emuenv.kernel.loaded_modules[main_module_id]->info.start_entry;
    auto process_param = emuenv.kernel.process_param.get(emuenv.mem);

    SceInt32 priority = SCE_KERNEL_DEFAULT_PRIORITY_USER;
    SceInt32 stack_size = SCE_KERNEL_STACK_SIZE_USER_MAIN;
    SceInt32 affinity = SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT;
    if (process_param) {
        auto priority_ptr = Ptr<int32_t>(process_param->main_thread_priority);
        if (priority_ptr) {
            priority = *priority_ptr.get(emuenv.mem);
        }

        auto stack_size_ptr = Ptr<int32_t>(process_param->main_thread_stacksize);
        if (stack_size_ptr) {
            stack_size = *stack_size_ptr.get(emuenv.mem);
        }

        auto affinity_ptr = Ptr<SceInt32>(process_param->main_thread_cpu_affinity_mask);
        if (affinity_ptr) {
            affinity = *affinity_ptr.get(emuenv.mem);
        }
    }
    const ThreadStatePtr main_thread = emuenv.kernel.create_thread(emuenv.mem, emuenv.io.title_id.c_str(), entry_point, priority, affinity, stack_size, nullptr);
    if (!main_thread) {
        app::error_dialog("Failed to init main thread.", emuenv.window.get());
        return InitThreadFailed;
    }
    emuenv.main_thread_id = main_thread->id;

    // Run `module_start` export (entry point) of loaded libraries
    for (auto &[_, module] : emuenv.kernel.loaded_modules) {
        if (module->info.modid != main_module_id)
            start_module(emuenv, module->info);
    }

    SceKernelThreadOptParam param{ 0, 0 };
    if (!emuenv.cfg.app_args.empty()) {
        auto args = split(emuenv.cfg.app_args, ",\\s+");
        // why is this flipped
        std::vector<uint8_t> buf;
        for (auto &arg : args)
            buf.insert(buf.end(), arg.c_str(), arg.c_str() + arg.size() + 1);
        auto arr = Ptr<uint8_t>(alloc(emuenv.mem, static_cast<uint32_t>(buf.size()), "arg"));
        memcpy(arr.get(emuenv.mem), buf.data(), buf.size());
        param.size = static_cast<SceSize>(buf.size());
        param.attr = arr.address();
    }
    if (main_thread->start(param.size, Ptr<void>(param.attr), true) < 0) {
        app::error_dialog("Failed to run main thread.", emuenv.window.get());
        return RunThreadFailed;
    }

    start_sync_thread(emuenv);

    if (emuenv.cfg.boot_apps_full_screen && !emuenv.display.fullscreen.load())
        switch_full_screen(emuenv);

    return Success;
}
