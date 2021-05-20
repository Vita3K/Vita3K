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

#include "interface.h"

#include <gui/functions.h>
#include <host/functions.h>
#include <host/load_self.h>
#include <host/pkg.h>
#include <host/sfo.h>
#include <io/device.h>
#include <io/functions.h>
#include <io/vfs.h>
#include <kernel/thread/thread_functions.h>
#include <modules/module_parent.h>
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

static bool read_file_from_zip(vfs::FileBuffer &buf, const fs::path &file, const ZipPtr &zip) {
    if (!mz_zip_reader_extract_file_to_callback(zip.get(), file.generic_path().string().c_str(), &write_to_buffer, &buf, 0)) {
        LOG_CRITICAL("miniz error: {} extracting file: {}", miniz_get_error(zip), file.generic_path().string());
        return false;
    }

    return true;
}

bool install_archive(HostState &host, GuiState *gui, const fs::path &archive_path, const std::function<void(float)> &progress_callback) {
    if (!fs::exists(archive_path)) {
        LOG_CRITICAL("Failed to load archive file in path: {}", archive_path.generic_path().string());
        return false;
    }
    const ZipPtr zip(new mz_zip_archive, delete_zip);
    std::memset(zip.get(), 0, sizeof(*zip));

    FILE *vpk_fp;

    if (progress_callback != nullptr) {
        progress_callback(0);
    }

#ifdef WIN32
    _wfopen_s(&vpk_fp, archive_path.generic_path().wstring().c_str(), L"rb");
#else
    vpk_fp = fopen(archive_path.generic_path().string().c_str(), "rb");
#endif

    if (!mz_zip_reader_init_cfile(zip.get(), vpk_fp, 0, 0)) {
        LOG_CRITICAL("miniz error reading archive: {}", miniz_get_error(zip));
        fclose(vpk_fp);
        return false;
    }

    int num_files = mz_zip_reader_get_num_files(zip.get());
    fs::path sfo_path = "sce_sys/param.sfo";
    bool theme = false;
    std::string extra_path;

    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat))
            continue;

        std::string m_filename = std::string(file_stat.m_filename);

        if (m_filename.find("sce_module/steroid.suprx") != std::string::npos) {
            LOG_CRITICAL("A Vitamin dump was detected, aborting installation...");
            fclose(vpk_fp);
            return false;
        }

        if (m_filename.find("theme.xml") != std::string::npos)
            theme = true;

        //This was here before to check if the game files were in the zip root, since this commit
        // allows support for the game to be inside folders and install it anyways
        //i thought we should comment this check

        //edited to be compatible right away if someone wants to uncomment it

        //if (m_filename.find(sfo_path.string()) != std::string::npos)
        //break;

        if (m_filename.find("eboot.bin") != std::string::npos) {
            extra_path = m_filename.replace(m_filename.find("eboot.bin"), strlen("eboot.bin"), "");
            continue;
        }
    }

    vfs::FileBuffer params;
    if (!read_file_from_zip(params, fs::path(extra_path) / sfo_path, zip)) {
        fclose(vpk_fp);
        return false;
    }

    SfoFile sfo_handle;
    sfo::load(sfo_handle, params);
    sfo::get_data_by_key(host.app_title_id, sfo_handle, "TITLE_ID");
    sfo::get_data_by_key(host.app_title, sfo_handle, "TITLE");
    sfo::get_data_by_key(host.app_version, sfo_handle, "APP_VER");
    sfo::get_data_by_key(host.app_category, sfo_handle, "CATEGORY");
    sfo::get_data_by_key(host.app_content_id, sfo_handle, "CONTENT_ID");
    fs::path output_path;

    if (host.app_category == "ac") {
        if (theme) {
            output_path = { fs::path(host.pref_path) / "ux0/theme" / host.app_content_id };
            host.app_title += " (Theme)";
            host.app_category = "theme";
        } else {
            host.app_content_id = host.app_content_id.substr(20);
            output_path = { fs::path(host.pref_path) / "ux0/addcont" / host.app_title_id / host.app_content_id };
            host.app_title = host.app_title + " (DLC)";
        }
    } else if (host.app_category == "gp")
        output_path = { fs::path(host.pref_path) / "ux0/patch" / host.app_title_id };
    else
        output_path = { fs::path(host.pref_path) / "ux0/app" / host.app_title_id };

    const auto created = fs::create_directories(output_path);
    if (!created) {
        if (!gui) {
            fs::remove_all(output_path);
        } else if (!gui->file_menu.archive_install_dialog) {
            gui::GenericDialogState status = gui::UNK_STATE;

            while (handle_events(host, *gui) && (status == 0)) {
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
            if (status == gui::CANCEL_STATE) {
                LOG_INFO("{} already installed, {}", host.app_title_id, host.app_category == "gd" ? "launching application..." : "open home");
                fclose(vpk_fp);
                return true;
            } else if (status == gui::UNK_STATE) {
                exit(0);
            }
        } else if (gui->file_menu.archive_install_dialog && !gui->content_reinstall_confirm) {
            vfs::FileBuffer params;
            vfs::read_app_file(params, host.pref_path, host.app_title_id, sfo_path);
            sfo::load(host.sfo_handle, params);
            sfo::get_data_by_key(gui->app_ver, host.sfo_handle, "APP_VER");
            gui->content_reinstall_confirm = true;
            fclose(vpk_fp);
            return false;
        }
    }

    float file_progress = 0;
    float decrypt_progress = 0;

    const auto update_progress = [&]() {
        if (progress_callback != nullptr) {
            progress_callback(file_progress * 0.7f + decrypt_progress * 0.3f);
        }
    };

    for (auto i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat)) {
            continue;
        }
        file_progress = static_cast<float>(i) / num_files * 100.0f;
        update_progress();
        std::string m_filename = file_stat.m_filename;
        if (extra_path.find(m_filename) != std::string::npos) {
            continue;
        }
        std::string replace_filename = m_filename.substr(extra_path.size());
        const fs::path file_output = { output_path / replace_filename };
        if (mz_zip_reader_is_file_a_directory(zip.get(), i)) {
            fs::create_directories(file_output);
        } else {
            if (!fs::exists(file_output.parent_path())) {
                fs::create_directories(file_output.parent_path());
            }

            LOG_INFO("Extracting {}", file_output.generic_path().string());
            mz_zip_reader_extract_to_file(zip.get(), i, file_output.generic_path().string().c_str(), 0);
        }
    }

    if (fs::exists(output_path / "sce_sys/package/")) {
        if (fs::exists(output_path / "sce_sys/package/work.bin")) {
            std::string licpath = output_path.string() + "/sce_sys/package/work.bin";
            update_progress();
            if (!decrypt_install_nonpdrm(host, licpath, output_path.string())) {
                LOG_ERROR("NoNpDrm installation failed, deleting data!");
                fs::remove_all(output_path);
                return false;
            }
            decrypt_progress = 100;
        } else {
            LOG_WARN("The nonpdrm license file (work.bin) is missing! If you're trying to install a NoNpDrm dump, please make sure that it is present in /sce_sys/package/ folder. Otherwise, ignore this warning.");
        }
    }

    update_progress();

    LOG_INFO("{} [{}] installed succesfully!", host.app_title_id, host.app_title);
    fclose(vpk_fp);
    return true;
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
            SceUID module_id = load_self(lib_entry_point, host.kernel, host.mem, module_buffer.data(), MODULE_PATH_ABS, host.cfg);
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
    if (host.cfg.current_config.auto_lle)
        LOG_INFO("{}: enabled", host.cfg[e_auto_lle]);
    else if (!host.cfg.current_config.lle_modules.empty()) {
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

    if (host.cfg.current_config.lle_kernel) {
        // Load kernel pre-loaded module
        const std::vector<std::string> lib_load_list = {
            "us/libkernel.suprx",
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
        SceUID module_id = load_self(entry_point, host.kernel, host.mem, eboot_buffer.data(), "app0:" + host.self_path, host.cfg);
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
            host.kernel.stop_all_threads();
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
                    gui::update_apps_list_opened(gui, host, host.io.app_path);
                    gui::init_live_area(gui, host);
                    gui.live_area.information_bar = !gui.live_area.information_bar;
                    gui.live_area.live_area_screen = !gui.live_area.live_area_screen;
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
            if ((drop_file.extension() == ".vpk") || (drop_file.extension() == ".zip"))
                install_archive(host, &gui, drop_file);
            else if ((drop_file.extension() == ".rif") || (drop_file.extension() == ".bin"))
                copy_license(host, drop_file);
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

    const SceUID main_thread_id = create_thread(entry_point, host.kernel, host.mem, host.io.title_id.c_str(), SCE_KERNEL_DEFAULT_PRIORITY_USER, static_cast<int>(SCE_KERNEL_STACK_SIZE_USER_MAIN), nullptr);

    if (main_thread_id < 0) {
        app::error_dialog("Failed to init main thread.", host.window.get());
        return InitThreadFailed;
    }

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
        const ThreadStatePtr module_thread = create_thread(host.kernel, host.mem, module_name);
        const auto ret = run_guest_function(host.kernel, *module_thread, module_start.address(), { 0, 0 });
        delete_thread(host.kernel, *module_thread);

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
    if (start_thread(host.kernel, main_thread_id, param.size, Ptr<void>(param.attr)) < 0) {
        app::error_dialog("Failed to run main thread.", host.window.get());
        return RunThreadFailed;
    }

    return Success;
}
