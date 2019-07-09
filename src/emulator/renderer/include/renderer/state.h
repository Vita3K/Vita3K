#pragma once

#include <crypto/hash.h>
#include <renderer/types.h>
#include <renderer/commands.h>
#include <threads/queue.h>
#include <condition_variable>

#include <map>
#include <string>

namespace renderer {

enum class Backend {
    OpenGL,
    Vulkan
};

struct Command;

struct State {
    GLSLCache fragment_glsl_cache;
    GLSLCache vertex_glsl_cache;

    GXPPtrMap gxp_ptr_map;
    Queue<Command> command_queue;

    void *active_context;
    std::condition_variable command_queue_finish_one;

    Backend current_backend;
};

} // namespace renderer
