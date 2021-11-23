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

#include <ngs/definitions/scream.h>
#include <ngs/modules/atrac9.h>
#include <ngs/modules/equalizer.h>
#include <ngs/modules/player.h>
#include <util/log.h>

namespace ngs::scream {
void PlayerVoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    LOG_INFO("Scream player voice definition stubbed");

    // the module name were obtained from uncharted error strings
    // I guess the scream modules are the same as the usual ones
    mods.push_back(std::make_unique<ngs::player::Module>());
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_ENVELOPE
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_DISTORTION
    mods.push_back(std::make_unique<ngs::equalizer::Module>()); // SCE_NGS_SCREAM_VOICE_EQ
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_SEND_1_FILTER
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_SEND_2_FILTER
}

std::size_t PlayerVoiceDefinition::get_total_buffer_parameter_size() const {
    return sizeof(ngs::player::Parameters) + default_normal_parameter_size * 5;
}

void Atrac9VoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    LOG_INFO("Scream Atrac9 voice definition stubbed");

    mods.push_back(std::make_unique<ngs::atrac9::Module>());
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_ENVELOPE
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_DISTORTION
    mods.push_back(std::make_unique<ngs::equalizer::Module>()); // SCE_NGS_SCREAM_VOICE_EQ
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_SEND_1_FILTER
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_SEND_2_FILTER
}

std::size_t Atrac9VoiceDefinition::get_total_buffer_parameter_size() const {
    return sizeof(ngs::atrac9::Parameters) + default_normal_parameter_size * 5;
}
} // namespace ngs::scream
