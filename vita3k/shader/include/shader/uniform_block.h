#pragma once

#include <gxm/types.h>
#include <util/align.h>

#include <array>
#include <cstring>
#include <utility>

namespace shader {

struct RenderVertUniformBlock {
    std::array<float, 4> viewport_flip;
    float viewport_flag;
    float screen_width;
    float screen_height;
    float z_offset;
    float z_scale;
};

// used internally to identify the field by the shader recompiler
// it is put next to the RenderVertUniformBlock so we don't forget to update both fields every time
enum VertUniformFieldId : uint32_t {
    VERT_UNIFORM_viewport_flip,
    VERT_UNIFORM_viewport_flag,
    VERT_UNIFORM_screen_width,
    VERT_UNIFORM_screen_height,
    VERT_UNIFORM_z_offset,
    VERT_UNIFORM_z_scale
};

struct RenderFragUniformBlock {
    float back_disabled;
    float front_disabled;
    float writing_mask;
    float use_raw_image;
    float res_multiplier;
};

enum FragUniformFieldId : uint32_t {
    FRAG_UNIFORM_back_disabled,
    FRAG_UNIFORM_front_disabled,
    FRAG_UNIFORM_writing_mask,
    FRAG_UNIFORM_use_raw_image,
    FRAG_UNIFORM_res_multiplier
};

template <typename T>
struct UniformBlockExtended {
    T base_block;

    uint64_t buffer_addresses[SCE_GXM_REAL_MAX_UNIFORM_BUFFER] = {};
    std::pair<float, float> viewport_ratio[SCE_GXM_MAX_TEXTURE_UNITS] = {};
    std::pair<float, float> viewport_offset[SCE_GXM_MAX_TEXTURE_UNITS] = {};
    bool changed = false;
    uint16_t buffer_count = 0;
    uint16_t texture_count = 0;

    void set_buffer_count(uint16_t new_buffer_count) {
        if (new_buffer_count == buffer_count)
            return;

        changed = true;
        buffer_count = new_buffer_count;
    }

    void set_texture_count(uint16_t new_texture_count) {
        if (new_texture_count == texture_count)
            return;

        changed = true;
        texture_count = new_texture_count;
    }

    void set_buffer_address(int idx, uint64_t buffer_address) {
        if (buffer_addresses[idx] != buffer_address) {
            changed = true;
            buffer_addresses[idx] = buffer_address;
        }
    }

    static constexpr uint32_t get_buffer_addresses_offset(uint16_t buffer_count, uint16_t texture_count) {
        return align(sizeof(T), 8);
    }

    void set_viewport_ratio(int idx, std::pair<float, float> ratio) {
        if (viewport_ratio[idx] != ratio) {
            changed = true;
            viewport_ratio[idx] = ratio;
        }
    }

    static uint32_t get_viewport_ratio_offset(uint16_t buffer_count, uint16_t texture_count) {
        return get_buffer_addresses_offset(buffer_count, texture_count) + buffer_count * sizeof(uint64_t);
    }

    void set_viewport_offset(int idx, std::pair<float, float> offset) {
        if (viewport_offset[idx] != offset) {
            changed = true;
            viewport_offset[idx] = offset;
        }
    }

    static uint32_t get_viewport_offset_offset(uint16_t buffer_count, uint16_t texture_count) {
        return get_viewport_ratio_offset(buffer_count, texture_count) + texture_count * sizeof(std::pair<float, float>);
    }

    static constexpr uint32_t get_size(const uint16_t buffer_count, const uint16_t texture_count) {
        uint32_t size = align(sizeof(T), 8);
        size += buffer_count * sizeof(uint64_t);
        size += texture_count * 4 * sizeof(uint32_t);
        return size;
    }

    uint32_t get_size() {
        return get_size(buffer_count, texture_count);
    }

    static constexpr uint32_t get_max_size() {
        return get_size(SCE_GXM_REAL_MAX_UNIFORM_BUFFER, SCE_GXM_MAX_TEXTURE_UNITS);
    }

    void copy_to(uint8_t *buffer) {
        memcpy(buffer, &base_block, sizeof(T));
        buffer += align(sizeof(T), 8);

        // buffer addresses
        memcpy(buffer, buffer_addresses, buffer_count * sizeof(uint64_t));
        buffer += buffer_count * sizeof(uint64_t);

        // texture viewport fields
        memcpy(buffer, viewport_ratio, texture_count * sizeof(viewport_ratio[0]));
        buffer += texture_count * sizeof(viewport_ratio[0]);
        memcpy(buffer, viewport_offset, texture_count * sizeof(viewport_offset[0]));

        changed = false;
    }
};

typedef UniformBlockExtended<RenderVertUniformBlock> RenderVertUniformBlockExtended;
typedef UniformBlockExtended<RenderFragUniformBlock> RenderFragUniformBlockExtended;

} // namespace shader
