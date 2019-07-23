#pragma once

struct FeatureState {
    bool direct_pack_unpack_half = false;
    bool support_shader_interlock = false;      ///< First option for blending. Using this with ordered execution mode.
    bool support_texture_barrier = false;       ///< Second option for blending. Slower but work on 3 vendors.
    bool direct_fragcolor = false;
};
