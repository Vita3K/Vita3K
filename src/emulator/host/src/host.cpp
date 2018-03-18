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

#include <SDL_events.h>
#include <SDL_filesystem.h>
#include <SDL_video.h>

#include <cassert>
#include <iomanip>
#include <iostream>

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

bool init(HostState &state) {
    const std::unique_ptr<char, void (&)(void *)> base_path(SDL_GetBasePath(), SDL_free);
    const std::unique_ptr<char, void (&)(void *)> pref_path(SDL_GetPrefPath(org_name, app_name), SDL_free);

    const ResumeAudioThread resume_thread = [&state](SceUID thread_id) {
        const ThreadStatePtr thread = lock_and_find(thread_id, state.kernel.threads, state.kernel.mutex);
        const std::unique_lock<std::mutex> lock(thread->mutex);
        assert(thread->to_do == ThreadToDo::wait);
        thread->to_do = ThreadToDo::run;
        thread->something_to_do.notify_all();
    };

    state.base_path = base_path.get();
    state.pref_path = pref_path.get();

    state.window = WindowPtr(SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 544, SDL_WINDOW_OPENGL), SDL_DestroyWindow);
    if (!state.window || !init(state.mem) || !init(state.audio, resume_thread) || !init(state.io, pref_path.get())) {
        return false;
    }

    state.kernel.base_tick = { rtc_base_ticks() };

    return true;
}

bool handle_events(HostState &host) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            stop_all_threads(host.kernel);
            return false;
        }
    }

    return true;
}

void call_import(HostState &host, uint32_t nid, SceUID thread_id) {
    if (LOG_IMPORT_CALLS) {
        const char *const name = import_name(nid);
		LOG_TRACE("NID {:#08x} ({})) called", nid, name);
    }

    ImportFn *const fn = resolve_import(nid);
    if (fn != nullptr) {
        (*fn)(host, thread_id);
    }
}
