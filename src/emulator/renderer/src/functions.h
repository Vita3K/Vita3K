#pragma once

#include <glutil/gl.h>

#include <psp2/gxm.h>

struct MemState;

namespace renderer {
    struct TextureCacheState;

    // Texture cache.
    bool init(TextureCacheState &cache);
    void cache_and_bind_texture(TextureCacheState &cache, const SceGxmTexture &gxm_texture, const MemState &mem, bool enabled);

    // Texture formats.
    const GLenum *translate_swizzle(SceGxmTextureFormat fmt);
    GLenum translate_internal_format(SceGxmTextureFormat src);
    GLenum translate_format(SceGxmTextureFormat src);
    GLenum translate_type(SceGxmTextureFormat format);
}
