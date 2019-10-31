#include <ngs/state.h>
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

    std::uint32_t System::get_required_memspace_size(SystemInitParameters *parameters) {
        return sizeof(System);
    }

    std::uint32_t Rack::get_required_memspace_size(RackDescription *description) {
        return sizeof(emu::ngs::Rack) + description->voice_count * sizeof(emu::ngs::Voice);
    }
    
    bool init(State &ngs, MemState &mem) {
        static constexpr std::uint32_t SIZE_OF_VOICE_DEFS = TOTAL_BUSS_TYPES * sizeof(VoiceDefinition);
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
            ngs.definitions[i] = ngs.alloc<VoiceDefinition>();

            if (!ngs.definitions[i]) {
                return false;
            }

            VoiceDefinition *definition = ngs.definitions[i].get(mem);
            definition->buss_type = static_cast<BussType>(i);
        }

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

        // Alloc spaces for voice
        rack->voices.resize(description->voice_count);

        for (auto &voice: rack->voices) {
            voice = rack->alloc<Voice>();

            if (!voice) {
                return false;
            }

            Voice *v = voice.get(mem);
            v->init(rack);
        }

        // Initialize voice definition
        rack->definition = description->definition.get(mem);
        rack->channels_per_voice = description->channels_per_voice;
        rack->max_patches_per_input = description->max_patches_per_input;
        rack->patches_per_output = description->patches_per_output;

        system->racks.push_back(rack);

        return true;
    }
}