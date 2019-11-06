#pragma once

#include <cstdint>
#include <mem/ptr.h>

#include <ngs/scheduler.h>
#include <ngs/mempool.h>
#include <ngs/common.h>

struct MemState;

namespace emu::ngs {
    struct State;
    
    struct ModuleParameterHeader {
        std::int32_t module_id;
        std::int32_t channel;
    };

    struct VoiceDefinition {
        BussType buss_type;

        explicit VoiceDefinition() = default;
        explicit VoiceDefinition(BussType buss_type) : buss_type(buss_type) {
        }

        virtual void process(const MemState &mem, Voice *voice) {

        }

        virtual std::size_t get_buffer_parameter_size() const {
            return 0;
        }
    };

    struct SystemInitParameters {
        std::int32_t max_racks;
        std::int32_t max_voices;
        std::int32_t granularity;
        std::int32_t sample_rate;
        std::int32_t unk16;
    };

    struct Voice;

    struct PatchSetupInfo {
        Ptr<Voice> source;
        std::int32_t source_output_index;
        std::int32_t source_output_subindex;
        Ptr<Voice> dest;
        std::int32_t dest_input_index;
    };

    struct Patch {
        std::int32_t output_index;
        std::int32_t output_sub_index;
        Voice *dest;
    };

    struct VoiceDefinition;

    struct RackDescription {
        Ptr<VoiceDefinition> definition;
        std::int32_t voice_count;
        std::int32_t channels_per_voice;
        std::int32_t max_patches_per_input;
        std::int32_t patches_per_output;
        Ptr<void> unk14;
    };

    struct BufferParamsInfo {
        Ptr<void> data;
        std::uint32_t size;
    };

    static_assert(sizeof(BufferParamsInfo) == 8);

    struct Rack;

    enum VoiceState {
        VOICE_STATE_AVAILABLE = 1 << 0,
        VOICE_STATE_ACTIVE = 1 << 1,
        VOICE_STATE_FINALIZING = 1 << 2,
        VOICE_STATE_UNLOADING = 1 << 3,
        VOICE_STATE_PENDING = 1 << 4,
        VOICE_STATE_PAUSED = 1 << 5,
        VOICE_STATE_KEY_OFF = 1 << 6
    };

    struct Voice {
        Rack *rack;

        BufferParamsInfo info;
        std::vector<std::uint8_t> last_info;
        VoiceState state;

        enum Flags {
            PARAMS_LOCK = 1 << 0,
        };

        std::uint8_t flags;

        std::vector<Ptr<Patch>> outputs;

        void init(Rack *mama);

        BufferParamsInfo *lock_params(const MemState &mem);
        bool unlock_params();

        Ptr<Patch> patch(const MemState &mem, const std::int32_t index, std::int32_t subindex, Voice *dest);

        template <typename T>
        T *get_parameters(const MemState &mem) {
            if (flags & PARAMS_LOCK) {
                // Use our previous set of data
                return reinterpret_cast<T*>(&last_info[0]);
            }

            return info.data.cast<T>().get(mem);
        }
    };
    
    struct System;

    struct Rack: public MempoolObject {
        System *system;
        VoiceDefinition *definition;
        std::int32_t channels_per_voice;
        std::int32_t max_patches_per_input;
        std::int32_t patches_per_output;

        std::vector<Ptr<Voice>> voices;

        explicit Rack(System *mama, const Ptr<void> memspace, const std::uint32_t memspace_size);

        static std::uint32_t get_required_memspace_size(MemState &mem, RackDescription *description);
    };

    struct System: public MempoolObject {
        std::vector<Rack*> racks;
        std::int32_t max_voices;
        std::int32_t granularity;
        std::int32_t sample_rate;

        VoiceScheduler voice_scheduler;

        explicit System(const Ptr<void> memspace, const std::uint32_t memspace_size);
        static std::uint32_t get_required_memspace_size(SystemInitParameters *parameters);
    };
    
    bool init_system(State &ngs, const MemState &mem, SystemInitParameters *parameters, Ptr<void> memspace, const std::uint32_t memspace_size);
    bool init_rack(State &ngs, const MemState &mem, System *system, BufferParamsInfo *init_info, const RackDescription *description);
}