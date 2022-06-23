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

#pragma once

#include <glutil/object_array.h>

#include <gxm/types.h>

#include <array>
#include <cstdint>
#include <functional>

struct MemState;

namespace renderer {
static constexpr size_t TextureCacheSize = 1024;
typedef uint64_t TextureCacheTimestamp;
typedef uint32_t TextureCacheHash;
enum class Backend : uint32_t;

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

struct TextureCacheState;

typedef std::array<TextureCacheInfo, TextureCacheSize> TextureCacheInfoes;
typedef std::function<void(std::size_t, const void *)> TextureCacheStateSelectCallback;
typedef std::function<void(TextureCacheState &, const void *)> TextureCacheStateConfigureTextureCallback;
typedef std::function<void(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height, uint32_t mip_index, const void *pixels, int face, bool is_compressed, size_t pixels_per_stride)> TextureCacheStateUploadTextureCallback;

struct TextureCacheState {
    Backend *backend;
    bool use_protect = false;
    int anisotropic_filtering = 1;
    size_t used = 0;
    TextureCacheTimestamp timestamp = 1;
    TextureCacheInfoes infoes;
    TextureCacheStateSelectCallback select_callback;
    TextureCacheStateConfigureTextureCallback configure_texture_callback;
    TextureCacheStateUploadTextureCallback upload_texture_callback;
};
} // namespace renderer
