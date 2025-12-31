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

#include <cpu/functions.h>
#include <kernel/state.h>

#include <ngs/state.h>
#include <ngs/system.h>
#include <util/lock_and_find.h>

#include <util/log.h>
#include <util/vector_utils.h>

namespace ngs {
Rack::Rack(System *mama, const Ptr<void> memspace, const uint32_t memspace_size)
    : MempoolObject(memspace, memspace_size)
    , system(mama) {}

System::System(const Ptr<void> memspace, const uint32_t memspace_size)
    : MempoolObject(memspace, memspace_size)
    , max_voices(0)
    , granularity(0)
    , sample_rate(0) {}

void VoiceInputManager::init(const uint32_t granularity, const uint16_t total_input) {
    inputs.resize(total_input);

    for (auto &input : inputs) {
        // FLTP and maximum channel count
        input.resize(granularity * 8);
    }

    reset_inputs();
}

void VoiceInputManager::reset_inputs() {
    for (auto &input : inputs) {
        std::fill(input.begin(), input.end(), 0);
    }
}

VoiceInputManager::PCMInput *VoiceInputManager::get_input_buffer_queue(const int32_t index) {
    if (index >= inputs.size()) {
        return nullptr;
    }

    return &inputs[index];
}

int32_t VoiceInputManager::receive(ngs::Patch *patch, const VoiceProduct &product) {
    PCMInput *input = get_input_buffer_queue(patch->dest_index);

    if (!input) {
        return -1;
    }

    float *dest_buffer = reinterpret_cast<float *>(input->data());
    const float *data_to_mix_in = reinterpret_cast<const float *>(product.data);

    float volume_matrix[2][2];
    memcpy(volume_matrix, patch->volume_matrix, sizeof(volume_matrix));

    // we always use stereo internally, so make sure not to add too many channels
    if (patch->source->rack->channels_per_voice == 1) {
        volume_matrix[1][0] = 0.0f;
        volume_matrix[1][1] = 0.0f;
    }

    if (patch->dest->rack->channels_per_voice == 1) {
        volume_matrix[0][1] = 0.0f;
        volume_matrix[1][1] = 0.0f;
    }

    // Try mixing, also with the use of this volume matrix
    // Dest is our voice to receive this data.
    for (int32_t k = 0; k < patch->dest->rack->system->granularity; k++) {
        dest_buffer[k * 2] = std::clamp(dest_buffer[k * 2] + data_to_mix_in[k * 2] * volume_matrix[0][0]
                + data_to_mix_in[k * 2 + 1] * volume_matrix[1][0],
            -1.0f, 1.0f);
        dest_buffer[k * 2 + 1] = std::clamp(dest_buffer[k * 2 + 1] + data_to_mix_in[k * 2] * volume_matrix[0][1] + data_to_mix_in[k * 2 + 1] * volume_matrix[1][1], -1.0f, 1.0f);
    }

    return 0;
}

ModuleData::ModuleData()
    : callback(0)
    , user_data(0)
    , is_bypassed(false)
    , flags(0) {
}

SceNgsBufferInfo *ModuleData::lock_params(const MemState &mem) {
    const std::lock_guard<std::mutex> guard(*parent->voice_mutex);

    // Save a copy of previous set of data
    if (flags & PARAMS_LOCK) {
        return nullptr;
    }

    const uint8_t *current_data = info.data.cast<const uint8_t>().get(mem);
    last_info.resize(info.size);
    memcpy(last_info.data(), current_data, info.size);

    flags |= PARAMS_LOCK;

    return &info;
}

bool ModuleData::unlock_params(const MemState &mem) {
    const std::lock_guard<std::mutex> guard(*parent->voice_mutex);

    parent->rack->modules[index]->on_param_change(mem, *this);

    if (flags & PARAMS_LOCK) {
        flags &= ~PARAMS_LOCK;
        return true;
    }

    return false;
}

void ModuleData::invoke_callback(KernelState &kernel, const MemState &mem, const SceUID thread_id, const uint32_t reason1,
    const uint32_t reason2, Address reason_ptr) {
    return parent->invoke_callback(kernel, mem, thread_id, callback, user_data, parent->rack->modules[index]->module_id(),
        reason1, reason2, reason_ptr);
}

void ModuleData::fill_to_fit_granularity() {
    const int start_fill = extra_storage.size();
    const int to_fill = parent->rack->system->granularity * 2 * sizeof(float) - start_fill;

    if (to_fill > 0) {
        extra_storage.resize(start_fill + to_fill);
        std::fill(extra_storage.begin() + start_fill, extra_storage.end(), 0);
    }
}

void Voice::init(Rack *mama) {
    rack = mama;
    state = VoiceState::VOICE_STATE_AVAILABLE;
    is_pending = false;
    is_paused = false;
    is_keyed_off = false;

    datas.resize(mama->modules.size());

    for (uint32_t i = 0; i < MAX_OUTPUT_PORT; i++)
        patches[i].resize(mama->patches_per_output);

    inputs.init(rack->system->granularity, 1);
    voice_mutex = std::make_unique<std::mutex>();
}

Ptr<Patch> Voice::patch(const MemState &mem, const int32_t index, int32_t subindex, int32_t dest_index, Voice *dest) {
    const std::lock_guard<std::mutex> guard(*voice_mutex);

    if (index >= MAX_OUTPUT_PORT) {
        // We don't have enough port for you!
        return {};
    }

    // Look if another patch has already been there
    if (subindex == -1) {
        for (int32_t i = 0; i < patches[index].size(); i++) {
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

    // Initialize the matrix
    memset(patch->volume_matrix, 0, sizeof(patch->volume_matrix));

    return patches[index][subindex];
}

bool Voice::remove_patch(const MemState &mem, const Ptr<Patch> patch) {
    if (!patch || !voice_mutex) {
        return false;
    }
    const std::lock_guard<std::mutex> guard(*voice_mutex);
    bool found = false;
    for (auto &patches_1 : patches) {
        if (vector_utils::contains(patches_1, patch)) {
            found = true;
            break;
        }
    }
    if (!found)
        return false;

    // Try to unroute. Free the destination index
    patch.get(mem)->output_sub_index = -1;

    return true;
}

ModuleData *Voice::module_storage(const uint32_t index) {
    if (index >= datas.size()) {
        return nullptr;
    }

    return &datas[index];
}

void Voice::transition(const MemState &mem, const VoiceState new_state) {
    const VoiceState old = state;
    state = new_state;

    for (size_t i = 0; i < datas.size(); i++) {
        rack->modules[i]->on_state_change(mem, datas[i], old);
    }
}

bool Voice::parse_params(const MemState &mem, const SceNgsModuleParamHeader *header) {
    ModuleData *storage = module_storage(header->module_id);

    if (!storage)
        return false;

    if (storage->flags & ModuleData::PARAMS_LOCK)
        return false;

    const auto *descr = reinterpret_cast<const SceNgsParamsDescriptor *>(header + 1);
    if (descr->size > storage->info.size)
        return false;

    memcpy(storage->info.data.get(mem), descr, descr->size);

    return true;
}

SceInt32 Voice::parse_params_block(const MemState &mem, const SceNgsModuleParamHeader *header, const SceUInt32 size) {
    const SceUInt8 *data = reinterpret_cast<const SceUInt8 *>(header);
    const SceUInt8 *data_end = data + size;

    SceInt32 num_error = 0;

    // after first loop, check if other module exist
    while (data < data_end) {
        if (!parse_params(mem, header))
            num_error++;

        // increment by the size of the header alone + the descriptor size
        data += sizeof(SceNgsModuleParamHeader) + reinterpret_cast<const SceNgsParamsDescriptor *>(header + 1)->size;

        // set new header for next module
        header = reinterpret_cast<const SceNgsModuleParamHeader *>(data);
    }

    return num_error;
}

bool Voice::set_preset(const MemState &mem, const SceNgsVoicePreset *preset) {
    // we ignore the name for now
    const uint8_t *data_origin = reinterpret_cast<const uint8_t *>(preset);

    if (preset->preset_data_offset) {
        const auto *preset_data = reinterpret_cast<const SceNgsModuleParamHeader *>(data_origin + preset->preset_data_offset);
        auto nb_errors = parse_params_block(mem, preset_data, preset->preset_data_size);
        if (nb_errors > 0)
            return false;
    }

    if (preset->bypass_flags_offset) {
        const auto *bypass_flags = reinterpret_cast<const SceUInt32 *>(data_origin + preset->bypass_flags_offset);
        // should we disable bypass on all modules first?
        for (SceUInt32 i = 0; i < preset->bypass_flags_nb; i++) {
            ModuleData *module_data = module_storage(*bypass_flags);
            if (!module_data)
                return false;
            module_data->is_bypassed = true;

            bypass_flags++;
        }
    }

    return true;
}

void Voice::invoke_callback(KernelState &kernel, const MemState &mem, const SceUID thread_id, Ptr<void> callback, Ptr<void> user_data,
    const uint32_t module_id, const uint32_t reason1, const uint32_t reason2, Address reason_ptr) {
    if (!callback) {
        return;
    }

    const ThreadStatePtr thread = kernel.get_thread(thread_id);
    const Address callback_info_addr = stack_alloc(*thread->cpu, sizeof(SceNgsCallbackInfo));

    SceNgsCallbackInfo *info = Ptr<SceNgsCallbackInfo>(callback_info_addr).get(mem);
    info->rack_handle = Ptr<void>(rack, mem);
    info->voice_handle = Ptr<void>(this, mem);
    info->module_id = module_id;
    info->callback_reason = reason1;
    info->callback_reason_2 = reason2;
    info->callback_ptr = Ptr<void>(reason_ptr);
    info->userdata = user_data;

    thread->run_callback(callback.address(), { callback_info_addr });
    stack_free(*thread->cpu, sizeof(SceNgsCallbackInfo));
}

uint32_t System::get_required_memspace_size(SceNgsSystemInitParams *parameters) {
    return sizeof(System);
}

uint32_t Rack::get_required_memspace_size(MemState &mem, SceNgsRackDescription *description) {
    uint32_t buffer_size = 0;
    if (description->definition)
        // multiply by 2 because there are 2 copies of each buffer
        buffer_size = 2 * get_voice_definition_size(description->definition.get(mem));

    return sizeof(ngs::Rack) + description->voice_count * (sizeof(ngs::Voice) + buffer_size + description->patches_per_output * description->definition.get(mem)->output_count * sizeof(ngs::Patch));
}

bool init(State &ngs, MemState &mem) {
    voice_definition_init(ngs, mem);

    return true;
}

bool init_system(State &ngs, const MemState &mem, SceNgsSystemInitParams *parameters, Ptr<void> memspace, const uint32_t memspace_size) {
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

void release_system(State &ngs, const MemState &mem, System *system) {
    // this function assumes no ngs mutex is being held

    // release all the racks first
    for (size_t i = 0; i < system->racks.size(); i++)
        release_rack(ngs, mem, system, system->racks[i]);

    vector_utils::erase_first(ngs.systems, system);

    system->~System();
}

bool init_rack(State &ngs, const MemState &mem, System *system, SceNgsBufferInfo *init_info, const SceNgsRackDescription *description) {
    Rack *rack = init_info->data.cast<Rack>().get(mem);
    rack = new (rack) Rack(system, init_info->data, init_info->size);

    // Alloc first block for Rack
    if (!rack->alloc<Rack>()) {
        return false;
    }

    if (description->definition)
        apply_voice_definition(description->definition.get(mem), rack->modules);
    else
        rack->modules.clear();

    // Initialize voice definition
    rack->channels_per_voice = description->channels_per_voice;
    rack->max_patches_per_input = description->max_patches_per_input;
    rack->patches_per_output = description->patches_per_output;

    // Alloc spaces for voice
    rack->voices.resize(description->voice_count);
    rack->vdef = description->definition.get(mem);

    for (auto &voice : rack->voices) {
        voice = rack->alloc<Voice>();

        if (!voice) {
            return false;
        }

        Voice *v = voice.get(mem);
        new (v) Voice();
        v->init(rack);

        // Allocate parameter buffer info for each voice
        for (size_t i = 0; i < rack->modules.size(); i++) {
            v->datas[i].info.size = rack->modules[i]->get_buffer_parameter_size();
            v->datas[i].info.data = rack->alloc_raw(v->datas[i].info.size);
            // from the behavior of games, it looks like the other info buffer (there are two copies because of VoiceLock) is located right after the first
            // one, so copy this behavior, to avoid a game overwriting some important ngs struct
            rack->alloc_raw(v->datas[i].info.size);

            v->datas[i].parent = v;
            v->datas[i].index = static_cast<uint32_t>(i);
        }
    }

    system->racks.push_back(rack);

    return true;
}

void release_rack(State &ngs, const MemState &mem, System *system, Rack *rack) {
    // this function should only be called outside of ngs update and with the scheduler mutex acquired (except when releasing the system)
    if (!rack) {
        LOG_WARN("Trying to release a rack that is already released or null.");
        return;
    }

    // remove all queued voices
    for (const auto &voice : rack->voices) {
        Voice *v = voice.get(mem);
        if (v) {
            system->voice_scheduler.deque_voice(v);
            v->~Voice();
        }
    }

    // remove from system
    vector_utils::erase_first(system->racks, rack);

    // free pointer memory
    rack->~Rack();
}

Ptr<VoiceDefinition> get_voice_definition(State &ngs, MemState &mem, ngs::BussType type) {
    return ngs.definitions + static_cast<int>(type);
}
} // namespace ngs
