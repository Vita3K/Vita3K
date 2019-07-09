#include <renderer/functions.h>
#include <renderer/types.h>
#include "functions.h"
#include <config/config.h>

namespace renderer::gl {
    bool switch_context(GxmContextState &new_gxm_state, GLContext &new_context, GxmContextState &old_gxm_state,
        GLContext &old_context, MemState &mem, Config &config) {
        // Reset last shader
        old_gxm_state.last_draw_vertex_program = 0;
        old_gxm_state.last_draw_fragment_program = 0;

        // Syncing new context state
        gl::sync_state(new_context, new_gxm_state, mem, config.texture_cache);
    }
}