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

#include <gui/functions.h>
#include <gui/imgui_impl_sdl_gl3.h>
#include <host/app.h>
#include <host/functions.h>
#include <host/sfo.h>
#include <host/state.h>
#include <host/version.h>
#include <io/functions.h>
#include <io/state.h>
#include <kernel/load_self.h>
#include <kernel/state.h>
#include <kernel/thread/thread_functions.h>
#include <util/find.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL.h>
#include <glutil/gl.h>

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <istream>
#include <iterator>
#include <sstream>

static const char *EBOOT_PATH = "eboot.bin";
static const char *EBOOT_PATH_ABS = "app0:eboot.bin";

using namespace gl;

void error_dialog(const std::string &message, SDL_Window *window) {
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message.c_str(), window) < 0) {
        LOG_ERROR("SDL Error: {}", message);
    }
}

static void delete_zip(mz_zip_archive *zip) {
    mz_zip_reader_end(zip);
    delete zip;
}

static size_t write_to_buffer(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n) {
    Buffer *const buffer = static_cast<Buffer *>(pOpaque);
    assert(file_ofs == buffer->size());
    const uint8_t *const first = static_cast<const uint8_t *>(pBuf);
    const uint8_t *const last = &first[n];
    buffer->insert(buffer->end(), first, last);

    return n;
}

bool read_file_from_disk(Buffer &buf, const char *file, HostState &host) {
    std::string file_path = host.pref_path + "ux0/app/" + host.io.title_id + "/" + file;
    std::ifstream f(file_path, std::ifstream::binary);
    if (f.fail()) {
        return false;
    }
    f.unsetf(std::ios::skipws);
    std::streampos size;
    f.seekg(0, std::ios::end);
    size = f.tellg();
    f.seekg(0, std::ios::beg);
    buf.reserve(size);
    buf.insert(buf.begin(), std::istream_iterator<uint8_t>(f), std::istream_iterator<uint8_t>());
    return true;
}

static const char *miniz_get_error(const ZipPtr &zip) {
    return mz_zip_get_error_string(mz_zip_get_last_error(zip.get()));
}

static bool read_file_from_zip(Buffer &buf, FILE *&vpk_fp, const char *file, const ZipPtr zip) {
    const int file_index = mz_zip_reader_locate_file(zip.get(), file, nullptr, 0);
    if (file_index < 0) {
        LOG_CRITICAL("Failed to locate {}.", file);
        return false;
    }

    if (!mz_zip_reader_extract_file_to_callback(zip.get(), file, &write_to_buffer, &buf, 0)) {
        LOG_CRITICAL("miniz error: {} extracting file: {}", miniz_get_error(zip), file);
        return false;
    }

    return true;
}

bool install_vpk(Ptr<const void> &entry_point, HostState &host, const std::wstring &path) {
    const ZipPtr zip(new mz_zip_archive, delete_zip);
    std::memset(zip.get(), 0, sizeof(*zip));

    FILE *vpk_fp;

#ifdef WIN32
    if (_wfopen_s(&vpk_fp, path.c_str(), L"rb")) {
#else
    if (!(vpk_fp = fopen(string_utils::wide_to_utf(path).c_str(), "rb"))) {
#endif
        LOG_CRITICAL("Failed to open the vpk.");
        return false;
    }

    if (!mz_zip_reader_init_cfile(zip.get(), vpk_fp, 0, 0)) {
        LOG_CRITICAL("miniz error reading archive: {}", miniz_get_error(zip));
        return false;
    }

    int num_files = mz_zip_reader_get_num_files(zip.get());

    std::string sfo_path = "sce_sys/param.sfo";

    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat)) {
            continue;
        }
        if (strstr(file_stat.m_filename, "sce_sys/param.sfo")) {
            sfo_path = file_stat.m_filename;
            break;
        }
    }

    Buffer params;
    if (!read_file_from_zip(params, vpk_fp, sfo_path.c_str(), zip)) {
        return false;
    }

    SfoFile sfo_handle;
    load_sfo(sfo_handle, params);
    find_data(host.io.title_id, sfo_handle, "TITLE_ID");

    std::string output_base_path = host.pref_path + "ux0/app/";
    std::string title_base_path = output_base_path;

    if (sfo_path.length() < 20) {
        output_base_path += host.io.title_id;
    }

    title_base_path += host.io.title_id;

    const bool created = fs::create_directory(title_base_path);
    if (!created) {
        GenericDialogState status = UNK_STATE;
        while (handle_events(host) && (status == 0)) {
            ImGui_ImplSdlGL3_NewFrame(host.window.get());
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            DrawUI(host);
            DrawReinstallDialog(host, &status);
            glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
            ImGui::Render();
            ImGui_ImplSdlGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(host.window.get());
        }
        if (status == CANCEL_STATE) {
            LOG_INFO("{} already installed, launching application...", host.io.title_id);
            return true;
        } else if (status == UNK_STATE) {
            exit(0);
        }
    }

    for (auto i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat)) {
            continue;
        }
        std::string output_path = output_base_path;
        output_path += "/";
        output_path += file_stat.m_filename;
        if (mz_zip_reader_is_file_a_directory(zip.get(), i)) {
            fs::create_directories(output_path);
        } else {
            const size_t slash = output_path.rfind('/');
            if (std::string::npos != slash) {
                std::string directory = output_path.substr(0, slash);
                fs::create_directories(directory);
            }

            LOG_INFO("Extracting {}", output_path);
            mz_zip_reader_extract_to_file(zip.get(), i, output_path.c_str(), 0);
        }
    }

    std::string savedata_path = host.pref_path + "ux0/user/00/savedata/" + host.io.title_id;
    fs::create_directory(savedata_path);

    LOG_INFO("{} installed succesfully!", host.io.title_id);
    return true;
}

bool load_app_impl(Ptr<const void> &entry_point, HostState &host, const std::wstring &path, AppRunType run_type) {
    if (path.empty())
        return InvalidApplicationPath;

    if (run_type == AppRunType::Vpk) {
        if (!install_vpk(entry_point, host, path)) {
            return false;
        }
    } else if (run_type == AppRunType::Extracted) {
        host.io.title_id = string_utils::wide_to_utf(path);
    }

    Buffer params;
    if (!read_file_from_disk(params, "sce_sys/param.sfo", host)) {
        return false;
    }

    load_sfo(host.sfo_handle, params);

    find_data(host.game_title, host.sfo_handle, "TITLE");
    find_data(host.io.title_id, host.sfo_handle, "TITLE_ID");
    std::string category;
    find_data(category, host.sfo_handle, "CATEGORY");

    LOG_INFO("Title: {}", host.game_title);
    LOG_INFO("Serial: {}", host.io.title_id);
    LOG_INFO("Category: {}", category);

    init_device_paths(host.io);

    // Load pre-loaded libraries
    const char *const lib_load_list[] = {
        "sce_module/libc.suprx",
        "sce_module/libfios2.suprx",
        "sce_module/libult.suprx",
    };

    for (auto module_path : lib_load_list) {
        Buffer module_buffer;
        Ptr<const void> lib_entry_point;

        if (read_file_from_disk(module_buffer, module_path, host)) {
            SceUID module_id = load_self(lib_entry_point, host.kernel, host.mem, module_buffer.data(), std::string("app0:") + module_path);
            if (module_id >= 0) {
                const auto module = host.kernel.loaded_modules[module_id];
                const auto module_name = module->module_name;
                LOG_INFO("Pre-load module {} (at \"{}\") loaded", module_name, module_path);
            } else
                return false;
        } else {
            LOG_DEBUG("Pre-load module at \"{}\" not present", module_path);
        }
    }

    // Load main executable (eboot.bin)
    Buffer eboot_buffer;
    if (read_file_from_disk(eboot_buffer, EBOOT_PATH, host)) {
        SceUID module_id = load_self(entry_point, host.kernel, host.mem, eboot_buffer.data(), EBOOT_PATH_ABS);
        if (module_id >= 0) {
            const auto module = host.kernel.loaded_modules[module_id];
            const auto module_name = module->module_name;

            LOG_INFO("Main executable {} ({}) loaded", module_name, EBOOT_PATH);
        } else
            return false;
    } else
        return false;

    return true;
}

ExitCode load_app(Ptr<const void> &entry_point, HostState &host, const std::wstring &path, AppRunType run_type) {
    if (!load_app_impl(entry_point, host, path, run_type)) {
        std::string message = "Failed to load \"";
        message += string_utils::wide_to_utf(path);
        message += "\"";
        message += "\nSee console output for details.";
        error_dialog(message.c_str(), host.window.get());
        return ModuleLoadFailed;
    }

    return Success;
}

ExitCode run_app(HostState &host, Ptr<const void> &entry_point) {
    const CallImport call_import = [&host](CPUState &cpu, uint32_t nid, SceUID main_thread_id) {
        ::call_import(host, cpu, nid, main_thread_id);
    };

    const SceUID main_thread_id = create_thread(entry_point, host.kernel, host.mem, host.io.title_id.c_str(), SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_STACK_SIZE_USER_MAIN, call_import, false);
    if (main_thread_id < 0) {
        error_dialog("Failed to init main thread.", host.window.get());
        return InitThreadFailed;
    }

    const ThreadStatePtr main_thread = find(main_thread_id, host.kernel.threads);

    // Run `module_start` export (entry point) of loaded libraries
    for (auto &mod : host.kernel.loaded_modules) {
        const auto module = mod.second;
        const auto module_start = module->module_start;
        const auto module_name = module->module_name;

        if (std::string(module->path) == EBOOT_PATH_ABS)
            continue;

        LOG_DEBUG("Running module_start of library: {}", module_name);

        Ptr<void> argp = Ptr<void>();
        const SceUID module_thread_id = create_thread(module_start, host.kernel, host.mem, module_name, SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_STACK_SIZE_USER_DEFAULT, call_import, false);
        const ThreadStatePtr module_thread = find(module_thread_id, host.kernel.threads);
        const auto ret = run_on_current(*module_thread, module_start, 0, argp);
        module_thread->to_do = ThreadToDo::exit;
        module_thread->something_to_do.notify_all(); // TODO Should this be notify_one()?
        host.kernel.running_threads.erase(module_thread_id);
        host.kernel.threads.erase(module_thread_id);

        LOG_INFO("Module {} (at \"{}\") module_start returned {}", module_name, module->path, log_hex(ret));
    }

    if (start_thread(host.kernel, main_thread_id, 0, Ptr<void>()) < 0) {
        error_dialog("Failed to run main thread.", host.window.get());
        return RunThreadFailed;
    }

    return Success;
}

void set_window_title(HostState &host) {
    const uint32_t sdl_ticks_now = SDL_GetTicks();
    const uint32_t ms = sdl_ticks_now - host.sdl_ticks;
    if (ms >= 1000 && host.frame_count > 0) {
        const uint32_t fps = (host.frame_count * 1000) / ms;
        const uint32_t ms_per_frame = ms / host.frame_count;
        std::ostringstream title;
        title << window_title << " | " << host.game_title << " (" << host.io.title_id << ") | " << ms_per_frame << " ms/frame (" << fps << " frames/sec)";
        SDL_SetWindowTitle(host.window.get(), title.str().c_str());
        host.sdl_ticks = sdl_ticks_now;
        host.frame_count = 0;
    }
}
