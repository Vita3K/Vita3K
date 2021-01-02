#include <gxm/functions.h>

namespace gxm {
size_t get_width(const SceGxmTexture *texture) {
    if (texture->type << 29 != SCE_GXM_TEXTURE_SWIZZLED) {
        return texture->width + 1;
    }
    return 1ull << (texture->width & 0xF);
}

size_t get_height(const SceGxmTexture *texture) {
    if (texture->type << 29 != SCE_GXM_TEXTURE_SWIZZLED) {
        return texture->height + 1;
    }
    return 1ull << (texture->height & 0xF);
}

SceGxmTextureFormat get_format(const SceGxmTexture *texture) {
    return static_cast<SceGxmTextureFormat>(
        texture->base_format << 24 | texture->format0 << 31 | texture->swizzle_format << 12);
}

SceGxmTextureBaseFormat get_base_format(SceGxmTextureFormat src) {
    return static_cast<SceGxmTextureBaseFormat>(src & SCE_GXM_TEXTURE_BASE_FORMAT_MASK);
}

size_t get_stride_in_bytes(const SceGxmTexture *texture) {
    return ((texture->mip_filter | (texture->min_filter << 1) | (texture->mip_count << 3) | (texture->lod_bias << 7)) + 1) * 4;
}

bool is_block_compressed_format(SceGxmTextureFormat src) {
    const auto base_format = get_base_format(src);

    return ((base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC1)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC2)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC3)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC4)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_SBC4)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC5)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_SBC5));
}

bool is_paletted_format(SceGxmTextureFormat src) {
    const auto base_format = get_base_format(src);

    return base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P8 || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P4;
}

bool is_yuv_format(SceGxmTextureFormat src) {
    const auto base_format = get_base_format(src);

    return ((base_format == SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_YUV422));
}
} // namespace gxm
