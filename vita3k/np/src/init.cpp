// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <np/common.h>
#include <np/functions.h>
#include <np/state.h>

#include <io/functions.h>

bool init(NpState &state, const np::CommunicationID *comm_id) {
    state.inited = true;

    if (comm_id) {
        state.comm_id = *comm_id;
    }

    return true;
}

bool deinit(NpState &state) {
    deinit(state.trophy_state);

    state.cbs.clear();
    state.state_cb_id = 0;
    state.comm_id = {};
    state.inited = false;
    return true;
}

bool init(NpTrophyState &state) {
    state.inited = true;
    return true;
}

bool deinit(NpTrophyState &state) {
    for (auto &ctx : state.contexts) {
        if (ctx.valid && ctx.io)
            close_file(*ctx.io, ctx.trophy_file_stream, "np_deinit");
    }
    state.contexts.clear();
    state.clear_trophy_unlock_callbacks();
    state.inited = false;
    return true;
}
