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

#include "interface.h"

#include <config/state.h>
#include <ctrl/functions.h>
#include <ctrl/state.h>
#include <display/functions.h>
#include <display/state.h>
#include <gui/functions.h>
#include <gxm/state.h>
#include <io/device.h>
#include <io/functions.h>
#include <io/vfs.h>
#include <kernel/load_self.h>
#include <kernel/state.h>
#include <packages/functions.h>
#include <packages/pkg.h>
#include <packages/sfo.h>

#include <modules/module_parent.h>
#include <string>
#include <touch/functions.h>
#include <touch/touch.h>
#include <util/find.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <gui/imgui_impl_sdl.h>

#include <regex>

#include <SDL.h>

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
    const auto app_license_path{ fs::path(emuenv.pref_path) / "ux0/license" / emuenv.app_info.app_title_id / fmt::format("{}.rif", emuenv.app_info.app_content_id) };
    const auto is_patch_found_app_license = (emuenv.app_info.app_category == "gp") && fs::exists(app_license_path);
    if (fs::exists(output_path / "sce_sys/package/work.bin") || is_patch_found_app_license) {
        std::string licpath = is_patch_found_app_license ? app_license_path.string() : output_path.string() + "/sce_sys/package/work.bin";
        LOG_INFO("Decrypt layer: {}", output_path.string());
        if (!decrypt_install_nonpdrm(emuenv, licpath, output_path.string())) {
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

bool install_archive_content(EmuEnvState &emuenv, GuiState *gui, const fs::path &archive_path, const ZipPtr &zip, const std::string &content_path, const std::function<void(ArchiveContents)> &progress_callback) {
    std::string sfo_path = "sce_sys/param.sfo";
    std::string theme_path = "theme.xml";
    vfs::FileBuffer buffer, theme;

    const auto is_theme = mz_zip_reader_extract_file_to_callback(zip.get(), (fs::path(content_path) / theme_path).string().c_str(), &write_to_buffer, &theme, 0);

    auto output_path{ fs::path(emuenv.pref_path) / "ux0" };
    if (mz_zip_reader_extract_file_to_callback(zip.get(), (fs::path(content_path) / sfo_path).string().c_str(), &write_to_buffer, &buffer, 0)) {
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
                ImGui_ImplSdl_NewFrame(gui->imgui_state.get());
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                gui::draw_ui(*gui, emuenv);
                ImGui::PushFont(gui->vita_font);
                gui::draw_reinstall_dialog(&status, *gui, emuenv);
                ImGui::PopFont();
                glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
                ImGui::Render();
                ImGui_ImplSdl_RenderDrawData(gui->imgui_state.get());
                SDL_GL_SwapWindow(emuenv.window.get());
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

    int num_files = mz_zip_reader_get_num_files(zip.get());
    for (auto i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat)) {
            continue;
        }
        const std::string m_filename = file_stat.m_filename;
        if (m_filename.find(content_path) != std::string::npos) {
            file_progress = static_cast<float>(i) / num_files * 100.0f;
            update_progress();

            std::string replace_filename = m_filename.substr(content_path.size());
            const fs::path file_output = { output_path / replace_filename };
            if (mz_zip_reader_is_file_a_directory(zip.get(), i)) {
                fs::create_directories(file_output);
            } else {
                if (!fs::exists(file_output.parent_path()))
                    fs::create_directories(file_output.parent_path());

                LOG_INFO("Extracting {}", file_output.generic_path().string());
                mz_zip_reader_extract_to_file(zip.get(), i, file_output.generic_path().string().c_str(), 0);
            }
        }
    }

    // Rename directory on correct name when is request, Todo of extract zip, no support unicode
    if (emuenv.app_info.app_category == "theme") {
        const auto dest = string_utils::utf_to_wide(output_path.string());
        if (output_path != dest) {
            if (fs::exists(dest))
                fs::remove_all(dest);

            fs::rename(output_path, dest);
        }
    }

    if (fs::exists(output_path / "sce_sys/package/")) {
        update_progress();
        if (is_nonpdrm(emuenv, output_path))
            decrypt_progress = 100.f;
        else
            return false;
    }
    if (!copy_path(output_path, emuenv.pref_path, emuenv.app_info.app_title_id, emuenv.app_info.app_category))
        return false;

    update_progress();

    LOG_INFO("{} [{}] installed succesfully!", emuenv.app_info.app_title, emuenv.app_info.app_title_id);

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
    int num_files = mz_zip_reader_get_num_files(zip.get());
    std::vector<std::string> content_path;
    std::string sfo_path = "sce_sys/param.sfo";
    std::string theme_path = "theme.xml";

    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat))
            continue;

        std::string m_filename = std::string(file_stat.m_filename);
        if (m_filename.find("sce_module/steroid.suprx") != std::string::npos) {
            LOG_CRITICAL("A Vitamin dump was detected, aborting installation...");
            content_path.clear();
            break;
        }

        const auto is_content = (m_filename.find(sfo_path) != std::string::npos) || (m_filename.find(theme_path) != std::string::npos);
        if (is_content) {
            const auto content_type = (m_filename.find(sfo_path) != std::string::npos) ? sfo_path : theme_path;
            m_filename.erase(m_filename.find(content_type));
            if (std::find(content_path.begin(), content_path.end(), m_filename) == content_path.end())
                content_path.push_back(m_filename);
        }
    }

    return content_path;
}

std::vector<ContentInfo> install_archive(EmuEnvState &emuenv, GuiState *gui, const fs::path &archive_path, const std::function<void(ArchiveContents)> &progress_callback) {
    if (!fs::exists(archive_path)) {
        LOG_CRITICAL("Failed to load archive file in path: {}", archive_path.generic_path().string());
        return {};
    }
    const ZipPtr zip(new mz_zip_archive, delete_zip);
    std::memset(zip.get(), 0, sizeof(*zip));

    FILE *vpk_fp;

#ifdef WIN32
    _wfopen_s(&vpk_fp, archive_path.generic_path().wstring().c_str(), L"rb");
#else
    vpk_fp = fopen(archive_path.generic_path().string().c_str(), "rb");
#endif

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

    const auto count = float(content_path.size());
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
        const bool state = install_archive_content(emuenv, gui, archive_path, zip, path, progress_callback);
        content_installed.push_back({ emuenv.app_info.app_title, emuenv.app_info.app_title_id, emuenv.app_info.app_category, emuenv.app_info.app_content_id, path, state });
    }

    fclose(vpk_fp);
    return content_installed;
}

static std::vector<fs::path> get_contents_path(const fs::path &path) {
    std::vector<fs::path> contents_path;

    for (const auto &p : fs::recursive_directory_iterator(path)) {
        const auto is_content = (p.path().filename() == "param.sfo") || (p.path().filename() == "theme.xml");
        if (is_content) {
            const auto content_path = (p.path().filename() == "param.sfo") ? p.path().parent_path().parent_path() : p.path().parent_path();
            if (std::find(content_path.begin(), content_path.end(), p.path().parent_path()) == content_path.end())
                contents_path.push_back(content_path);
        }
    }

    return contents_path;
}

static bool install_content(EmuEnvState &emuenv, GuiState *gui, const fs::path &content_path) {
    const auto sfo_path{ content_path / "sce_sys/param.sfo" };
    const auto theme_path{ content_path / "theme.xml" };
    vfs::FileBuffer buffer;

    const auto get_buffer = [&](const fs::path &path) {
        fs::ifstream f{ path, fs::ifstream::binary };
        if (!f)
            return false;

        f.unsetf(fs::ifstream::skipws);
        buffer.reserve(fs::file_size(path));
        buffer.insert(buffer.begin(), std::istream_iterator<uint8_t>(f), std::istream_iterator<uint8_t>());

        return true;
    };

    const auto is_theme = fs::exists(content_path / "theme.xml");
    auto dst_path{ fs::path(emuenv.pref_path) / "ux0" };
    if (get_buffer(sfo_path)) {
        sfo::get_param_info(emuenv.app_info, buffer, emuenv.cfg.sys_lang);
        if (!set_content_path(emuenv, is_theme, dst_path))
            return false;

        if (exists(dst_path))
            fs::remove_all(dst_path);

    } else if (get_buffer(theme_path)) {
        set_theme_name(emuenv, buffer);
        dst_path /= fs::path("theme") / string_utils::utf_to_wide(emuenv.app_info.app_title_id);
    } else {
        LOG_ERROR("Param.sfo file is missing in path", sfo_path.string());
        return false;
    }

    if (!copy_directories(content_path, dst_path)) {
        LOG_ERROR("Failed to copy directory to: {}", dst_path.string());
        return false;
    }

    if (fs::exists(dst_path / "sce_sys/package/") && !is_nonpdrm(emuenv, dst_path))
        return false;

    if (!copy_path(dst_path, emuenv.pref_path, emuenv.app_info.app_title_id, emuenv.app_info.app_category))
        return false;

    LOG_INFO("{} [{}] installed succesfully!", emuenv.app_info.app_title, emuenv.app_info.app_title_id);

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

    LOG_WARN_IF(src_path.empty(), "No found any content compatible on this path: {}", path.string());

    uint32_t installed = 0;
    for (const auto &src : src_path) {
        if (install_content(emuenv, gui, src))
            ++installed;
    }

    if (installed) {
        gui::save_apps_cache(*gui, emuenv);
        LOG_INFO("Succesfully installed {} content!", installed);
    }

    return installed;
}

static auto pre_load_module(EmuEnvState &emuenv, const std::vector<std::string> &lib_load_list, const VitaIoDevice &device) {
    for (const auto &module_path : lib_load_list) {
        vfs::FileBuffer module_buffer;
        Ptr<const void> lib_entry_point;
        bool res;
        const auto MODULE_PATH_ABS = fmt::format("{}:{}", device._to_string(), module_path);

        if (device == VitaIoDevice::app0)
            res = vfs::read_app_file(module_buffer, emuenv.pref_path, emuenv.io.app_path, module_path);
        else
            res = vfs::read_file(device, module_buffer, emuenv.pref_path, module_path);

        if (res) {
            SceUID module_id = load_self(lib_entry_point, emuenv.kernel, emuenv.mem, module_buffer.data(), MODULE_PATH_ABS);
            if (module_id >= 0) {
                const auto module = emuenv.kernel.loaded_modules[module_id];

                LOG_INFO("Pre-load module {} (at \"{}\") loaded", module->module_name, module_path);
            } else
                return FileNotFound;
        } else {
            LOG_DEBUG("Pre-load module at \"{}\" not present", module_path);
            return FileNotFound;
        }
    }

    return Success;
}

static ExitCode load_app_impl(Ptr<const void> &entry_point, EmuEnvState &emuenv, const std::wstring &path) {
    if (path.empty())
        return InvalidApplicationPath;

    const auto call_import = [&emuenv](CPUState &cpu, uint32_t nid, SceUID thread_id) {
        ::call_import(emuenv, cpu, nid, thread_id);
    };
    if (!emuenv.kernel.init(emuenv.mem, call_import, emuenv.kernel.cpu_backend, emuenv.kernel.cpu_opt)) {
        LOG_WARN("Failed to init kernel!");
        return KernelInitFailed;
    }

    if (emuenv.cfg.archive_log) {
        const fs::path log_directory{ emuenv.base_path + "/logs" };
        fs::create_directory(log_directory);
        const auto log_path{ log_directory / string_utils::utf_to_wide(emuenv.io.title_id + " - [" + string_utils::remove_special_chars(emuenv.current_app_title) + "].log") };
        if (logging::add_sink(log_path) != Success)
            return InitConfigFailed;
        logging::set_level(static_cast<spdlog::level::level_enum>(emuenv.cfg.log_level));
    }

    LOG_INFO("{}: {}", emuenv.cfg[e_cpu_backend], emuenv.cfg.current_config.cpu_backend);
    LOG_INFO_IF(emuenv.kernel.cpu_backend == CPUBackend::Dynarmic, "CPU Optimisation state: {}", emuenv.cfg.current_config.cpu_opt);
    LOG_INFO("ngs state: {}", emuenv.cfg.current_config.ngs_enable);
    LOG_INFO("Resolution multiplier: {}", emuenv.cfg.resolution_multiplier);
    if (emuenv.ctrl.controllers_num) {
        LOG_INFO("{} Controllers Connected", emuenv.ctrl.controllers_num);
        for (auto i = 0; i < emuenv.ctrl.controllers_num; i++)
            LOG_INFO("Controller {}: {}", i, emuenv.ctrl.controllers_name[i]);
    }
    LOG_INFO("modules mode: {}", config_modules_mode[emuenv.cfg.current_config.modules_mode][ModulesModeType::MODE]);
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

    for (const auto &var : get_var_exports()) {
        auto addr = var.factory(emuenv);
        emuenv.kernel.export_nids.emplace(var.nid, addr);
    }

    // FIXME: The application EBOOT should be the first module ever loaded in the address space!

    // Load pre-loaded libraries
    const auto module_app_path{ fs::path(emuenv.pref_path) / "ux0/app" / emuenv.io.app_path / "sce_module" };
    const auto is_app = fs::exists(module_app_path) && !fs::is_empty(module_app_path);
    if (is_app) {
        // Load application module
        const std::vector<std::string> lib_load_list = {
            "sce_module/libc.suprx",
            "sce_module/libfios2.suprx",
            "sce_module/libult.suprx",
        };

        pre_load_module(emuenv, lib_load_list, VitaIoDevice::app0);
    }

    // Load pre-loaded font fw libraries
    std::vector<std::string> lib_load_list = {
        "sys/external/libSceFt2.suprx",
        "sys/external/libpvf.suprx",
    };

    if (!is_app) {
        // Load pre-loaded fw libraries if app libraries not exist
        const std::vector<std::string> lib_load_list_to_add = {
            "sys/external/libc.suprx",
            "sys/external/libfios2.suprx",
            "sys/external/libult.suprx"
        };

        lib_load_list.insert(lib_load_list.begin(), lib_load_list_to_add.begin(), lib_load_list_to_add.end());
    }

    pre_load_module(emuenv, lib_load_list, VitaIoDevice::vs0);

    // Load main executable
    emuenv.self_path = !emuenv.cfg.self_path.empty() ? emuenv.cfg.self_path : EBOOT_PATH;
    vfs::FileBuffer eboot_buffer;
    if (vfs::read_app_file(eboot_buffer, emuenv.pref_path, emuenv.io.app_path, emuenv.self_path)) {
        SceUID module_id = load_self(entry_point, emuenv.kernel, emuenv.mem, eboot_buffer.data(), "app0:" + emuenv.self_path);
        if (module_id >= 0) {
            const auto module = emuenv.kernel.loaded_modules[module_id];

            LOG_INFO("Main executable {} ({}) loaded", module->module_name, emuenv.self_path);
        } else
            return FileNotFound;
    } else
        return FileNotFound;

    // Set self name from self path, can contain folder, get file name only
    emuenv.self_name = fs::path(emuenv.self_path).filename().string();

    return Success;
}

static void handle_window_event(EmuEnvState &state, const SDL_WindowEvent &event) {
    switch (static_cast<SDL_WindowEventID>(event.event)) {
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        app::update_viewport(state);
        break;
    default:
        break;
    }
}

static void switch_full_screen(EmuEnvState &emuenv) {
    emuenv.display.fullscreen = !emuenv.display.fullscreen;

    SDL_SetWindowFullscreen(emuenv.window.get(), emuenv.display.fullscreen.load() ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

    // Refresh Viewport Size
    app::update_viewport(emuenv);
}

bool handle_events(EmuEnvState &emuenv, GuiState &gui) {
    refresh_controllers(emuenv.ctrl);
    const auto allow_switch_state = !emuenv.io.title_id.empty() && !gui.vita_area.app_close && !gui.vita_area.user_management && !gui.configuration_menu.custom_settings_dialog && !gui.configuration_menu.settings_dialog && !gui.controls_menu.controls_dialog && gui::get_sys_apps_state(gui);
    const auto switch_live_area_state = [](EmuEnvState &emuenv, GuiState &gui) {
        // Show/Hide Live Area during app running
        if (!gui.is_key_locked && !gui.vita_area.home_screen) {
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

            app::switch_state(emuenv, !emuenv.kernel.is_threads_paused());
        }
    };
    const auto close_and_run_new_app = [&gui, &emuenv](const uint32_t button) {
        if (button == SCE_CTRL_CROSS) {
            const auto app_path = gui.vita_area.live_area_screen ? gui.live_area_current_open_apps_list[gui.live_area_app_current_open] : emuenv.app_path;
            gui::close_and_run_new_app(gui, emuenv, app_path);
        } else if (button == SCE_CTRL_CIRCLE)
            gui.vita_area.app_close = false;
    };

    const auto browse_common_dialog = [&gui, &emuenv](const uint32_t button) {
        switch (emuenv.common_dialog.type) {
        case SAVEDATA_DIALOG:
            gui::browse_save_data_dialog(gui, emuenv, button);
            break;
        default:
            break;
        }
    };

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSdl_ProcessEvent(gui.imgui_state.get(), &event);
        switch (event.type) {
        case SDL_QUIT:
            if (!emuenv.io.app_path.empty())
                gui::update_time_app_used(gui, emuenv, emuenv.io.app_path);
            emuenv.kernel.exit_delete_all_threads();
            emuenv.gxm.display_queue.abort();
            emuenv.display.abort = true;
            if (emuenv.display.vblank_thread) {
                emuenv.display.vblank_thread->join();
            }
            return false;

        case SDL_KEYDOWN: {
            if (ImGui::GetIO().WantTextInput || gui.is_key_locked)
                continue;

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
            const auto sce_ctrl_btn = get_sce_ctrl_btn_from_scancode(event.key.keysym.scancode);

            if (gui.is_capturing_keys && event.key.keysym.scancode) {
                gui.is_key_capture_dropped = false;
                if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    LOG_ERROR("Key is reserved!");
                    gui.captured_key = gui.old_captured_key;
                    gui.is_key_capture_dropped = true;
                } else {
                    gui.captured_key = static_cast<int>(event.key.keysym.scancode);
                }
                gui.is_capturing_keys = false;
            }
            if (allow_switch_state) {
                // toggle gui state
                if (event.key.keysym.scancode == emuenv.cfg.keyboard_gui_toggle_gui)
                    emuenv.display.imgui_render = !emuenv.display.imgui_render;
                else if (sce_ctrl_btn & SCE_CTRL_PSBUTTON)
                    switch_live_area_state(emuenv, gui);
            } else if (!gui::get_sys_apps_state(gui)) {
                if (sce_ctrl_btn & SCE_CTRL_PSBUTTON)
                    gui::close_system_app(gui, emuenv);
            }
            if (event.key.keysym.scancode == emuenv.cfg.keyboard_gui_toggle_touch && !gui.is_key_capture_dropped)
                toggle_touchscreen();
            if (event.key.keysym.scancode == emuenv.cfg.keyboard_gui_fullscreen && !gui.is_key_capture_dropped)
                switch_full_screen(emuenv);

            if (sce_ctrl_btn != 0) {
                if (gui.vita_area.user_management)
                    gui::browse_users_management(gui, emuenv, sce_ctrl_btn);
                else if (gui.vita_area.app_close)
                    close_and_run_new_app(sce_ctrl_btn);
                else if (gui.vita_area.manual)
                    gui::browse_pages_manual(gui, emuenv, sce_ctrl_btn);
                else if (gui.vita_area.home_screen)
                    gui::browse_home_apps_list(gui, emuenv, sce_ctrl_btn);
                else if (gui.vita_area.live_area_screen)
                    gui::browse_live_area_apps_list(gui, emuenv, sce_ctrl_btn);
                else if (emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING)
                    browse_common_dialog(sce_ctrl_btn);

                switch (sce_ctrl_btn) {
                case SCE_CTRL_CROSS:
                case SCE_CTRL_CIRCLE:
                case SCE_CTRL_PSBUTTON:
                    gui.is_key_locked = true;
                    break;
                default:
                    break;
                }
            }

            break;
        }
        case SDL_KEYUP:
            gui.is_key_locked = false;
            break;

        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEWHEEL:
            gui.is_nav_button = false;
            break;

        case SDL_CONTROLLERBUTTONDOWN:
            if (ImGui::GetIO().WantTextInput)
                continue;

            for (const auto &binding : controller_bindings_ext) {
                if (event.cbutton.button == binding.controller) {
                    if (gui.vita_area.user_management)
                        gui::browse_users_management(gui, emuenv, binding.button);
                    else if (gui.vita_area.app_close)
                        close_and_run_new_app(binding.button);
                    else if (gui.vita_area.manual)
                        gui::browse_pages_manual(gui, emuenv, binding.button);
                    else if (gui.vita_area.home_screen)
                        gui::browse_home_apps_list(gui, emuenv, binding.button);
                    else if (gui.vita_area.live_area_screen)
                        gui::browse_live_area_apps_list(gui, emuenv, binding.button);
                    else if (emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING) {
                        browse_common_dialog(binding.button);
                    }
                    break;
                }
            }

            switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_A:
            case SDL_CONTROLLER_BUTTON_B:
                if (gui.vita_area.start_screen) {
                    gui.vita_area.start_screen = false;
                    gui.vita_area.home_screen = true;
                    if (emuenv.cfg.show_info_bar)
                        gui.vita_area.information_bar = true;
                }

                break;

            case SDL_CONTROLLER_BUTTON_GUIDE:
                if (allow_switch_state)
                    switch_live_area_state(emuenv, gui);
                else if (!gui::get_sys_apps_state(gui))
                    gui::close_system_app(gui, emuenv);
                break;

            default: break;
            }
            break;

        case SDL_WINDOWEVENT:
            handle_window_event(emuenv, event.window);
            break;

        case SDL_FINGERDOWN:
            handle_touch_event(event.tfinger);
            break;

        case SDL_FINGERMOTION:
            handle_touch_event(event.tfinger);
            break;

        case SDL_FINGERUP:
            handle_touch_event(event.tfinger);
            break;
        case SDL_DROPFILE: {
            const auto drop_file = fs::path(string_utils::utf_to_wide(event.drop.file));
            const auto extension = string_utils::tolower(drop_file.extension().string());
            if ((extension == ".vpk") || (extension == ".zip"))
                install_archive(emuenv, &gui, drop_file);
            else if ((extension == ".rif") || (drop_file.filename() == "work.bin"))
                copy_license(emuenv, drop_file);
            else if (fs::is_directory(drop_file))
                install_contents(emuenv, &gui, drop_file);
            else if (drop_file.filename() == "theme.xml")
                install_content(emuenv, &gui, drop_file.parent_path());
            else
                LOG_ERROR("File droped: [{}] is not supported.", drop_file.filename().string());
            SDL_free(event.drop.file);
            break;
        }
        }
    }

    return true;
}

ExitCode load_app(Ptr<const void> &entry_point, EmuEnvState &emuenv, const std::wstring &path) {
    if (load_app_impl(entry_point, emuenv, path) != Success) {
        std::string message = "Failed to load \"";
        message += string_utils::wide_to_utf(path);
        message += "\"";
        message += "\nSee console output for details.";
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

ExitCode run_app(EmuEnvState &emuenv, Ptr<const void> &entry_point) {
    const ThreadStatePtr thread = emuenv.kernel.create_thread(emuenv.mem, emuenv.io.title_id.c_str(), entry_point, SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT, static_cast<int>(SCE_KERNEL_STACK_SIZE_USER_MAIN), nullptr);
    if (!thread) {
        app::error_dialog("Failed to init main thread.", emuenv.window.get());
        return InitThreadFailed;
    }
    const SceUID main_thread_id = thread->id;

    const ThreadStatePtr main_thread = util::find(main_thread_id, emuenv.kernel.threads);

    // Run `module_start` export (entry point) of loaded libraries
    for (auto &mod : emuenv.kernel.loaded_modules) {
        const auto module = mod.second;
        const auto module_start = module->start_entry;
        const auto module_name = module->module_name;

        if (!module_start || (std::string(module->path) == "app0:" + emuenv.self_path))
            continue;

        LOG_DEBUG("Running module_start of library: {} at address {}", module_name, log_hex(module_start.address()));

        // TODO: why does fios need separate thread its stack freed anyways?
        const ThreadStatePtr module_thread = emuenv.kernel.create_thread(emuenv.mem, module_name);
        const auto ret = module_thread->run_guest_function(emuenv.kernel, module_start.address());
        module_thread->exit_delete();

        LOG_INFO("Module {} (at \"{}\") module_start returned {}", module_name, module->path, log_hex(ret));
    }

    emuenv.main_thread_id = main_thread_id;

    SceKernelThreadOptParam param{ 0, 0 };
    if (!emuenv.cfg.app_args.empty()) {
        auto args = split(emuenv.cfg.app_args, ",\\s+");
        // why is this flipped
        std::vector<uint8_t> buf;
        for (auto &arg : args)
            buf.insert(buf.end(), arg.c_str(), arg.c_str() + arg.size() + 1);
        auto arr = Ptr<uint8_t>(alloc(emuenv.mem, static_cast<uint32_t>(buf.size()), "arg"));
        memcpy(arr.get(emuenv.mem), buf.data(), buf.size());
        param.size = SceSize(buf.size());
        param.attr = arr.address();
    }
    if (main_thread->start(emuenv.kernel, param.size, Ptr<void>(param.attr)) < 0) {
        app::error_dialog("Failed to run main thread.", emuenv.window.get());
        return RunThreadFailed;
    }

    start_sync_thread(emuenv);

    if (emuenv.cfg.boot_apps_full_screen && !emuenv.display.fullscreen.load())
        switch_full_screen(emuenv);

    return Success;
}
