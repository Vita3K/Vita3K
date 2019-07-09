#pragma once

#include <glutil/object_array.h>

#include <gxm/types.h>
#include <psp2/gxm.h>

#include <array>

namespace renderer::gl {
constexpr size_t TextureCacheSize = 1024;
typedef uint64_t TextureCacheTimestamp;
typedef uint32_t TextureCacheHash;

typedef std::array<emu::SceGxmTexture, TextureCacheSize> TextureCacheGxmTextures;
typedef std::array<TextureCacheTimestamp, TextureCacheSize> TextureCacheTimestamps;
typedef std::array<TextureCacheHash, TextureCacheSize> TextureCacheHashes;

struct TextureCacheState {
    size_t used = 0;
    TextureCacheTimestamp timestamp = 1;
    TextureCacheGxmTextures gxm_textures;
    TextureCacheTimestamps timestamps;
    TextureCacheHashes hashes;
    GLObjectArray<TextureCacheSize> textures;
};
} // namespace renderer
