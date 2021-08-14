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

#include <ngs/modules/passthrough.h>
#include <util/log.h>

namespace ngs::passthrough {
Module::Module()
    : ngs::Module(ngs::BussType::BUSS_REVERB) {}

std::size_t Module::get_buffer_parameter_size() const {
    return default_passthrough_parameter_size;
}

bool Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) {
    if (data.parent->inputs.inputs.empty()) {
        return false;
    }

    assert(data.parent->inputs.inputs.size() == 1);
    data.parent->products[0].data = data.parent->inputs.inputs[0].data();
    return false;
}
} // namespace ngs::passthrough
