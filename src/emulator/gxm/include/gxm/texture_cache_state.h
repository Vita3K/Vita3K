#pragma once

#include <glutil/object_array.h>

#include <array>

struct SceGxmTexture;

constexpr size_t TextureCacheSize = 1024;
typedef uint64_t TextureCacheTimestamp;

typedef std::array<SceGxmTexture, TextureCacheSize> TextureCacheGxmTextures;
typedef std::array<TextureCacheTimestamp, TextureCacheSize> TextureCacheTimestamps;

struct TextureCacheState {
    size_t used = 0;
    TextureCacheTimestamp timestamp = 1;
    TextureCacheGxmTextures gxm_textures;
    TextureCacheTimestamps timestamps;
    GLObjectArray<TextureCacheSize> textures;
};
