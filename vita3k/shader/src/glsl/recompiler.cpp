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

#include <features/state.h>
#include <gxm/functions.h>
#include <gxm/types.h>
#include <shader/disasm.h>
#include <shader/glsl/code_writer.h>
#include <shader/glsl/consts.h>
#include <shader/glsl/params.h>
#include <shader/glsl/recompiler.h>
#include <shader/glsl/translator.h>
#include <shader/gxp_parser.h>
#include <shader/recompiler.h>
#include <shader/types.h>

#include <util/log.h>
#include <util/overloaded.h>

namespace shader::usse::glsl {
static void nicen_name_for_glsl_rules(std::string &prev) {
    while (true) {
        std::size_t dot_pos = prev.find('.');
        if (dot_pos != std::string::npos) {
            prev.replace(dot_pos, 1, "_");
        } else {
            break;
        }
    }

    while (true) {
        std::size_t bracket_open_pos = prev.find('[');
        if (bracket_open_pos != std::string::npos) {
            prev.replace(bracket_open_pos, 1, "_");
        } else {
            break;
        }
    }

    while (true) {
        std::size_t bracket_open_pos = prev.find(']');
        if (bracket_open_pos != std::string::npos) {
            prev.replace(bracket_open_pos, 1, "_");
        } else {
            break;
        }
    }

    // GLSL does not allow name with double dash inside
    while (true) {
        std::size_t pos_double_dash = prev.find("__");
        if (pos_double_dash != std::string::npos) {
            prev.replace(pos_double_dash, 2, "_dd_");
        } else {
            break;
        }
    }
}
static void create_uniform_buffers(CodeWriter &writer, const SceGxmProgram &program, const ProgramInput &input, std::map<int, int> &buffer_bases) {
    std::map<int, std::uint32_t> buffer_sizes;

    for (const auto &buffer : input.uniform_buffers) {
        const auto buffer_size = (buffer.size + 3) / 4;
        buffer_sizes.emplace(buffer.index, buffer_size);
    }

    if (buffer_sizes.empty()) {
        return;
    }

    writer.add_declaration(fmt::format("layout (std140, binding = {}) buffer {}Type {{", program.is_vertex() ? 0 : 1, program.is_vertex() ? VERTEX_UB_GROUP_NAME : FRAGMENT_UB_GROUP_NAME));
    std::uint32_t last_offset = 0;
    for (auto [buffer_index, buffer_size] : buffer_sizes) {
        writer.add_declaration(fmt::format("\tvec4 buffer{}[{}];", buffer_index, buffer_size));
        buffer_bases.emplace(buffer_index, last_offset);

        last_offset += buffer_size * 16;
    }

    writer.add_declaration(fmt::format("}} {};\n", program.is_vertex() ? VERTEX_UB_GROUP_NAME : FRAGMENT_UB_GROUP_NAME));
}

static void create_samplers(CodeWriter &writer, SamplerMap &sampler_map, const SceGxmProgram &program, const ProgramInput &input) {
    writer.add_declaration("vec4 textureProjCube(samplerCube sampler, vec4 coord) {");
    writer.add_declaration("\treturn texture(sampler, coord.xyz / coord.w);");
    writer.add_declaration("}");

    for (const auto &sampler : input.samplers) {
        std::string sampler_name = fmt::format("{}_{}", program.is_vertex() ? "vertTex" : "fragTex", sampler.name);
        nicen_name_for_glsl_rules(sampler_name);

        writer.add_declaration(fmt::format("layout (binding = {}) uniform {} {};", sampler.index + (program.is_vertex() ? SCE_GXM_MAX_TEXTURE_UNITS : 0),
            sampler.is_cube ? "samplerCube" : "sampler2D", sampler_name));

        sampler_map.emplace(sampler.index, std::make_pair(sampler_name, sampler.is_cube));
    }
}

static void create_fragment_inputs(CodeWriter &writer, ShaderVariables &variables, const FeatureState &features, const SceGxmProgram &program) {
    writer.add_declaration(fmt::format("layout (binding = {}, rgba8) uniform image2D f_mask;", MASK_TEXTURE_SLOT_IMAGE));
    writer.add_to_preload("if (all(lessThan(imageLoad(f_mask, ivec4(gl_FragCoord).xy / ivec2(renderFragInfo.res_multiplier)), vec4(0.5)))) discard;");

    if (program.is_frag_color_used()) {
        if (features.direct_fragcolor) {
            // The GPU supports gl_LastFragData. It's only OpenGL though
            if (features.preserve_f16_nan_as_u16) {
                writer.add_to_preload("if (renderFragInfo.use_raw_image >= 0.5) {");
                writer.indent_preload();
                writer.add_to_preload("o0 = gl_LastFragData[1].xyzw;");
                writer.dedent_preload();
                writer.add_to_preload("} else {");
                writer.indent_preload();
                writer.add_to_preload("o0 = gl_LastFragData[0];");
                writer.dedent_preload();
                writer.add_to_preload("}");
            } else {
                writer.add_to_preload("o0 = gl_LastFragData[0];");
            }
        } else if (features.support_shader_interlock || features.support_texture_barrier) {
            writer.add_declaration(fmt::format("layout (binding = {}, rgba8) uniform image2D f_colorAttachment;", COLOR_ATTACHMENT_TEXTURE_SLOT_IMAGE));
            writer.add_declaration("");

            variables.should_gen_pack_unpack[ShaderVariables::GEN_PACK_U8] = true;
            variables.should_gen_pack_unpack[ShaderVariables::GEN_PACK_U16] = true;

            if (features.preserve_f16_nan_as_u16) {
                writer.add_declaration(fmt::format("layout (binding = {}, rgba16ui) uniform uimage2D f_colorAttachment_rawUI;", COLOR_ATTACHMENT_RAW_TEXTURE_SLOT_IMAGE));
                writer.add_to_preload("if (renderFragInfo.use_raw_image >= 0.5) {");
                writer.indent_preload();
                writer.add_to_preload("uvec4 sampled = imageLoad(f_colorAttachment_rawUI, ivec2(gl_FragCoord.xy));");
                writer.add_to_preload("o0.xy = vec2(pack2xU16(sampled.xy), pack2xU16(sampled.zw));");
                writer.dedent_preload();
                writer.add_to_preload("} else {");
                writer.indent_preload();
                writer.add_to_preload("o0.x = pack4xU8(uvec4(imageLoad(f_colorAttachment, ivec2(gl_FragCoord.xy)) * vec4(255.0)));");
                writer.dedent_preload();
                writer.add_to_preload("}");
            } else {
                writer.add_to_preload("o0.x = pack4xU8(uvec4(imageLoad(f_colorAttachment, ivec2(gl_FragCoord.xy)) * vec4(255.0)));");
            }

            variables.mark_f32_raw_dirty(RegisterBank::OUTPUT, 0);
            variables.mark_f32_raw_dirty(RegisterBank::OUTPUT, 1);
        } else {
            // Try to initialize outs[0] to some nice value. In case the GPU has garbage data for our shader
            writer.add_to_preload("o0 = vec4(0.0);");
        }
    }
}

static void create_parameters(ProgramState &state, CodeWriter &writer, ShaderVariables &params, const FeatureState &features, const SceGxmProgram &program, const ProgramInput &program_input,
    const std::vector<SceGxmVertexAttribute> *hint_attributes = nullptr) {
    if (program.is_fragment()) {
        writer.add_to_preload("if ((renderFragInfo.front_disabled != 0.0) && gl_FrontFacing) discard;");
        writer.add_to_preload("if ((renderFragInfo.back_disabled != 0.0) && !gl_FrontFacing) discard;");
    }

    writer.add_declaration("bool p0;");
    writer.add_declaration("bool p1;");
    writer.add_declaration("bool p2;");
    writer.add_declaration("float temp1;");
    writer.add_declaration("vec2 temp2;");
    writer.add_declaration("vec3 temp3;");
    writer.add_declaration("vec4 temp4;");
    writer.add_declaration("float temp1_1;");
    writer.add_declaration("vec2 temp2_1;");
    writer.add_declaration("vec3 temp3_1;");
    writer.add_declaration("vec4 temp4_1;");
    writer.add_declaration("float temp1_2;");
    writer.add_declaration("vec2 temp2_2;");
    writer.add_declaration("vec3 temp3_2;");
    writer.add_declaration("vec4 temp4_2;");
    writer.add_declaration("int itemp1;");
    writer.add_declaration("ivec2 itemp2;");
    writer.add_declaration("ivec3 itemp3;");
    writer.add_declaration("ivec4 itemp4;");
    writer.add_declaration("int base_temp;");
    writer.add_declaration("\n");

    std::map<int, int> buffer_bases;
    SamplerMap sampler_map_on_binding;
    std::vector<VarToReg> var_to_regs;

    create_uniform_buffers(writer, program, program_input, buffer_bases);
    create_samplers(writer, sampler_map_on_binding, program, program_input);

    std::uint32_t in_fcount_allocated = 0;

    for (const auto &input : program_input.inputs) {
        std::visit(overloaded{
                       [&](const LiteralInputSource &s) {
                           params.mark_f32_raw_dirty(RegisterBank::SECATTR, input.offset);
                           writer.add_to_current_body(fmt::format("sa{}.{} = uintBitsToFloat(0x{:X});", input.offset / 4 * 4, static_cast<char>('w' + ((input.offset + 1) % 4)),
                               *reinterpret_cast<const std::uint32_t *>(&s.data)));
                       },
                       [&](const UniformBufferInputSource &s) {
                           const int base = buffer_bases.at(s.index) + s.base;
                           params.mark_f32_raw_dirty(RegisterBank::SECATTR, input.offset);
                           writer.add_to_current_body(fmt::format("sa{}.{} = intBitsToFloat({});", static_cast<std::int32_t>(input.offset) / 4 * 4, static_cast<char>('w' + ((input.offset + 1) % 4)), base));
                       },
                       [&](const DependentSamplerInputSource &s) {
                           const auto sampler_name = sampler_map_on_binding.at(s.index);
                           state.samplers_on_offset.emplace(input.offset, sampler_name);
                       },
                       [&](const AttributeInputSource &s) {
                           if (input.bank == RegisterBank::SPECIAL) {
                               // Texcoord base location
                               writer.add_declaration(fmt::format("layout (location = {}) in vec4 v_TexCoord{};", s.opt_location + 4, s.index));
                           } else {
                               VarToReg store_info;
                               store_info.offset = input.offset;
                               store_info.comp_count = input.array_size * input.component_count;
                               store_info.reg_format = s.regformat;
                               store_info.var_name = s.name;
                               store_info.location = (s.opt_location != 0xFFFFFFFF) ? s.opt_location : in_fcount_allocated / 4;
                               store_info.builtin = false;

                               // Some compilers does not allow in variables to be casted
                               switch (s.semantic) {
                               case SCE_GXM_PARAMETER_SEMANTIC_INDEX:
                                   writer.add_to_preload("int vertexIdTemp = gl_VertexID;");
                                   store_info.var_name = "intBitsToFloat(vertexIdTemp)";
                                   store_info.data_type = static_cast<int>(DataType::F32);
                                   store_info.comp_count = 1;
                                   store_info.builtin = true;

                                   break;

                               case SCE_GXM_PARAMETER_SEMANTIC_INSTANCE:
                                   writer.add_to_preload("int instanceIdTemp = gl_InstanceID;");
                                   store_info.var_name = "intBitsToFloat(instanceIdTemp)";
                                   store_info.data_type = static_cast<int>(DataType::F32);
                                   store_info.comp_count = 1;
                                   store_info.builtin = true;

                                   break;

                               default:
                                   store_info.data_type = static_cast<int>(input.type);
                               }

                               var_to_regs.push_back(store_info);
                               in_fcount_allocated += ((input.array_size * input.component_count + 3) / 4 * 4);
                           }
                       },
                       [&](const NonDependentSamplerSampleSource &s) {
                           NonDependentTextureQueryInfo info;
                           info.sampler_name = sampler_map_on_binding[s.sampler_index].first;
                           info.sampler_cube = sampler_map_on_binding[s.sampler_index].second;
                           info.sampler_index = s.sampler_index;
                           info.coord_index = s.coord_index;
                           info.proj_pos = s.proj_pos;
                           info.data_type = static_cast<int>(input.type);
                           info.offset_in_pa = input.offset;

                           state.non_dependent_queries.push_back(info);
                       } },
            input.source);
    }

    if ((in_fcount_allocated == 0) && (program.primary_reg_count != 0)) {
        // Using hint to create attribute. Looks like attribute with F32 types are stripped, otherwise
        // whole shader symbols are kept...
        if (hint_attributes) {
            LOG_INFO("Shader stripped all symbols, trying to use hint attributes");

            for (std::size_t i = 0; i < hint_attributes->size(); i++) {
                VarToReg store_info;
                store_info.offset = hint_attributes->at(i).regIndex;
                store_info.comp_count = (hint_attributes->at(i).componentCount + 3) / 4 * 4;
                store_info.reg_format = false;
                store_info.var_name = fmt::format("attribute{}", i);

                var_to_regs.push_back(store_info);
            }
        }
    }

    // Store var to regs
    for (auto &var_to_reg : var_to_regs) {
        nicen_name_for_glsl_rules(var_to_reg.var_name);

        if (!var_to_reg.builtin) {
            writer.add_declaration(fmt::format("layout (location = {}) in vec4 {};", var_to_reg.location, var_to_reg.var_name));
        }

        Operand dest;
        dest.num = var_to_reg.offset;
        dest.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
        dest.bank = RegisterBank::PRIMATTR;
        dest.type = static_cast<DataType>(var_to_reg.data_type);

        if (is_integer_data_type(dest.type)) {
            var_to_reg.var_name = fmt::format("{}vec4({})", (is_signed_integer_data_type(dest.type) ? "i" : "u"), var_to_reg.var_name);
        }

        if (var_to_reg.comp_count == 4) {
            params.store(dest, var_to_reg.var_name, 0b1111, 0, true);
        } else {
            std::string swizz_str = std::string(".xyzw").substr(0, var_to_reg.comp_count + 1);
            params.store(dest, var_to_reg.var_name + swizz_str, 0b1111 >> (4 - var_to_reg.comp_count), 0, true);
        }
    }

    if (program.is_vertex()) {
        // Add vertex render info
        writer.add_declaration("layout (std140, binding = 2) uniform GxmRenderVertBufferBlock {");
        writer.add_declaration("\tvec4 viewport_flip;");
        writer.add_declaration("\tfloat viewport_flag;");
        writer.add_declaration("\tfloat screen_width;");
        writer.add_declaration("\tfloat screen_height;");
        writer.add_declaration("\tvec4 integral_query_formats[4];");
        writer.add_declaration("} renderVertInfo;");
    } else {
        // Add fragment render info
        writer.add_declaration("layout (std140, binding = 3) uniform GxmRenderFragBufferBlock {");
        writer.add_declaration("\tfloat back_disabled;");
        writer.add_declaration("\tfloat front_disabled;");
        writer.add_declaration("\tfloat writing_mask;");
        writer.add_declaration("\tfloat use_raw_image;");
        writer.add_declaration("\tvec4 integral_query_formats[4];");
        writer.add_declaration("\tint res_multiplier;");
        writer.add_declaration("} renderFragInfo;");

        create_fragment_inputs(writer, params, features, program);
    }

    writer.add_declaration("\n");
}

struct VertexProgramOutputProperties {
    std::string name;
    std::uint32_t component_count;
    std::uint32_t location;

    VertexProgramOutputProperties()
        : name(nullptr)
        , component_count(0)
        , location(0) {}

    VertexProgramOutputProperties(const char *name, std::uint32_t component_count, std::uint32_t location)
        : name(name)
        , component_count(component_count)
        , location(location) {}
};

using VertexProgramOutputPropertiesMap = std::map<SceGxmVertexProgramOutputs, VertexProgramOutputProperties>;

static void create_output(ProgramState &state, CodeWriter &writer, ShaderVariables &params, const FeatureState &features, const SceGxmProgram &program) {
    if (program.is_vertex()) {
        gxp::GxmVertexOutputTexCoordInfos coord_infos;
        SceGxmVertexProgramOutputs vertex_outputs = gxp::get_vertex_outputs(program, &coord_infos);

        static const auto calculate_copy_comp_count = [](uint8_t info) {
            // TexCoord info uses preset values described below for determining lengths.
            uint8_t length = 0;
            if (info & 0b001u)
                length += 2; // uses xy
            if (info & 0b010u)
                length += 1; // uses z
            if (info & 0b100u)
                length += 1; // uses w

            return length;
        };

        VertexProgramOutputPropertiesMap vertex_properties_map;

        // list is used here to gurantee the vertex outputs are written in right order
        std::list<SceGxmVertexProgramOutputs> vertex_outputs_list;
        const auto add_vertex_output_info = [&](SceGxmVertexProgramOutputs vo, const char *name, std::uint32_t component_count, std::uint32_t location) {
            vertex_properties_map.emplace(vo, VertexProgramOutputProperties(name, component_count, location));
            vertex_outputs_list.push_back(vo);
        };

        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION, "v_Position", 4, 0);
        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR0, "v_Color0", 4, 1);
        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR1, "v_Color1", 4, 2);
        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_FOG, "v_Fog", 2, 3);
        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD0, "v_TexCoord0", calculate_copy_comp_count(coord_infos[0]), 4);
        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD1, "v_TexCoord1", calculate_copy_comp_count(coord_infos[1]), 5);
        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD2, "v_TexCoord2", calculate_copy_comp_count(coord_infos[2]), 6);
        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD3, "v_TexCoord3", calculate_copy_comp_count(coord_infos[3]), 7);
        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD4, "v_TexCoord4", calculate_copy_comp_count(coord_infos[4]), 8);
        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD5, "v_TexCoord5", calculate_copy_comp_count(coord_infos[5]), 9);
        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD6, "v_TexCoord6", calculate_copy_comp_count(coord_infos[6]), 10);
        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD7, "v_TexCoord7", calculate_copy_comp_count(coord_infos[7]), 11);
        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD8, "v_TexCoord8", calculate_copy_comp_count(coord_infos[8]), 12);
        add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD9, "v_TexCoord9", calculate_copy_comp_count(coord_infos[9]), 13);
        // TODO: this should be translated to gl_PointSize
        // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_PSIZE, "v_Psize", 1);
        // TODO: these should be translated to gl_ClipDistance
        // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP0, "v_Clip0", 1);
        // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP1, "v_Clip1", 1);
        // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP2, "v_Clip2", 1);
        // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP3, "v_Clip3", 1);
        // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP4, "v_Clip4", 1);
        // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP5, "v_Clip5", 1);
        // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP6, "v_Clip6", 1);
        // add_vertex_output_info(SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP7, "v_Clip7", 1);

        Operand o_load;
        o_load.bank = RegisterBank::OUTPUT;
        o_load.num = 0;
        o_load.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
        o_load.type = DataType::F32;

        for (const auto vo : vertex_outputs_list) {
            if (vertex_outputs & vo) {
                const auto vo_typed = static_cast<SceGxmVertexProgramOutputs>(vo);
                VertexProgramOutputProperties properties = vertex_properties_map.at(vo_typed);

                std::string param_loaded = params.load(o_load, 0b1111, 0);

                if (vo_typed == SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION) {
                    writer.add_to_current_body(fmt::format("gl_Position = {} * renderVertInfo.viewport_flip;", param_loaded));
                    writer.add_to_current_body("if (renderVertInfo.viewport_flag < 0.5) {");
                    writer.indent_current_body();
                    writer.add_to_current_body("gl_Position.xy = gl_Position.xy * vec2(2.0 / renderVertInfo.screen_width, -2.0 / renderVertInfo.screen_height) + vec2(-1.0, 1.0);");
                    writer.add_to_current_body("gl_Position.z = gl_Position.w;");
                    writer.dedent_current_body();

                    // if (!features.spirv_shader) {
                    //  If we throw it to GL, we must adjust the Z clip space from 0.1 in vita to -1.1!
                    //  We use the same formula as RPCS3: (z * 2) - w
                    writer.add_to_current_body("} else {");
                    writer.indent_current_body();
                    writer.add_to_current_body("gl_Position.z = (gl_Position.z + gl_Position.z) - gl_Position.w;");
                    writer.dedent_current_body();
                    //}

                    writer.add_to_current_body("}");
                } else {
                    writer.add_declaration(fmt::format("layout (location = {}) out vec4 {};", properties.location, properties.name));
                    writer.add_to_current_body(fmt::format("{} = {};", properties.name, param_loaded));
                }

                o_load.num += properties.component_count;
            }
        }
    } else {
        Operand color_val_operand;
        color_val_operand.bank = program.is_native_color() ? RegisterBank::OUTPUT : RegisterBank::PRIMATTR;
        color_val_operand.num = 0;
        color_val_operand.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
        color_val_operand.type = std::get<0>(shader::get_parameter_type_store_and_name(program.get_fragment_output_type()));

        auto vertex_varyings_ptr = program.vertex_varyings();
        if (!program.is_native_color() && vertex_varyings_ptr->output_param_type == 1) {
            color_val_operand.num = vertex_varyings_ptr->fragment_output_start;
        }

        std::string result = params.load(color_val_operand, 0b1111, 0);
        if (is_unsigned_integer_data_type(color_val_operand.type)) {
            result = fmt::format("vec4(uvec4({})) / 255.0", result);
        }

        bool use_outs = true;

        if (program.is_frag_color_used()) {
            if (features.is_programmable_blending_need_to_bind_color_attachment()) {
                writer.add_to_current_body(fmt::format("imageStore(f_colorAttachment, ivec2(gl_FragCoord.xy), {});", result));

                if (features.preserve_f16_nan_as_u16) {
                    color_val_operand.type = DataType::UINT16;
                    result = params.load(color_val_operand, 0b1111, 0);

                    writer.add_to_current_body(fmt::format("imageStore(f_colorAttachment_rawUI, ivec2(gl_FragCoord.xy), {});", result));
                }

                use_outs = false;
            }
        }

        if (use_outs) {
            writer.add_declaration("layout (location = 0) out vec4 out_color;");
            writer.add_to_current_body(fmt::format("out_color = {};", result));

            if (features.preserve_f16_nan_as_u16) {
                color_val_operand.type = DataType::UINT16;
                result = params.load(color_val_operand, 0b1111, 0);

                writer.add_declaration("layout (location = 1) out uvec4 out_color_ui;");
                writer.add_to_current_body(fmt::format("out_color_ui = {};", result));
            }
        }
    }

    writer.add_declaration("\n");
}

static void create_neccessary_headers(CodeWriter &writer, const SceGxmProgram &program, const FeatureState &features, const std::string &shader_hash) {
    // Must at least be version 430
    writer.add_declaration("#version 430");
    writer.add_declaration("");
    writer.add_declaration(std::string("// Shader hash: ") + shader_hash);

    if (program.is_fragment() && program.is_frag_color_used()) {
        if (features.direct_fragcolor) {
            writer.add_declaration("#extension GL_EXT_shader_framebuffer_fetch: require");
        } else if (features.should_use_shader_interlock()) {
            writer.add_declaration("#extension GL_ARB_fragment_shader_interlock: require");
            writer.add_to_preload("beginInvocationInterlockARB();");
        }
    }

    writer.add_declaration("");
    if (program.is_fragment() && program.is_frag_color_used() && !features.direct_fragcolor) {
        writer.add_declaration("layout(early_fragment_tests) in;");
    }
}

static void create_neccessary_footers(CodeWriter &writer, const SceGxmProgram &program, const FeatureState &features) {
    if (program.is_fragment() && program.is_frag_color_used()) {
        if (features.should_use_shader_interlock()) {
            writer.add_to_current_body("endInvocationInterlockARB();");
        }
    }
}

static void create_program_needed_functions(const ProgramState &state, const ProgramInput &input, CodeWriter &writer) {
    if (state.should_generate_vld_func) {
        writer.add_declaration("float fetchMemory(int offset) {");
        writer.indent_declaration();

        const char *name_buffer_glob = (state.actual_program.is_vertex()) ? VERTEX_UB_GROUP_NAME : FRAGMENT_UB_GROUP_NAME;

        std::uint32_t base = 0;
        for (std::size_t i = 0; i < input.uniform_buffers.size(); i++) {
            std::uint32_t real_size = static_cast<std::uint32_t>((input.uniform_buffers[i].size + 3) / 4 * 16);
            writer.add_declaration(fmt::format("if ((offset >= {}) && (offset < {})) {{", base, base + real_size));
            writer.indent_declaration();
            if (base != 0)
                writer.add_declaration(fmt::format("offset -= {};", base));
            writer.add_declaration("int vec_index = offset / 16;");
            writer.add_declaration("int bytes_in_vec = offset - vec_index * 16;");
            writer.add_declaration("int comp_in_vec = bytes_in_vec / 4;");
            writer.add_declaration("int start_byte_in_comp = bytes_in_vec - comp_in_vec * 4;");
            writer.add_declaration("int lshift_amount = 4 - start_byte_in_comp;");
            writer.add_declaration("int next_comp = comp_in_vec + 1;");
            writer.add_declaration("bool aligned = (lshift_amount == 4);");
            writer.add_declaration(fmt::format("return uintBitsToFloat((floatBitsToUint({}.buffer{}[vec_index][comp_in_vec]) << uint(start_byte_in_comp * 8))"
                                               " | (aligned ? 0u : (floatBitsToUint({}.buffer{}[vec_index + (next_comp / 4)][next_comp - (next_comp / 4) * 4]) >> uint((aligned ? 0 : lshift_amount) * 8))));",
                name_buffer_glob, input.uniform_buffers[i].index, name_buffer_glob, input.uniform_buffers[i].index));
            writer.dedent_declaration();
            writer.add_declaration("}");
            base += real_size;
        }

        writer.add_declaration("return 0.0;");
        writer.dedent_declaration();
        writer.add_declaration("}\n");
    }
}
} // namespace shader::usse::glsl

namespace shader {
std::string convert_gxp_to_glsl(const SceGxmProgram &program, const std::string &shader_hash, const FeatureState &features,
    const std::vector<SceGxmVertexAttribute> *hint_attributes, bool maskupdate, bool force_shader_debug,
    std::function<bool(const std::string &ext, const std::string &dump)> dumper) {
    usse::glsl::ProgramState program_state{ program };
    usse::glsl::CodeWriter code_writer;

    usse::ProgramInput inputs = shader::get_program_input(program);
    usse::glsl::ShaderVariables variables(code_writer, inputs, program.is_vertex());

    std::string disasm_dump;
    usse::disasm::disasm_storage = &disasm_dump;

    usse::glsl::create_neccessary_headers(code_writer, program, features, shader_hash);
    usse::glsl::create_parameters(program_state, code_writer, variables, features, program, inputs, hint_attributes);

    usse::glsl::USSERecompilerGLSL recomp(program_state, code_writer, variables, features);

    if (program.secondary_program_instr_count) {
        recomp.visitor->set_secondary_program(true);
        recomp.reset(program.secondary_program_start(), program.secondary_program_instr_count);
        recomp.compile_to_function();
    }

    recomp.visitor->set_secondary_program(false);
    recomp.reset(program.primary_program_start(), program.primary_program_instr_count);
    recomp.compile_to_function();

    code_writer.set_active_body_type(usse::glsl::BODY_TYPE_POSTWORK);

    usse::glsl::create_output(program_state, code_writer, variables, features, program);
    usse::glsl::create_neccessary_footers(code_writer, program, features);

    code_writer.set_active_body_type(usse::glsl::BODY_TYPE_MAIN);

    usse::glsl::create_program_needed_functions(program_state, inputs, code_writer);

    variables.generate_helper_functions();
    std::string final_result = code_writer.assemble();

    if (dumper) {
        dumper(program.is_vertex() ? "vert" : "frag", final_result);
        dumper("dsm", disasm_dump);
    }

    LOG_INFO("GLSL Code:\n {}", final_result);

    return final_result;
}
} // namespace shader