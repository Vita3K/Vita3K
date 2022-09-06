// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <gxm/functions.h>
#include <renderer/gl/functions.h>
#include <renderer/gl/surface_cache.h>
#include <renderer/gl/types.h>
#include <util/log.h>

#include <chrono>

namespace renderer::gl {
static constexpr std::uint64_t CASTED_UNUSED_TEXTURE_PURGE_SECS = 40;

GLSurfaceCache::GLSurfaceCache() = default;

void GLSurfaceCache::do_typeless_copy(const GLuint dest_texture, const GLuint source_texture, const GLenum dest_internal,
    const GLenum dest_upload_format, const GLenum dest_type, const GLenum source_format, const GLenum source_type, const int offset_x,
    const int offset_y, const int width, const int height, const int dest_width, const int dest_height, const std::size_t total_source_size) {
    static constexpr GLsizei I32_SIGNED_MAX = 0x7FFFFFFF;

    if (!typeless_copy_buffer[0]) {
        if (!typeless_copy_buffer.init(glGenBuffers, glDeleteBuffers)) {
            LOG_ERROR("Unable to initialize a typeless copy buffer");
            return;
        }
    }

    if (total_source_size > typeless_copy_buffer_size) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, typeless_copy_buffer[0]);
        glBufferData(GL_PIXEL_PACK_BUFFER, total_source_size, nullptr, GL_STATIC_COPY);

        typeless_copy_buffer_size = total_source_size;
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, typeless_copy_buffer[0]);
    glGetTextureSubImage(source_texture, 0, offset_x, offset_y, 0, width, height, 1, source_format, source_type, I32_SIGNED_MAX, nullptr);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, GL_NONE);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, typeless_copy_buffer[0]);
    glBindTexture(GL_TEXTURE_2D, dest_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, dest_internal, dest_width, dest_height, 0, dest_upload_format, dest_type, nullptr);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GL_NONE);
}

GLuint GLSurfaceCache::retrieve_color_surface_texture_handle(const State &state, std::uint16_t width, std::uint16_t height, const std::uint16_t pixel_stride,
    const SceGxmColorBaseFormat base_format, Ptr<void> address, SurfaceTextureRetrievePurpose purpose, std::uint32_t &swizzle,
    std::uint16_t *stored_height, std::uint16_t *stored_width) {
    // Create the key to access the cache struct
    const std::uint64_t key = address.address();

    const uint32_t original_width = width;
    const uint32_t original_height = height;

    width = static_cast<uint16_t>(width * state.res_multiplier);
    height = static_cast<uint16_t>(height * state.res_multiplier);

    // Of course, this works under the assumption that range must be unique :D
    auto ite = color_surface_textures.lower_bound(key);
    bool invalidated = false;

    bool overlap = false;
    if (ite != color_surface_textures.end()) {
        // if the surface found does not overlap the surface we want, it's useless to look at it
        overlap = (ite->first + ite->second->total_bytes) > key;
    }

    if (!overlap && purpose != SurfaceTextureRetrievePurpose::WRITING) {
        // not part of a surface, let the texture cache handle it
        return 0;
    }

    GLenum surface_internal_format = color::translate_internal_format(base_format);
    GLenum surface_upload_format = color::translate_format(base_format);
    GLenum surface_data_type = color::translate_type(base_format);

    std::size_t bytes_per_stride = pixel_stride * color::bytes_per_pixel(base_format);
    std::size_t total_surface_size = bytes_per_stride * original_height;

    if (overlap) {
        GLColorSurfaceCacheInfo &info = *ite->second;
        auto used_iterator = std::find(last_use_color_surface_index.begin(), last_use_color_surface_index.end(), ite->first);

        if (stored_height) {
            *stored_height = info.original_height;
        }

        if (stored_width) {
            *stored_width = info.original_width;
        }

        if (purpose == SurfaceTextureRetrievePurpose::READING
            && (base_format == SCE_GXM_COLOR_BASE_FORMAT_U8U8U8 || info.format == SCE_GXM_COLOR_BASE_FORMAT_U8U8U8)
            && base_format != info.format)
            // don't even try to match u8u8u8 with something else
            return 0;

        // There are four situations I think of:
        // 1. Different base address, lookup for write, in this case, if the cached surface range contains the given address, then
        // probably this cached surface has already been freed GPU-wise. So erase.
        // 2. Same base address, but width and height change to be larger, or format change if write. Remake a new one for both read and write situation.
        // 3. Out of cache range. In write case, create a new one, in read case, lul
        // 4. Read situation with smaller width and height, probably need to extract the needed region out.
        const bool addr_in_range_of_cache = ((key + total_surface_size) <= (ite->first + info.total_bytes));
        const bool cache_probably_freed = ((ite->first != key) && addr_in_range_of_cache && (purpose == SurfaceTextureRetrievePurpose::WRITING));
        const bool surface_extent_changed = (info.width < width) || (info.height < height);
        bool surface_stat_changed = false;

        if (ite->first == key) {
            if (purpose == SurfaceTextureRetrievePurpose::WRITING) {
                surface_stat_changed = surface_extent_changed || (base_format != info.format);
            } else {
                // If the extent changed but format is not the same, then the probability of it being a cast is high
                surface_stat_changed = surface_extent_changed && (base_format == info.format);
            }
        }

        if (cache_probably_freed) {
            for (auto fbuf_ite = framebuffer_array.begin(); fbuf_ite != framebuffer_array.end();) {
                if ((fbuf_ite->first & 0xFFFFFFFF) == key) {
                    fbuf_ite = framebuffer_array.erase(fbuf_ite);
                } else {
                    ++fbuf_ite;
                }
            }
            // Clear out. We will recreate later
            color_surface_textures.erase(ite);
            invalidated = true;
        } else if (surface_stat_changed) {
            // Remake locally to avoid making changes to framebuffer array
            uint16_t prev_width = info.width;
            uint16_t prev_height = info.height;
            info.width = width;
            info.height = height;
            info.original_width = original_width;
            info.original_height = original_height;
            info.pixel_stride = pixel_stride;
            info.format = base_format;
            info.total_bytes = total_surface_size;
            info.flags = 0;

            bool store_rawly = false;

            auto remake_and_apply_filters = [&](GLuint bind_texture_id) {
                if (prev_width <= info.width && prev_height <= info.height && surface_extent_changed) {
                    GLuint temp_texture;
                    glGenTextures(1, &temp_texture);
                    glBindTexture(GL_TEXTURE_2D, temp_texture);
                    glTexImage2D(GL_TEXTURE_2D, 0, surface_internal_format, prev_width, prev_height, 0, surface_upload_format, surface_data_type, nullptr);

                    if (!store_rawly) {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    } else {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    }
                    glCopyImageSubData(bind_texture_id, GL_TEXTURE_2D, 0, 0, 0, 0, temp_texture, GL_TEXTURE_2D, 0, 0, 0, 0, prev_width, prev_height, 1);

                    glBindTexture(GL_TEXTURE_2D, bind_texture_id);
                    glTexImage2D(GL_TEXTURE_2D, 0, surface_internal_format, width, height, 0, surface_upload_format, surface_data_type, nullptr);

                    if (!store_rawly) {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    } else {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    }
                    glCopyImageSubData(temp_texture, GL_TEXTURE_2D, 0, 0, 0, 0, bind_texture_id, GL_TEXTURE_2D, 0, 0, 0, 0, prev_width, prev_height, 1);

                    glDeleteTextures(1, &temp_texture);
                } else {
                    glBindTexture(GL_TEXTURE_2D, bind_texture_id);
                    glTexImage2D(GL_TEXTURE_2D, 0, surface_internal_format, width, height, 0, surface_upload_format, surface_data_type, nullptr);

                    if (!store_rawly) {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    } else {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    }
                }
            };

            if (info.gl_expected_read_texture_view[0]) {
                remake_and_apply_filters(info.gl_ping_pong_texture[0]);
            }

            if (state.features.preserve_f16_nan_as_u16 && color::is_write_surface_stored_rawly(base_format)) {
                surface_internal_format = color::get_raw_store_internal_type(base_format);
                surface_upload_format = color::get_raw_store_upload_format_type(base_format);
                surface_data_type = color::get_raw_store_upload_data_type(base_format);

                store_rawly = true;
            }

            // This handles some situation where game may stores texture in a larger texture then rebind it
            remake_and_apply_filters(info.gl_texture[0]);

            if (info.gl_ping_pong_texture[0]) {
                remake_and_apply_filters(info.gl_ping_pong_texture[0]);
            }

            info.casted_textures.clear();
        }
        if ((purpose == SurfaceTextureRetrievePurpose::WRITING) && (swizzle != info.swizzle)) {
            info.swizzle = swizzle;
        } else if (purpose == SurfaceTextureRetrievePurpose::READING) {
            swizzle = info.swizzle;
        }
        if (!addr_in_range_of_cache) {
            if (purpose == SurfaceTextureRetrievePurpose::WRITING) {
                invalidated = true;
            }
        } else if (purpose == SurfaceTextureRetrievePurpose::READING) {
            // If we read and it's still in range
            if (used_iterator != last_use_color_surface_index.end()) {
                last_use_color_surface_index.erase(used_iterator);
            }

            last_use_color_surface_index.push_back(ite->first);

            if (info.flags & GLSurfaceCacheInfo::FLAG_DIRTY) {
                // We can't use this texture sadly :( If it uses for writing of course it will be gud gud
                return 0;
            }

            bool castable = (info.pixel_stride == pixel_stride);

            std::size_t bytes_per_pixel_requested = color::bytes_per_pixel(base_format);
            std::size_t bytes_per_pixel_in_store = color::bytes_per_pixel(info.format);

            // Check if castable. Technically the income format should be texture format, but this is for easier logic.
            // When it's required. I may change :p
            if (base_format != info.format) {
                if (bytes_per_pixel_requested > bytes_per_pixel_in_store) {
                    castable = (((bytes_per_pixel_requested % bytes_per_pixel_in_store) == 0) && (info.pixel_stride % pixel_stride == 0) && ((info.pixel_stride / pixel_stride) == (bytes_per_pixel_requested / bytes_per_pixel_in_store)));
                } else {
                    castable = (((bytes_per_pixel_in_store % bytes_per_pixel_requested) == 0) && (pixel_stride % info.pixel_stride == 0) && ((pixel_stride / info.pixel_stride) == (bytes_per_pixel_in_store / bytes_per_pixel_requested)));
                }

                if (castable) {
                    // Check if the GL implementation actually store raw like this (a safe check)
                    if ((bytes_per_pixel_requested != color::bytes_per_pixel_in_gl_storage(base_format)) || (bytes_per_pixel_in_store != color::bytes_per_pixel_in_gl_storage(info.format))) {
                        LOG_ERROR("One or both two surface formats requested=0x{:X} and inStore=0x{:X} does not support bit-casting. Please report to developers!",
                            fmt::underlying(base_format), fmt::underlying(info.format));

                        return 0;
                    }
                } else {
                    LOG_ERROR_ONCE("Two surface formats requested=0x{:X} and inStore=0x{:X} are not castable!", fmt::underlying(base_format), fmt::underlying(info.format));
                    return 0;
                }
            }

            if (castable) {
                const std::size_t data_delta = address.address() - ite->first;
                std::size_t start_sourced_line = static_cast<size_t>((data_delta / bytes_per_stride) * state.res_multiplier);
                std::size_t start_x = static_cast<size_t>((data_delta % bytes_per_stride) / color::bytes_per_pixel(base_format) * state.res_multiplier);

                if (static_cast<std::uint16_t>(start_sourced_line + height) > info.height) {
                    LOG_ERROR("Trying to present non-existen segment in cached color surface!");
                    return 0;
                }

                if ((start_sourced_line != 0) || (start_x != 0) || (info.width != width) || (info.height != height) || (info.format != base_format)) {
                    std::uint64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                                                     .count();

                    std::vector<std::unique_ptr<GLCastedTexture>> &casted_vec = info.casted_textures;

                    GLenum source_format, source_data_type = 0;

                    if (state.features.preserve_f16_nan_as_u16 && color::is_write_surface_stored_rawly(info.format)) {
                        source_format = color::get_raw_store_upload_format_type(info.format);
                        source_data_type = color::get_raw_store_upload_data_type(info.format);
                    } else {
                        source_format = color::translate_format(info.format);
                        source_data_type = color::translate_type(info.format);
                    }

                    if ((base_format != info.format) || (info.height != height) || (info.width != width) || (ite->first != address.address())) {
                        // Look in cast cache and grab one. The cache really does not store immediate grab on now, but rather to reduce the synchronization in the pipeline (use different texture)
                        for (std::size_t i = 0; i < casted_vec.size();) {
                            if ((casted_vec[i]->cropped_height == height) && (casted_vec[i]->cropped_width == width) && (casted_vec[i]->cropped_y == start_sourced_line) && (casted_vec[i]->cropped_x == start_x) && (casted_vec[i]->format == base_format)) {
                                glBindTexture(GL_TEXTURE_2D, casted_vec[i]->texture[0]);

                                if (color::bytes_per_pixel_in_gl_storage(base_format) == color::bytes_per_pixel_in_gl_storage(info.format)) {
                                    glCopyImageSubData(info.gl_texture[0], GL_TEXTURE_2D, 0, static_cast<GLint>(start_x), static_cast<GLint>(start_sourced_line), 0, casted_vec[i]->texture[0], GL_TEXTURE_2D,
                                        0, 0, 0, 0, width, height, 1);
                                } else {
                                    do_typeless_copy(casted_vec[i]->texture[0], info.gl_texture[0], surface_internal_format, surface_upload_format,
                                        surface_data_type, source_format, source_data_type, static_cast<GLint>(start_x), static_cast<GLint>(start_sourced_line), info.width,
                                        height, width, height, info.total_bytes);
                                }

                                casted_vec[i]->last_used_time = current_time;
                                return casted_vec[i]->texture[0];
                            } else {
                                if (current_time - info.casted_textures[i]->last_used_time >= CASTED_UNUSED_TEXTURE_PURGE_SECS) {
                                    casted_vec.erase(casted_vec.begin() + i);
                                    continue;
                                }
                            }

                            i++;
                        }
                    }

                    // Try to crop + cast
                    std::unique_ptr<GLCastedTexture> casted_info_unq = std::make_unique<GLCastedTexture>();
                    GLCastedTexture &casted_info = *casted_info_unq;

                    if (!casted_info.texture.init(glGenTextures, glDeleteTextures)) {
                        LOG_ERROR("Failed to initialise cast color surface texture!");
                        return 0;
                    }

                    glBindTexture(GL_TEXTURE_2D, casted_info.texture[0]);

                    if (color::bytes_per_pixel_in_gl_storage(base_format) == color::bytes_per_pixel_in_gl_storage(info.format)) {
                        glTexImage2D(GL_TEXTURE_2D, 0, surface_internal_format, width, height, 0, surface_upload_format, surface_data_type, nullptr);

                        // Make it a complete texture (what kind of requirement is this)?
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                        glCopyImageSubData(info.gl_texture[0], GL_TEXTURE_2D, 0, static_cast<GLint>(start_x), static_cast<GLint>(start_sourced_line), 0, casted_info.texture[0], GL_TEXTURE_2D,
                            0, 0, 0, 0, width, height, 1);
                    } else {
                        // TODO: Copy sub region of typeless copy is still not handled ((
                        // We must do a typeless copy (RPCS3)
                        do_typeless_copy(casted_info.texture[0], info.gl_texture[0], surface_internal_format, surface_upload_format,
                            surface_data_type, source_format, source_data_type, static_cast<GLint>(start_x), static_cast<GLint>(start_sourced_line), info.width,
                            height, width, height, info.total_bytes);

                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    }

                    casted_info.format = base_format;
                    casted_info.cropped_x = start_x;
                    casted_info.cropped_y = start_sourced_line;
                    casted_info.cropped_width = width;
                    casted_info.cropped_height = height;
                    casted_info.last_used_time = current_time;
                    casted_vec.push_back(std::move(casted_info_unq));

                    return casted_info.texture[0];
                } else {
                    if (state.features.preserve_f16_nan_as_u16 && color::is_write_surface_stored_rawly(info.format)) {
                        // Create a texture view
                        if (!info.gl_expected_read_texture_view[0]) {
                            if (!info.gl_expected_read_texture_view.init(glGenTextures, glDeleteTextures)) {
                                LOG_ERROR("Unable to initialize texture view for casting texture!");
                                return 0;
                            }

                            glBindTexture(GL_TEXTURE_2D, info.gl_expected_read_texture_view[0]);
                            glTexImage2D(GL_TEXTURE_2D, 0, surface_internal_format, width, height, 0, surface_upload_format, surface_data_type, nullptr);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        }

                        glCopyImageSubData(info.gl_texture[0], GL_TEXTURE_2D, 0, 0, 0, 0, info.gl_expected_read_texture_view[0], GL_TEXTURE_2D,
                            0, 0, 0, 0, width, height, 1);

                        return info.gl_expected_read_texture_view[0];
                    }

                    return info.gl_texture[0];
                }
            }
        }

        if (!invalidated) {
            if (purpose == SurfaceTextureRetrievePurpose::WRITING) {
                if (used_iterator != last_use_color_surface_index.end()) {
                    last_use_color_surface_index.erase(used_iterator);
                }

                last_use_color_surface_index.push_back(ite->first);
                info.is_ping_pong_dirty = true;
                return info.gl_texture[0];
            } else {
                return 0;
            }
        } else if (used_iterator != last_use_color_surface_index.end()) {
            last_use_color_surface_index.erase(used_iterator);
        }
    }

    std::unique_ptr<GLColorSurfaceCacheInfo> info_added = std::make_unique<GLColorSurfaceCacheInfo>();

    info_added->width = width;
    info_added->height = height;
    info_added->original_width = original_width;
    info_added->original_height = original_height;
    info_added->pixel_stride = pixel_stride;
    info_added->data = address;
    info_added->total_bytes = bytes_per_stride * original_height;
    info_added->format = base_format;
    info_added->swizzle = swizzle;
    info_added->flags = 0;
    info_added->is_ping_pong_dirty = true;

    if (!info_added->gl_texture.init(glGenTextures, glDeleteTextures)) {
        LOG_ERROR("Failed to initialise color surface texture!");
        color_surface_textures.erase(key);

        return 0;
    }

    GLint texture_handle_return = info_added->gl_texture[0];
    bool store_rawly = false;

    if (state.features.preserve_f16_nan_as_u16 && color::is_write_surface_stored_rawly(base_format)) {
        surface_internal_format = color::get_raw_store_internal_type(base_format);
        surface_upload_format = color::get_raw_store_upload_format_type(base_format);
        surface_data_type = color::get_raw_store_upload_data_type(base_format);

        store_rawly = true;
    }

    glBindTexture(GL_TEXTURE_2D, texture_handle_return);
    glTexImage2D(GL_TEXTURE_2D, 0, surface_internal_format, width, height, 0, surface_upload_format, surface_data_type, nullptr);

    if (!store_rawly) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    if (color_surface_textures.contains(key)) {
        LOG_WARN_ONCE("Two different surfaces have the same base address, this is not handled, an openGL error will happen.");
    }
    color_surface_textures.emplace(key, std::move(info_added));

    // Now that everything goes well, we can start rearranging
    if (last_use_color_surface_index.size() >= MAX_CACHE_SIZE_PER_CONTAINER) {
        // We have to purge a cache along with framebuffer
        // So choose the one that is last used
        const std::uint64_t first_key = last_use_color_surface_index.front();
        GLuint texture_handle = color_surface_textures.find(first_key)->second->gl_texture[0];

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

    return texture_handle_return;
}

GLuint GLSurfaceCache::retrieve_ping_pong_color_surface_texture_handle(Ptr<void> address) {
    auto ite = color_surface_textures.find(address.address());
    if (ite == color_surface_textures.end()) {
        return 0;
    }

    GLColorSurfaceCacheInfo &info = *ite->second;

    if (!info.is_ping_pong_dirty)
        return info.gl_ping_pong_texture[0];

    GLenum surface_internal_format = color::translate_internal_format(info.format);
    GLenum surface_upload_format = color::translate_format(info.format);
    GLenum surface_data_type = color::translate_type(info.format);

    if (!info.gl_ping_pong_texture[0]) {
        if (!info.gl_ping_pong_texture.init(glGenTextures, glDeleteTextures)) {
            LOG_ERROR("Failed to initialise ping pong surface texture!");
            return 0;
        }

        glBindTexture(GL_TEXTURE_2D, info.gl_ping_pong_texture[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, surface_internal_format, info.width, info.height, 0, surface_upload_format, surface_data_type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glBindTexture(GL_TEXTURE_2D, info.gl_ping_pong_texture[0]);
    }

    glCopyImageSubData(info.gl_texture[0], GL_TEXTURE_2D, 0, 0, 0, 0, info.gl_ping_pong_texture[0], GL_TEXTURE_2D, 0, 0, 0, 0, info.width, info.height, 1);
    info.is_ping_pong_dirty = false;

    return info.gl_ping_pong_texture[0];
}

GLuint GLSurfaceCache::retrieve_depth_stencil_texture_handle(const State &state, const MemState &mem, const SceGxmDepthStencilSurface &surface, std::int32_t force_width, std::int32_t force_height, const bool is_reading) {
    if (!target) {
        LOG_ERROR("Unable to retrieve Depth Stencil texture with no active render target!");
        return 0;
    }

    if (force_width > 0)
        force_width = static_cast<int32_t>(force_width * state.res_multiplier);
    else
        force_width = target->width;

    if (force_height > 0)
        force_height = static_cast<int32_t>(force_height * state.res_multiplier);
    else
        force_height = target->height;

    const bool is_stencil_only = surface.depth_data.address() == 0;
    std::size_t found_index = static_cast<std::size_t>(-1);

    // The whole depth stencil struct is reserved for future use
    for (std::size_t i = 0; i < depth_stencil_textures.size(); i++) {
        if ((!is_stencil_only && depth_stencil_textures[i].surface.depth_data == surface.depth_data)
            || (is_stencil_only && depth_stencil_textures[i].surface.stencil_data == surface.stencil_data)) {
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

        GLDepthStencilSurfaceCacheInfo &cached_info = depth_stencil_textures[found_index];
        bool need_remake = false;
        if (cached_info.width < force_width) {
            cached_info.width = force_width;
            need_remake = true;
        }

        if (cached_info.height < force_height) {
            cached_info.height = force_height;
            need_remake = true;
        }

        if (need_remake) {
            glBindTexture(GL_TEXTURE_2D, depth_stencil_textures[found_index].gl_texture[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, cached_info.width, cached_info.height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
        }

        return depth_stencil_textures[found_index].gl_texture[0];
    }

    if (is_reading) {
        return 0;
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
                    if (!depth_stencil_textures[i].gl_texture.init(glGenTextures, glDeleteTextures)) {
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
    depth_stencil_textures[found_index].width = force_width;
    depth_stencil_textures[found_index].height = force_height;

    glBindTexture(GL_TEXTURE_2D, depth_stencil_textures[found_index].gl_texture[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, force_width, force_height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

    return depth_stencil_textures[found_index].gl_texture[0];
}

GLuint GLSurfaceCache::retrieve_framebuffer_handle(const State &state, const MemState &mem, SceGxmColorSurface *color, SceGxmDepthStencilSurface *depth_stencil,
    GLuint *color_texture_handle, GLuint *ds_texture_handle, std::uint16_t *stored_height) {
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
        std::uint32_t swizzle_set = color->colorFormat & SCE_GXM_COLOR_SWIZZLE_MASK;
        color_handle = retrieve_color_surface_texture_handle(state, color->width,
            color->height, color->strideInPixels, gxm::get_base_format(color->colorFormat), color->data,
            SurfaceTextureRetrievePurpose::WRITING, swizzle_set, stored_height);
    } else {
        color_handle = target->attachments[0];
    }

    if (depth_stencil) {
        ds_handle = retrieve_depth_stencil_texture_handle(state, mem, *depth_stencil);
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
    if (!fb.init(glGenFramebuffers, glDeleteFramebuffers)) {
        LOG_ERROR("Can't initialize framebuffer!");
        return 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fb[0]);

    if (color && state.features.preserve_f16_nan_as_u16 && renderer::gl::color::is_write_surface_stored_rawly(gxm::get_base_format(color->colorFormat))) {
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, color_handle, 0);

        const GLenum buffers[] = { GL_NONE, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, buffers);
    } else {
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color_handle, 0);
    }

    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, ds_handle, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        LOG_ERROR("Framebuffer is not completed. Proceed anyway...");

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepthf(1.0f);
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

GLuint GLSurfaceCache::sourcing_color_surface_for_presentation(Ptr<const void> address, uint32_t width, uint32_t height, const std::uint32_t pitch, float *uvs, const float res_multiplier, SceFVector2 &texture_size) {
    auto ite = color_surface_textures.lower_bound(address.address());
    if (ite == color_surface_textures.end()) {
        return 0;
    }

    width = static_cast<uint32_t>(width * res_multiplier);
    height = static_cast<uint32_t>(height * res_multiplier);

    const GLColorSurfaceCacheInfo &info = *ite->second;

    if (info.pixel_stride == pitch) {
        // In assumption the format is RGBA8
        const std::size_t data_delta = address.address() - ite->first;
        std::uint32_t limited_height = height;
        if ((data_delta % (pitch * 4)) == 0) {
            std::uint32_t start_sourced_line = static_cast<uint32_t>((data_delta / (pitch * 4)) * res_multiplier);
            if ((start_sourced_line + height) > info.height) {
                // Sometimes the surface is just missing a little bit of lines
                if (start_sourced_line < info.height) {
                    // Just limit the height and display it
                    limited_height = info.height - start_sourced_line;
                } else {
                    LOG_ERROR("Trying to present non-existen segment in cached color surface!");
                    return 0;
                }
            }

            // Calculate uvs
            // First two top left, the two others bottom right
            uvs[0] = 0.0f;
            uvs[1] = static_cast<float>(start_sourced_line) / info.height;
            uvs[2] = static_cast<float>(width) / info.width;
            uvs[3] = static_cast<float>(start_sourced_line + limited_height) / info.height;

            texture_size.x = info.width;
            texture_size.y = info.height;

            return info.gl_texture[0];
        }
    }

    return 0;
}

std::vector<uint32_t> GLSurfaceCache::dump_frame(Ptr<const void> address, uint32_t width, uint32_t height, uint32_t pitch, float res_multiplier, bool support_get_texture_sub_image) {
    auto ite = color_surface_textures.lower_bound(address.address());
    if (ite == color_surface_textures.end() || ite->second->pixel_stride != pitch) {
        return {};
    }

    const GLColorSurfaceCacheInfo &info = *ite->second;

    const uint32_t data_delta = address.address() - ite->first;
    const uint32_t pitch_byte = pitch * 4;
    if (info.pixel_stride != pitch || data_delta % pitch_byte != 0)
        return {};

    const uint32_t line_delta = static_cast<uint32_t>((data_delta / pitch_byte) * res_multiplier);
    if (line_delta >= info.height)
        return {};

    if (!support_get_texture_sub_image && (line_delta != 0 || info.width != width || info.height != height)) {
        LOG_ERROR("Dumping this frame is not supported on the OpenGL renderer");
        return {};
    }

    const uint32_t real_height = std::min(height, info.height - line_delta);

    std::vector<uint32_t> frame(width * height, 0);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);

    // retrieve the texture, it is on the GPU right now
    if (support_get_texture_sub_image) {
        glGetTextureSubImage(info.gl_texture[0], 0, 0, line_delta, 0, width, real_height, 1, GL_RGBA, GL_UNSIGNED_BYTE, frame.size() * 4, frame.data());
    } else {
        GLint last_texture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);

        glBindTexture(GL_TEXTURE_2D, info.gl_texture[0]);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame.data());

        glBindTexture(GL_TEXTURE_2D, last_texture);
    }

    return frame;
}

} // namespace renderer::gl
