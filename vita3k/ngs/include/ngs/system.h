#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <mutex>

#include <mem/ptr.h>

#include <ngs/scheduler.h>
#include <ngs/mempool.h>
#include <ngs/common.h>

struct MemState;

namespace ngs {
    struct State;
    
    struct ModuleParameterHeader {
        std::int32_t module_id;
        std::int32_t channel;
    };

    enum AudioDataType {
        S16 = 0,
        F32 = 1
    };

    struct Module {
        BussType buss_type;

        explicit Module(BussType buss_type) : buss_type(buss_type) { }
        virtual ~Module() = default;

        virtual void process(const MemState &mem, Voice *voice) = 0;
        virtual void get_expectation(AudioDataType *expect_audio_type, std::int16_t *expect_channel_count) = 0;
    };

    struct VoiceDefinition {
        virtual std::unique_ptr<Module> new_module() = 0;
        virtual std::size_t get_buffer_parameter_size() const = 0;
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
        std::int32_t dest_index;
        std::int16_t output_channels;
        std::int16_t dest_channels;
        AudioDataType dest_data_type;
        Voice *dest;
        Voice *source;

        float volume_matrix[2][2];
    };

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

    struct VoiceInputManager {
        using PCMInput = std::vector<std::uint8_t>;
        using PCMInputs = std::vector<PCMInput>;

        PCMInputs inputs;

        void init(const std::uint32_t granularity, const std::uint16_t total_input);
        void reset_inputs();

        PCMInput *get_input_buffer_queue(const std::int32_t index);
        std::int32_t receive(Patch *patch, const std::uint8_t **data);
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
        std::uint32_t frame_count;

        using Patches = std::vector<Ptr<Patch>>;

        std::array<Patches, MAX_OUTPUT_PORT> patches;
        std::vector<std::uint8_t> voice_state_data;         ///< Voice state.
        std::vector<std::uint8_t> extra_voice_data;         ///< Local data storage for module.

        VoiceInputManager inputs;
        std::unique_ptr<std::mutex> voice_lock;

        void init(Rack *mama);

        BufferParamsInfo *lock_params(const MemState &mem);
        bool unlock_params();

        bool remove_patch(const MemState &mem, const Ptr<Patch> patch);
        Ptr<Patch> patch(const MemState &mem, const std::int32_t index, std::int32_t subindex, std::int32_t dest_index, Voice *dest);
        
        template <typename T>
        T *get_state() {
            if (voice_state_data.size() == 0) {
                voice_state_data.resize(sizeof(T));
            }

            return reinterpret_cast<T*>(&voice_state_data[0]);
        }

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
        std::unique_ptr<Module> module;
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

    bool deliver_data(const MemState &mem, Voice *source, const std::uint8_t output_port,
        const std::uint8_t *data_to_deliver);

    bool init_system(State &ngs, const MemState &mem, SystemInitParameters *parameters, Ptr<void> memspace, const std::uint32_t memspace_size);
    bool init_rack(State &ngs, const MemState &mem, System *system, BufferParamsInfo *init_info, const RackDescription *description);

    Ptr<VoiceDefinition> get_voice_definition(State &ngs, MemState &mem, ngs::BussType type);
}