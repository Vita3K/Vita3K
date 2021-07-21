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

#include <ngs/modules/master.h>
#include <util/log.h>

#include <algorithm>
#include <fstream>

namespace ngs::master {
Module::Module()
    : ngs::Module(ngs::BussType::BUSS_MASTER) {
}

bool Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) {
    // Merge all voices. This buss manually outputs 2 channels
    if (data.voice_state_data.empty()) {
        data.voice_state_data.resize(data.parent->rack->system->granularity * sizeof(std::uint16_t) * 2);
    }

    std::fill(data.voice_state_data.begin(), data.voice_state_data.end(), 0);

    if (data.parent->inputs.inputs.empty()) {
        return false;
    }

    std::int16_t *dest_data = reinterpret_cast<std::int16_t *>(data.voice_state_data.data());
    float *source_data = reinterpret_cast<float *>(data.parent->inputs.inputs[0].data());

    // Convert FLTP to S16
    for (std::uint32_t i = 0; i < data.parent->rack->system->granularity * 2; i++) {
        dest_data[i] = static_cast<std::int16_t>(std::clamp(source_data[i] * 32768.0f, -32768.0f, 32767.0f));
    }

    return false;
}
} // namespace ngs::master
