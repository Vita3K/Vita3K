#pragma once

#include <yaml-cpp/yaml.h>

struct FeatureState {
#define FEATURE(name, value) bool name = value;
#include <features/features.h>
#undef FEATURE

    bool is_programmable_blending_supported() const {
        return support_shader_interlock || support_texture_barrier || direct_fragcolor;
    }

    bool is_programmable_blending_need_to_bind_color_attachment() const {
        return (support_texture_barrier || support_shader_interlock) && !direct_fragcolor;
    }

    bool should_use_shader_interlock() const {
        return support_shader_interlock && !direct_fragcolor;
    }

    bool should_use_texture_barrier() const {
        return support_texture_barrier && !support_shader_interlock && !direct_fragcolor;
    }

    YAML::Node searialize() {
        YAML::Node node;
#define FEATURE(name, value) node[#name] = name;
#include <features/features.h>
#undef FEATURE
        return node;
    }

    void deserialize(const YAML::Node &node) {
#define FEATURE(name, value) name = node[#name];
#include <features/features.h>
#undef FEATURE
    }
};
