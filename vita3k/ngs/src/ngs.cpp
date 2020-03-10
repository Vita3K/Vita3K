#include <ngs/system.h>
#include <ngs/state.h>
#include <ngs/modules/atrac9.h>
#include <ngs/modules/master.h>
#include <ngs/modules/player.h>
#include <ngs/modules/reverb.h>
#include <mem/mem.h>

#include <util/log.h>

namespace ngs {
    Rack::Rack(System *mama, const Ptr<void> memspace, const std::uint32_t memspace_size)
        : MempoolObject(memspace, memspace_size),
        system(mama) { }

    System::System(const Ptr<void> memspace, const std::uint32_t memspace_size)
        : MempoolObject(memspace, memspace_size)
        , max_voices(0)
        , granularity(0)
        , sample_rate(0) { }

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

    std::int32_t VoiceInputManager::receive(const std::int32_t index, const std::int32_t subindex, const std::uint8_t **data,
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

        if (subindex > 32) {
            return false;
        }

        if (subindex < 0) {
            return true;
        }

        inputs[index].occupied &= ~(1 << subindex);
        return true;
    }

    VoiceInputManager::PCMSubInputs::Iterator::Iterator(PCMSubInputs *inputs, const std::int32_t dest_subindex) 
        : inputs(inputs)
        , dest_subindex(dest_subindex) {

    }

    VoiceInputManager::PCMSubInputs::Iterator VoiceInputManager::PCMSubInputs::Iterator::operator++() {
        do {
            dest_subindex++;
        } while (dest_subindex < inputs->bufs.size() && !(inputs->occupied & (1 << dest_subindex)));

        return *this;
    }

    bool VoiceInputManager::PCMSubInputs::Iterator::operator !=(const VoiceInputManager::PCMSubInputs::Iterator &rhs) const {
        return (dest_subindex != rhs.dest_subindex);
    }

    VoiceInputManager::PCMBuf &VoiceInputManager::PCMSubInputs::Iterator::operator *() {
        return inputs->bufs[dest_subindex];
    }
    
    VoiceInputManager::PCMSubInputs::Iterator VoiceInputManager::PCMSubInputs::begin() {
        std::int32_t subindex = static_cast<std::int32_t>(bufs.size());

        for (std::int32_t i = 0; i < static_cast<std::int32_t>(bufs.size()); i++) {
            if (occupied & (1 << i)) {
                subindex = i;
                break;
            }
        }

        return Iterator(this, subindex);
    }
    
    VoiceInputManager::PCMSubInputs::Iterator VoiceInputManager::PCMSubInputs::end() {
        return Iterator(this, static_cast<std::int32_t>(bufs.size()));
    }

    void Voice::init(Rack *mama) {
        rack = mama;
        state = VoiceState::VOICE_STATE_AVAILABLE;
        flags = 0;
        frame_count = 0;

        outputs.resize(mama->patches_per_output);
        inputs.init(1);
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
        patch->source = this;
        
        // Set default value
        patch->dest_data_type = AudioDataType::S16;
        patch->dest_channels = 2;

        dest->rack->module->get_expectation(&patch->dest_data_type, &patch->dest_channels);

        return outputs[subindex];
    }

    bool Voice::remove_patch(const MemState &mem, const Ptr<Patch> patch) {
        const std::lock_guard<std::mutex> guard(*voice_lock);
        auto iterator = std::find(outputs.begin(), outputs.end(), patch);

        if (iterator == outputs.end()) {
            return false;
        }

        // Try to unroute. Free the destination subindex
        Patch *patch_info = patch.get(mem);

        if (!patch_info) {
            return false;
        }

        patch_info->dest->inputs.free_input(patch_info->dest_index, patch_info->dest_sub_index);
        patch_info->output_sub_index = -1;

        return true;
    }

    std::uint32_t System::get_required_memspace_size(SystemInitParameters *parameters) {
        return sizeof(System);
    }

    std::uint32_t Rack::get_required_memspace_size(MemState &mem, RackDescription *description) {
        uint32_t buffer_size = 0;
        if (description->definition)
            buffer_size = description->definition.get(mem)->get_buffer_parameter_size() * description->voice_count;

        return sizeof(ngs::Rack) + description->voice_count * sizeof(ngs::Voice) +
            buffer_size + description->patches_per_output * sizeof(ngs::Patch);
    }
    
    bool init(State &ngs, MemState &mem) {
        // this looks strange... maybe catch in review?
        static constexpr std::uint32_t SIZE_OF_VOICE_DEFS = sizeof(ngs::atrac9::Module);
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

        for (auto &voice: rack->voices) {
            voice = rack->alloc<Voice>();

            if (!voice) {
                return false;
            }

            Voice *v = voice.get(mem);
            v->init(rack);

            // Allocate parameter buffer info for each voice
            if (description->definition)
                v->info.size = description->definition.get(mem)->get_buffer_parameter_size();
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

    Ptr<VoiceDefinition> get_voice_definition(State &ngs, MemState &mem, ngs::BussType type) {
        switch (type) {
            case ngs::BussType::BUSS_ATRAC9:
                return ngs.alloc_and_init<ngs::atrac9::VoiceDefinition>(mem);
            case ngs::BussType::BUSS_NORMAL_PLAYER:
                return ngs.alloc_and_init<ngs::player::VoiceDefinition>(mem);
            case ngs::BussType::BUSS_MASTER:
                return ngs.alloc_and_init<ngs::master::VoiceDefinition>(mem);
            case ngs::BussType::BUSS_REVERB:
                return ngs.alloc_and_init<ngs::reverb::VoiceDefinition>(mem);

            default:
                LOG_ERROR("Missing voice definition for Buss Type {}.", static_cast<uint32_t>(type));
                return Ptr<VoiceDefinition>();
        }
    }
}