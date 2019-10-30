#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <mem/ptr.h>

struct MemState;

namespace emu::ngs {
    struct SystemInitParameters {
        std::int32_t max_racks;
        std::int32_t max_voices;
        std::int32_t granularity;
        std::int32_t sample_rate;
        std::int32_t unk16;
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

    struct System;

    enum BussType {
        BUSS_MASTER = 0,
        BUSS_COMPRESSOR = 1,
        BUSS_SIDE_CHAIN_COMPRESSOR = 2,
        BUSS_DELAY = 3,
        BUSS_DISTORTION = 4,
        BUSS_ENVELOPE = 5,
        BUSS_EQUALIZATION = 6,
        BUSS_MIXER = 7,
        BUSS_PAUSER = 8,
        BUSS_PITCH_SHIFT = 9,
        BUSS_REVERB = 10,
        BUSS_SAS_EMULATION = 11,
        BUSS_SIMPLE = 12,
        BUSS_ATRAC9 = 13,
        BUSS_SIMPLE_ATRAC9 = 14,
        BUSS_SCREAM = 15,
        BUSS_SCREAM_ATRAC9 = 16,
        BUSS_NORMAL_PLAYER = 17,
        BUSS_TOTAL
    };

    static constexpr std::uint32_t TOTAL_BUSS_TYPES = BUSS_TOTAL;

    struct VoiceDefinition {
        BussType buss_type;
    };

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
        Rack *parent;

        BufferParamsInfo info;
        std::vector<std::uint8_t> last_info;
        VoiceState state;

        enum Flags {
            PARAMS_LOCK = 1 << 0,
        };

        std::uint8_t flags;

        void init(Rack *mama);
    };

    struct MemspaceBlockAllocator {
        struct Block {
            std::uint32_t size;
            std::uint32_t offset;
            bool free;
        };

        std::vector<Block> blocks;      /// All block sorted by size

        explicit MemspaceBlockAllocator() = default;
        explicit MemspaceBlockAllocator(const std::uint32_t memspace_size);
        
        void init(const std::uint32_t memspace_size);

        std::uint32_t alloc(const std::uint32_t size);
        bool free(const std::uint32_t offset);
    };

    struct MempoolObject {
        Ptr<void> memspace;
        MemspaceBlockAllocator allocator;

        explicit MempoolObject(const Ptr<void> memspace, const std::uint32_t memspace_size);
        explicit MempoolObject() = default;

        Ptr<void> alloc_raw(const std::uint32_t size);
        bool free_raw(const Ptr<void> ptr);

        template <typename T>
        Ptr<T> alloc() {
            return alloc_raw(sizeof(T)).cast<T>();
        }

        template <typename T>
        bool free(const Ptr<T> ptr) {
            return free_raw(ptr.cast<void>());
        }
    };

    struct System;

    struct Rack: public MempoolObject {
        System *mama;
        VoiceDefinition *definition;
        std::int32_t channels_per_voice;
        std::int32_t max_patches_per_input;
        std::int32_t patches_per_output;

        std::vector<Ptr<Voice>> voices;

        explicit Rack(System *mama, const Ptr<void> memspace, const std::uint32_t memspace_size);

        static std::uint32_t get_required_memspace_size(RackDescription *description);
    };

    struct System: public MempoolObject {
        std::vector<Rack*> racks;
        std::int32_t max_voices;
        std::int32_t granularity;
        std::int32_t sample_rate;

        explicit System(const Ptr<void> memspace, const std::uint32_t memspace_size);
        static std::uint32_t get_required_memspace_size(SystemInitParameters *parameters);
    };

    struct State: public MempoolObject {
        std::array<Ptr<VoiceDefinition>, TOTAL_BUSS_TYPES> definitions;
        std::vector<System*> systems;
    };

    bool init(State &ngs, MemState &mem);
    bool init_system(State &ngs, const MemState &mem, SystemInitParameters *parameters, Ptr<void> memspace, const std::uint32_t memspace_size);
    bool init_rack(State &ngs, const MemState &mem, System *system, BufferParamsInfo *init_info, const RackDescription *description);
}