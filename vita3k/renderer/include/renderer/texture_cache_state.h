#pragma once

#include <glutil/object_array.h>

#include <gxm/types.h>

#include <array>
#include <cstdint>
#include <functional>

struct MemState;

namespace renderer {
constexpr size_t TextureCacheSize = KB(1);
typedef uint64_t TextureCacheTimestamp;
typedef uint32_t TextureCacheHash;

struct TextureCacheInfo {
    bool use_hash = false;
    bool dirty = false;
    TextureCacheHash hash = 0;
    uint64_t timestamp = 0;
    SceGxmTexture texture;

    explicit TextureCacheInfo(SceGxmTexture texture)
        : texture(texture) {}

    TextureCacheInfo() = default;
};

typedef std::array<TextureCacheInfo, TextureCacheSize> TextureCacheInfoes;
typedef std::function<void(std::size_t)> TextureCacheStateSelectCallback;
typedef std::function<void(std::size_t, const void *)> TextureCacheStateConfigureTextureCallback;
typedef std::function<void(std::size_t, const void *, const MemState &)> TextureCacheStateUploadTextureCallback;

struct TextureCacheState {
    bool use_protect = false;
    size_t used = 0;
    TextureCacheTimestamp timestamp = 1;
    TextureCacheInfoes infoes;
    TextureCacheStateSelectCallback select_callback;
    TextureCacheStateConfigureTextureCallback configure_texture_callback;
    TextureCacheStateUploadTextureCallback upload_texture_callback;
};
} // namespace renderer
