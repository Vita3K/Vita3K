// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <gxm/types.h>
#include <util/containers.h>

#include <array>
#include <cstdint>
#include <functional>

struct MemState;

enum SceGxmTextureBaseFormat : uint32_t;

namespace renderer {
enum class Backend : uint32_t;
static constexpr size_t TextureCacheSize = 1024;

typedef std::array<uint32_t, 4> TextureGxmDataRepr;
struct TextureCacheInfo {
    uint64_t hash = 0;
    uint64_t timestamp = 0;
    TextureGxmDataRepr texture;
    int index = 0;
    uint32_t texture_size = 0;
    bool use_hash = false;
    bool dirty = false;
};

struct SamplerCacheInfo {
    // compact representation of the sampler state
    uint32_t value = 0;
    int index = 0;
};

typedef std::array<TextureCacheInfo, TextureCacheSize> TextureCacheInfoes;

class TextureCache {
public:
    Backend backend;
    bool use_protect = false;
    // use a separate sampler cache
    bool use_sampler_cache = false;
    int anisotropic_filtering = 1;
    uint64_t timestamp = 1;

    // used to quicky get the info from a hash of a gxm_texture
    unordered_map_fast<TextureGxmDataRepr, TextureCacheInfo *> texture_lookup;
    lru::Queue<TextureCacheInfo> texture_queue;

    // when use_sampler_cache is set to true, used to quickly get a cached sampler
    unordered_map_fast<uint32_t, SamplerCacheInfo *> sampler_lookup;
    lru::Queue<SamplerCacheInfo> sampler_queue;
    size_t last_bound_sampler_index;

    bool init(const bool hashless_texture_cache, const size_t sampler_cache_size = 0);
    virtual void select(size_t index, const SceGxmTexture &texture) = 0;
    virtual void configure_texture(const SceGxmTexture &texture) = 0;
    virtual void upload_texture_impl(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height, uint32_t mip_index, const void *pixels, int face, uint32_t pixels_per_stride) = 0;
    virtual void upload_done() {}

    virtual void configure_sampler(size_t index, const SceGxmTexture &texture) {}

    void upload_texture(const SceGxmTexture &gxm_texture, MemState &mem);
    void cache_and_bind_texture(const SceGxmTexture &gxm_texture, MemState &mem);

    // is called by cache_and_bind_texture if use_sampler_cache is set to true
    int cache_and_bind_sampler(const SceGxmTexture &gxm_texture);
};
} // namespace renderer
