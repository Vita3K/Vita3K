#include <mem/mem.h>
#include <ngs/modules/atrac9.h>
#include <ngs/modules/master.h>
#include <ngs/modules/passthrough.h>
#include <ngs/modules/player.h>
#include <ngs/state.h>
#include <ngs/system.h>

#include <util/log.h>

namespace ngs {
Rack::Rack(System *mama, const Ptr<void> memspace, const std::uint32_t memspace_size)
    : MempoolObject(memspace, memspace_size)
    , system(mama) {}

System::System(const Ptr<void> memspace, const std::uint32_t memspace_size)
    : MempoolObject(memspace, memspace_size)
    , max_voices(0)
    , granularity(0)
    , sample_rate(0) {}

void VoiceInputManager::init(const std::uint32_t granularity, const std::uint16_t total_input) {
    inputs.resize(total_input);

    for (auto &input : inputs) {
        // PCM16 and maximum channel count
        input.resize(granularity * 4);
    }

    reset_inputs();
}

void VoiceInputManager::reset_inputs() {
    for (auto &input : inputs) {
        std::fill(input.begin(), input.end(), 0);
    }
}

VoiceInputManager::PCMInput *VoiceInputManager::get_input_buffer_queue(const std::int32_t index) {
    if (index >= inputs.size()) {
        return nullptr;
    }

    return &inputs[index];
}

std::int32_t VoiceInputManager::receive(ngs::Patch *patch, const std::uint8_t **data) {
    PCMInput *input = get_input_buffer_queue(patch->dest_index);

    if (!input) {
        return -1;
    }

    std::int16_t *dest_buffer = reinterpret_cast<std::int16_t *>(input->data());
    const std::int16_t *data_to_mix_in = reinterpret_cast<const std::int16_t *>(*data);

    // Try mixing, also with the use of this volume matrix
    // Dest is our voice to receive this data.
    for (std::int32_t k = 0; k < patch->dest->rack->system->granularity; k++) {
        // General mixing case, pretty straight foward
        for (std::uint8_t i = 0; i < patch->dest_channels; i++) {
            std::int32_t sample_to_be_mixed = static_cast<std::int32_t>(data_to_mix_in[k * patch->output_channels + i]
                * patch->volume_matrix[i][i]);

            if (patch->output_channels != patch->dest_channels) {
                if ((patch->output_channels == 2) && (patch->dest_channels == 1)) {
                    sample_to_be_mixed = static_cast<std::int32_t>((sample_to_be_mixed + data_to_mix_in[k * 2 + 1] * patch->volume_matrix[1][0]) / 2);
                } else {
                    sample_to_be_mixed = static_cast<std::int32_t>(data_to_mix_in[k] * patch->volume_matrix[0][i]);
                }
            }

            std::int32_t mixed_sample = dest_buffer[k * patch->dest_channels + i] + sample_to_be_mixed;

            if (mixed_sample > 32767)
                mixed_sample = 32767;

            if (mixed_sample < -32768)
                mixed_sample = -32768;

            dest_buffer[k * patch->dest_channels + i] = mixed_sample;
        }
    }

    return 0;
}

void Voice::init(Rack *mama) {
    rack = mama;
    state = VoiceState::VOICE_STATE_AVAILABLE;
    flags = 0;
    frame_count = 0;

    for (std::uint32_t i = 0; i < MAX_OUTPUT_PORT; i++)
        patches[i].resize(mama->patches_per_output);

    inputs.init(rack->system->granularity, 1);
    voice_lock = std::make_unique<std::mutex>();
}

BufferParamsInfo *Voice::lock_params(const MemState &mem) {
    const std::lock_guard<std::mutex> guard(*voice_lock);

    // Save a copy of previous set of data
    if (flags & PARAMS_LOCK) {
        return nullptr;
    }

    if (info.data) {
        const std::uint8_t *current_data = info.data.cast<const std::uint8_t>().get(mem);
        last_info.resize(info.size);
        std::copy(current_data, current_data + info.size, last_info.data());
    }

    flags |= PARAMS_LOCK;

    return &info;
}

bool Voice::unlock_params() {
    const std::lock_guard<std::mutex> guard(*voice_lock);

    if (flags & PARAMS_LOCK) {
        flags &= ~PARAMS_LOCK;

        // Reset the state, empty them out
        voice_state_data.clear();
        return true;
    }

    return false;
}

Ptr<Patch> Voice::patch(const MemState &mem, const std::int32_t index, std::int32_t subindex, std::int32_t dest_index, Voice *dest) {
    const std::lock_guard<std::mutex> guard(*voice_lock);

    if (index >= MAX_OUTPUT_PORT) {
        // We don't have enough port for you!
        return {};
    }

    // Look if another patch has already been there
    if (subindex == -1) {
        for (std::int32_t i = 0; i < patches[index].size(); i++) {
            if (!patches[index][i] || (patches[index])[i].get(mem)->output_sub_index == -1) {
                subindex = i;
                break;
            }
        }
    }

    if (subindex >= patches[index].size()) {
        return {};
    }

    if (patches[index][subindex] && patches[index][subindex].get(mem)->output_sub_index != -1) {
        // You just hit an occupied subindex! You won't get to eat, stay in detention.
        return {};
    }

    if (!patches[index][subindex]) {
        // Create the patch incase it's not yet existed
        patches[index][subindex] = rack->alloc_and_init<Patch>(mem);
    }

    Patch *patch = patches[index][subindex].get(mem);

    patch->output_sub_index = subindex;
    patch->output_index = index;
    patch->dest_index = dest_index;
    patch->dest = dest;
    patch->source = this;

    // Default output channel count
    patch->output_channels = 2;

    AudioDataType source_data_type;
    rack->module->get_expectation(&source_data_type, &patch->output_channels);

    // Set default value
    patch->dest_data_type = AudioDataType::S16;
    patch->dest_channels = 2;

    dest->rack->module->get_expectation(&patch->dest_data_type, &patch->dest_channels);

    // Initialize the matrix
    patch->volume_matrix[0][1] = 1.0f;
    patch->volume_matrix[0][0] = 1.0f;
    patch->volume_matrix[1][0] = 1.0f;
    patch->volume_matrix[1][1] = 1.0f;

    return patches[index][subindex];
}

bool Voice::remove_patch(const MemState &mem, const Ptr<Patch> patch) {
    const std::lock_guard<std::mutex> guard(*voice_lock);
    for (std::uint8_t i = 0; i < patches.size(); i++) {
        auto iterator = std::find(patches[i].begin(), patches[i].end(), patch);

        if (iterator == patches[i].end()) {
            return false;
        }
    }

    // Try to unroute. Free the destination index
    Patch *patch_info = patch.get(mem);

    if (!patch_info) {
        return false;
    }

    patch_info->output_sub_index = -1;

    return true;
}

std::uint32_t System::get_required_memspace_size(SystemInitParameters *parameters) {
    return sizeof(System);
}

std::uint32_t Rack::get_required_memspace_size(MemState &mem, RackDescription *description) {
    uint32_t buffer_size = 0;
    if (description->definition)
        buffer_size = static_cast<std::uint32_t>(description->definition.get(mem)->get_buffer_parameter_size() * description->voice_count);

    return sizeof(ngs::Rack) + description->voice_count * sizeof(ngs::Voice) + buffer_size + description->patches_per_output * MAX_OUTPUT_PORT * description->voice_count * sizeof(ngs::Patch);
}

bool init(State &ngs, MemState &mem) {
    static constexpr std::uint32_t SIZE_OF_VOICE_DEFS = sizeof(ngs::atrac9::VoiceDefinition) * 50;
    static constexpr std::uint32_t SIZE_OF_GLOBAL_MEMSPACE = SIZE_OF_VOICE_DEFS;

    // Alloc the space for voice definition
    ngs.memspace = alloc(mem, SIZE_OF_GLOBAL_MEMSPACE, "NGS voice definitions");

    if (!ngs.memspace) {
        LOG_ERROR("Can't alloc global memspace for NGS!");
        return false;
    }

    ngs.allocator.init(SIZE_OF_GLOBAL_MEMSPACE);

    return true;
}

bool init_system(State &ngs, const MemState &mem, SystemInitParameters *parameters, Ptr<void> memspace, const std::uint32_t memspace_size) {
    // Reserve first memory allocation for our System struct
    System *sys = memspace.cast<System>().get(mem);
    sys = new (sys) System(memspace, memspace_size);

    sys->racks.resize(parameters->max_racks);

    sys->max_voices = parameters->max_voices;
    sys->granularity = parameters->granularity;
    sys->sample_rate = parameters->sample_rate;

    // Alloc first block for System struct
    if (!sys->alloc_raw(sizeof(System))) {
        return false;
    }

    ngs.systems.push_back(sys);
    return true;
}

bool init_rack(State &ngs, const MemState &mem, System *system, BufferParamsInfo *init_info, const RackDescription *description) {
    Rack *rack = init_info->data.cast<Rack>().get(mem);
    rack = new (rack) Rack(system, init_info->data, init_info->size);

    // Alloc first block for Rack
    if (!rack->alloc<Rack>()) {
        return false;
    }

    if (description->definition)
        rack->module = description->definition.get(mem)->new_module();
    else
        rack->module = nullptr;

    // Initialize voice definition
    rack->channels_per_voice = description->channels_per_voice;
    rack->max_patches_per_input = description->max_patches_per_input;
    rack->patches_per_output = description->patches_per_output;

    // Alloc spaces for voice
    rack->voices.resize(description->voice_count);

    for (auto &voice : rack->voices) {
        voice = rack->alloc<Voice>();

        if (!voice) {
            return false;
        }

        Voice *v = voice.get(mem);
        v->init(rack);

        // Allocate parameter buffer info for each voice
        if (description->definition)
            v->info.size = static_cast<std::uint32_t>(description->definition.get(mem)->get_buffer_parameter_size());
        else
            v->info.size = 0;

        if (v->info.size != 0) {
            v->info.data = rack->alloc_raw(v->info.size);
        } else {
            v->info.data = 0;
        }
    }

    system->racks.push_back(rack);

    return true;
}

Ptr<VoiceDefinition> create_voice_definition(State &ngs, MemState &mem, ngs::BussType type) {
    switch (type) {
    case ngs::BussType::BUSS_ATRAC9:
        return ngs.alloc_and_init<ngs::atrac9::VoiceDefinition>(mem);
    case ngs::BussType::BUSS_NORMAL_PLAYER:
        return ngs.alloc_and_init<ngs::player::VoiceDefinition>(mem);
    case ngs::BussType::BUSS_MASTER:
        return ngs.alloc_and_init<ngs::master::VoiceDefinition>(mem);

    default:
        LOG_WARN("Missing voice definition for Buss Type {}, using passthrough.", static_cast<uint32_t>(type));
        return ngs.alloc_and_init<ngs::passthrough::VoiceDefinition>(mem);
    }
}

Ptr<VoiceDefinition> get_voice_definition(State &ngs, MemState &mem, ngs::BussType type) {
    if (ngs.definitions.find(type) == ngs.definitions.end()) {
        ngs.definitions[type] = create_voice_definition(ngs, mem, type);
    }

    return ngs.definitions[type];
}
} // namespace ngs
