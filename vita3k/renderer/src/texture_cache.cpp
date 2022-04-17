// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <renderer/functions.h>
#include <renderer/profile.h>
#include <renderer/texture_cache_state.h>

#include <gxm/functions.h>
#include <mem/ptr.h>
#include <util/align.h>
#include <util/log.h>

#include <algorithm> // find
#include <cstring> // memcmp
#include <numeric> // accumulate, reduce
#include <xxh3.h>
#ifdef WIN32
#include <execution>
#endif

namespace renderer {
namespace texture {

static TextureCacheHash hash_data(const void *data, size_t size) {
    auto hash = XXH_INLINE_XXH3_64bits(data, size);
    return TextureCacheHash(hash);
}

static TextureCacheHash hash_palette_data(const SceGxmTexture &texture, size_t count, const MemState &mem) {
    const uint32_t *const palette_bytes = get_texture_palette(texture, mem);
    const TextureCacheHash palette_hash = hash_data(palette_bytes, count * sizeof(uint32_t));
    return palette_hash;
}

TextureCacheHash hash_texture_data(const SceGxmTexture &texture, const MemState &mem) {
    R_PROFILE(__func__);
    const SceGxmTextureFormat format = gxm::get_format(&texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    const size_t size = texture_size(texture);
    const Ptr<const void> data(texture.data_addr << 2);
    TextureCacheHash data_hash = 0;

    if (data.address()) {
        data_hash = hash_data(data.get(mem), size);
    }

    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
        return data_hash ^ hash_palette_data(texture, 16, mem);
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
        return data_hash ^ hash_palette_data(texture, 256, mem);
    default:
        return data_hash;
    }
}

static size_t find_lru(const TextureCacheState &cache, TextureCacheTimestamp current_time) {
    R_PROFILE(__func__);

    uint64_t oldest_age = current_time - cache.infoes.front().timestamp;
    size_t oldest_index = 0;

    for (size_t index = 1; index < cache.infoes.size(); ++index) {
        const uint64_t age = current_time - cache.infoes[index].timestamp;
        if (age > oldest_age) {
            oldest_age = age;
            oldest_index = index;
        }
    }

    return oldest_index;
}

void cache_and_bind_texture(TextureCacheState &cache, const SceGxmTexture &gxm_texture, MemState &mem) {
    R_PROFILE(__func__);

    size_t index = 0;
    bool configure = false;
    bool upload = false;
    const size_t size = texture_size(gxm_texture);

    // Try to find GXM texture in cache.
    int cached_gxm_texture_index = -1;
    for (size_t a = 0; a < cache.used; a++) {
        if (memcmp(&cache.infoes[a].texture, &gxm_texture, sizeof(SceGxmTexture)) == 0) {
            cached_gxm_texture_index = a;
            break;
        }
    }

    Address range_protect_begin = 0;
    Address range_protect_end = 0;
    bool should_use_hash = true;

    // To prevent protecting too commonly accessed data that belongs to the page where the texture also resides
    // (for example, uniform buffer value and texture data got mixed, so page faults are triggered too many, it's not always good).
    // This works under the assumption that once this big enough texture decided to modify. It will have to modify either all of its data,
    // or replace with an entire new texture.
    if (cache.use_protect && size >= mem.page_size * 4) {
        range_protect_begin = align(gxm_texture.data_addr << 2, mem.page_size);
        range_protect_end = align_down((gxm_texture.data_addr << 2) + size, mem.page_size);

        if (range_protect_end - range_protect_begin >= mem.page_size * 4) {
            should_use_hash = false;
        }
    }

    TextureCacheInfo *info;
    if (cached_gxm_texture_index == -1) {
        // Texture not found in cache.
        if (cache.used < TextureCacheSize) {
            // Cache is not full. Add texture to cache.
            index = cache.used;
            ++cache.used;
        } else {
            // Cache is full. Find least recently used texture.
            index = find_lru(cache, cache.timestamp);
            LOG_DEBUG("Evicting texture {} (t = {}) from cache. Current t = {}.", index, cache.infoes[index].timestamp, cache.timestamp);
        }
        configure = true;
        upload = true;
        cache.infoes[index] = TextureCacheInfo(gxm_texture);
        info = &cache.infoes[index];
        info->use_hash = should_use_hash;
        if (info->use_hash) {
            info->hash = hash_texture_data(gxm_texture, mem);
        }
    } else {
        // Texture is cached.
        index = cached_gxm_texture_index;
        info = &cache.infoes[index];
        configure = false;
        if (info->use_hash) {
            const TextureCacheHash hash = hash_texture_data(gxm_texture, mem);
            upload = info->hash != hash;
            info->hash = hash;
        } else {
            upload = info->dirty;
        }
    }

    if (gxm_texture.data_addr == 0) {
        upload = false;
    }

// Fix memory access error in the condition check for texture cache method
// (hashed vs hashless) in Clang compilers due to compiler optimizations
#ifdef __clang__
    if (!info->use_hash) {
        std::cout << "";
    }
#endif

    cache.select_callback(index, &gxm_texture);

    if (configure) {
        cache.configure_texture_callback(index, &gxm_texture);
    }
    if (upload) {
        cache.upload_texture_callback(index, &gxm_texture, mem);
        if (!info->use_hash) {
            info->dirty = false;
            add_protect(mem, range_protect_begin, range_protect_end - range_protect_begin, MEM_PERM_READONLY, [&cache, info, gxm_texture](Address, bool) {
                if (memcmp(&info->texture, &gxm_texture, sizeof(SceGxmTexture)) == 0) {
                    info->dirty = true;
                }

                return true;
            });
        }
    }

    info->timestamp = cache.timestamp++;
}

} // namespace texture
} // namespace renderer
