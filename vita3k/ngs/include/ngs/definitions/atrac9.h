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

#pragma once

#include <ngs/system.h>

namespace ngs::atrac9 {
struct VoiceDefinition : public ngs::VoiceDefinition {
    void new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) override;
    std::size_t get_total_buffer_parameter_size() const override;
    std::uint32_t output_count() const override { return 4; }
};
} // namespace ngs::atrac9
