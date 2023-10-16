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

#include <cstdint>
#include <string>
#include <vector>

namespace shader {
struct Hints;
}

struct SceGxmProgram;
struct FeatureState;
struct SceGxmVertexAttribute;

namespace renderer {

struct ShadersHash;
struct State;

// Shaders.
bool get_shaders_cache_hashs(State &renderer);
void save_shaders_cache_hashs(State &renderer, std::vector<ShadersHash> &shaders_cache_hashs);
std::string load_glsl_shader(const SceGxmProgram &program, const FeatureState &features, const shader::Hints &hints, bool maskupdate, const char *cache_path, const char *title_id, const char *self_name, const std::string &shader_version, bool shader_cache);
std::vector<uint32_t> load_spirv_shader(const SceGxmProgram &program, const FeatureState &features, bool is_vulkan, const shader::Hints &hints, bool maskupdate, const char *cache_path, const char *title_id, const char *self_name, const std::string &shader_version, bool shader_cache);
std::string pre_load_shader_glsl(const char *hash_text, const char *shader_type_str, const char *cache_path, const char *title_id, const char *self_name);
std::vector<uint32_t> pre_load_shader_spirv(const char *hash_text, const char *shader_type_str, const char *cache_path, const char *title_id, const char *self_name);
} // namespace renderer