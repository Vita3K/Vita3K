#pragma once

#include <renderer/types.h>
#include <renderer/state.h>

#include <map>
#include <string>

namespace renderer::gl {

struct CommandBuffer;

struct GLState: public renderer::State {
    ShaderCache fragment_shader_cache;
    ShaderCache vertex_shader_cache;
    ProgramCache program_cache;
};

} // namespace renderer
