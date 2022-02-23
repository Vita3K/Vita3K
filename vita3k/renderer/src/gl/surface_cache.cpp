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

#include <renderer/gl/surface_cache.h>
#include <renderer/gl/types.h>
#include <util/log.h>

namespace renderer::gl {
GLSurfaceCache::GLSurfaceCache() {
}

std::uint64_t GLSurfaceCache::retrieve_color_surface_texture_handle(const std::uint16_t width, const std::uint16_t height, const std::uint16_t pixel_stride,
    Ptr<void> address, SurfaceTextureRetrievePurpose purpose, std::uint16_t *stored_height, std::uint16_t *stored_width) {
    // Create the key to access the cache struct
    const std::uint64_t key = address.address();
    auto used_iterator = std::find(last_use_color_surface_index.begin(), last_use_color_surface_index.end(), key);

    if (used_iterator != last_use_color_surface_index.end()) {
        last_use_color_surface_index.erase(used_iterator);
        last_use_color_surface_index.push_back(key);

        GLColorSurfaceCacheInfo &info = color_surface_textures[key];

        if ((info.width < width) || (info.height < height)) {
            // May clear one frame (hopefully!)
            // This handles some situation where game may stores texture in a larger texture then rebind it
            glBindTexture(GL_TEXTURE_2D, info.gl_texture[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            if (info.gl_ping_pong_texture[0]) {
                glBindTexture(GL_TEXTURE_2D, info.gl_ping_pong_texture[0]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }

            info.width = width;
            info.height = height;
        } else {
            if (purpose == SurfaceTextureRetrievePurpose::READING) {
                if (info.flags & GLSurfaceCacheInfo::FLAG_DIRTY) {
                    // We can't use this texture sadly :( If it uses for writing of course it will be gud gud
                    return 0;
                }
            }
        }

        // Use for presentation only, update anyway
        if (purpose == SurfaceTextureRetrievePurpose::WRITING) {
            info.pixel_stride = pixel_stride;
        }

        if (stored_height) {
            *stored_height = info.height;
        }

        if (stored_width) {
            *stored_width = info.width;
        }

        return info.gl_texture[0];
    }

    if (purpose == SurfaceTextureRetrievePurpose::READING) {
        return 0;
    }

    GLColorSurfaceCacheInfo &info_added = color_surface_textures[key];
    info_added.width = width;
    info_added.height = height;
    info_added.pixel_stride = pixel_stride;
    info_added.data = address;
    info_added.flags = 0;

    if (!info_added.gl_texture.init(reinterpret_cast<renderer::Generator *>(glGenTextures), reinterpret_cast<renderer::Deleter *>(glDeleteTextures))) {
        LOG_ERROR("Failed to initialise color surface texture!");
        color_surface_textures.erase(key);

        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, info_added.gl_texture[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Now that everything goes well, we can start rearranging
    if (last_use_color_surface_index.size() >= MAX_CACHE_SIZE_PER_CONTAINER) {
        // We have to purge a cache along with framebuffer
        // So choose the one that is last used
        const std::uint64_t first_key = last_use_color_surface_index.front();
        GLuint texture_handle = color_surface_textures[first_key].gl_texture[0];

        for (auto it = framebuffer_array.cbegin(); it != framebuffer_array.cend();) {
            if ((it->first & 0xFFFFFFFF) == texture_handle) {
                it = framebuffer_array.erase(it);
            } else {
                ++it;
            }
        }

        last_use_color_surface_index.erase(last_use_color_surface_index.begin());
        color_surface_textures.erase(first_key);
    }

    last_use_color_surface_index.push_back(key);

    if (stored_height) {
        *stored_height = height;
    }

    if (stored_width) {
        *stored_width = width;
    }

    return info_added.gl_texture[0];
}

std::uint64_t GLSurfaceCache::retrieve_ping_pong_color_surface_texture_handle(Ptr<void> address) {
    auto ite = color_surface_textures.find(address.address());
    if (ite == color_surface_textures.end()) {
        return 0;
    }

    GLColorSurfaceCacheInfo &info = ite->second;
    if (!info.gl_ping_pong_texture[0]) {
        if (!info.gl_ping_pong_texture.init(reinterpret_cast<renderer::Generator *>(glGenTextures), reinterpret_cast<renderer::Deleter *>(glDeleteTextures))) {
            LOG_ERROR("Failed to initialise ping pong surface texture!");
            return 0;
        }

        glBindTexture(GL_TEXTURE_2D, info.gl_ping_pong_texture[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info.width, info.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glBindTexture(GL_TEXTURE_2D, info.gl_ping_pong_texture[0]);
    }

    glCopyImageSubData(info.gl_texture[0], GL_TEXTURE_2D, 0, 0, 0, 0, info.gl_ping_pong_texture[0], GL_TEXTURE_2D, 0, 0, 0, 0, info.width, info.height, 1);
    return info.gl_ping_pong_texture[0];
}

std::uint64_t GLSurfaceCache::retrieve_depth_stencil_texture_handle(const SceGxmDepthStencilSurface &surface) {
    if (!target) {
        LOG_ERROR("Unable to retrieve Depth Stencil texture with no active render target!");
        return 0;
    }

    std::size_t found_index = static_cast<std::size_t>(-1);
    for (std::size_t i = 0; i < depth_stencil_textures.size(); i++) {
        if (std::memcmp(&depth_stencil_textures[i].surface, &surface, sizeof(SceGxmDepthStencilSurface)) == 0) {
            found_index = i;
            break;
        }
    }

    if (found_index != static_cast<std::size_t>(-1)) {
        auto ite = std::find(last_use_depth_stencil_surface_index.begin(), last_use_depth_stencil_surface_index.end(), found_index);
        if (ite != last_use_depth_stencil_surface_index.end()) {
            last_use_depth_stencil_surface_index.erase(ite);
            last_use_depth_stencil_surface_index.push_back(found_index);
        }

        return depth_stencil_textures[found_index].gl_texture[0];
    }

    // Now that everything goes well, we can start rearranging
    // Almost carbon copy but still too specific
    if (last_use_depth_stencil_surface_index.size() >= MAX_CACHE_SIZE_PER_CONTAINER) {
        // We have to purge a cache along with framebuffer
        // So choose the one that is last used
        const std::size_t index = last_use_depth_stencil_surface_index.front();
        GLuint ds_texture_handle = depth_stencil_textures[index].gl_texture[0];

        for (auto it = framebuffer_array.cbegin(); it != framebuffer_array.cend();) {
            if (((it->first >> 32) & 0xFFFFFFFF) == ds_texture_handle) {
                it = framebuffer_array.erase(it);
            } else {
                ++it;
            }
        }

        last_use_depth_stencil_surface_index.erase(last_use_depth_stencil_surface_index.begin());
        depth_stencil_textures[index].flags = GLSurfaceCacheInfo::FLAG_FREE;

        found_index = index;
    }

    if (found_index == static_cast<std::size_t>(-1)) {
        // Still nowhere to found a free slot? We can search maybe
        for (std::size_t i = 0; i < depth_stencil_textures.size(); i++) {
            if (depth_stencil_textures[i].flags & GLSurfaceCacheInfo::FLAG_FREE) {
                if (depth_stencil_textures[i].gl_texture[0] == 0) {
                    if (!depth_stencil_textures[i].gl_texture.init(reinterpret_cast<renderer::Generator *>(glGenTextures), reinterpret_cast<renderer::Deleter *>(glDeleteTextures))) {
                        LOG_ERROR("Fail to initialize depth stencil texture!");
                        return 0;
                    }
                }
                found_index = i;
                break;
            }
        }
    }

    if (found_index == static_cast<std::size_t>(-1)) {
        LOG_ERROR("No free depth stencil texture cache slot!");
        return 0;
    }

    last_use_depth_stencil_surface_index.push_back(found_index);
    depth_stencil_textures[found_index].flags = 0;
    depth_stencil_textures[found_index].surface = surface;

    glBindTexture(GL_TEXTURE_2D, depth_stencil_textures[found_index].gl_texture[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, target->width, target->height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

    return depth_stencil_textures[found_index].gl_texture[0];
}

std::uint64_t GLSurfaceCache::retrieve_framebuffer_handle(SceGxmColorSurface *color, SceGxmDepthStencilSurface *depth_stencil,
    std::uint64_t *color_texture_handle, std::uint64_t *ds_texture_handle, std::uint16_t *stored_height) {
    if (!target) {
        LOG_ERROR("Unable to retrieve framebuffer with no active render target!");
        return 0;
    }

    if (!color && !depth_stencil) {
        LOG_ERROR("Depth stencil and color surface are both null!");
        return 0;
    }

    GLuint color_handle = 0;
    GLuint ds_handle = 0;

    if (color) {
        color_handle = static_cast<GLuint>(retrieve_color_surface_texture_handle(color->width,
            color->height, color->strideInPixels, color->data, renderer::SurfaceTextureRetrievePurpose::WRITING, stored_height));
    } else {
        color_handle = target->attachments[0];
    }

    if (ds_handle) {
        ds_handle = static_cast<GLuint>(retrieve_depth_stencil_texture_handle(*depth_stencil));
    } else {
        ds_handle = target->attachments[1];
    }

    const std::uint64_t key = (color_handle | (static_cast<std::uint64_t>(ds_handle) << 32));
    auto ite = framebuffer_array.find(key);

    if (ite != framebuffer_array.end()) {
        if (color_texture_handle) {
            *color_texture_handle = color_handle;
        }

        if (ds_texture_handle) {
            *ds_texture_handle = ds_handle;
        }

        return ite->second[0];
    }

    // Create a new framebuffer for our sake
    GLObjectArray<1> &fb = framebuffer_array[key];
    if (!fb.init(reinterpret_cast<renderer::Generator *>(glGenFramebuffers), reinterpret_cast<renderer::Deleter *>(glDeleteFramebuffers))) {
        LOG_ERROR("Can't initialize framebuffer!");
        return 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fb[0]);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color_handle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, ds_handle, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        LOG_ERROR("Framebuffer is not completed. Proceed anyway...");

    glClearColor(0.968627450f, 0.776470588f, 0.0f, 1.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (color_texture_handle) {
        *color_texture_handle = color_handle;
    }

    if (ds_texture_handle) {
        *ds_texture_handle = ds_handle;
    }

    return fb[0];
}

std::uint64_t GLSurfaceCache::sourcing_color_surface_for_presentation(Ptr<const void> address, const std::uint32_t width, const std::uint32_t height, const std::uint32_t pitch, float *uvs) {
    for (const auto &[base_addr, info] : color_surface_textures) {
        // Assuming surface is stored in RGBA8
        if ((info.pixel_stride == pitch) && (base_addr <= address.address()) && (address.address() < (base_addr + info.pixel_stride * info.height * 4))) {
            const std::size_t data_delta = address.address() - base_addr;
            if ((data_delta % (pitch * 4)) == 0) {
                std::uint32_t start_sourced_line = (data_delta / (pitch * 4));
                if ((start_sourced_line + height) > info.height) {
                    LOG_ERROR("Trying to present non-existen segment in cached color surface!");
                    return 0;
                }

                // Calculate uvs
                // First two top left, the two others bottom right
                uvs[0] = 0.0f;
                uvs[1] = static_cast<float>(start_sourced_line) / info.height;
                uvs[2] = static_cast<float>(width) / info.width;
                uvs[3] = static_cast<float>(start_sourced_line + height) / info.height;

                return info.gl_texture[0];
            }
        }
    }

    return 0;
}

} // namespace renderer::gl
