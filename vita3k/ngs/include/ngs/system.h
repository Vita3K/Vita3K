// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#pragma once

#include <mem/mempool.h>
#include <mem/ptr.h>
#include <ngs/common.h>
#include <ngs/scheduler.h>
#include <ngs/types.h>
#include <util/types.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

struct MemState;
struct KernelState;

namespace ngs {
// random number of bytes to make sure nothing bad happens
constexpr uint32_t default_passthrough_parameter_size = 140;
constexpr uint32_t default_normal_parameter_size = 100;

struct State;
struct Voice;
struct System;

enum VoiceState {
    VOICE_STATE_AVAILABLE,
    VOICE_STATE_ACTIVE,
    VOICE_STATE_FINALIZING,
    VOICE_STATE_UNLOADING,
};

struct VoiceAddress {
    int32_t rack_index = -1;
    int32_t voice_index = -1;

    explicit operator bool() const {
        return rack_index >= 0 && voice_index >= 0;
    }
};

struct Patch {
    int32_t output_index;
    int32_t output_sub_index;
    int32_t dest_index;
    // runtime-only caches
    ngs::System *system;
    ngs::Voice *dest;
    ngs::Voice *source;

    VoiceAddress dest_address;
    VoiceAddress source_address;

    float volume_matrix[2][2];

    bool is_active() const;
    Voice *resolve_source(const MemState &mem) const;
    Voice *resolve_dest(const MemState &mem) const;
    void refresh_endpoints(const MemState &mem);
};

struct ModuleLogicalState {
    virtual ~ModuleLogicalState() = default;
};

struct ModuleRuntimeState {
    virtual ~ModuleRuntimeState() = default;
};

struct PCMFrameQueue {
    std::vector<float> samples;
    uint32_t read_offset_frames = 0;

    void clear() {
        samples.clear();
        read_offset_frames = 0;
    }

    bool empty() const {
        return available_frames() == 0;
    }

    uint32_t total_frames() const {
        return static_cast<uint32_t>(samples.size() / 2);
    }

    uint32_t available_frames() const {
        const uint32_t total = total_frames();
        return (read_offset_frames >= total) ? 0 : (total - read_offset_frames);
    }

    void compact() {
        if (read_offset_frames == 0) {
            return;
        }

        if (read_offset_frames >= total_frames()) {
            clear();
            return;
        }

        samples.erase(samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(read_offset_frames) * 2);
        read_offset_frames = 0;
    }

    uint8_t *append_uninitialized_bytes(const uint32_t frames) {
        const size_t old_samples = samples.size();
        samples.resize(old_samples + static_cast<size_t>(frames) * 2);
        return reinterpret_cast<uint8_t *>(samples.data() + old_samples);
    }

    void append_bytes(const uint8_t *source, const uint32_t frames) {
        if (frames == 0) {
            return;
        }

        uint8_t *dest = append_uninitialized_bytes(frames);
        std::memcpy(dest, source, static_cast<size_t>(frames) * sizeof(float) * 2);
    }

    void append_frame(const float left, const float right) {
        samples.push_back(left);
        samples.push_back(right);
    }

    uint8_t *read_bytes() {
        if (samples.empty()) {
            return nullptr;
        }

        return reinterpret_cast<uint8_t *>(samples.data() + static_cast<size_t>(read_offset_frames) * 2);
    }

    void ensure_available_frames(const uint32_t frames) {
        const size_t required_samples = static_cast<size_t>(read_offset_frames + frames) * 2;
        if (samples.size() < required_samples) {
            samples.resize(required_samples, 0.0f);
        }
    }

    void consume_frames(const uint32_t frames) {
        read_offset_frames += std::min(frames, available_frames());
    }
};

struct ModuleData {
    Voice *parent;
    uint32_t index;

    Ptr<void> callback;
    Ptr<void> user_data;

    bool is_bypassed;

    std::vector<uint8_t> guest_state_data; ///< guest-visible voice state
    std::vector<uint8_t> scratch_data; ///< temp local data storage for module
    std::unique_ptr<ModuleLogicalState> logical_state; ///< non-guest state needed to resume processing later
    std::unique_ptr<ModuleRuntimeState> runtime_state; ///< host runtime objects rebuilt from logical state

    SceNgsBufferInfo info;
    std::vector<uint8_t> last_info;

    enum Flags {
        PARAMS_LOCK = 1 << 0,
    };

    uint8_t flags;

    explicit ModuleData();

    template <typename T>
    T *get_state() {
        if (guest_state_data.empty()) {
            guest_state_data.resize(sizeof(T));
            new (&guest_state_data[0]) T();
        }

        return reinterpret_cast<T *>(&guest_state_data[0]);
    }

    template <typename T>
    T *get_logical_state() {
        if (!logical_state) {
            logical_state = std::make_unique<T>();
        }

        return static_cast<T *>(logical_state.get());
    }

    template <typename T>
    T *get_runtime_state() {
        if (!runtime_state) {
            runtime_state = std::make_unique<T>();
        }

        return static_cast<T *>(runtime_state.get());
    }

    template <typename T>
    T *get_parameters(const MemState &mem) {
        if (flags & PARAMS_LOCK) {
            // Use our previous set of data
            return reinterpret_cast<T *>(&last_info[0]);
        }

        return info.data.cast<T>().get(mem);
    }

    void invoke_callback(KernelState &kern, const MemState &mem, const SceUID thread_id, const uint32_t reason1,
        const uint32_t reason2, Address reason_ptr);

    SceNgsBufferInfo *lock_params(const MemState &mem);
    bool unlock_params(const MemState &mem);

    void ensure_scratch_size(const size_t size) {
        if (scratch_data.size() < size) {
            scratch_data.resize(size);
        }
    }
};

class Module {
public:
    virtual ~Module() = default;

    virtual void set_default_preset(const MemState &mem, ModuleData &data) {}
    virtual bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) = 0;
    virtual uint32_t module_id() const { return 0; }
    virtual uint32_t get_buffer_parameter_size() const = 0;
    virtual uint32_t get_guest_state_size() const { return 0; }
    virtual std::unique_ptr<ModuleLogicalState> create_logical_state() const { return nullptr; }
    virtual std::unique_ptr<ModuleRuntimeState> create_runtime_state() const { return nullptr; }
    virtual void on_state_change(const MemState &mem, ModuleData &v, const VoiceState previous) {}
    virtual void on_param_change(const MemState &mem, ModuleData &data) {}
    virtual void cleanup_voice_state(ModuleData &data) {}

    virtual void initialize_voice_data(ModuleData &data) const {
        const uint32_t guest_state_size = get_guest_state_size();
        if (guest_state_size != 0) {
            data.guest_state_data.assign(guest_state_size, 0);
        } else {
            data.guest_state_data.clear();
        }

        data.scratch_data.clear();
        data.logical_state = create_logical_state();
        data.runtime_state = create_runtime_state();
    }
};

static constexpr uint32_t MAX_VOICE_OUTPUT = 4;

struct VoiceDefinition {
    BussType type;
    uint32_t output_count;
};

struct VoiceProduct {
    uint8_t reserved[2];
    uint8_t *data;
};

struct VoiceInputManager {
    using PCMInput = std::vector<uint8_t>;
    using PCMInputs = std::vector<PCMInput>;

    PCMInputs inputs;

    void init(const uint32_t granularity, const uint16_t total_input);
    void reset_inputs();

    PCMInput *get_input_buffer_queue(const int32_t index);
    int32_t receive(const MemState &mem, Patch *patch, const VoiceProduct &data);
};

struct Voice {
    Rack *rack;

    std::vector<ModuleData> datas;
    VoiceState state;
    bool is_pending;
    bool is_paused;
    bool is_keyed_off;
    uint32_t frame_count;

    using Patches = std::vector<Ptr<Patch>>;

    std::array<Patches, MAX_OUTPUT_PORT> patches;

    VoiceInputManager inputs;

    std::unique_ptr<std::mutex> voice_mutex;
    VoiceProduct products[MAX_VOICE_OUTPUT];

    Ptr<void> finished_callback;
    Ptr<void> finished_callback_user_data;

    void init(Rack *mama);

    ModuleData *module_storage(const uint32_t index);

    bool remove_patch(const MemState &mem, const Ptr<Patch> patch);
    Ptr<Patch> patch(const MemState &mem, const int32_t index, int32_t subindex, int32_t dest_index, Voice *dest);

    void transition(const MemState &mem, const VoiceState new_state);
    bool parse_params(const MemState &mem, const SceNgsModuleParamHeader *header);
    // Return the number of errors that happened
    SceInt32 parse_params_block(const MemState &mem, const SceNgsModuleParamHeader *header, const SceUInt32 size);
    bool set_preset(const MemState &mem, const SceNgsVoicePreset *preset);

    void invoke_callback(KernelState &kernel, const MemState &mem, const SceUID thread_id, Ptr<void> callback, Ptr<void> user_data,
        const uint32_t module_id, const uint32_t reason = 0, const uint32_t reason2 = 0, Address reason_ptr = 0);

    VoiceAddress address(const MemState &mem) const;
};

struct System;

struct Rack : public MempoolObject {
    System *system;
    VoiceDefinition *vdef;

    int32_t channels_per_voice;
    int32_t max_patches_per_input;
    int32_t patches_per_output;

    std::vector<Ptr<Voice>> voices;
    std::vector<std::unique_ptr<Module>> modules;

    explicit Rack(System *mama, const Ptr<void> memspace, const uint32_t memspace_size);

    int32_t index_of_voice(const MemState &mem, const Voice *voice) const;

    static uint32_t get_required_memspace_size(MemState &mem, SceNgsRackDescription *description);
};

struct System : public MempoolObject {
    std::vector<Rack *> racks;
    int32_t max_voices;
    int32_t granularity;
    int32_t sample_rate;

    VoiceScheduler voice_scheduler;

    explicit System(const Ptr<void> memspace, const uint32_t memspace_size);
    int32_t index_of_rack(const Rack *rack) const;
    VoiceAddress address_of(const MemState &mem, const Voice *voice) const;
    Voice *find_voice(const MemState &mem, const VoiceAddress &address) const;
    static uint32_t get_required_memspace_size(SceNgsSystemInitParams *parameters);
};

bool deliver_data(const MemState &mem, const std::vector<Voice *> &voice_queue, Voice *source, const uint8_t output_port,
    const VoiceProduct &data_to_deliver);

bool init_system(State &ngs, const MemState &mem, SceNgsSystemInitParams *parameters, Ptr<void> memspace, const uint32_t memspace_size);
void release_system(State &ngs, const MemState &mem, System *system);
bool init_rack(State &ngs, const MemState &mem, System *system, SceNgsBufferInfo *init_info, const SceNgsRackDescription *description);
void release_rack(State &ngs, const MemState &mem, System *system, Rack *rack);

void voice_definition_init(State &ngs, MemState &mem);
Ptr<VoiceDefinition> get_voice_definition(State &ngs, MemState &mem, ngs::BussType type);
void apply_voice_definition(VoiceDefinition *definition, std::vector<std::unique_ptr<ngs::Module>> &mods);
uint32_t get_voice_definition_size(VoiceDefinition *definition);
} // namespace ngs
