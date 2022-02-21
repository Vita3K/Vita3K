// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <ctrl/functions.h>
#include <gui/functions.h>
#include <host/functions.h>
#include <host/pkg.h>
#include <host/sfo.h>
#include <io/device.h>
#include <io/functions.h>
#include <io/vfs.h>
#include <kernel/load_self.h>

#include <modules/module_parent.h>
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

static void set_theme_name(HostState &host, vfs::FileBuffer &buf) {
    host.app_title = gui::get_theme_title_from_buffer(buf);
    host.app_title_id = string_utils::remove_special_chars(host.app_title);
    const auto nospace = std::remove_if(host.app_title_id.begin(), host.app_title_id.end(), isspace);
    host.app_title_id.erase(nospace, host.app_title_id.end());
    host.app_category = "theme";
    host.app_title += " (Theme)";
}

static bool is_nonpdrm(HostState &host, const fs::path &output_path) {
    const auto app_license_path{ fs::path(host.pref_path) / "ux0/license" / host.app_title_id / fmt::format("{}.rif", host.app_content_id) };
    const auto is_patch_found_app_license = (host.app_category == "gp") && fs::exists(app_license_path);
    if (fs::exists(output_path / "sce_sys/package/work.bin") || is_patch_found_app_license) {
        std::string licpath = is_patch_found_app_license ? app_license_path.string() : output_path.string() + "/sce_sys/package/work.bin";
        LOG_INFO("Decrypt layer: {}", output_path.string());
        if (!decrypt_install_nonpdrm(host, licpath, output_path.string())) {
            LOG_ERROR("NoNpDrm installation failed, deleting data!");
            fs::remove_all(output_path);
            return false;
        }
        return true;
    }

    return false;
}

static bool set_content_path(HostState &host, const bool is_theme, fs::path &dest_path) {
    const auto app_path = dest_path / "app" / host.app_title_id;

    if (host.app_category == "ac") {
        if (is_theme) {
            dest_path /= fs::path("theme") / host.app_content_id;
            host.app_title += " (Theme)";
        } else {
            host.app_content_id = host.app_content_id.substr(20);
            dest_path /= fs::path("addcont") / host.app_title_id / host.app_content_id;
            host.app_title += " (DLC)";
        }
    } else if (host.app_category.find("gp") != std::string::npos) {
        if (!fs::exists(app_path) || fs::is_empty(app_path)) {
            LOG_ERROR("Install app before patch");
            return false;
        }
        dest_path /= fs::path("patch") / host.app_title_id;
        host.app_title += " (Patch)";
    } else {
        dest_path = app_path;
        host.app_title += " (App)";
    }

    return true;
}

bool install_archive_content(HostState &host, GuiState *gui, const fs::path &archive_path, const ZipPtr &zip, const std::string &content_path, const std::function<void(ArchiveContents)> &progress_callback) {
    std::string sfo_path = "sce_sys/param.sfo";
    std::string theme_path = "theme.xml";
    vfs::FileBuffer buffer, theme;

    const auto is_theme = mz_zip_reader_extract_file_to_callback(zip.get(), (fs::path(content_path) / theme_path).string().c_str(), &write_to_buffer, &theme, 0);

    auto output_path{ fs::path(host.pref_path) / "ux0" };
    if (mz_zip_reader_extract_file_to_callback(zip.get(), (fs::path(content_path) / sfo_path).string().c_str(), &write_to_buffer, &buffer, 0)) {
        sfo::get_param_info(host, buffer);
        if (!set_content_path(host, is_theme, output_path))
            return false;
    } else if (is_theme) {
        set_theme_name(host, theme);
        output_path /= fs::path("theme") / host.app_title_id;
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

            while (handle_events(host, *gui) && (status == gui::UNK_STATE)) {
                ImGui_ImplSdl_NewFrame(gui->imgui_state.get());
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                gui::draw_ui(*gui, host);
                ImGui::PushFont(gui->vita_font);
                gui::draw_reinstall_dialog(&status, host);
                ImGui::PopFont();
                glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
                ImGui::Render();
                ImGui_ImplSdl_RenderDrawData(gui->imgui_state.get());
                SDL_GL_SwapWindow(host.window.get());
            }
            switch (status) {
            case gui::CANCEL_STATE:
                LOG_INFO("{} already installed, {}", host.app_title_id, host.app_category.find("gd") != std::string::npos ? "launching application..." : "Open home");
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
    if (host.app_category == "theme") {
        const auto dest = string_utils::utf_to_wide(output_path.string());
        if (output_path != dest) {
            if (fs::exists(dest))
                fs::remove_all(dest);

            fs::rename(output_path, dest);
        }
    }

    if (fs::exists(output_path / "sce_sys/package/")) {
        update_progress();
        if (is_nonpdrm(host, output_path))
            decrypt_progress = 100.f;
        else
            return false;
    }

    if (!copy_path(host, output_path))
        return false;

    update_progress();

    LOG_INFO("{} [{}] installed succesfully!", host.app_title, host.app_title_id);

    if (!gui->file_menu.archive_install_dialog && (host.app_category != "theme")) {
        gui::update_notice_info(*gui, host, "content");
        if ((host.app_category.find("gd") != std::string::npos) || (host.app_category.find("gp") != std::string::npos)) {
            gui::init_user_app(*gui, host, host.app_title_id);
            gui::save_apps_cache(*gui, host);
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

std::vector<ContentInfo> install_archive(HostState &host, GuiState *gui, const fs::path &archive_path, const std::function<void(ArchiveContents)> &progress_callback) {
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
        const bool state = install_archive_content(host, gui, archive_path, zip, path, progress_callback);
        content_installed.push_back({ host.app_title, host.app_title_id, host.app_category, host.app_content_id, path, state });
    }

    fclose(vpk_fp);
    return content_installed;
}

static bool copy_directories(const fs::path &src_path, const fs::path &dst_path) {
    try {
        if (!fs::exists(dst_path))
            fs::create_directories(dst_path);

        for (const auto &src : fs::recursive_directory_iterator(src_path)) {
            const auto dst_parent_path = dst_path / fs::relative(src, src_path).parent_path();
            const auto dst_path = dst_parent_path / src.path().filename();

            LOG_INFO("Copy {}", dst_path.string());

            if (fs::is_regular_file(src))
                fs::copy_file(src, dst_path, fs::copy_option::overwrite_if_exists);
            else if (!fs::exists(dst_path))
                fs::create_directory(dst_path);
        }

        return true;
    } catch (std::exception &e) {
        std::cout << e.what();
        return false;
    }
}

bool copy_path(HostState &host, const fs::path src_path) {
    // Check if is path
    if (host.app_category.find("gp") != std::string::npos) {
        const auto app_path{ fs::path(host.pref_path) / "ux0/app" / host.app_title_id };
        const auto result = copy_directories(src_path, app_path);

        fs::remove_all(src_path);

        return result;
    }

    return true;
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

static bool install_content(HostState &host, GuiState *gui, const fs::path &content_path) {
    const auto sfo_path{ content_path / "sce_sys/param.sfo" };
    const auto theme_path{ content_path / "theme.xml" };
    vfs::FileBuffer buffer;

    const auto get_buffer = [&](const fs::path path) {
        fs::ifstream f{ path, fs::ifstream::binary };
        if (!f)
            return false;

        f.unsetf(fs::ifstream::skipws);
        buffer.reserve(fs::file_size(path));
        buffer.insert(buffer.begin(), std::istream_iterator<uint8_t>(f), std::istream_iterator<uint8_t>());

        return true;
    };

    const auto is_theme = fs::exists(content_path / "theme.xml");
    auto dst_path{ fs::path(host.pref_path) / "ux0" };
    if (get_buffer(sfo_path)) {
        sfo::get_param_info(host, buffer);
        if (!set_content_path(host, is_theme, dst_path))
            return false;

        if (exists(dst_path))
            fs::remove_all(dst_path);

    } else if (get_buffer(theme_path)) {
        set_theme_name(host, buffer);
        dst_path /= fs::path("theme") / string_utils::utf_to_wide(host.app_title_id);
    } else {
        LOG_ERROR("Param.sfo file is missing in path", sfo_path.string());
        return false;
    }

    if (!copy_directories(content_path, dst_path)) {
        LOG_ERROR("Failed to copy directory to: {}", dst_path.string());
        return false;
    }

    if (fs::exists(dst_path / "sce_sys/package/") && !is_nonpdrm(host, dst_path))
        return false;

    if (!copy_path(host, dst_path))
        return false;

    LOG_INFO("{} [{}] installed succesfully!", host.app_title, host.app_title_id);

    if ((host.app_category.find("gd") != std::string::npos) || (host.app_category.find("gp") != std::string::npos)) {
        gui::init_user_app(*gui, host, host.app_title_id);
        gui::save_apps_cache(*gui, host);
    }

    if (host.app_category != "theme")
        gui::update_notice_info(*gui, host, "content");

    return true;
}

uint32_t install_contents(HostState &host, GuiState *gui, const fs::path &path) {
    const auto src_path = get_contents_path(path);

    LOG_WARN_IF(src_path.empty(), "No found any content compatible on this path: {}", path.string());

    uint32_t installed = 0;
    for (const auto &src : src_path) {
        if (install_content(host, gui, src))
            ++installed;
    }

    if (installed) {
        gui::save_apps_cache(*gui, host);
        LOG_INFO("Succesfully installed {} content!", installed);
    }

    return installed;
}

static auto pre_load_module(HostState &host, const std::vector<std::string> &lib_load_list, const VitaIoDevice &device) {
    for (const auto &module_path : lib_load_list) {
        vfs::FileBuffer module_buffer;
        Ptr<const void> lib_entry_point;
        bool res;
        const auto MODULE_PATH_ABS = fmt::format("{}:{}", device._to_string(), module_path);

        if (device == VitaIoDevice::app0)
            res = vfs::read_app_file(module_buffer, host.pref_path, host.io.app_path, module_path);
        else
            res = vfs::read_file(device, module_buffer, host.pref_path, module_path);

        if (res) {
            SceUID module_id = load_self(lib_entry_point, host.kernel, host.mem, module_buffer.data(), MODULE_PATH_ABS);
            if (module_id >= 0) {
                const auto module = host.kernel.loaded_modules[module_id];

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

static ExitCode load_app_impl(Ptr<const void> &entry_point, HostState &host, const std::wstring &path) {
    if (path.empty())
        return InvalidApplicationPath;

    const auto call_import = [&host](CPUState &cpu, uint32_t nid, SceUID thread_id) {
        ::call_import(host, cpu, nid, thread_id);
    };
    if (!host.kernel.init(host.mem, call_import, host.kernel.cpu_backend, host.kernel.cpu_opt)) {
        LOG_WARN("Failed to init kernel!");
        return KernelInitFailed;
    }

    if (host.cfg.archive_log) {
        const fs::path log_directory{ host.base_path + "/logs" };
        fs::create_directory(log_directory);
        const auto log_path{ log_directory / string_utils::utf_to_wide(host.io.title_id + " - [" + string_utils::remove_special_chars(host.current_app_title) + "].log") };
        if (logging::add_sink(log_path) != Success)
            return InitConfigFailed;
        logging::set_level(static_cast<spdlog::level::level_enum>(host.cfg.log_level));
    }

    LOG_INFO("{}: {}", host.cfg[e_cpu_backend], host.cfg.current_config.cpu_backend);
    LOG_INFO_IF(host.kernel.cpu_backend == CPUBackend::Dynarmic, "CPU Optimisation state: {}", host.cfg.current_config.cpu_opt);
    LOG_INFO("at9 audio decoder state: {}", !host.cfg.current_config.disable_at9_decoder);
    LOG_INFO("ngs experimental state: {}", !host.cfg.current_config.disable_ngs);
    LOG_INFO("video player state: {}", host.cfg.current_config.video_playing);
    refresh_controllers(host.ctrl);
    if (host.ctrl.controllers_num) {
        LOG_INFO("{} Controllers Connected", host.ctrl.controllers_num);
        for (auto i = 0; i < host.ctrl.controllers_num; i++)
            LOG_INFO("Controller {}: {}", i, host.ctrl.controllers_name[i]);
    }
    LOG_INFO("modules mode: {}", config_modules_mode[host.cfg.current_config.modules_mode][ModulesModeType::MODE]);
    if ((host.cfg.current_config.modules_mode != ModulesMode::AUTOMATIC) && !host.cfg.current_config.lle_modules.empty()) {
        std::string modules;
        for (const auto &mod : host.cfg.current_config.lle_modules) {
            modules += mod + ",";
        }
        modules.pop_back();
        LOG_INFO("lle-modules: {}", modules);
    }

    LOG_INFO("Title: {}", host.current_app_title);
    LOG_INFO("Serial: {}", host.io.title_id);
    LOG_INFO("Version: {}", host.app_version);
    LOG_INFO("Category: {}", host.app_category);

    init_device_paths(host.io);
    init_savedata_app_path(host.io, host.pref_path);

    for (const auto &var : get_var_exports()) {
        auto addr = var.factory(host);
        host.kernel.export_nids.emplace(var.nid, addr);
    }

    if (host.cfg.current_config.lle_driver_user) {
        // Load Driver User pre-loaded module
        const std::vector<std::string> lib_load_list = {
            "us/driver_us.suprx",
        };

        pre_load_module(host, lib_load_list, VitaIoDevice::os0);
    }

    // Load pre-loaded libraries
    const auto module_app_path{ fs::path(host.pref_path) / "ux0/app" / host.io.app_path / "sce_module" };
    const auto is_app = fs::exists(module_app_path) && !fs::is_empty(module_app_path);
    if (is_app) {
        // Load application module
        const std::vector<std::string> lib_load_list = {
            "sce_module/libc.suprx",
            "sce_module/libfios2.suprx",
            "sce_module/libult.suprx",
        };

        pre_load_module(host, lib_load_list, VitaIoDevice::app0);
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

    pre_load_module(host, lib_load_list, VitaIoDevice::vs0);

    // Load main executable
    host.self_path = !host.cfg.self_path.empty() ? host.cfg.self_path : EBOOT_PATH;
    vfs::FileBuffer eboot_buffer;
    if (vfs::read_app_file(eboot_buffer, host.pref_path, host.io.app_path, host.self_path)) {
        SceUID module_id = load_self(entry_point, host.kernel, host.mem, eboot_buffer.data(), "app0:" + host.self_path);
        if (module_id >= 0) {
            const auto module = host.kernel.loaded_modules[module_id];

            LOG_INFO("Main executable {} ({}) loaded", module->module_name, host.self_path);
        } else
            return FileNotFound;
    } else
        return FileNotFound;

    return Success;
}

static void handle_window_event(HostState &state, const SDL_WindowEvent event) {
    switch (static_cast<SDL_WindowEventID>(event.event)) {
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        app::update_viewport(state);
        break;
    default:
        break;
    }
}

bool handle_events(HostState &host, GuiState &gui) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSdl_ProcessEvent(gui.imgui_state.get(), &event);
        switch (event.type) {
        case SDL_QUIT:
            if (!host.io.app_path.empty())
                gui::update_time_app_used(gui, host, host.io.app_path);
            host.kernel.exit_delete_all_threads();
            host.gxm.display_queue.abort();
            host.display.abort.exchange(true);
            host.display.condvar.notify_all();
            return false;

        case SDL_KEYDOWN:
            if (gui.is_capturing_keys && event.key.keysym.scancode) {
                if ((event.key.keysym.scancode == SDL_SCANCODE_G) || (event.key.keysym.scancode == SDL_SCANCODE_F11) || (event.key.keysym.scancode == SDL_SCANCODE_T) || (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)) {
                    LOG_ERROR("Key is reserved!");
                    gui.captured_key = gui.old_captured_key;
                    gui.is_capturing_keys = false;
                } else {
                    gui.captured_key = static_cast<int>(event.key.keysym.scancode);
                    gui.is_capturing_keys = false;
                }
            }
            if (!host.io.title_id.empty() && !gui.live_area.user_management && !gui.configuration_menu.custom_settings_dialog && !gui.configuration_menu.settings_dialog && !gui.controls_menu.controls_dialog) {
                // toggle gui state
                if (event.key.keysym.sym == SDLK_g)
                    host.display.imgui_render = !host.display.imgui_render;
                // Show/Hide Live Area during app run, TODO pause app running
                else if (gui::get_sys_apps_state(gui) && (event.key.keysym.scancode == host.cfg.keyboard_button_psbutton)) {
                    if (gui::get_app_open_list_index(gui, host.io.app_path) == gui.apps_list_opened.end())
                        gui::open_live_area(gui, host, host.io.app_path);
                    else {
                        gui.live_area.information_bar = !gui.live_area.information_bar;
                        gui.live_area.live_area_screen = !gui.live_area.live_area_screen;
                    }
                }
            }
            if (event.key.keysym.sym == SDLK_t)
                toggle_touchscreen();
            if (event.key.keysym.sym == SDLK_F11) {
                host.display.fullscreen = !host.display.fullscreen;

                if (host.display.fullscreen.load())
                    SDL_MaximizeWindow(host.window.get());

                SDL_SetWindowFullscreen(host.window.get(), host.display.fullscreen.load() ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            }
            break;

        case SDL_WINDOWEVENT:
            handle_window_event(host, event.window);
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
                install_archive(host, &gui, drop_file);
            else if ((extension == ".rif") || (drop_file.filename() == "work.bin"))
                copy_license(host, drop_file);
            else if (fs::is_directory(drop_file))
                install_contents(host, &gui, drop_file);
            else if (drop_file.filename() == "theme.xml")
                install_content(host, &gui, drop_file.parent_path());
            else
                LOG_ERROR("File droped: [{}] is not supported.", drop_file.filename().string());
            SDL_free(event.drop.file);
            break;
        }
        }
    }

    return true;
}

ExitCode load_app(Ptr<const void> &entry_point, HostState &host, const std::wstring &path) {
    if (load_app_impl(entry_point, host, path) != Success) {
        std::string message = "Failed to load \"";
        message += string_utils::wide_to_utf(path);
        message += "\"";
        message += "\nSee console output for details.";
        app::error_dialog(message, host.window.get());
        return ModuleLoadFailed;
    }

    if (!host.cfg.show_gui)
        host.display.imgui_render = false;

    if (host.cfg.gdbstub) {
        host.kernel.debugger.wait_for_debugger = true;
        server_open(host);
    }

#if USE_DISCORD
    if (host.cfg.discord_rich_presence)
        discordrpc::update_presence(host.io.title_id, host.current_app_title);
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

ExitCode run_app(HostState &host, Ptr<const void> &entry_point) {
    const CallImport call_import = [&host](CPUState &cpu, uint32_t nid, SceUID main_thread_id) {
        ::call_import(host, cpu, nid, main_thread_id);
    };

    const ResolveNIDName resolve_nid_name = [&host](Address addr) {
        return ::resolve_nid_name(host.kernel, addr);
    };

    const ThreadStatePtr thread = host.kernel.create_thread(host.mem, host.io.title_id.c_str(), entry_point, SCE_KERNEL_DEFAULT_PRIORITY_USER, static_cast<int>(SCE_KERNEL_STACK_SIZE_USER_MAIN), nullptr);
    if (!thread) {
        app::error_dialog("Failed to init main thread.", host.window.get());
        return InitThreadFailed;
    }
    const SceUID main_thread_id = thread->id;

    const ThreadStatePtr main_thread = util::find(main_thread_id, host.kernel.threads);

    // Run `module_start` export (entry point) of loaded libraries
    for (auto &mod : host.kernel.loaded_modules) {
        const auto module = mod.second;
        const auto module_start = module->start_entry;
        const auto module_name = module->module_name;

        if (!module_start || (std::string(module->path) == "app0:" + host.self_path))
            continue;

        LOG_DEBUG("Running module_start of library: {} at address {}", module_name, log_hex(module_start.address()));

        // TODO: why does fios need separate thread its stack freed anyways?
        const ThreadStatePtr module_thread = host.kernel.create_thread(host.mem, module_name);
        const auto ret = module_thread->run_guest_function(module_start.address(), { 0, 0 });
        host.kernel.exit_delete_thread(module_thread);

        LOG_INFO("Module {} (at \"{}\") module_start returned {}", module_name, module->path, log_hex(ret));
    }

    host.main_thread_id = main_thread_id;

    SceKernelThreadOptParam param{ 0, 0 };
    if (!host.cfg.app_args.empty()) {
        auto args = split(host.cfg.app_args, ",\\s+");
        // why is this flipped
        std::vector<uint8_t> buf;
        for (auto &arg : args)
            buf.insert(buf.end(), arg.c_str(), arg.c_str() + arg.size() + 1);
        auto arr = Ptr<uint8_t>(alloc(host.mem, buf.size(), "arg"));
        memcpy(arr.get(host.mem), buf.data(), buf.size());
        auto abc = arr.get(host.mem);
        param.size = SceSize(buf.size());
        param.attr = arr.address();
    }
    if (main_thread->start(host.kernel, param.size, Ptr<void>(param.attr)) < 0) {
        app::error_dialog("Failed to run main thread.", host.window.get());
        return RunThreadFailed;
    }

    return Success;
}
