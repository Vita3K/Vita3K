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

#include <host/functions.h>

#include <host/import_fn.h>
#include <host/rtc.h>
#include <host/state.h>
#include <host/version.h>

#include <audio/functions.h>
#include <io/functions.h>
#include <kernel/functions.h>
#include <kernel/thread_state.h>
#include <nids/functions.h>
#include <util/lock_and_find.h>
#include <util/log.h>

#include <cpu/functions.h>

#include <SDL_events.h>
#include <SDL_filesystem.h>
#include <SDL_video.h>

#include <glbinding/Binding.h>

#include <cassert>
#include <iomanip>
#include <iostream>

#include <imgui.h>
#include <gui/imgui_impl_sdl_gl2.h>

using namespace glbinding;

static const bool LOG_IMPORT_CALLS = false;

#define NID(name, nid) extern ImportFn *const import_##name;
#include <nids/nids.h>
#undef NID

static ImportFn *resolve_import(uint32_t nid) {
    switch (nid) {
#define NID(name, nid) \
    case nid:          \
        return import_##name;
#include <nids/nids.h>
#undef NID
    }

    return nullptr;
}

bool init(HostState &state, std::uint32_t window_width, std::uint32_t window_height) {
    const std::unique_ptr<char, void (&)(void *)> base_path(SDL_GetBasePath(), SDL_free);
    const std::unique_ptr<char, void (&)(void *)> pref_path(SDL_GetPrefPath(org_name, app_name), SDL_free);

    const ResumeAudioThread resume_thread = [&state](SceUID thread_id) {
        const ThreadStatePtr thread = lock_and_find(thread_id, state.kernel.threads, state.kernel.mutex);
        const std::unique_lock<std::mutex> lock(thread->mutex);
        if (thread->to_do == ThreadToDo::wait) {
            thread->to_do = ThreadToDo::run;
        }
        thread->something_to_do.notify_all();
    };

    state.base_path = base_path.get();
    state.pref_path = pref_path.get();
    state.window = WindowPtr(SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_OPENGL), SDL_DestroyWindow);
    if (!state.window || !init(state.mem) || !init(state.audio, resume_thread) || !init(state.io, pref_path.get())) {
        return false;
    }

    state.glcontext = GLContextPtr(SDL_GL_CreateContext(state.window.get()), SDL_GL_DeleteContext);
    Binding::initialize(false);
    state.kernel.base_tick = { rtc_base_ticks() };
    state.display.set_window_dims(window_width, window_height);

    return true;
}

bool handle_events(HostState &host) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSdlGL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT) {
            stop_all_threads(host.kernel);
            host.gxm.display_queue.abort();
            host.display.abort.exchange(true);
            host.display.condvar.notify_all();
            return false;
        }
    }

    return true;
}

/**
 * \brief Resolves a function imported from a loaded module.
 * \param kernel Kernel state struct
 * \param nid NID to resolve
 * \return Resolved address, 0 if not found
 */
Address resolve_export(KernelState &kernel, uint32_t nid) {
    const ExportNids::iterator export_address = kernel.export_nids.find(nid);
    if (export_address == kernel.export_nids.end()) {
        return 0;
    }

    return export_address->second;
}

void call_import(HostState &host, uint32_t nid, SceUID thread_id) {
    Address export_pc = resolve_export(host.kernel, nid);

    if (!export_pc) {
        // HLE - call our C++ function

        if (LOG_IMPORT_CALLS) {
            const char *const name = import_name(nid);
            LOG_TRACE("THREAD_ID {} NID {:#08x} ({})) called", thread_id, nid, name);
        }
        ImportFn *const fn = resolve_import(nid);
        if (fn != nullptr) {
            (*fn)(host, thread_id);
        }
    } else {
        // LLE - directly run ARM code imported from some loaded module

        if (LOG_IMPORT_CALLS) {
            const char *const name = import_name(nid);
            LOG_TRACE("THREAD_ID {} EXPORTED NID {:#08x} at {:#08x} ({})) called", thread_id, nid, export_pc, name);
        }
        const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
        const std::unique_lock<std::mutex> lock(thread->mutex);
        write_pc(*thread->cpu, export_pc);
    }
}
