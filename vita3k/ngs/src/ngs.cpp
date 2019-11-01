#include <ngs/system.h>
#include <ngs/state.h>
#include <ngs/modules/atrac9.h>
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

    void Voice::init(Rack *mama) {
        rack = mama;
        state = VoiceState::VOICE_STATE_AVAILABLE;
        flags = 0;
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
            return true;
        }

        return false;
    }

    std::uint32_t System::get_required_memspace_size(SystemInitParameters *parameters) {
        return sizeof(System);
    }

    std::uint32_t Rack::get_required_memspace_size(MemState &mem, RackDescription *description) {
        return sizeof(emu::ngs::Rack) + description->voice_count * sizeof(emu::ngs::Voice) +
            description->definition.get(mem)->get_buffer_parameter_size() * description->voice_count;
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

        // Create voice definition templates.
        for (std::size_t i = 0; i < ngs.definitions.size(); i++) {
            if (i == emu::ngs::BUSS_ATRAC9 || i == emu::ngs::BUSS_NORMAL_PLAYER) {
                continue;
            }

            ngs.definitions[i] = ngs.alloc_and_init<VoiceDefinition>(mem, static_cast<BussType>(i));

            if (!ngs.definitions[i]) {
                return false;
            }
        }

        ngs.definitions[emu::ngs::BUSS_ATRAC9] = ngs.alloc_and_init<emu::ngs::atrac9::Module>(mem);
        ngs.definitions[emu::ngs::BUSS_NORMAL_PLAYER] = ngs.alloc_and_init<emu::ngs::player::Module>(mem);

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

        rack->definition = description->definition.get(mem);

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
            v->info.size = rack->definition->get_buffer_parameter_size();

            if (v->info.size != 0) {
                v->info.data = rack->alloc_raw(v->info.size);
            } else {
                v->info.data = 0;
            }
        }

        // Initialize voice definition
        rack->channels_per_voice = description->channels_per_voice;
        rack->max_patches_per_input = description->max_patches_per_input;
        rack->patches_per_output = description->patches_per_output;

        system->racks.push_back(rack);

        return true;
    }
}