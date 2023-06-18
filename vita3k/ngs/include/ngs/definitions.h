// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

namespace ngs {
struct Atrac9VoiceDefinition : public VoiceDefinition {
    void new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) override;
    uint32_t get_total_buffer_parameter_size() const override;
    uint32_t output_count() const override { return 4; }
};

struct MasterVoiceDefinition : public VoiceDefinition {
    void new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) override;
    uint32_t get_total_buffer_parameter_size() const override;
    uint32_t output_count() const override { return 1; }
};

struct PassthroughVoiceDefinition : public VoiceDefinition {
    void new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) override;
    uint32_t get_total_buffer_parameter_size() const override;
    uint32_t output_count() const override { return 4; }
};

struct PlayerVoiceDefinition : public VoiceDefinition {
    void new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) override;
    uint32_t get_total_buffer_parameter_size() const override;
    uint32_t output_count() const override { return 1; }
};

struct ScreamPlayerVoiceDefinition : public VoiceDefinition {
    void new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) override;
    uint32_t get_total_buffer_parameter_size() const override;
    uint32_t output_count() const override { return 4; }
};

struct ScreamAtrac9VoiceDefinition : public VoiceDefinition {
    void new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) override;
    uint32_t get_total_buffer_parameter_size() const override;
    uint32_t output_count() const override { return 4; }
};

struct SimplePlayerVoiceDefinition : public VoiceDefinition {
    void new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) override;
    uint32_t get_total_buffer_parameter_size() const override;
    uint32_t output_count() const override { return 4; }
};

struct SimpleAtrac9VoiceDefinition : public VoiceDefinition {
    void new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) override;
    uint32_t get_total_buffer_parameter_size() const override;
    std::uint32_t output_count() const override { return 4; }
};
} // namespace ngs
