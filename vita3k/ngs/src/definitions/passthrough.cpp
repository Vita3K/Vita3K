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

#include <ngs/definitions/passthrough.h>
#include <ngs/modules/passthrough.h>

namespace ngs::passthrough {
void VoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    mods.push_back(std::make_unique<Module>());
    mods.push_back(std::make_unique<Module>());
    mods.push_back(std::make_unique<Module>());
    mods.push_back(std::make_unique<Module>());
}

std::size_t VoiceDefinition::get_total_buffer_parameter_size() const {
    return default_passthrough_parameter_size * 4;
}
} // namespace ngs::passthrough
