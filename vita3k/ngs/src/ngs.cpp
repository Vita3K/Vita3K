#include <ngs/system.h>
#include <ngs/state.h>
#include <ngs/modules/atrac9.h>
#include <ngs/modules/master.h>
#include <ngs/modules/player.h>
#include <mem/mem.h>

#include <util/log.h>

namespace emu::ngs {
    Rack::Rack(System *mama, const Ptr<void> memspace, const std::uint32_t memspace_size)
        : MempoolObject(memspace, memspace_size)
        , system(mama) {

    }

    System::System(const Ptr<void> memspace, const std::uint32_t memspace_size)
        : MempoolObject(memspace, memspace_size)
        , max_voices(0)
        , granularity(0)
        , sample_rate(0) {

    }

    void VoiceInputManager::init(const std::uint16_t total_input) {
        inputs.resize(total_input);
    }

    VoiceInputManager::PCMBuf *VoiceInputManager::get_input_buffer_queue(const std::int32_t index, const std::int32_t subindex) {
        if (index >= inputs.size()) {
            return nullptr;
        }

        if (subindex >= inputs[index].bufs.size() || subindex < -1) {
            return nullptr;
        }

        return &inputs[index].bufs[subindex];
    }

    std::int32_t VoiceInputManager::receive(const std::int32_t index, const std::int32_t subindex, std::uint8_t **data,
        const std::int16_t channel_count, const std::size_t size_each) {
        if (index >= inputs.size()) {
            return -1;
        }

        std::int32_t dest_subindex = subindex;

        if (subindex > 32) {
            return -1;
        }

        if (subindex == -1) {
            // Find a free subindex
            for (std::int8_t i = 0; i < 32; i++) {
                if (!(inputs[index].occupied & (1 << i))) {
                    dest_subindex = i;
                    inputs[index].occupied |= (1 << i);
                    break;
                }
            }
        }

        if (dest_subindex == -1) {
            LOG_ERROR("Out of input audio slot for this voice!");
            return -1;
        }

        if (dest_subindex >= inputs[index].bufs.size()) {
            inputs[index].bufs.resize(dest_subindex + 1);
        }

        inputs[index].bufs[dest_subindex].insert(inputs[index].bufs[dest_subindex].end(), *data,
            *data + size_each * channel_count);

        return dest_subindex;
    }

    bool VoiceInputManager::free_input(const std::int32_t index, const std::int32_t subindex) {
        if (index >= inputs.size()) {
            return false;
        }

        if (subindex > 32 || subindex < 0) {
            return false;
        }

        inputs[index].occupied &= ~(1 << subindex);
        return true;
    }
    
    void Voice::init(Rack *mama) {
        rack = mama;
        state = VoiceState::VOICE_STATE_AVAILABLE;
        flags = 0;
        frame_count = 0;

        outputs.resize(mama->patches_per_output);
        inputs.init(1);
    }

    BufferParamsInfo *Voice::lock_params(const MemState &mem) {
        // Save a copy of previous set of data
        if (flags & PARAMS_LOCK) {
            return nullptr;
        }

        last_info.resize(info.size);
        const std::uint8_t *current_data = info.data.cast<const std::uint8_t>().get(mem);
        std::copy(current_data, current_data + info.size, &last_info[0]);

        flags |= PARAMS_LOCK;

        return &info;
    }
    
    bool Voice::unlock_params() {
        if (flags & PARAMS_LOCK) {
            flags &= ~PARAMS_LOCK;

            // Reset the state, empty them out
            voice_state_data.clear();
            return true;
        }

        return false;
    }
    
    Ptr<Patch> Voice::patch(const MemState &mem, const std::int32_t index, std::int32_t subindex, std::int32_t dest_index, Voice *dest) {
        // Look if another patch has already been there
        if (subindex == -1) {
            for (std::int32_t i = 0; i < outputs.size(); i++) {
                if (!outputs[i] || outputs[i].get(mem)->output_sub_index == -1) {
                    subindex = i;
                    break;
                }
            }
        }

        if (subindex >= outputs.size()) {
            return {};
        }

        if (outputs[subindex] && outputs[subindex].get(mem)->output_sub_index != -1) {
            return {};
        }

        if (!outputs[subindex]) {
            outputs[subindex] = rack->alloc_and_init<Patch>(mem);
        }

        Patch *patch = outputs[subindex].get(mem);

        patch->output_sub_index = subindex;
        patch->output_index = index;
        patch->dest_index = dest_index;
        patch->dest = dest;
        patch->dest_sub_index = -1;
        
        // Set default value
        patch->dest_data_type = AudioDataType::S16;
        patch->dest_channels = 2;

        dest->rack->module->get_expectation(&patch->dest_data_type, &patch->dest_channels);

        return outputs[subindex];
    }

    std::uint32_t System::get_required_memspace_size(SystemInitParameters *parameters) {
        return sizeof(System);
    }

    std::uint32_t Rack::get_required_memspace_size(MemState &mem, RackDescription *description) {
        return sizeof(emu::ngs::Rack) + description->voice_count * sizeof(emu::ngs::Voice) +
            description->definition.get(mem)->get_buffer_parameter_size() * description->voice_count +
            description->patches_per_output * sizeof(emu::ngs::Patch);
    }
    
    bool init(State &ngs, MemState &mem) {
        static constexpr std::uint32_t SIZE_OF_VOICE_DEFS = (TOTAL_BUSS_TYPES - 1) * sizeof(VoiceDefinition)
            + sizeof(emu::ngs::atrac9::Module);
        static constexpr std::uint32_t SIZE_OF_GLOBAL_MEMSPACE = SIZE_OF_VOICE_DEFS;

        // Alloc the space for voice definition
        ngs.memspace = alloc(mem, SIZE_OF_GLOBAL_MEMSPACE, "NGS voice definitions");

        if (!ngs.memspace) {
            LOG_ERROR("Can't alloc global memspace for NGS!");
            return false;
        }

        ngs.allocator.init(SIZE_OF_GLOBAL_MEMSPACE);

        ngs.definitions[emu::ngs::BUSS_ATRAC9] = ngs.alloc_and_init<emu::ngs::atrac9::VoiceDefinition>(mem);
        ngs.definitions[emu::ngs::BUSS_NORMAL_PLAYER] = ngs.alloc_and_init<emu::ngs::player::VoiceDefinition>(mem);
        ngs.definitions[emu::ngs::BUSS_MASTER] = ngs.alloc_and_init<emu::ngs::master::VoiceDefinition>(mem);

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

        rack->module = description->definition.get(mem)->new_module();

        // Initialize voice definition
        rack->channels_per_voice = description->channels_per_voice;
        rack->max_patches_per_input = description->max_patches_per_input;
        rack->patches_per_output = description->patches_per_output;

        // Alloc spaces for voice
        rack->voices.resize(description->voice_count);

        for (auto &voice: rack->voices) {
            voice = rack->alloc<Voice>();

            if (!voice) {
                return false;
            }

            Voice *v = voice.get(mem);
            v->init(rack);

            // Allocate parameter buffer info for each voice
            v->info.size = description->definition.get(mem)->get_buffer_parameter_size();

            if (v->info.size != 0) {
                v->info.data = rack->alloc_raw(v->info.size);
            } else {
                v->info.data = 0;
            }
        }

        system->racks.push_back(rack);

        return true;
    }
}