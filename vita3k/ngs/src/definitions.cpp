// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <ngs/state.h>

#include <ngs/modules/atrac9.h>
#include <ngs/modules/compressor.h>
#include <ngs/modules/delay.h>
#include <ngs/modules/distortion.h>
#include <ngs/modules/envelope.h>
#include <ngs/modules/equalizer.h>
#include <ngs/modules/filter.h>
#include <ngs/modules/generator.h>
#include <ngs/modules/mixer.h>
#include <ngs/modules/output.h>
#include <ngs/modules/pauser.h>
#include <ngs/modules/pitchshift.h>
#include <ngs/modules/player.h>
#include <ngs/modules/reverb.h>

#include <util/log.h>

namespace ngs {

template <typename... Args>
static std::enable_if_t<sizeof...(Args) == 0> push_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    // Nothing to do.
}

template <typename Head, typename... Tail>
static void push_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    // Add the argument at the head of the list.
    mods.push_back(std::make_unique<Head>());
    push_modules<Tail...>(mods);
}

template <typename... Args>
constexpr static std::enable_if_t<sizeof...(Args) == 0, uint32_t> get_modules_size() {
    return 0;
}

template <typename Head, typename... Tail>
constexpr static uint32_t get_modules_size() {
    // Add the argument at the head of the list.
    constexpr uint32_t param_size = Head::get_max_parameter_size();
    return param_size + get_modules_size<Tail...>();
}

template <typename... Args>
static void apply_definition_with_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    push_modules<Args...>(mods);
}

template <typename... Args>
static uint32_t apply_definition_with_modules(uint32_t) {
    constexpr uint32_t result = get_modules_size<Args...>();
    return result;
}

template <typename Arg, typename Ret>
static Ret apply_definition(BussType type, Arg &arg) {
    switch (type) {
    case BussType::BUSS_MASTER:
        return apply_definition_with_modules<
            InputMixerModule,
            OutputModule>(arg);
    case BussType::BUSS_COMPRESSOR:
        return apply_definition_with_modules<
            InputMixerModule,
            CompressorModule>(arg);
    case BussType::BUSS_SIDE_CHAIN_COMPRESSOR:
        return apply_definition_with_modules<
            InputMixerModule,
            InputMixerModule,
            CompressorModule>(arg);
    case BussType::BUSS_DELAY:
        return apply_definition_with_modules<
            InputMixerModule,
            DelayModule>(arg);
    case BussType::BUSS_DISTORTION:
        return apply_definition_with_modules<
            InputMixerModule,
            DistortionModule>(arg);
    case BussType::BUSS_ENVELOPE:
        return apply_definition_with_modules<
            InputMixerModule,
            EnvelopeModule>(arg);
    case BussType::BUSS_EQUALIZATION:
        return apply_definition_with_modules<
            InputMixerModule,
            EqualizerModule>(arg);
    case BussType::BUSS_MIXER:
        return apply_definition_with_modules<
            InputMixerModule>(arg);
    case BussType::BUSS_PAUSER:
        return apply_definition_with_modules<
            InputMixerModule,
            PauserModule>(arg);
    case BussType::BUSS_PITCH_SHIFT:
        return apply_definition_with_modules<
            InputMixerModule,
            PitchShiftModule>(arg);
    case BussType::BUSS_REVERB:
        return apply_definition_with_modules<
            InputMixerModule,
            ReverbModule>(arg);
    case BussType::BUSS_SAS_EMULATION:
        return apply_definition_with_modules<
            PlayerModule,
            EnvelopeModule>(arg);
    case BussType::BUSS_SIMPLE:
        return apply_definition_with_modules<
            PlayerModule,
            EqualizerModule,
            EnvelopeModule,
            PauserModule,
            FilterModule,
            FilterModule>(arg);
    case BussType::BUSS_ATRAC9:
        return apply_definition_with_modules<
            Atrac9Module,
            GeneratorModule,
            MixerModule,
            EqualizerModule,
            EnvelopeModule,
            DistortionModule,
            EqualizerModule,
            EqualizerModule,
            EqualizerModule,
            EqualizerModule>(arg);
    case BussType::BUSS_SIMPLE_ATRAC9:
        return apply_definition_with_modules<
            Atrac9Module,
            EqualizerModule,
            EnvelopeModule,
            PauserModule,
            FilterModule,
            FilterModule>(arg);
    case BussType::BUSS_SCREAM:
        // the module name were obtained from uncharted error strings
        // I guess the scream modules are the same as the usual ones
        return apply_definition_with_modules<
            PlayerModule,
            EnvelopeModule, // SCE_NGS_SCREAM_VOICE_ENVELOPE
            DistortionModule, // SCE_NGS_SCREAM_VOICE_DISTORTION
            EqualizerModule, // SCE_NGS_SCREAM_VOICE_EQ
            FilterModule, // SCE_NGS_SCREAM_VOICE_SEND_1_FILTER
            FilterModule>(arg); // SCE_NGS_SCREAM_VOICE_SEND_2_FILTER
    case BussType::BUSS_SCREAM_ATRAC9:
        return apply_definition_with_modules<
            Atrac9Module,
            EnvelopeModule, // SCE_NGS_SCREAM_VOICE_ENVELOPE
            DistortionModule, // SCE_NGS_SCREAM_VOICE_DISTORTION
            EqualizerModule, // SCE_NGS_SCREAM_VOICE_EQ
            FilterModule, // SCE_NGS_SCREAM_VOICE_SEND_1_FILTER
            FilterModule>(arg); // SCE_NGS_SCREAM_VOICE_SEND_2_FILTER
    case BussType::BUSS_NORMAL_PLAYER:
        return apply_definition_with_modules<
            PlayerModule,
            GeneratorModule,
            MixerModule,
            EqualizerModule,
            EnvelopeModule,
            DistortionModule,
            EqualizerModule,
            EqualizerModule,
            EqualizerModule,
            EqualizerModule>(arg);
        break;
    default:
        LOG_ERROR("Unkown buss type {}", static_cast<int>(type));
        return apply_definition_with_modules<>(arg);
    }
}

void apply_voice_definition(VoiceDefinition *definition, std::vector<std::unique_ptr<ngs::Module>> &mods) {
    apply_definition<std::vector<std::unique_ptr<ngs::Module>>, void>(definition->type, mods);
}

uint32_t get_voice_definition_size(VoiceDefinition *definition) {
    uint32_t temp;
    return apply_definition<uint32_t, uint32_t>(definition->type, temp);
}

void voice_definition_init(State &ngs, MemState &mem) {
    uint32_t voice_output_counts[static_cast<int>(BussType::BUSS_MAX)];
    // most of them have only 1 output
    std::fill_n(voice_output_counts, static_cast<int>(BussType::BUSS_MAX), 1);

    voice_output_counts[static_cast<int>(BussType::BUSS_SIMPLE)] = 2;
    voice_output_counts[static_cast<int>(BussType::BUSS_SIMPLE_ATRAC9)] = 2;
    voice_output_counts[static_cast<int>(BussType::BUSS_SCREAM)] = 2;
    voice_output_counts[static_cast<int>(BussType::BUSS_SCREAM_ATRAC9)] = 2;
    voice_output_counts[static_cast<int>(BussType::BUSS_ATRAC9)] = 4;
    voice_output_counts[static_cast<int>(BussType::BUSS_NORMAL_PLAYER)] = 4;

    ngs.definitions = Ptr<VoiceDefinition>(alloc(mem, static_cast<int>(BussType::BUSS_MAX) * sizeof(VoiceDefinition), "NGS voice definitions"));
    VoiceDefinition *definitions = ngs.definitions.get(mem);

    for (int i = 0; i < static_cast<int>(BussType::BUSS_MAX); i++) {
        definitions[i].type = static_cast<BussType>(i);
        definitions[i].output_count = voice_output_counts[i];
    }
}

} // namespace ngs
