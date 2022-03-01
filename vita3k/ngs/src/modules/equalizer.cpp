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

#include <ngs/modules/equalizer.h>
#include <util/log.h>

namespace ngs::equalizer {
Module::Module()
    : ngs::Module(ngs::BussType::BUSS_EQUALIZATION) {}

std::size_t Module::get_buffer_parameter_size() const {
    return default_normal_parameter_size;
}

bool Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) {
    // TODO: Proper implement it, for now just lower volume lol
    float *product_before = reinterpret_cast<float *>(data.parent->products[0].data);

    if (product_before) {
        for (std::int32_t i = 0; i < data.parent->rack->system->granularity * 2; i++) {
            product_before[i] *= 0.5f;
        }
    }

    // It should do some modifications to create 4 outputs, but I'm not sure what yet kkk
    data.parent->products[1] = data.parent->products[0];
    data.parent->products[2] = data.parent->products[0];
    data.parent->products[3] = data.parent->products[0];

    return false;
}
} // namespace ngs::equalizer
