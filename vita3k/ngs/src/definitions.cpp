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

#include <ngs/definitions.h>
#include <ngs/modules.h>

#include <util/log.h>

namespace ngs {
void Atrac9VoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    mods.push_back(std::make_unique<Atrac9Module>());
    mods.push_back(nullptr); // Not yet known
    mods.push_back(nullptr); // Seems to be mixer?
    mods.push_back(std::make_unique<EqualizerModule>());
    for (int i = 0; i < 6; i++) {
        mods.push_back(nullptr);
    }
}

uint32_t Atrac9VoiceDefinition::get_total_buffer_parameter_size() const {
    return sizeof(SceNgsAT9Params) + default_normal_parameter_size * 9;
}

void MasterVoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    mods.push_back(std::make_unique<NullModule>());
    mods.push_back(std::make_unique<MasterModule>());
}

uint32_t MasterVoiceDefinition::get_total_buffer_parameter_size() const {
    return 0;
}

void PassthroughVoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    mods.push_back(std::make_unique<PassthroughModule>());
    mods.push_back(std::make_unique<PassthroughModule>());
    mods.push_back(std::make_unique<PassthroughModule>());
    mods.push_back(std::make_unique<PassthroughModule>());
}

uint32_t PassthroughVoiceDefinition::get_total_buffer_parameter_size() const {
    return default_passthrough_parameter_size * 4;
}

void PlayerVoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    mods.push_back(std::make_unique<PlayerModule>());
    mods.push_back(nullptr); // Not yet known
    mods.push_back(nullptr); // Seems to be mixer?
    mods.push_back(std::make_unique<EqualizerModule>());

    for (int i = 0; i < 6; i++) {
        mods.push_back(nullptr);
    }
}

uint32_t PlayerVoiceDefinition::get_total_buffer_parameter_size() const {
    return sizeof(SceNgsPlayerParams) + default_normal_parameter_size * 9;
}

void ScreamPlayerVoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    LOG_INFO("Scream player voice definition stubbed");

    // the module name were obtained from uncharted error strings
    // I guess the scream modules are the same as the usual ones
    mods.push_back(std::make_unique<PlayerModule>());
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_ENVELOPE
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_DISTORTION
    mods.push_back(std::make_unique<EqualizerModule>()); // SCE_NGS_SCREAM_VOICE_EQ
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_SEND_1_FILTER
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_SEND_2_FILTER
}

uint32_t ScreamPlayerVoiceDefinition::get_total_buffer_parameter_size() const {
    return sizeof(SceNgsPlayerParams) + default_normal_parameter_size * 5;
}

void ScreamAtrac9VoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    LOG_INFO("Scream Atrac9 voice definition stubbed");

    mods.push_back(std::make_unique<Atrac9Module>());
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_ENVELOPE
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_DISTORTION
    mods.push_back(std::make_unique<EqualizerModule>()); // SCE_NGS_SCREAM_VOICE_EQ
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_SEND_1_FILTER
    mods.push_back(nullptr); // SCE_NGS_SCREAM_VOICE_SEND_2_FILTER
}

uint32_t ScreamAtrac9VoiceDefinition::get_total_buffer_parameter_size() const {
    return sizeof(SceNgsAT9Params) + default_normal_parameter_size * 5;
}

void SimplePlayerVoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    LOG_TRACE("Simple player voice definition stubbed");

    mods.push_back(std::make_unique<PlayerModule>());
    mods.push_back(std::make_unique<EqualizerModule>());
    mods.push_back(nullptr);
    mods.push_back(nullptr);
    mods.push_back(nullptr);
    mods.push_back(nullptr);
}

uint32_t SimplePlayerVoiceDefinition::get_total_buffer_parameter_size() const {
    return sizeof(SceNgsPlayerParams) + default_normal_parameter_size * 5;
}

void SimpleAtrac9VoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    LOG_TRACE("AT9 player voice definition stubbed");

    mods.push_back(std::make_unique<Atrac9Module>());
    mods.push_back(std::make_unique<EqualizerModule>());
    mods.push_back(nullptr);
    mods.push_back(nullptr);
    mods.push_back(nullptr);
    mods.push_back(nullptr);
}

uint32_t SimpleAtrac9VoiceDefinition::get_total_buffer_parameter_size() const {
    return sizeof(SceNgsAT9Params) + default_normal_parameter_size * 5;
}
} // namespace ngs