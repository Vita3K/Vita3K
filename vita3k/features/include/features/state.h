#pragma once

struct FeatureState {
    bool direct_pack_unpack_half = false;
    bool pack_unpack_half_through_ext = false;
    bool support_shader_interlock = false; ///< First option for blending. Using this with ordered execution mode.
    bool support_texture_barrier = false; ///< Second option for blending. Slower but work on 3 vendors.
    bool direct_fragcolor = false;
    bool use_shader_binding = false;
    bool spirv_shader = false;

    bool is_programmable_blending_supported() const {
        return support_shader_interlock || support_texture_barrier || direct_fragcolor;
    }

    bool is_programmable_blending_need_to_bind_color_attachment() const {
        return (support_texture_barrier || support_shader_interlock) && !direct_fragcolor;
    }

    bool should_use_shader_interlock() const {
        return support_shader_interlock && !direct_fragcolor && !spirv_shader;
    }

    bool should_use_texture_barrier() const {
        return support_texture_barrier && (spirv_shader || (!support_shader_interlock && !direct_fragcolor));
    }
};
