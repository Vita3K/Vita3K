#pragma once

#include <renderer/types.h>
#include <renderer/commands.h>
#include <psp2/gxm.h>
#include <gxm/types.h>

struct GxmContextState;
struct MemState;
struct FeatureState;
struct Config;

namespace emu {
struct SceGxmBlendInfo;
}

namespace renderer {
struct Context;
struct FragmentProgram;
struct RenderTarget;
struct State;
struct VertexProgram;

bool create(std::unique_ptr<FragmentProgram> &fp, State &state, const SceGxmProgram &program, const emu::SceGxmBlendInfo *blend, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id);
bool create(std::unique_ptr<VertexProgram> &vp, State &state, const SceGxmProgram &program, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id);
void finish(State &state, Context &context);

/**
 * \brief Wait for all subjects to be done with the given sync object.
 */
void wishlist(SceGxmSyncObject *sync_object, const SyncObjectSubject subjects);

/**
 * \brief Set list of subject with sync object to done.
 * 
 * This will also signals wishlists that are waiting.
 */
void subject_done(SceGxmSyncObject *sync_object, const SyncObjectSubject subjects);

/**
 * Set some subjects to be in progress.
 */
void subject_in_progress(SceGxmSyncObject *sync_object, const SyncObjectSubject subjects);

int wait_for_status(State &state, int *result_code);
void reset_command_list(CommandList &command_list);
void submit_command_list(State &state, renderer::Context *context, GxmContextState *context_state, CommandList &command_list);
void process_batch(State &state, MemState &mem, Config &config, CommandList &command_list, const char *base_path, const char *title_id);
void process_batches(State &state, const FeatureState &features, MemState &mem, Config &config, const char *base_path, const char *title_id);
bool init(std::unique_ptr<State> &state, Backend backend);

/**
 * \brief Copy uniform data and queue it to available command list.
 * 
 * Later when the uniform commands are processed, resources will be automatically freed.
 * 
 * For backend that support command list/buffer, this will be queued directly to API's command list/buffer.
 * 
 * \param state   The renderer state.
 * \param ctx     The context to queue uniform command in.
 * \param program Target program to get uniforms from.
 * \param buffers Set of all uniform buffers.
 * 
 */
void set_uniforms(State &state, Context *ctx, const SceGxmProgram &program, const UniformBuffers &buffers, const MemState &mem);

void set_vertex_data(State &state, Context *ctx, const StreamDatas &datas);

template <typename ...Args>
bool add_command(Context *ctx, const CommandOpcode opcode, int *status, Args... arguments) {
    auto cmd_maked = make_command(opcode, status, arguments...);

    if (!cmd_maked) {
        return false;
    }

    if (!ctx->command_list.first) {
        ctx->command_list.first = cmd_maked;
        ctx->command_list.last = cmd_maked;
    } else {
        ctx->command_list.last->next = cmd_maked;
        ctx->command_list.last = cmd_maked;
    }
    
    return true;
}

template <typename ...Args>
bool add_state_set_command(Context *ctx, const GXMState state, Args... arguments) {
    return add_command(ctx, CommandOpcode::SetState, nullptr, state, arguments...);
}

template <typename ...Args>
int send_single_command(State &state, Context *ctx, GxmContextState *gxm_state, const CommandOpcode opcode, 
    Args... arguments) {
    // Make a temporary command list
    int status = CommandErrorCodePending;    // Pending.
    auto cmd = make_command(opcode, &status, arguments...);

    if (!cmd) {
        return CommandErrorArgumentsTooLarge;
    }

    CommandList list;
    list.first = cmd;
    list.last = cmd;
    
    // Submit it
    submit_command_list(state, ctx, gxm_state, list);
    return wait_for_status(state, &status);
}

bool sync_state(State &state, Context &context, const MemState &mem);

} // namespace renderer
