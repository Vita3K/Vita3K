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
void submit_command_list(State &state, renderer::Context *context, GxmContextState *gxm_context_state, CommandList &command_list);
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

void set_depth_bias(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, int factor, int units);
void set_depth_func(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, SceGxmDepthFunc depth_func);
void set_depth_write_enable_mode(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, SceGxmDepthWriteMode enable);
void set_point_line_width(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, unsigned int width);
void set_polygon_mode(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, SceGxmPolygonMode mode);
void set_stencil_func(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail, SceGxmStencilOp depthPass, unsigned char compareMask, unsigned char writeMask);
void set_stencil_ref(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, unsigned char sref);
void set_program(State &state, Context *ctx, GxmContextState *gxm_context, Ptr<const void> program, const bool is_fragment);
void set_cull_mode(State &state, Context *ctx, GxmContextState *gxm_context, SceGxmCullMode cull);
void set_fragment_texture(State &state, Context *ctx, GxmContextState *gxm_context, const std::uint32_t tex_index, const emu::SceGxmTexture tex);
void set_viewport(State &state, Context *ctx, GxmContextState *gxm_context, float xOffset, float yOffset, float zOffset, float xScale, float yScale, float zScale);
void set_viewport_enable(State &state, Context *ctx, GxmContextState *gxm_context, SceGxmViewportMode enable);
void set_region_clip(State &state, Context *ctx, GxmContextState *gxm_context, SceGxmRegionClipMode mode, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax);
void set_two_sided_enable(State &state, Context *ctx, GxmContextState *gxm_context, SceGxmTwoSidedMode mode);
void set_uniform(State &state, Context *ctx, const bool is_vertex_uniform, const SceGxmProgramParameter *parameter, const void *data);

void set_context(State &state, Context *ctx, GxmContextState *gxm_context, RenderTarget *target, emu::SceGxmColorSurface *color_surface, emu::SceGxmDepthStencilSurface *depth_stencil_surface);
void set_vertex_stream(State &state, Context *ctx, GxmContextState *gxm_context, const std::size_t index, const std::size_t data_len, const void *data);
void draw(State &state, Context *ctx, GxmContextState *gxm_context, SceGxmPrimitiveType prim_type, SceGxmIndexFormat index_type, const void *index_data, const std::uint32_t index_count);
void sync_surface_data(State &state, Context *ctx, GxmContextState *gxm_context);

bool create_context(State &state, std::unique_ptr<Context> &context);
bool create_render_target(State &state, std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams *params);
void destroy_render_target(State &state, std::unique_ptr<RenderTarget> &rt);

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

struct TextureCacheState;

namespace texture {

// Paletted textures.
void palette_texture_to_rgba_4(uint32_t *dst, const uint8_t *src, size_t width, size_t height, const uint32_t *palette);
void palette_texture_to_rgba_8(uint32_t *dst, const uint8_t *src, size_t width, size_t height, const uint32_t *palette);
const uint32_t *get_texture_palette(const emu::SceGxmTexture &texture, const MemState &mem);

void cache_and_bind_texture(TextureCacheState &cache, const emu::SceGxmTexture &gxm_texture, const MemState &mem);
size_t bits_per_pixel(SceGxmTextureBaseFormat base_format);

}

} // namespace renderer
