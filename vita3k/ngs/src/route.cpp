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

#include <ngs/system.h>

#include <codec/state.h>
#include <util/log.h>

namespace ngs {
bool deliver_data(const MemState &mem, Voice *source, const std::uint8_t output_port,
    const VoiceProduct &data_to_deliver) {
    if (!data_to_deliver.data) {
        return false;
    }

    for (std::size_t i = 0; i < source->patches[output_port].size(); i++) {
        Patch *patch = source->patches[output_port][i].get(mem);

        if (!patch || patch->output_sub_index == -1) {
            continue;
        }

        if (data_to_deliver.data != nullptr) {
            const std::lock_guard<std::mutex> guard(*patch->dest->voice_lock);
            patch->dest->inputs.receive(patch, data_to_deliver);
        }
    }

    return true;
}
} // namespace ngs