#pragma once

#include <crypto/hash.h>
#include <dlmalloc.h>
#include <glutil/object.h>
#include <glutil/object_array.h>
#include <gxm/types.h>
#include <renderer/commands.h>

#include <array>
#include <map>
#include <string>
#include <tuple>
#include <vector>

static constexpr auto DEFAULT_RES_WIDTH = 960;
static constexpr auto DEFAULT_RES_HEIGHT = 544;

struct SceGxmProgram;

struct SDL_Window;

using UniformBufferSizes = std::array<std::uint32_t, 15>;

namespace renderer {

using ContextAllocFunc = std::function<void *(std::size_t)>;
using ContextFreeFunc = std::function<void(void *)>;

typedef std::map<GLuint, std::string> AttributeLocations;
typedef std::map<std::string, SharedGLObject> ShaderCache;
typedef std::tuple<std::string, std::string> ProgramHashes;
typedef std::map<ProgramHashes, SharedGLObject> ProgramCache;
typedef std::vector<std::string> ExcludedUniforms; // vector instead of unordered_set since it's much faster for few elements
typedef std::map<GLuint, GLenum> UniformTypes;

// State types
typedef std::map<Sha256Hash, const SceGxmProgram *> GXPPtrMap;

struct CommandBuffer;

enum class Backend {
    OpenGL,
#ifdef USE_VULKAN
    Vulkan,
#endif
};

enum class GXMState : std::uint16_t {
    RegionClip = 0,
    Program = 1,
    Viewport = 2,
    DepthBias = 3,
    DepthFunc = 4,
    DepthWriteEnable = 5,
    PolygonMode = 6,
    PointLineWidth = 7,
    StencilFunc = 8,
    FragmentTexture = 9,
    StencilRef = 10,
    VertexStream = 11,
    TwoSided = 12,
    CullMode = 13,
    Uniform = 14,
    UniformBuffer = 15,
    FragmentProgramEnable = 16,
    TotalState
};

enum SyncObjectSubject : std::uint32_t {
    None = 0,
    Fragment = 1 << 0,
    DisplayQueue = 1 << 1
};

struct RenderTarget;

struct GxmRecordState {
    // Programs.
    Ptr<const SceGxmFragmentProgram> fragment_program;
    Ptr<const SceGxmVertexProgram> vertex_program;

    SceGxmColorSurface color_surface;
    SceGxmDepthStencilSurface depth_stencil_surface;

    SceGxmCullMode cull_mode = SCE_GXM_CULL_NONE;
    SceGxmTwoSidedMode two_sided = SCE_GXM_TWO_SIDED_DISABLED;
    SceGxmRegionClipMode region_clip_mode = SCE_GXM_REGION_CLIP_OUTSIDE;

    SceIVector2 region_clip_min;
    SceIVector2 region_clip_max;

    GxmStencilState front_stencil_state;
    GxmStencilState back_stencil_state;

    SceGxmDepthFunc front_depth_func = SCE_GXM_DEPTH_FUNC_LESS_EQUAL;
    SceGxmDepthFunc back_depth_func = SCE_GXM_DEPTH_FUNC_LESS_EQUAL;

    SceGxmDepthWriteMode front_depth_write_mode = SCE_GXM_DEPTH_WRITE_ENABLED;
    SceGxmDepthWriteMode back_depth_write_mode = SCE_GXM_DEPTH_WRITE_ENABLED;

    SceGxmFragmentProgramMode front_side_fragment_program_mode = SCE_GXM_FRAGMENT_PROGRAM_ENABLED;
    SceGxmFragmentProgramMode back_side_fragment_program_mode = SCE_GXM_FRAGMENT_PROGRAM_ENABLED;

    bool writing_mask = false;
    bool viewport_flat = false;
};

struct Context {
    const RenderTarget *current_render_target{};
    GxmRecordState record;

    CommandList command_list;
    int render_finish_status = 0;

    mspace alloc_space = nullptr;

    std::string last_draw_fragment_program_hash;
    std::string last_draw_vertex_program_hash;
};

struct ShaderProgram {
    std::string hash;
    UniformBufferSizes uniform_buffer_sizes;
};

struct FragmentProgram : ShaderProgram {
};

struct VertexProgram : ShaderProgram {
};

struct RenderTarget {
    int holder;
};

} // namespace renderer
