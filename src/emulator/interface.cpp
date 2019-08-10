// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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
#include <gui/imgui_impl_sdl_gl3.h>
#include <host/functions.h>
#include <host/load_self.h>
#include <host/sfo.h>
#include <io/functions.h>
#include <io/vfs.h>
#include <kernel/functions.h>
#include <kernel/thread/thread_functions.h>
#include <modules/module_parent.h>
#include <touch/touch.h>
#include <util/find.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL.h>

#ifdef USE_GDBSTUB
#include <gdbstub/functions.h>
#endif

#ifdef USE_DISCORD_RICH_PRESENCE
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

static bool install_vpk(Ptr<const void> &entry_point, HostState &host, GuiState &gui, const fs::path &path) {
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
        return false;
    }

    int num_files = mz_zip_reader_get_num_files(zip.get());
    fs::path sfo_path = "sce_sys/param.sfo";

    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat)) {
            continue;
        }
        if (fs::path(file_stat.m_filename) == "sce_module/steroid.suprx") {
            LOG_CRITICAL("A Vitamin dump was detected, aborting installation...");
            return false;
        }
        if (fs::path(file_stat.m_filename) == sfo_path) {
            break;
        }
    }

    vfs::FileBuffer params;
    if (!read_file_from_zip(params, sfo_path, zip)) {
        return false;
    }

    SfoFile sfo_handle;
    sfo::load(sfo_handle, params);
    sfo::get_data_by_key(host.io.title_id, sfo_handle, "TITLE_ID");

    fs::path output_path{ fs::path(host.pref_path) / "ux0/app" / host.io.title_id };

    const auto created = fs::create_directories(output_path);
    if (!created) {
        gui::GenericDialogState status = gui::UNK_STATE;
        while (handle_events(host) && (status == 0)) {
            ImGui_ImplSdlGL3_NewFrame(host.window.get());
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            gui::draw_ui(gui, host);
            gui::draw_reinstall_dialog(&status);
            glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
            ImGui::Render();
            ImGui_ImplSdlGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(host.window.get());
        }
        if (status == gui::CANCEL_STATE) {
            LOG_INFO("{} already installed, launching application...", host.io.title_id);
            return true;
        } else if (status == gui::UNK_STATE) {
            exit(0);
        }
    }

    for (auto i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat)) {
            continue;
        }

        const fs::path file_output = { output_path / file_stat.m_filename };
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

    LOG_INFO("{} installed succesfully!", host.io.title_id);
    return true;
}

static ExitCode load_app_impl(Ptr<const void> &entry_point, HostState &host, GuiState &gui, const std::wstring &path, const app::AppRunType run_type) {
    if (path.empty())
        return InvalidApplicationPath;

    if (run_type == app::AppRunType::Vpk) {
        if (!install_vpk(entry_point, host, gui, path)) {
            return FileNotFound;
        }
    } else if (run_type == app::AppRunType::Extracted) {
        host.io.title_id = string_utils::wide_to_utf(path);
    }

    vfs::FileBuffer params;
    bool params_found = vfs::read_app_file(params, host.pref_path, host.io.title_id, "sce_sys/param.sfo");

    std::string game_category;

    if (params_found) {
        sfo::load(host.sfo_handle, params);

        sfo::get_data_by_key(host.game_title, host.sfo_handle, "TITLE");
        std::replace(host.game_title.begin(), host.game_title.end(), '\n', ' '); // Restrict title to one line
        sfo::get_data_by_key(host.io.title_id, host.sfo_handle, "TITLE_ID");
        sfo::get_data_by_key(host.game_version, host.sfo_handle, "APP_VER");
        sfo::get_data_by_key(game_category, host.sfo_handle, "CATEGORY");
    } else {
        host.game_title = host.io.title_id; // Use TitleID as Title
        host.game_version = "N/A";
        game_category = "N/A";
    }

    if (host.cfg.archive_log) {
        const fs::path log_directory{ host.base_path + "/logs" };
        fs::create_directory(log_directory);
        const auto log_name{ log_directory / (string_utils::remove_special_chars(host.game_title) + " - [" + host.io.title_id + "].log") };
        if (logging::add_sink(log_name) != Success)
            return InitConfigFailed;
    }

    LOG_INFO("Title: {}", host.game_title);
    LOG_INFO("Serial: {}", host.io.title_id);
    LOG_INFO("Version: {}", host.game_version);
    LOG_INFO("Category: {}", game_category);

    init_device_paths(host.io);

    // Load pre-loaded libraries
    const char *const lib_load_list[] = {
        "sce_module/libc.suprx",
        "sce_module/libfios2.suprx",
        "sce_module/libult.suprx",
    };

    for (auto module_path : lib_load_list) {
        vfs::FileBuffer module_buffer;
        Ptr<const void> lib_entry_point;

        if (vfs::read_app_file(module_buffer, host.pref_path, host.io.title_id, module_path)) {
            SceUID module_id = load_self(lib_entry_point, host.kernel, host.mem, module_buffer.data(), std::string("app0:") + module_path, host.cfg);
            if (module_id >= 0) {
                const auto module = host.kernel.loaded_modules[module_id];
                const auto module_name = module->module_name;
                LOG_INFO("Pre-load module {} (at \"{}\") loaded", module_name, module_path);
            } else
                return FileNotFound;
        } else {
            LOG_DEBUG("Pre-load module at \"{}\" not present", module_path);
        }
    }

    // Load main executable (eboot.bin)
    vfs::FileBuffer eboot_buffer;
    if (vfs::read_app_file(eboot_buffer, host.pref_path, host.io.title_id, EBOOT_PATH)) {
        SceUID module_id = load_self(entry_point, host.kernel, host.mem, eboot_buffer.data(), EBOOT_PATH_ABS, host.cfg);
        if (module_id >= 0) {
            const auto module = host.kernel.loaded_modules[module_id];
            const auto module_name = module->module_name;

            LOG_INFO("Main executable {} ({}) loaded", module_name, EBOOT_PATH);
        } else
            return FileNotFound;
    } else
        return FileNotFound;

    host.io.pref_save_path = host.pref_path + "/ux0/user/00/savedata/" + host.io.title_id + '/';
    if (!fs::exists(host.io.pref_save_path))
        fs::create_directories(host.io.pref_save_path);

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

bool handle_events(HostState &host) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSdlGL3_ProcessEvent(&event);
        switch (event.type) {
        case SDL_QUIT:
            stop_all_threads(host.kernel);
            host.gxm.display_queue.abort();
            host.display.abort.exchange(true);
            host.display.condvar.notify_all();
            return false;

        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_g) {
                auto &display = host.display;

                // toggle gui state
                bool old_imgui_render = display.imgui_render.load();
                while (!display.imgui_render.compare_exchange_weak(old_imgui_render, !old_imgui_render)) {
                }
            }
            if (event.key.keysym.sym == SDLK_t) {
                toggle_touchscreen();
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

ExitCode load_app(Ptr<const void> &entry_point, HostState &host, GuiState &gui, const std::wstring &path, const app::AppRunType run_type) {
    if (load_app_impl(entry_point, host, gui, path, run_type) != Success) {
        std::string message = "Failed to load \"";
        message += string_utils::wide_to_utf(path);
        message += "\"";
        message += "\nSee console output for details.";
        app::error_dialog(message, host.window.get());
        return ModuleLoadFailed;
    }
    if (!host.cfg.show_gui) {
        auto &imgui_render = host.display.imgui_render;

        bool old_imgui_render = imgui_render.load();
        while (!imgui_render.compare_exchange_weak(old_imgui_render, !old_imgui_render)) {
        }
    }

#ifdef USE_GDBSTUB
    server_open(host);
#endif

#ifdef USE_DISCORD_RICH_PRESENCE
    if (host.cfg.discord_rich_presence)
        discord::update_presence(host.io.title_id, host.game_title);
#endif

    return Success;
}

ExitCode run_app(HostState &host, Ptr<const void> &entry_point) {
    const CallImport call_import = [&host](CPUState &cpu, uint32_t nid, SceUID main_thread_id) {
        ::call_import(host, cpu, nid, main_thread_id);
    };

    const SceUID main_thread_id = create_thread(entry_point, host.kernel, host.mem, host.io.title_id.c_str(), SCE_KERNEL_DEFAULT_PRIORITY_USER, static_cast<int>(SCE_KERNEL_STACK_SIZE_USER_MAIN),
        call_import, false);

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

        if (std::string(module->path) == EBOOT_PATH_ABS)
            continue;

        LOG_DEBUG("Running module_start of library: {}", module_name);

        Ptr<void> argp = Ptr<void>();
        const SceUID module_thread_id = create_thread(module_start, host.kernel, host.mem, module_name, SCE_KERNEL_DEFAULT_PRIORITY_USER, static_cast<int>(SCE_KERNEL_STACK_SIZE_USER_DEFAULT),
            call_import, false);
        const ThreadStatePtr module_thread = util::find(module_thread_id, host.kernel.threads);
        const auto ret = run_on_current(*module_thread, module_start, 0, argp);
        module_thread->to_do = ThreadToDo::exit;
        module_thread->something_to_do.notify_all(); // TODO Should this be notify_one()?
        host.kernel.running_threads.erase(module_thread_id);
        host.kernel.threads.erase(module_thread_id);

        LOG_INFO("Module {} (at \"{}\") module_start returned {}", module_name, module->path, log_hex(ret));
    }

    if (start_thread(host.kernel, main_thread_id, 0, Ptr<void>()) < 0) {
        app::error_dialog("Failed to run main thread.", host.window.get());
        return RunThreadFailed;
    }

    return Success;
}
