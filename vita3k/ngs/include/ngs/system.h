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

#pragma once

#include <mem/mempool.h>
#include <mem/ptr.h>
#include <ngs/common.h>
#include <ngs/scheduler.h>
#include <ngs/types.h>
#include <util/types.h>

#include <array>
#include <cstdint>
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

enum VoiceState {
    VOICE_STATE_AVAILABLE,
    VOICE_STATE_ACTIVE,
    VOICE_STATE_FINALIZING,
    VOICE_STATE_UNLOADING,
};

struct Patch {
    int32_t output_index;
    int32_t output_sub_index;
    int32_t dest_index;
    ngs::Voice *dest;
    ngs::Voice *source;

    float volume_matrix[2][2];
};

struct ModuleData {
    Voice *parent;
    uint32_t index;

    Ptr<void> callback;
    Ptr<void> user_data;

    bool is_bypassed;

    std::vector<uint8_t> voice_state_data; ///< Voice state.
    std::vector<uint8_t> extra_storage; ///< Local data storage for module.

    SceNgsBufferInfo info;
    std::vector<uint8_t> last_info;

    enum Flags {
        PARAMS_LOCK = 1 << 0,
    };

    uint8_t flags;

    explicit ModuleData();

    template <typename T>
    T *get_state() {
        if (voice_state_data.empty()) {
            voice_state_data.resize(sizeof(T));
            new (&voice_state_data[0]) T();
        }

        return reinterpret_cast<T *>(&voice_state_data[0]);
    }

    template <typename T>
    T *get_parameters(const MemState &mem) {
        if (flags & PARAMS_LOCK) {
            // Use our previous set of data
            return reinterpret_cast<T *>(&last_info[0]);
        }

        return info.data.cast<T>().get(mem);
    }

    void fill_to_fit_granularity();

    void invoke_callback(KernelState &kern, const MemState &mem, const SceUID thread_id, const uint32_t reason1,
        const uint32_t reason2, Address reason_ptr);

    SceNgsBufferInfo *lock_params(const MemState &mem);
    bool unlock_params(const MemState &mem);
};

class Module {
public:
    virtual ~Module() = default;

    virtual void set_default_preset(const MemState &mem, ModuleData &data) {}
    virtual bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data, std::unique_lock<std::recursive_mutex> &scheduler_lock, std::unique_lock<std::mutex> &voice_lock) = 0;
    virtual uint32_t module_id() const { return 0; }
    virtual uint32_t get_buffer_parameter_size() const = 0;
    virtual void on_state_change(const MemState &mem, ModuleData &v, const VoiceState previous) {}
    virtual void on_param_change(const MemState &mem, ModuleData &data) {}
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
    int32_t receive(Patch *patch, const VoiceProduct &data);
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

    static uint32_t get_required_memspace_size(MemState &mem, SceNgsRackDescription *description);
};

struct System : public MempoolObject {
    std::vector<Rack *> racks;
    int32_t max_voices;
    int32_t granularity;
    int32_t sample_rate;

    VoiceScheduler voice_scheduler;

    explicit System(const Ptr<void> memspace, const uint32_t memspace_size);
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
