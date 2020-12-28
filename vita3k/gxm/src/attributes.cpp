#include <gxm/functions.h>
#include <util/log.h>

namespace gxm {
size_t attribute_format_size(SceGxmAttributeFormat format) {
    switch (format) {
    case SCE_GXM_ATTRIBUTE_FORMAT_U8:
    case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
    case SCE_GXM_ATTRIBUTE_FORMAT_S8:
    case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
        return 1;
    case SCE_GXM_ATTRIBUTE_FORMAT_U16:
    case SCE_GXM_ATTRIBUTE_FORMAT_U16N:
    case SCE_GXM_ATTRIBUTE_FORMAT_S16:
    case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
    case SCE_GXM_ATTRIBUTE_FORMAT_F16:
        return 2;
    case SCE_GXM_ATTRIBUTE_FORMAT_F32:
        return 4;
    default:
        LOG_ERROR("Unsupported attribute format {}", log_hex(format));
        return 4;
    }
}

size_t index_element_size(SceGxmIndexFormat format) {
    return (format == SCE_GXM_INDEX_FORMAT_U16) ? 2 : 4;
}
} // namespace gxm