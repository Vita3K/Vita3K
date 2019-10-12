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

#include "private.h"

#include <host/functions.h>
#include <host/pup_types.h>
#include <host/state.h>
#include <util/log.h>

#include <nfd.h>

namespace gui {

void draw_install_firmware_dialog(GuiState &gui, HostState &host) {
    nfdchar_t *outpath = nullptr;
    nfdresult_t result = NFD_OpenDialog("PUP", nullptr, &outpath);

    if (result == NFD_OKAY) {
        install_pup(outpath, host.pref_path + "/PUP_DEC");
        get_modules_list(gui, host);
        gui.file_menu.install_firmware_dialog = false;
    } else if (result == NFD_CANCEL) {
        gui.file_menu.install_firmware_dialog = false;
    } else {
        LOG_ERROR("Error initializing file dialog: {}", NFD_GetError());
    }
}
} // namespace gui
