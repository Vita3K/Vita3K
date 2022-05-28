// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <gxm/types.h>

#include <array>
#include <string>

namespace gxm {
// Color.
SceGxmColorBaseFormat get_base_format(SceGxmColorFormat src);
size_t bits_per_pixel(SceGxmColorBaseFormat base_format);
size_t get_stride_in_bytes(const SceGxmColorFormat src, const std::size_t stride_in_pixels);
// Textures.
size_t get_width(const SceGxmTexture *texture);
size_t get_height(const SceGxmTexture *texture);
SceGxmTextureFormat get_format(const SceGxmTexture *texture);
SceGxmTextureBaseFormat get_base_format(SceGxmTextureFormat src);
size_t get_stride_in_bytes(const SceGxmTexture *texture);
bool is_block_compressed_format(SceGxmTextureBaseFormat base_format);
bool is_paletted_format(SceGxmTextureBaseFormat base_format);
bool is_yuv_format(SceGxmTextureBaseFormat base_format);
size_t attribute_format_size(SceGxmAttributeFormat format);
size_t index_element_size(SceGxmIndexFormat format);
bool is_stream_instancing(SceGxmIndexSource source);
bool convert_color_format_to_texture_format(SceGxmColorFormat format, SceGxmTextureFormat &dest_format);
// Transfer
uint32_t get_bits_per_pixel(SceGxmTransferFormat Format);
} // namespace gxm

namespace gxp {
// Used to map GXM program parameters to GLSL data types
enum class GenericParameterType {
    Scalar,
    Vector,
    Matrix,
    Array
};

using GxmVertexOutputTexCoordInfos = std::array<uint8_t, 10>;

void log_parameter(const SceGxmProgramParameter &parameter);

/**
 * \brief Returns raw parameter name from GXP
 *        Therefore, if parameter belongs in a struct, includes it in the form of "struct_name.field_name"
 */
std::string parameter_name_raw(const SceGxmProgramParameter &parameter);

/**
 * \brief If parameter belongs in a struct, returns the struct field name only
 */
std::string parameter_name(const SceGxmProgramParameter &parameter);

/**
 * \brief If parameter belongs in a struct, returns the struct name only
 */
std::string parameter_struct_name(const SceGxmProgramParameter &parameter);
const SceGxmProgramParameter *program_parameters(const SceGxmProgram &program);
SceGxmParameterType parameter_type(const SceGxmProgramParameter &parameter);
GenericParameterType parameter_generic_type(const SceGxmProgramParameter &parameter);
/**
 * \return SceGxmVertexProgramOutput (bitfield)
 */
SceGxmVertexProgramOutputs get_vertex_outputs(const SceGxmProgram &program, GxmVertexOutputTexCoordInfos *coord_infos = nullptr);
SceGxmFragmentProgramInputs get_fragment_inputs(const SceGxmProgram &program);

const int get_parameter_type_size(const SceGxmParameterType type);
const int get_num_32_bit_components(const SceGxmParameterType type, const uint16_t num_comp);

const SceGxmProgramParameterContainer *get_containers(const SceGxmProgram &program);
const SceGxmProgramParameterContainer *get_container_by_index(const SceGxmProgram &program, const std::uint16_t idx);
const char *get_container_name(const std::uint16_t idx);

int get_uniform_buffer_base(const SceGxmProgram &program, const SceGxmProgramParameter &parameter);

} // namespace gxp
