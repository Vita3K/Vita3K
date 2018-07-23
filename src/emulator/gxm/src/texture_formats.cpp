#include <gxm/functions.h>

namespace texture {
    SceGxmTextureFormat get_format(const SceGxmTexture *texture) {
        return static_cast<SceGxmTextureFormat>(
            texture->base_format << 24 | texture->format0 << 31 | texture->swizzle_format << 12);
    }

    SceGxmTextureBaseFormat get_base_format(SceGxmTextureFormat src) {
        return static_cast<SceGxmTextureBaseFormat>(src & 0xFF000000);
    }

    bool is_paletted_format(SceGxmTextureFormat src) {
        const auto base_format = get_base_format(src);

        return base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P8 || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P4;
    }
} // namespace texture
