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

#include "vpk.h"
#include "sfo.h"
#include "pkg.h"

#include <host/functions.h>
#include <host/state.h>
#include <host/version.h>
#include <kernel/thread_functions.h>
#include <util/string_convert.h>
#include <util/log.h>
#include <io/vfs.h>

#include <SDL.h>

#include <algorithm> // find_if_not
#include <cassert>
#include <iostream>

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

typedef std::unique_ptr<const void, void (*)(const void *)> SDLPtr;

enum ExitCode {
    Success = 0,
    IncorrectArgs,
    SDLInitFailed,
    HostInitFailed,
    ModuleLoadFailed,
    InitThreadFailed,
    RunThreadFailed
};

enum VitaFileType {
	Vpk = 0,
	Pkg = 1
};

static bool is_macos_process_arg(const char *arg) {
    return strncmp(arg, "-psn_", 5) == 0;
}

static void error(const std::string& message, SDL_Window *window = nullptr) {
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message.c_str(), window) < 0) {
        LOG_ERROR("SDL Error: {}", message);
    }
}

static void term_sdl(const void *succeeded) {
    assert(succeeded != nullptr);

    SDL_Quit();
}

VitaFileType file_type(const char* path) {
	FILE* temp = fopen(path, "rb");

	int magic;

	fread(&magic, sizeof(int), 1, temp);

	if (magic == 0x474b507f) {
		return Pkg;
	}

	return Vpk;
}

int main(int argc, char *argv[]) {
	init_logging();

	LOG_INFO("{}", window_title);    

    ProgramArgsWide argv_wide = process_args(argc, argv);

    const SDLPtr sdl(reinterpret_cast<const void *>(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_VIDEO) >= 0), term_sdl);
    if (!sdl) {
        error("SDL initialisation failed.");
        return SDLInitFailed;
    }

    const char *const *const path_arg = std::find_if_not(&argv[1], &argv[argc], is_macos_process_arg);
    std::wstring path;
    if (path_arg != &argv[argc]) {
        path = utf_to_wide(*path_arg);
    } else {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_DROPFILE) {
                path = utf_to_wide(ev.drop.file);
                SDL_free(ev.drop.file);
                break;
            }
        }
    }

	path = L"E:\\Netflix.pkg";

    if (path.empty()) {
        std::string message = "Usage: ";
        message += argv[0];
        message += " <path to VPK file>";
        error(message);
        return IncorrectArgs;
    }

    HostState host;
    if (!init(host)) {
        error("Host initialisation failed.", host.window.get());
        return HostInitFailed;
    }

    vfs::mount("ux0", fs::absolute(host.pref_path + "ux0").string());

    Ptr<const void> entry_point;

	VitaFileType type = file_type(wide_to_utf(path).c_str());

	if (type == Vpk) {
		if (!load_vpk(entry_point, host.io, host.mem, host.game_title, host.title_id, path)) {
			std::string message = "Failed to load \"";
			message += wide_to_utf(path);
			message += "\"";
			message += "\nSee console output for details.";
			error(message.c_str(), host.window.get());
			return ModuleLoadFailed;
		}
		else {
			load_pkg(entry_point, host.io, host.mem, host.game_title, host.title_id, path);
			return 0;
		}
	}
    // TODO This is hacky. Belongs in kernel?
    const SceUID main_thread_id = host.kernel.next_uid++;

    const CallImport call_import = [&host, main_thread_id](uint32_t nid) {
        ::call_import(host, nid, main_thread_id);
    };

    const size_t stack_size = MB(1); // TODO Get main thread stack size from somewhere?
    const bool log_code = false;
    const ThreadStatePtr main_thread = init_thread(entry_point, stack_size, log_code, host.mem, call_import);
    if (!main_thread) {
        error("Failed to init main thread.", host.window.get());
        return InitThreadFailed;
    }

    // TODO Move this to kernel.
    host.kernel.threads.emplace(main_thread_id, main_thread);

    host.t1 = SDL_GetTicks();
    if (!run_thread(*main_thread)) {
        error("Failed to run main thread.", host.window.get());
        return RunThreadFailed;
    }

    return Success;
}
