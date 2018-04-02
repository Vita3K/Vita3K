#pragma once

#include <crypto/hash.h>
#include <condition_variable>
#include <glutil/object.h>
#include <glutil/object_array.h>
#include <mem/ptr.h>
#include <mutex>

#include <psp2/gxm.h>
#include <SDL_video.h>

#include <array>
#include <map>
#include <tuple>

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

    struct SceGxmColorSurface {
        uint32_t pbeSidebandWord;
        uint32_t pbeEmitWords[6];
        uint32_t outputRegisterSize;
        SceGxmTexture backgroundTex;
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
}

typedef std::unique_ptr<void, std::function<void(SDL_GLContext)>> GLContextPtr;
typedef std::shared_ptr<GLObject> SharedGLObject;
typedef std::tuple<std::string, std::string> ProgramGLSLs;
typedef std::map<ProgramGLSLs, SharedGLObject> ProgramCache;
typedef std::array<Ptr<void>, 16> UniformBuffers;

struct SceGxmContext {
    // This is an opaque type.
    emu::SceGxmContextParams params;
    GLContextPtr gl;
    size_t fragment_ring_buffer_used = 0;
    size_t vertex_ring_buffer_used = 0;
    emu::SceGxmColorSurface color_surface;
    ProgramCache program_cache;
    Ptr<const SceGxmFragmentProgram> fragment_program;
    Ptr<const SceGxmVertexProgram> vertex_program;
    UniformBuffers fragment_uniform_buffers;
    UniformBuffers vertex_uniform_buffers;
    GLObjectArray<1> texture;
    SceGxmCullMode cull_mode = SCE_GXM_CULL_NONE;
};

namespace emu {
    struct SceGxmDepthStencilSurface {
        uint32_t zlsControl;
        Ptr<void> depthData;
        Ptr<void> stencilData;
        float backgroundDepth;
        uint32_t backgroundControl;
    };
}

struct SceGxmFragmentProgram {
    size_t reference_count = 1;
    
    Ptr<const SceGxmProgram> program;
    std::string glsl;
    
    GLboolean color_mask_red = GL_TRUE;
    GLboolean color_mask_green = GL_TRUE;
    GLboolean color_mask_blue = GL_TRUE;
    GLboolean color_mask_alpha = GL_TRUE;
    bool blend_enabled = false;
    GLenum color_func = GL_ADD;
    GLenum alpha_func = GL_ADD;
    GLenum color_src = GL_ONE;
    GLenum color_dst = GL_ZERO;
    GLenum alpha_src = GL_ONE;
    GLenum alpha_dst = GL_ZERO;
};

namespace emu {
    struct SceGxmNotification {
        Ptr<volatile uint32_t> address;
        uint32_t value;
    };
}

struct SceGxmProgram {
    char magic[4];
    char maybe_version[2];
    char maybe_padding[2];
    uint32_t size;
    uint8_t unknown1[8];
    uint16_t maybe_type;
    uint16_t unknown2[7];
    uint32_t parameter_count;
    uint32_t parameters_offset; // Number of bytes from the start of this field to the first parameter.
};

struct SceGxmProgramParameter {
    int32_t name_offset; // Number of bytes from the start of this structure to the name string.
    uint16_t category : 4; // SceGxmParameterCategory.
    uint16_t type : 4; // SceGxmParameterType.
    uint16_t component_count : 4;
    uint16_t container_index : 4;
    uint16_t unknown; // Maybe relevant to SCE_GXM_PARAMETER_CATEGORY_AUXILIARY_SURFACE or SCE_GXM_PARAMETER_CATEGORY_UNIFORM_BUFFER.
    uint32_t array_size;
    int32_t resource_index;
};

static_assert(sizeof(SceGxmProgramParameter) == 16, "Incorrect structure layout.");

struct SceGxmRegisteredProgram {
    // TODO This is an opaque type.
    Ptr<const SceGxmProgram> program;
};

struct SceGxmRenderTarget {
    GLObjectArray<2> renderbuffers;
    GLObjectArray<1> framebuffer;
};

struct FragmentProgramCacheKey {
    SceGxmRegisteredProgram fragment_program;
    emu::SceGxmBlendInfo blend_info;
    Ptr<const SceGxmProgram> vertex_program;
};

typedef std::map<FragmentProgramCacheKey, Ptr<SceGxmFragmentProgram>> FragmentProgramCache;
typedef std::map<Sha256Hash, std::string> GLSLCache;

struct SceGxmShaderPatcher {
    // TODO This is an opaque struct.
    FragmentProgramCache fragment_program_cache;
    GLSLCache fragment_glsl_cache;
    GLSLCache vertex_glsl_cache;
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
}

struct SceGxmSyncObject {
    std::mutex mutex;
    std::condition_variable cond_var;
    int value;
};

namespace emu {
    struct SceGxmVertexAttribute {
        uint16_t streamIndex;
        uint16_t offset;
        uint8_t format; // SceGxmAttributeFormat.
        uint8_t componentCount;
        uint16_t regIndex; // Returned from sceGxmProgramParameterGetResourceIndex().
    };

    static_assert(sizeof(SceGxmVertexAttribute) == 8, "Structure has been incorrectly packed.");
}

typedef std::map<GLuint, std::string> AttributeLocations;

struct SceGxmVertexProgram {
    // TODO I think this is an opaque type.
    size_t reference_count = 1;
    
    Ptr<const SceGxmProgram> program;
    std::string glsl;
    
    AttributeLocations attribute_locations;
    std::vector<SceGxmVertexStream> streams;
    std::vector<emu::SceGxmVertexAttribute> attributes;
};
