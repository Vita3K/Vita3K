#pragma once

#include <gxm/types.h>
#include <psp2/gxm.h>

#include <string>

namespace gxm {
// Textures.
size_t get_width(const emu::SceGxmTexture *texture);
size_t get_height(const emu::SceGxmTexture *texture);
SceGxmTextureFormat get_format(const emu::SceGxmTexture *texture);
SceGxmTextureBaseFormat get_base_format(SceGxmTextureFormat src);
bool is_paletted_format(SceGxmTextureFormat src);
bool is_yuv_format(SceGxmTextureFormat src);
size_t attribute_format_size(SceGxmAttributeFormat format);
} // namespace gxm

namespace gxp {
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
SceGxmVertexProgramOutputs get_vertex_outputs(const SceGxmProgram &program, SceGxmVertexOutputTexCoordInfos *coord_infos = nullptr);
SceGxmFragmentProgramInputs get_fragment_inputs(const SceGxmProgram &program);

const int get_parameter_type_size(const SceGxmParameterType type);
const int get_num_32_bit_components(const SceGxmParameterType type, const uint16_t num_comp);

const SceGxmProgramParameterContainer *get_containers(const SceGxmProgram &program);
const SceGxmProgramParameterContainer *get_container_by_index(const SceGxmProgram &program, const std::uint16_t idx);
const char *get_container_name(const std::uint16_t idx);

} // namespace gxp
