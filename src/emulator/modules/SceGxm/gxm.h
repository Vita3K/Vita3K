#pragma once

#include <glutil/object.h>
#include <glutil/object_array.h>
#include <mem/ptr.h>
#include <util/types.h>

#include <psp2/gxm.h>

#include <SDL_video.h>

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
        //u32 controlWords[4];
        SceGxmTextureFormat format;
        u16 width;
        u16 height;
        Ptr<const void> data;
        Ptr<void> palette;
    };

    static_assert(sizeof(SceGxmTexture) == 16, "Incorrect size.");

    struct SceGxmColorSurface {
        u32 pbeSidebandWord;
        u32 pbeEmitWords[6];
        u32 outputRegisterSize;
        SceGxmTexture backgroundTex;
    };

    struct SceGxmContextParams {
        Ptr<void> hostMem;
        u32 hostMemSize;
        Ptr<void> vdmRingBufferMem;
        u32 vdmRingBufferMemSize;
        Ptr<void> vertexRingBufferMem;
        u32 vertexRingBufferMemSize;
        Ptr<void> fragmentRingBufferMem;
        u32 fragmentRingBufferMemSize;
        Ptr<void> fragmentUsseRingBufferMem;
        u32 fragmentUsseRingBufferMemSize;
        u32 fragmentUsseRingBufferOffset;
    };
}

typedef std::unique_ptr<void, std::function<void(SDL_GLContext)>> GLContextPtr;

struct SceGxmContext {
    // This is an opaque type.
    emu::SceGxmContextParams params;
    GLContextPtr gl;
    size_t fragment_ring_buffer_used = 0;
    size_t vertex_ring_buffer_used = 0;
    emu::SceGxmColorSurface color_surface;
    const SceGxmVertexProgram *vertex_program = nullptr;
    GLObjectArray<1> texture;
    SceGxmCullMode cull_mode = SCE_GXM_CULL_NONE;
};

namespace emu {
    struct SceGxmDepthStencilSurface {
        u32 zlsControl;
        Ptr<void> depthData;
        Ptr<void> stencilData;
        float backgroundDepth;
        u32 backgroundControl;
    };
}

struct SceGxmFragmentProgram {
    // TODO This is an opaque type.
    GLObject program;
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
        Ptr<volatile u32> address;
        u32 value;
    };
}

struct SceGxmProgram {
    char magic[4];
    char maybe_version[2];
    char maybe_padding[2];
    u32 size;
    u8 unknown1[8];
    u16 maybe_type;
    u16 unknown2[7];
    u32 parameter_count;
    u32 parameters_offset; // Number of bytes from the start of this field to the first parameter.
};

struct SceGxmProgramParameter {
    s32 name_offset; // Number of bytes from the start of this structure to the name string.
    u8 category; // SceGxmParameterCategory.
    u8 container_index : 4;
    u8 component_count : 4;
    u8 unknown1[2];
    u8 array_size;
    u8 unknown2[3];
    u8 resource_index;
    u8 unknown3[3];
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

typedef std::shared_ptr<GLObject> GLObjectPtr;
typedef std::map<Ptr<const SceGxmProgram>, GLObjectPtr> ProgramToVertexShader;

struct SceGxmShaderPatcher {
    // TODO This is an opaque struct.
    ProgramToVertexShader vertex_shaders;
};

namespace emu {
    typedef Ptr<SceGxmRegisteredProgram> SceGxmShaderPatcherId;

    typedef Ptr<void> SceGxmShaderPatcherHostAllocCallback(Ptr<void> userData, u32 size);
    typedef void SceGxmShaderPatcherHostFreeCallback(Ptr<void> userData, Ptr<void> mem);
    typedef Ptr<void> SceGxmShaderPatcherBufferAllocCallback(Ptr<void> userData, u32 size);
    typedef void SceGxmShaderPatcherBufferFreeCallback(Ptr<void> userData, Ptr<void> mem);
    typedef Ptr<void> SceGxmShaderPatcherUsseAllocCallback(Ptr<void> userData, u32 size, Ptr<u32> usseOffset);
    typedef void SceGxmShaderPatcherUsseFreeCallback(Ptr<void> userData, Ptr<void> mem);

    struct SceGxmShaderPatcherParams {
        Ptr<void> userData;
        Ptr<SceGxmShaderPatcherHostAllocCallback> hostAllocCallback;
        Ptr<SceGxmShaderPatcherHostFreeCallback> hostFreeCallback;
        Ptr<SceGxmShaderPatcherBufferAllocCallback> bufferAllocCallback;
        Ptr<SceGxmShaderPatcherBufferFreeCallback> bufferFreeCallback;
        Ptr<void> bufferMem;
        u32 bufferMemSize;
        Ptr<SceGxmShaderPatcherUsseAllocCallback> vertexUsseAllocCallback;
        Ptr<SceGxmShaderPatcherUsseFreeCallback> vertexUsseFreeCallback;
        Ptr<void> vertexUsseMem;
        u32 vertexUsseMemSize;
        u32 vertexUsseOffset;
        Ptr<SceGxmShaderPatcherUsseAllocCallback> fragmentUsseAllocCallback;
        Ptr<SceGxmShaderPatcherUsseFreeCallback> fragmentUsseFreeCallback;
        Ptr<void> fragmentUsseMem;
        u32 fragmentUsseMemSize;
        u32 fragmentUsseOffset;
    };
}

struct SceGxmSyncObject {
};

namespace emu {
    struct SceGxmVertexAttribute {
        u16 streamIndex;
        u16 offset;
        u8 format; // SceGxmAttributeFormat.
        u8 componentCount;
        u16 regIndex; // Returned from sceGxmProgramParameterGetResourceIndex().
    };

    static_assert(sizeof(SceGxmVertexAttribute) == 8, "Structure has been incorrectly packed.");
}

struct SceGxmVertexProgram {
    // TODO I think this is an opaque type.
    GLObjectPtr shader = std::make_shared<GLObject>();
    std::vector<SceGxmVertexStream> streams;
    std::vector<emu::SceGxmVertexAttribute> attributes;
};
