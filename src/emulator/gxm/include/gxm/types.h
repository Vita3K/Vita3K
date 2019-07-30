#pragma once

#include <mem/ptr.h>

#include <psp2/gxm.h>
#include <rpcs3/BitField.h>

#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>

#define SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX 0xE

namespace renderer {
struct FragmentProgram;
struct VertexProgram;
} // namespace renderer

namespace emu {
struct SceGxmBlendInfo {
    SceGxmColorMask colorMask : 8;
    SceGxmBlendFunc colorFunc : 4;
    SceGxmBlendFunc alphaFunc : 4;
    SceGxmBlendFactor colorSrc : 4;
    SceGxmBlendFactor colorDst : 4;
    SceGxmBlendFactor alphaSrc : 4;
    SceGxmBlendFactor alphaDst : 4;
};

static_assert(sizeof(SceGxmBlendInfo) == 4, "Incorrect size.");

struct SceGxmTexture {
    // Control Word 0
    uint32_t unk0 : 3;
    uint32_t vaddr_mode : 3;
    uint32_t uaddr_mode : 3;
    uint32_t mip_filter : 1;
    uint32_t min_filter : 2;
    uint32_t mag_filter : 2;
    uint32_t unk1 : 3;
    uint32_t mip_count : 4;
    uint32_t lod_bias : 6;
    uint32_t gamma_mode : 2;
    uint32_t unk2 : 2;
    uint32_t format0 : 1;
    // Control Word 1
    uint32_t height : 12;
    uint32_t width : 12;
    uint32_t base_format : 5;
    uint32_t type : 3;
    // Control Word 2
    uint32_t lod_min0 : 2;
    uint32_t data_addr : 30;
    // Control Word 3
    uint32_t palette_addr : 26;
    uint32_t lod_min1 : 2;
    uint32_t swizzle_format : 3;
    uint32_t normalize_mode : 1;

    uint32_t texture_type() const {
        return type << 29;
    }
};

struct SceGxmColorSurface {
    uint32_t pbeSidebandWord;
    uint32_t pbeEmitWords[6];
    uint32_t outputRegisterSize;
    emu::SceGxmTexture backgroundTex;
};

struct SceGxmDepthStencilSurface {
    uint32_t zlsControl;
    Ptr<void> depthData;
    Ptr<void> stencilData;
    float backgroundDepth;
    uint32_t backgroundControl;
};

struct SceGxmContextParams {
    Ptr<void> hostMem;
    uint32_t hostMemSize;
    Ptr<void> vdmRingBufferMem;
    uint32_t vdmRingBufferMemSize;
    Ptr<void> vertexRingBufferMem;
    uint32_t vertexRingBufferMemSize;
    Ptr<void> fragmentRingBufferMem;
    uint32_t fragmentRingBufferMemSize;
    Ptr<void> fragmentUsseRingBufferMem;
    uint32_t fragmentUsseRingBufferMemSize;
    uint32_t fragmentUsseRingBufferOffset;
};
} // namespace emu

typedef std::array<Ptr<void>, 16> UniformBuffers;
typedef std::array<Ptr<const void>, SCE_GXM_MAX_VERTEX_STREAMS> StreamDatas;

struct GxmViewport {
    SceGxmViewportMode enable = SCE_GXM_VIEWPORT_DISABLED;
    SceFVector3 offset;
    SceFVector3 scale;
};

struct GxmStencilState {
    SceGxmStencilFunc func = SCE_GXM_STENCIL_FUNC_NEVER; // TODO What's the default value?
    SceGxmStencilOp stencil_fail = SCE_GXM_STENCIL_OP_KEEP; // TODO What's the default value?
    SceGxmStencilOp depth_fail = SCE_GXM_STENCIL_OP_KEEP; // TODO What's the default value?
    SceGxmStencilOp depth_pass = SCE_GXM_STENCIL_OP_KEEP; // TODO What's the default value?
    uint8_t compare_mask = 0xff; // TODO What's the default value?
    uint8_t write_mask = 0xff; // TODO What's the default value?
    uint8_t ref = 0; // TODO What's the default value?
};

enum class SceGxmLastReserveStatus {
    Available = 0, // No reservations have happened since last allocation
    Reserved = 1, // A reservation succeeded and space needs to be allocated
    AvailableAgain = 2 // TODO: Not sure what's the point of this third state (why libgxm doesn't set this back to Available)
};

struct SceGxmSyncObject {
    std::uint32_t done;

    std::mutex lock;
    std::condition_variable cond;
};

struct GXMRecordState {
    // Programs.
    Ptr<const SceGxmFragmentProgram> fragment_program;
    Ptr<const SceGxmVertexProgram> vertex_program;
};

struct GxmContextState {
    // Constant after initialisation.
    emu::SceGxmContextParams params;

    // Surfaces.
    emu::SceGxmColorSurface color_surface;
    emu::SceGxmDepthStencilSurface depth_stencil_surface;

    // Clipping.
    SceGxmRegionClipMode region_clip_mode = SCE_GXM_REGION_CLIP_NONE;
    SceIVector2 region_clip_min;
    SceIVector2 region_clip_max;
    GxmViewport viewport;

    // Triangle setup?
    SceGxmCullMode cull_mode = SCE_GXM_CULL_NONE;
    SceGxmTwoSidedMode two_sided = SCE_GXM_TWO_SIDED_DISABLED;

    // Programs.
    Ptr<const SceGxmFragmentProgram> fragment_program;
    Ptr<const SceGxmVertexProgram> vertex_program;

    Ptr<const SceGxmFragmentProgram> last_draw_fragment_program;
    Ptr<const SceGxmVertexProgram> last_draw_vertex_program;

    // Uniforms.
    UniformBuffers fragment_uniform_buffers;
    UniformBuffers vertex_uniform_buffers;
    size_t fragment_ring_buffer_used = 0;
    size_t vertex_ring_buffer_used = 0;
    SceGxmLastReserveStatus fragment_last_reserve_status = SceGxmLastReserveStatus::Available;
    SceGxmLastReserveStatus vertex_last_reserve_status = SceGxmLastReserveStatus::Available;

    // Vertex streams.
    StreamDatas stream_data;

    // Depth.
    SceGxmDepthFunc front_depth_func = SCE_GXM_DEPTH_FUNC_LESS_EQUAL;
    SceGxmDepthFunc back_depth_func = SCE_GXM_DEPTH_FUNC_LESS_EQUAL;
    SceGxmDepthWriteMode front_depth_write_enable = SCE_GXM_DEPTH_WRITE_ENABLED;
    SceGxmDepthWriteMode back_depth_write_enable = SCE_GXM_DEPTH_WRITE_ENABLED;

    // Stencil.
    GxmStencilState front_stencil;
    GxmStencilState back_stencil;

    // Polygon Mode
    SceGxmPolygonMode front_polygon_mode = SCE_GXM_POLYGON_MODE_TRIANGLE_FILL;
    SceGxmPolygonMode back_polygon_mode = SCE_GXM_POLYGON_MODE_TRIANGLE_FILL;

    // Line Width
    unsigned int front_point_line_width = 1;
    unsigned int back_point_line_width = 1;

    // Depth Bias
    int front_depth_bias_factor = 0;
    int front_depth_bias_units = 0;
    int back_depth_bias_factor = 0;
    int back_depth_bias_units = 0;

    // Textures.
    std::array<emu::SceGxmTexture, SCE_GXM_MAX_TEXTURE_UNITS> fragment_textures;

    // Fragment Sync Object
    Ptr<SceGxmSyncObject> fragment_sync_object;
};

struct SceGxmFragmentProgram {
    size_t reference_count = 1;
    Ptr<const SceGxmProgram> program;
    std::unique_ptr<renderer::FragmentProgram> renderer_data;
};

namespace emu {
struct SceGxmNotification {
    Ptr<volatile uint32_t> address;
    uint32_t value;
};
// identical to ::SceGxmProgramType for now
// see SceGxmProgram.type field for details
enum SceGxmProgramType : std::uint8_t {
    Vertex = 0,
    Fragment = 1
};
} // namespace emu

enum SceGxmVertexProgramOutputs : int {
    _SCE_GXM_VERTEX_PROGRAM_OUTPUT_INVALID = 0,

    SCE_GXM_VERTEX_PROGRAM_OUTPUT_POSITION = 1 << 0,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_FOG = 1 << 1,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR0 = 1 << 2,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_COLOR1 = 1 << 3,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD0 = 1 << 4,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD1 = 1 << 5,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD2 = 1 << 6,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD3 = 1 << 7,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD4 = 1 << 8,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD5 = 1 << 9,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD6 = 1 << 10,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD7 = 1 << 11,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD8 = 1 << 12,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD9 = 1 << 13,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_PSIZE = 1 << 14,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP0 = 1 << 15,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP1 = 1 << 16,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP2 = 1 << 17,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP3 = 1 << 18,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP4 = 1 << 19,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP5 = 1 << 20,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP6 = 1 << 21,
    SCE_GXM_VERTEX_PROGRAM_OUTPUT_CLIP7 = 1 << 22,

    _SCE_GXM_VERTEX_PROGRAM_OUTPUT_LAST = 1 << 23
};

union SceGxmVertexOutputTexCoordInfo {
    bf_t<uint8_t, 0, 2> comp_count;
    bf_t<uint8_t, 2, 1> type;
};

using SceGxmVertexOutputTexCoordInfos = std::array<SceGxmVertexOutputTexCoordInfo, 10>;

#pragma pack(push, 1)
struct SceGxmProgramVertexOutput {
    std::uint8_t unk0[10];
    std::uint8_t output_param_type;
    std::uint8_t output_comp_count;

    std::uint16_t varyings_count;
    std::uint16_t pad0; // padding maybe
    std::uint32_t vertex_outputs1; // includes everything except texcoord outputs
    std::uint32_t vertex_outputs2; // includes texcoord outputs
};
#pragma pack(pop)

static_assert(sizeof(SceGxmProgramVertexOutput) == 24);

struct SceGxmProgramAttributeDescriptor {
    std::uint32_t attribute_info;
    std::uint32_t resource_index;
    std::uint32_t size;
    std::uint32_t component_info; ///< Total components and type
};

struct SceGxmDependentSampler {
    std::uint16_t resource_index_layout_offset; ///< The resource index of the sampler, in range of [index * 4, (index + 1) * 4)
    std::uint16_t sa_offset; ///< The SA offset correspond to the sampler
};

enum SceGxmFragmentProgramInputs : int {
    _SCE_GXM_FRAGMENT_PROGRAM_INPUT_NONE = 0,

    SCE_GXM_FRAGMENT_PROGRAM_INPUT_POSITION = 1 << 0,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_FOG = 1 << 1,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_COLOR0 = 1 << 2,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_COLOR1 = 1 << 3,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD0 = 1 << 4,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD1 = 1 << 5,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD2 = 1 << 6,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD3 = 1 << 7,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD4 = 1 << 8,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD5 = 1 << 9,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD6 = 1 << 10,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD7 = 1 << 11,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD8 = 1 << 12,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_TEXCOORD9 = 1 << 13,
    SCE_GXM_FRAGMENT_PROGRAM_INPUT_SPRITECOORD = 1 << 14,

    _SCE_GXM_FRAGMENT_PROGRAM_INPUT_LAST = 1 << 15
};

struct SceGxmProgram {
    std::uint32_t magic; // should be "GXP\0"

    std::uint8_t major_version; //min 1
    std::uint8_t minor_version; //min 4
    std::uint16_t unk6; //maybe padding

    std::uint32_t size; //size of file - ignoring padding bytes at the end after SceGxmProgramParameter table
    std::uint32_t unkC;

    std::uint16_t unk10;
    std::uint8_t unk12;
    std::uint8_t unk13;

private:
    std::uint8_t type{ 0 }; // shader profile, seems to contain more info in bits after the first(bitfield?)
public:
    std::uint8_t unk15;
    std::uint8_t unk16;
    std::uint8_t unk17;

    std::uint32_t unk18;
    std::uint32_t unk1C;

    std::uint32_t unk20; // bit 6 denotes whether a frag shader writes directly to output (usees __nativecolor modifier) or not
    std::uint32_t parameter_count;
    std::uint32_t parameters_offset; // Number of bytes from the start of this field to the first parameter.
    std::uint32_t varyings_offset; // offset to vertex outputs / fragment inputs, relative to this field

    std::uint16_t primary_reg_count; // (PAs)
    std::uint16_t secondary_reg_count; // (SAs)
    std::uint16_t temp_reg_count1; //not sure // - verify this
    std::uint16_t unk36;
    std::uint16_t temp_reg_count2; //not sure // - verify this
    std::uint16_t unk3A; //some item count?

    std::uint32_t primary_program_instr_count;
    std::uint32_t primary_program_offset;

    std::uint32_t unk44;

    std::uint32_t secondary_program_offset; // relative to the beginning of this field
    std::uint32_t secondary_program_offset_end; // relative to the beginning of this field

    std::uint32_t unk_50; //usually zero?
    std::uint32_t unk_54; //usually zero?
    std::uint32_t unk_58; //usually zero?
    std::uint32_t unk_5C; //usually zero?

    std::uint32_t unk_60;
    std::uint32_t default_uniform_buffer_count;
    std::uint32_t unk_68;
    std::uint32_t unk_6C;

    std::uint32_t literals_count;
    std::uint32_t literals_offset;
    std::uint32_t unk_78;
    std::uint32_t alternative_parameters_offset; // Point to the same thing as the upper field.

    std::uint32_t dependent_sampler_count;
    std::uint32_t dependent_sampler_offset;
    std::uint32_t unk_88;
    std::uint32_t unk_8C;
    std::uint32_t container_count;
    std::uint32_t container_offset;

    emu::SceGxmProgramType get_type() const {
        return static_cast<emu::SceGxmProgramType>(type & 1);
    }
    bool is_vertex() const {
        return get_type() == emu::SceGxmProgramType::Vertex;
    }
    bool is_fragment() const {
        return get_type() == emu::SceGxmProgramType::Fragment;
    }
    uint64_t *primary_program_start() const {
        return (uint64_t *)((uint8_t *)&primary_program_offset + primary_program_offset);
    }
    uint64_t *secondary_program_start() const {
        return (uint64_t *)((uint8_t *)&secondary_program_offset + secondary_program_offset);
    }
    uint64_t *secondary_program_end() const {
        return (uint64_t *)((uint8_t *)&secondary_program_offset_end + secondary_program_offset_end);
    }
    bool is_native_color() const {
        return ((type >> 6) & 1);
    }
    bool is_reg_format() const {
        return ((type >> 7) & 1);
    }
    SceGxmParameterType get_fragment_output_type() const {
        return static_cast<const SceGxmParameterType>(reinterpret_cast<const SceGxmProgramVertexOutput *>(
            reinterpret_cast<const std::uint8_t *>(&varyings_offset) + varyings_offset)
                                                          ->output_param_type);
    }
    std::uint8_t get_fragment_output_component_count() const {
        return reinterpret_cast<const SceGxmProgramVertexOutput *>(reinterpret_cast<const std::uint8_t *>(&varyings_offset) + varyings_offset)->output_comp_count;
    }
};

struct SceGxmProgramParameter {
    int32_t name_offset; // Number of bytes from the start of this structure to the name string.
    union {
        bf_t<uint16_t, 0, 4> category; // SceGxmParameterCategory
        bf_t<uint16_t, 4, 4> type; // SceGxmParameterType - applicable for constants, not applicable for samplers (select type like float, half, fixed ...)
        bf_t<uint16_t, 8, 4> component_count; // applicable for constants, not applicable for samplers (select size like float2, float3, float3 ...)
        bf_t<uint16_t, 12, 4> container_index; // applicable for constants, not applicable for samplers (buffer, default, texture)
    };
    uint16_t semantic; // applicable only for for vertex attributes, for everything else it's 0
    uint32_t array_size;
    int32_t resource_index;

    bool is_sampler_cube() const {
        return (semantic >> 12) & 1;
    }
};

static_assert(sizeof(SceGxmProgramParameter) == 16, "Incorrect structure layout.");

struct SceGxmProgramParameterContainer {
    uint16_t container_index;
    uint16_t unk02;
    uint16_t base_sa_offset;
    uint16_t max_resource_index;
};

struct SceGxmRegisteredProgram {
    // TODO This is an opaque type.
    Ptr<const SceGxmProgram> program;
};

namespace emu {
typedef Ptr<SceGxmRegisteredProgram> SceGxmShaderPatcherId;

typedef Ptr<void> SceGxmShaderPatcherHostAllocCallback(Ptr<void> userData, uint32_t size);
typedef void SceGxmShaderPatcherHostFreeCallback(Ptr<void> userData, Ptr<void> mem);
typedef Ptr<void> SceGxmShaderPatcherBufferAllocCallback(Ptr<void> userData, uint32_t size);
typedef void SceGxmShaderPatcherBufferFreeCallback(Ptr<void> userData, Ptr<void> mem);
typedef Ptr<void> SceGxmShaderPatcherUsseAllocCallback(Ptr<void> userData, uint32_t size, Ptr<uint32_t> usseOffset);
typedef void SceGxmShaderPatcherUsseFreeCallback(Ptr<void> userData, Ptr<void> mem);

struct SceGxmShaderPatcherParams {
    Ptr<void> userData;
    Ptr<SceGxmShaderPatcherHostAllocCallback> hostAllocCallback;
    Ptr<SceGxmShaderPatcherHostFreeCallback> hostFreeCallback;
    Ptr<SceGxmShaderPatcherBufferAllocCallback> bufferAllocCallback;
    Ptr<SceGxmShaderPatcherBufferFreeCallback> bufferFreeCallback;
    Ptr<void> bufferMem;
    uint32_t bufferMemSize;
    Ptr<SceGxmShaderPatcherUsseAllocCallback> vertexUsseAllocCallback;
    Ptr<SceGxmShaderPatcherUsseFreeCallback> vertexUsseFreeCallback;
    Ptr<void> vertexUsseMem;
    uint32_t vertexUsseMemSize;
    uint32_t vertexUsseOffset;
    Ptr<SceGxmShaderPatcherUsseAllocCallback> fragmentUsseAllocCallback;
    Ptr<SceGxmShaderPatcherUsseFreeCallback> fragmentUsseFreeCallback;
    Ptr<void> fragmentUsseMem;
    uint32_t fragmentUsseMemSize;
    uint32_t fragmentUsseOffset;
};
} // namespace emu

namespace emu {
struct SceGxmVertexAttribute {
    uint16_t streamIndex;
    uint16_t offset;
    uint8_t format; // SceGxmAttributeFormat.
    uint8_t componentCount;
    uint16_t regIndex; // Returned from sceGxmProgramParameterGetResourceIndex().
};

static_assert(sizeof(SceGxmVertexAttribute) == 8, "Structure has been incorrectly packed.");
} // namespace emu

struct SceGxmVertexProgram {
    size_t reference_count = 1;
    Ptr<const SceGxmProgram> program;
    std::vector<SceGxmVertexStream> streams;
    std::vector<emu::SceGxmVertexAttribute> attributes;
    std::unique_ptr<renderer::VertexProgram> renderer_data;
};

namespace gxp {
// Used to map GXM program parameters to GLSL data types
enum class GenericParameterType {
    Scalar,
    Vector,
    Matrix,
    Array
};
} // namespace gxp
