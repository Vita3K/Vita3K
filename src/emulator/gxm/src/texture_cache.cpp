#include <gxm/texture_cache_functions.h>

#include <gxm/functions.h>
#include <gxm/texture_cache_state.h>

#include <mem/ptr.h>

#include <algorithm> // find
#include <cstring> // memcmp

static const bool operator==(const SceGxmTexture &a, const SceGxmTexture &b) {
    return memcmp(&a, &b, sizeof(a)) == 0;
}

static size_t find_lru(const TextureCacheTimestamps &timestamps, TextureCacheTimestamp current_time) {
    uint64_t oldest_age = current_time - timestamps.front();
    size_t oldest_index = 0;
    
    for (size_t index = 1; index < timestamps.size(); ++index) {
        const uint64_t age = current_time - timestamps[index];
        if (age > oldest_age) {
            oldest_age = age;
            oldest_index = index;
        }
    }
    
    return oldest_index;
}

static void configure_bound_texture(const SceGxmTexture &gxm_texture) {
    const SceGxmTextureFormat fmt = texture::get_format(&gxm_texture);
    const SceGxmTextureAddrMode uaddr = (SceGxmTextureAddrMode)(gxm_texture.uaddr_mode);
    const SceGxmTextureAddrMode vaddr = (SceGxmTextureAddrMode)(gxm_texture.vaddr_mode);
    const GLenum *const swizzle = texture::translate_swizzle(fmt);
    
    // TODO Support mip-mapping.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture::translate_wrap_mode(uaddr));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture::translate_wrap_mode(vaddr));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture::translate_minmag_filter((SceGxmTextureFilter)gxm_texture.min_filter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture::translate_minmag_filter((SceGxmTextureFilter)gxm_texture.mag_filter));
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
    
    const GLenum internal_format = texture::translate_internal_format(fmt);
    const unsigned int width = texture::get_width(&gxm_texture);
    const unsigned int height = texture::get_height(&gxm_texture);
    const GLenum format = texture::translate_format(fmt);
    const GLenum type = texture::translate_type(fmt);
    
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, nullptr);
}

static void upload_bound_texture(const SceGxmTexture &gxm_texture, const MemState &mem) {
    const SceGxmTextureFormat fmt = texture::get_format(&gxm_texture);
    const unsigned int width = texture::get_width(&gxm_texture);
    const unsigned int height = texture::get_height(&gxm_texture);
    const Ptr<const uint8_t> data(gxm_texture.data_addr << 2);
    const uint8_t *const texture_data = data.get(mem);
    std::vector<uint32_t> palette_texture_pixels; // TODO Move to context to avoid frequent allocation?
    
    const void *pixels = nullptr;
    size_t stride = 0;
    if (texture::is_paletted_format(fmt)) {
        const auto base_format = texture::get_base_format(fmt);
        const Ptr<const uint32_t> palette_ptr(gxm_texture.palette_addr << 6);
        const uint32_t *const palette_bytes = palette_ptr.get(mem);
        palette_texture_pixels.resize(width * height);
        if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P8) {
            texture::palette_texture_to_rgba_8(palette_texture_pixels.data(), texture_data, width, height, palette_bytes);
        } else {
            texture::palette_texture_to_rgba_4(palette_texture_pixels.data(), texture_data, width, height, palette_bytes);
        }
        pixels = palette_texture_pixels.data();
        stride = width;
    } else {
        pixels = texture_data;
        stride = (width + 7) & ~7; // NOTE: This is correct only with linear textures.
    }
    
    const GLenum format = texture::translate_format(fmt);
    const GLenum type = texture::translate_type(fmt);
    
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

bool init(TextureCacheState &cache) {
    return cache.textures.init(&glGenTextures, &glDeleteTextures);
}

void cache_and_bind_texture(TextureCacheState &cache, const SceGxmTexture &gxm_texture, const MemState &mem) {
    size_t index = 0;
    bool configure = false;
    bool upload = false;
    
    // Try to find GXM texture in cache.
    const TextureCacheGxmTextures::const_iterator gxm_begin = cache.gxm_textures.cbegin();
    const TextureCacheGxmTextures::const_iterator gxm_end = gxm_begin + cache.used;
    const TextureCacheGxmTextures::const_iterator cached_gxm_texture = std::find(gxm_begin, gxm_end, gxm_texture);
    if (cached_gxm_texture == gxm_end) {
        // Texture not found in cache.
        if (cache.used < TextureCacheSize) {
            // Cache is not full. Add texture to cache.
            index = cache.used;
            ++cache.used;
        } else {
            // Cache is full. Find least recently used texture.
            index = find_lru(cache.timestamps, cache.timestamp);
        }
        configure = true;
        upload = true;
        cache.gxm_textures[index] = gxm_texture;
    } else {
        // Texture is cached.
        index = cached_gxm_texture - cache.gxm_textures.begin();
        // TODO Has data changed?
        if (true) {
            upload = true;
        }
    }
    
    const GLuint gl_texture = cache.textures[index];
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    
    if (configure) {
        configure_bound_texture(gxm_texture);
    }
    if (upload) {
        upload_bound_texture(gxm_texture, mem);
    }
    
    cache.timestamps[index] = cache.timestamp;
}
