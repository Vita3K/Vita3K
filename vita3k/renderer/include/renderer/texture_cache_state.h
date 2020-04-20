#pragma once

#include <glutil/object_array.h>

#include <gxm/types.h>

#include <array>
#include <cstdint>
#include <functional>

struct MemState;

namespace renderer {
constexpr size_t TextureCacheSize = 1024;
typedef uint64_t TextureCacheTimestamp;
typedef uint32_t TextureCacheHash;

typedef std::array<SceGxmTexture, TextureCacheSize> TextureCacheGxmTextures;
typedef std::array<TextureCacheTimestamp, TextureCacheSize> TextureCacheTimestamps;
typedef std::array<TextureCacheHash, TextureCacheSize> TextureCacheHashes;
typedef std::function<void(std::size_t)> TextureCacheStateSelectCallback;
typedef std::function<void(std::size_t, const void *)> TextureCacheStateConfigureTextureCallback;
typedef std::function<void(std::size_t, const void *, const MemState &)> TextureCacheStateUploadTextureCallback;

struct TextureCacheState {
    size_t used = 0;
    TextureCacheTimestamp timestamp = 1;
    TextureCacheGxmTextures gxm_textures;
    TextureCacheTimestamps timestamps;
    TextureCacheHashes hashes;
    TextureCacheStateSelectCallback select_callback;
    TextureCacheStateConfigureTextureCallback configure_texture_callback;
    TextureCacheStateUploadTextureCallback upload_texture_callback;
};
} // namespace renderer
