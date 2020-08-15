// Vita3K emulator project
// Copyright (C) 2020 Vita3K team
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

#include <config/functions.h>
#include <gui/functions.h>
#include <host/functions.h>
#include <host/load_self.h>
#include <host/pkg.h>
#include <host/sfo.h>
#include <io/device.h>
#include <io/functions.h>
#include <io/vfs.h>
#include <kernel/functions.h>
#include <kernel/thread/thread_functions.h>
#include <modules/module_parent.h>
#include <touch/touch.h>
#include <util/find.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <gui/imgui_impl_sdl.h>

#include <regex>

#include <SDL.h>

#ifdef USE_GDBSTUB
#include <gdbstub/functions.h>
#endif

#if DISCORD_RPC
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

bool install_archive(HostState &host, GuiState *gui, const fs::path &path) {
    if (!fs::exists(path)) {
        LOG_CRITICAL("Failed to load VPK file path: {}", path.generic_path().string());
        return false;
    }
    const ZipPtr zip(new mz_zip_archive, delete_zip);
    std::memset(zip.get(), 0, sizeof(*zip));

    FILE *vpk_fp;

#ifdef WIN32
    _wfopen_s(&vpk_fp, path.generic_path().wstring().c_str(), L"rb");
#else
    vpk_fp = fopen(path.generic_path().string().c_str(), "rb");
#endif

    if (!mz_zip_reader_init_cfile(zip.get(), vpk_fp, 0, 0)) {
        LOG_CRITICAL("miniz error reading archive: {}", miniz_get_error(zip));
        fclose(vpk_fp);
        return false;
    }

    int num_files = mz_zip_reader_get_num_files(zip.get());
    fs::path sfo_path = "sce_sys/param.sfo";
    bool theme = false;
    std::string extra_path = "";

    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat))
            continue;

        if (fs::path(file_stat.m_filename).string().find("sce_module/steroid.suprx") != std::string::npos) {
            LOG_CRITICAL("A Vitamin dump was detected, aborting installation...");
            fclose(vpk_fp);
            return false;
        }

        if (fs::path(file_stat.m_filename).string().find("theme.xml") != std::string::npos)
            theme = true;

        //This was here before to check if the game files were in the zip root, since this commit
        // allows support for the game to be inside folders and install it anyways
        //i thought we should comment this check

        //edited to be compatible right away if someone wants to uncomment it

        //if (fs::path(file_stat.m_filename).string().find(sfo_path.string()) != std::string::npos)
        //break;

        if (fs::path(file_stat.m_filename).string().find("eboot.bin") != std::string::npos) {
            extra_path = std::string(file_stat.m_filename).replace(std::string(file_stat.m_filename).find("eboot.bin"), sizeof("eboot.bin") - 1, "");
            continue;
        }
    }

    vfs::FileBuffer params;
    if (!read_file_from_zip(params, fs::path(extra_path) / sfo_path, zip)) {
        fclose(vpk_fp);
        return false;
    }

    SfoFile sfo_handle;
    std::string content_id, dlc_foldername;
    sfo::load(sfo_handle, params);
    sfo::get_data_by_key(host.io.title_id, sfo_handle, "TITLE_ID");
    sfo::get_data_by_key(host.app_title, sfo_handle, "TITLE");
    sfo::get_data_by_key(host.app_version, sfo_handle, "APP_VER");
    sfo::get_data_by_key(host.app_category, sfo_handle, "CATEGORY");
    sfo::get_data_by_key(content_id, sfo_handle, "CONTENT_ID");
    fs::path output_path;

    if (host.app_category == "ac") {
        if (theme) {
            output_path = { fs::path(host.pref_path) / "ux0/theme" / content_id };
            host.app_title += " (Theme)";
        } else {
            dlc_foldername = content_id.substr(20);
            output_path = { fs::path(host.pref_path) / "ux0/addcont" / host.io.title_id / dlc_foldername };
            host.app_title = host.app_title + " (DLC)";
        }
    } else if (host.app_category == "gp")
        output_path = { fs::path(host.pref_path) / "ux0/patch" / host.io.title_id };
    else
        output_path = { fs::path(host.pref_path) / "ux0/app" / host.io.title_id };

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
                gui::draw_reinstall_dialog(&status);
                glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
                ImGui::Render();
                ImGui_ImplSdl_RenderDrawData(gui->imgui_state.get());
                SDL_GL_SwapWindow(host.window.get());
            }
            if (status == gui::CANCEL_STATE) {
                LOG_INFO("{} already installed, launching application...", host.io.title_id);
                fclose(vpk_fp);
                return true;
            } else if (status == gui::UNK_STATE) {
                exit(0);
            }
        } else if (gui->file_menu.archive_install_dialog && !gui->content_reinstall_confirm) {
            vfs::FileBuffer params;
            vfs::read_app_file(params, host.pref_path, host.io.title_id, sfo_path);
            sfo::load(host.sfo_handle, params);
            sfo::get_data_by_key(gui->app_ver, host.sfo_handle, "APP_VER");
            gui->content_reinstall_confirm = true;
            fclose(vpk_fp);
            return false;
        }
    }

    for (auto i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat)) {
            continue;
        }

        std::string m_filename = file_stat.m_filename;
        std::string replace_filename = m_filename.substr(extra_path.size());
        const fs::path file_output = { output_path / replace_filename };
        if (mz_zip_reader_is_file_a_directory(zip.get(), i)) {
            fs::create_directories(output_path);
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
            if (!decrypt_install_nonpdrm(licpath, output_path.string())) {
                LOG_ERROR("NoNpDrm installation failed, deleting data!");
                fs::remove_all(output_path);
                return false;
            }
        } else {
            LOG_WARN("The nonpdrm license file (work.bin) is missing! If you're trying to install a NoNpDrm dump, please make sure that it is present in /sce_sys/package/ folder. Otherwise, ignore this warning.");
        }
    }

    LOG_INFO("{} [{}] installed succesfully!", host.io.title_id, host.app_title);
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
            res = vfs::read_app_file(module_buffer, host.pref_path, host.io.title_id, module_path);
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

static ExitCode load_app_impl(Ptr<const void> &entry_point, HostState &host, const std::wstring &path, const app::AppRunType run_type) {
    if (path.empty())
        return InvalidApplicationPath;

    vfs::FileBuffer params;
    bool params_found = vfs::read_app_file(params, host.pref_path, host.io.title_id, "sce_sys/param.sfo");

    if (params_found) {
        sfo::load(host.sfo_handle, params);

        sfo::get_data_by_key(host.app_title, host.sfo_handle, "TITLE");
        std::replace(host.app_title.begin(), host.app_title.end(), '\n', ' '); // Restrict title to one line
        sfo::get_data_by_key(host.io.title_id, host.sfo_handle, "TITLE_ID");
        sfo::get_data_by_key(host.app_version, host.sfo_handle, "APP_VER");
        sfo::get_data_by_key(host.app_category, host.sfo_handle, "CATEGORY");
    } else {
        host.app_title = host.io.title_id; // Use TitleID as Title
        host.app_version = host.app_category = "N/A";
    }

    if (static_cast<int>(host.cfg.online_id.size()) - 1 < host.cfg.user_id || host.cfg.user_id < 0) {
        host.cfg.user_id = 0;
        if (host.cfg.overwrite_config)
            config::serialize_config(host.cfg, host.cfg.config_path);
    }

    if (host.io.user_id.empty())
        host.io.user_id = fmt::format("{:0>2d}", host.cfg.user_id);

    if (host.cfg.archive_log) {
        const fs::path log_directory{ host.base_path + "/logs" };
        fs::create_directory(log_directory);
        const auto log_name{ log_directory / (string_utils::remove_special_chars(host.app_title) + " - [" + host.io.title_id + "].log") };
        if (logging::add_sink(log_name) != Success)
            return InitConfigFailed;
    }

    LOG_INFO_IF(host.cfg.hardware_flip, "{}: enabled", host.cfg[e_hardware_flip]);
    if (host.cfg.auto_lle)
        LOG_INFO("{}: enabled", host.cfg[e_auto_lle]);
    else if (!host.cfg.lle_modules.empty()) {
        std::string modules;
        for (const auto &mod : host.cfg.lle_modules) {
            modules += mod + ",";
        }
        modules.pop_back();
        LOG_INFO("lle-modules: {}", modules);
    }

    LOG_INFO("Title: {}", host.app_title);
    LOG_INFO("Serial: {}", host.io.title_id);
    LOG_INFO("Version: {}", host.app_version);
    LOG_INFO("Category: {}", host.app_category);

    init_device_paths(host.io);
    init_savedata_game_path(host.io, host.pref_path);

    for (const auto &var : get_var_exports()) {
        auto addr = var.factory(host);
        host.kernel.export_nids.emplace(var.nid, addr);
    }

    // Load pre-loaded libraries
    const auto module_app_path{ fs::path(host.pref_path) / "ux0/app" / host.io.title_id / "sce_module" };
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

    // Load main executable (eboot.bin)
    vfs::FileBuffer eboot_buffer;
    if (vfs::read_app_file(eboot_buffer, host.pref_path, host.io.title_id, EBOOT_PATH)) {
        SceUID module_id = load_self(entry_point, host.kernel, host.mem, eboot_buffer.data(), EBOOT_PATH_ABS, host.cfg);
        if (module_id >= 0) {
            const auto module = host.kernel.loaded_modules[module_id];

            LOG_INFO("Main executable {} ({}) loaded", module->module_name, EBOOT_PATH);
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
            stop_all_threads(host.kernel);
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
            if (!gui.app_selector.selected_title_id.empty()) {
                // toggle gui state
                if (event.key.keysym.sym == SDLK_g)
                    host.display.imgui_render = !host.display.imgui_render;
                // Show/Hide Live Area during game run
                // TODO pause game running
                if (!gui.live_area.manual && (event.key.keysym.scancode == host.cfg.keyboard_button_psbutton)) {
                    if (gui.live_area_contents.find(host.io.title_id) == gui.live_area_contents.end())
                        gui::init_live_area(gui, host);
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
        }
    }

    return true;
}

ExitCode load_app(Ptr<const void> &entry_point, HostState &host, const std::wstring &path, const app::AppRunType run_type) {
    if (load_app_impl(entry_point, host, path, run_type) != Success) {
        std::string message = "Failed to load \"";
        message += string_utils::wide_to_utf(path);
        message += "\"";
        message += "\nSee console output for details.";
        app::error_dialog(message, host.window.get());
        return ModuleLoadFailed;
    }

    if (!host.cfg.show_gui)
        host.display.imgui_render = false;

#ifdef USE_GDBSTUB
    server_open(host);
#endif

#if DISCORD_RPC
    if (host.cfg.discord_rich_presence)
        discord::update_presence(host.io.title_id, host.app_title);
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

    auto inject = create_cpu_dep_inject(host);
    const SceUID main_thread_id = create_thread(entry_point, host.kernel, host.mem, host.io.title_id.c_str(), SCE_KERNEL_DEFAULT_PRIORITY_USER, static_cast<int>(SCE_KERNEL_STACK_SIZE_USER_MAIN),
        inject, nullptr);

    if (main_thread_id < 0) {
        app::error_dialog("Failed to init main thread.", host.window.get());
        return InitThreadFailed;
    }

    const ThreadStatePtr main_thread = util::find(main_thread_id, host.kernel.threads);

    // Run `module_start` export (entry point) of loaded libraries
    for (auto &mod : host.kernel.loaded_modules) {
        const auto module = mod.second;
        const auto module_start = module->module_start;
        const auto module_name = module->module_name;

        if (!module_start || (std::string(module->path) == EBOOT_PATH_ABS))
            continue;

        LOG_DEBUG("Running module_start of library: {} at address {}", module_name, log_hex(module_start.address()));

        auto argp = Ptr<void>();
        auto inject = create_cpu_dep_inject(host);
        const SceUID module_thread_id = create_thread(module_start, host.kernel, host.mem, module_name, SCE_KERNEL_DEFAULT_PRIORITY_USER, static_cast<int>(SCE_KERNEL_STACK_SIZE_USER_DEFAULT),
            inject, nullptr);
        const ThreadStatePtr module_thread = util::find(module_thread_id, host.kernel.threads);
        const auto ret = run_on_current(*module_thread, module_start, 0, argp);
        module_thread->to_do = ThreadToDo::exit;
        module_thread->something_to_do.notify_all(); // TODO Should this be notify_one()?
        host.kernel.running_threads.erase(module_thread_id);
        host.kernel.threads.erase(module_thread_id);

        LOG_INFO("Module {} (at \"{}\") module_start returned {}", module_name, module->path, log_hex(ret));
    }

    host.main_thread_id = main_thread_id;

    SceKernelThreadOptParam param;
    if (host.cfg.console && host.cfg.console_arguments != "") {
        auto args = split(host.cfg.console_arguments, "\\s+");
        // why is this flipped
        std::vector<uint8_t> buf;
        for (int i = 0; i < args.size(); ++i) {
            buf.insert(buf.end(), args[i].c_str(), args[i].c_str() + args[i].size() + 1);
        }
        auto arr = Ptr<uint8_t>(alloc(host.mem, buf.size(), "arg"));
        memcpy(arr.get(host.mem), buf.data(), buf.size());
        auto abc = arr.get(host.mem);
        param.size = buf.size();
        param.attr = arr.address();
    }
    if (start_thread(host.kernel, main_thread_id, param.size, Ptr<void>(param.attr)) < 0) {
        app::error_dialog("Failed to run main thread.", host.window.get());
        return RunThreadFailed;
    }

    return Success;
}
