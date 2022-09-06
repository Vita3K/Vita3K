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

#include <renderer/commands.h>
#include <renderer/types.h>

struct MemState;
struct FeatureState;
struct Config;
struct SDL_Window;

namespace renderer {
struct Context;
struct FragmentProgram;
struct RenderTarget;
struct State;
struct VertexProgram;

bool create(std::unique_ptr<FragmentProgram> &fp, State &state, const SceGxmProgram &program, const SceGxmBlendInfo *blend, GXPPtrMap &gxp_ptr_map);
bool create(std::unique_ptr<VertexProgram> &vp, State &state, const SceGxmProgram &program, GXPPtrMap &gxp_ptr_map, const std::vector<SceGxmVertexAttribute> &attributes);
void create(SceGxmSyncObject *sync, State &state);
void destroy(SceGxmSyncObject *sync, State &state);
void finish(State &state, Context *context);

/**
 * \brief Wait for all subjects to be done with the given sync object.
 *
 * Return true if the wait didn't timeout
 */
bool wishlist(SceGxmSyncObject *sync_object, const uint32_t timestamp, const int32_t timeout_micros = -1);

/**
 * \brief Set list of subject with sync object to done.
 *
 * This will also signals wishlists that are waiting.
 */
void subject_done(SceGxmSyncObject *sync_object, const uint32_t timestamp);

int wait_for_status(State &state, int *status, int signal, bool wake_on_equal);
void reset_command_list(CommandList &command_list);
void submit_command_list(State &state, renderer::Context *context, CommandList &command_list);
bool is_cmd_ready(MemState &mem, CommandList &command_list);
void process_batch(State &state, MemState &mem, Config &config, CommandList &command_list);
void process_batches(State &state, const FeatureState &features, MemState &mem, Config &config);
bool init(SDL_Window *window, std::unique_ptr<State> &state, Backend backend, const Config &config, const Root &root_paths);

void set_depth_bias(State &state, Context *ctx, bool is_front, int factor, int units);
void set_depth_func(State &state, Context *ctx, bool is_front, SceGxmDepthFunc depth_func);
void set_depth_write_enable_mode(State &state, Context *ctx, bool is_front, SceGxmDepthWriteMode enable);
void set_point_line_width(State &state, Context *ctx, bool is_front, unsigned int width);
void set_polygon_mode(State &state, Context *ctx, bool is_front, SceGxmPolygonMode mode);
void set_stencil_func(State &state, Context *ctx, bool is_front, SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail, SceGxmStencilOp depthPass, unsigned char compareMask, unsigned char writeMask);
void set_stencil_ref(State &state, Context *ctx, bool is_front, unsigned char sref);
void set_program(State &state, Context *ctx, Ptr<const void> program, const bool is_fragment);
void set_cull_mode(State &state, Context *ctx, SceGxmCullMode cull);
void set_texture(State &state, Context *ctx, const std::uint32_t tex_index, const SceGxmTexture tex);
void set_viewport_real(State &state, Context *ctx, float xOffset, float yOffset, float zOffset, float xScale, float yScale, float zScale);
void set_viewport_flat(State &state, Context *ctx);
void set_region_clip(State &state, Context *ctx, SceGxmRegionClipMode mode, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax);
void set_two_sided_enable(State &state, Context *ctx, SceGxmTwoSidedMode mode);
void set_side_fragment_program_enable(State &state, Context *ctx, const bool is_front, SceGxmFragmentProgramMode mode);
void set_uniform_buffer(State &state, Context *ctx, const bool is_vertex_uniform, const int block_num, const std::uint16_t buffer_size, const Ptr<const void> buffer);
void set_visibility_buffer(State &state, Context *ctx, Ptr<uint32_t> visibility_address, uint32_t visibility_stride);
void set_visibility_index(State &state, Context *ctx, bool enable, uint32_t index, bool is_increment);

void set_context(State &state, Context *ctx, RenderTarget *target, SceGxmColorSurface *color_surface, SceGxmDepthStencilSurface *depth_stencil_surface);
void set_vertex_stream(State &state, Context *ctx, const std::size_t index, const std::size_t data_len, const Ptr<const void> stream);
void draw(State &state, Context *ctx, SceGxmPrimitiveType prim_type, SceGxmIndexFormat index_type, Ptr<const void> index_data, const std::uint32_t index_count, const std::uint32_t instance_count);
void transfer_copy(State &state, uint32_t colorKeyValue, uint32_t colorKeyMask, SceGxmTransferColorKeyMode colorKeyMode, const SceGxmTransferImage *images, SceGxmTransferType srcType, SceGxmTransferType destType);
void transfer_downscale(State &state, const SceGxmTransferImage *src, const SceGxmTransferImage *dest);
void transfer_fill(State &state, uint32_t fillColor, const SceGxmTransferImage *dest);
void sync_surface_data(State &state, Context *ctx, const SceGxmNotification vertex_notification, const SceGxmNotification fragment_notification);

bool create_context(State &state, std::unique_ptr<Context> &context);
void destroy_context(State &state, std::unique_ptr<Context> &context);
bool create_render_target(State &state, std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams *params);
void destroy_render_target(State &state, std::unique_ptr<RenderTarget> &rt);

Command *generic_command_allocate();
void generic_command_free(Command *cmd);

template <typename... Args>
bool add_command(Context *ctx, const CommandOpcode opcode, int *status, Args... arguments) {
    if (!ctx) {
        return false;
    }
    auto cmd_maked = make_command(ctx->alloc_func, ctx->free_func, opcode, status, arguments...);

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

template <typename... Args>
bool add_state_set_command(Context *ctx, const GXMState state, Args... arguments) {
    return add_command(ctx, CommandOpcode::SetState, nullptr, state, arguments...);
}

template <typename... Args>
int send_single_command(State &state, Context *ctx, const CommandOpcode opcode, bool wait, Args... arguments) {
    // Make a temporary command list
    int status = CommandErrorCodePending; // Pending.
    auto cmd = make_command(ctx ? ctx->alloc_func : generic_command_allocate, ctx ? ctx->free_func : generic_command_free,
        opcode, wait ? &status : nullptr, arguments...);

    if (!cmd) {
        return CommandErrorArgumentsTooLarge;
    }

    CommandList list;
    list.first = cmd;
    list.last = cmd;

    // Submit it
    submit_command_list(state, ctx, list);
    if (wait)
        return wait_for_status(state, &status, CommandErrorCodePending, false);
    else
        return 0;
}

namespace texture {

// Paletted textures.
void palette_texture_to_rgba_4(uint32_t *dst, const uint8_t *src, uint32_t width, uint32_t height, const uint32_t *palette);
void palette_texture_to_rgba_8(uint32_t *dst, const uint8_t *src, uint32_t width, uint32_t height, const uint32_t *palette);
void yuv420_texture_to_rgb(uint8_t *dst, const uint8_t *src, uint32_t width, uint32_t height, uint32_t layout_width, uint32_t layout_height, bool is_p3);
const uint32_t *get_texture_palette(const SceGxmTexture &texture, const MemState &mem);

// Assume fmt is a bcn format
SceGxmTextureBaseFormat get_matching_decompressed_format(SceGxmTextureBaseFormat fmt);

bool is_astc_format(SceGxmTextureBaseFormat base_format);

/**
 * \brief Try to resolve Z-order of block compressed texture
 *
 * \param fmt    Texture base format.
 * \param dest   Destination texture data. Size must be sufficient enough of align(width, 4) * align(height,4) * 4 (bytes).
 * \param data   Source data to solve.
 * \param width  Texture width.
 * \param height Texture height.
 *
 * \return Void.
 */
void resolve_z_order_compressed_texture(SceGxmTextureBaseFormat fmt, void *dest, const void *data, const uint32_t width, const uint32_t height);

/**
 * \brief Try to decompress texture to 32-bit RGBA.
 *
 * \param fmt    Texture base format.
 * \param dest   Destination texture data. Size must be sufficient enough of align(width, 4) * align(height,4) * 4 (bytes).
 * \param data   Source data to decompress.
 * \param width  Texture width.
 * \param height Texture height.
 *
 * \return Size of source taken.
 */
uint32_t decompress_compressed_texture(SceGxmTextureBaseFormat fmt, void *dest, const void *data, const uint32_t width, const uint32_t height);

/**
 * \brief Decompresses all the blocks of a block compressed texture and stores the resulting pixels in 'image'.
 *
 * Output results is in format RGBA, with each channel being 8 bits.
 *
 * \param width             Texture width.
 * \param height            Texture height.
 * \param block_storage     Pointer to compressed blocks.
 * \param image             Pointer to the image where the decompressed pixels will be stored.
 * \param format_id         Id of the compressed format, in order: BC1 (DXT1), BC2 (DXT3), BC3 (DXT5), BC4U (RGTC1), BC4S (RGTC1), BC5U (RGTC2) or BC5S (RGTC2).
 */
void decompress_bc_image(uint32_t width, uint32_t height, const uint8_t *block_storage, uint32_t *image, const uint8_t format_id);

/**
 * \brief Try to decompress texture to 16-bit RGB floating point color.
 *
 * \param fmt    Texture base format.
 * \param dest   Destination texture data. Size must be sufficient enough of align(width, 4) * height * 4 (bytes).
 * \param data   Source data to decompress.
 * \param width  Texture width.
 * \param height Texture height.
 *
 * \return Void.
 */
void decompress_packed_float_e5m9m9m9(SceGxmTextureBaseFormat fmt, void *dest, const void *data, const uint32_t width, const uint32_t height);

/**
 * \brief Solves Z-order on all the blocks of a block compressed texture and stores the resulting pixels in 'dest'.
 *
 * Output results is in format RGBA, with each channel being 8 bits.
 *
 * \param width         Texture width.
 * \param height        Texture height.
 * \param src           Pointer to compressed blocks.
 * \param dest          Pointer to the image where the decompressed pixels will be stored.
 * \param block_size    The size of a compressed block in (8 bytes or 16 bytes)
 */
void resolve_z_order_compressed_image(uint32_t width, uint32_t height, const uint8_t *src, uint8_t *dest, const uint32_t block_size);

// Convert x8u24 (or u24x8) format to f32 (only keep the u24 part)
// Do not use a depth-stencil format as x8d24 is not supported on all GPUs for Vulkan
void convert_x8u24_to_f32(void *dest, const void *data, const uint32_t width, const uint32_t height, const SceGxmTextureFormat format);
void convert_U8U3U3U2_to_U8U8U8U8(void *dest, const void *data, const uint32_t width, const uint32_t height);
void convert_x8u24_to_u24x8(void *dest, const void *data, const uint32_t width, const uint32_t height);
void convert_f32m_to_f32(void *dest, const void *data, const uint32_t width, const uint32_t height);
void convert_u2f10f10f10_to_f16f16f16f16(void *dest, const void *data, const uint32_t width, const uint32_t height, const SceGxmTextureFormat format);

void swizzled_texture_to_linear_texture(uint8_t *dest, const uint8_t *src, uint16_t width, uint16_t height, uint8_t bits_per_pixel);
void tiled_texture_to_linear_texture(uint8_t *dest, const uint8_t *src, uint16_t width, uint16_t height, uint8_t bits_per_pixel);

uint16_t get_upload_mip(const uint16_t true_mip, const uint16_t width, const uint16_t height);

uint32_t decode_morton2_x(uint32_t code);
uint32_t decode_morton2_y(uint32_t code);
uint32_t encode_morton(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
bool can_texture_be_unswizzled_without_decode(SceGxmTextureBaseFormat fmt, bool is_vulkan);
uint32_t get_compressed_size(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height);
uint64_t hash_texture_data(const SceGxmTexture &texture, uint32_t texture_size, const MemState &mem);
// hash texture used for texture replacement such that byte in the stride are not hashed
// this prevent texture duplication in case there are random bytes in the stride
uint64_t hash_texture_nostride(const SceGxmTexture &texture, const MemState &mem);
bool convert_base_texture_format_to_base_color_format(SceGxmTextureBaseFormat format, SceGxmColorBaseFormat &color_format);

} // namespace texture

} // namespace renderer
