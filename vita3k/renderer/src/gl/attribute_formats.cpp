#include <renderer/gl/functions.h>
#include <renderer/profile.h>

#include <util/log.h>

namespace renderer::gl {
GLenum attribute_format_to_gl_type(SceGxmAttributeFormat format) {
    R_PROFILE(__func__);

    switch (format) {
    case SCE_GXM_ATTRIBUTE_FORMAT_U8:
    case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
        return GL_UNSIGNED_BYTE;

    case SCE_GXM_ATTRIBUTE_FORMAT_S8:
    case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
        return GL_BYTE;
    case SCE_GXM_ATTRIBUTE_FORMAT_U16:
    case SCE_GXM_ATTRIBUTE_FORMAT_U16N:
        return GL_UNSIGNED_SHORT;
    case SCE_GXM_ATTRIBUTE_FORMAT_S16:
    case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
        return GL_SHORT;
    case SCE_GXM_ATTRIBUTE_FORMAT_F16:
        return GL_HALF_FLOAT;
    case SCE_GXM_ATTRIBUTE_FORMAT_F32:
        return GL_FLOAT;
    case SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED:
        LOG_WARN("Unsupported attribute format SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED");
        return GL_UNSIGNED_BYTE;
    }

    LOG_ERROR("Unsupported attribute format {}", log_hex(format));
    return GL_UNSIGNED_BYTE;
}

GLboolean attribute_format_normalised(SceGxmAttributeFormat format) {
    R_PROFILE(__func__);

    switch (format) {
    case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
    case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
    case SCE_GXM_ATTRIBUTE_FORMAT_U16N:
    case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
        return GL_TRUE;
    default:
        return GL_FALSE;
    }
}
} // namespace renderer::gl
