#include <gxm/types.h>
#include <renderer/state.h>
#include <renderer/types.h>
#include <renderer/commands.h>

#include "gl/functions.h"
#include "gl/texture_cache_state.h"
#include "driver_functions.h"

#include <util/log.h>
#include <renderer/functions.h>

namespace renderer {
    COMMAND(handle_create_context) {
        std::unique_ptr<Context> *ctx = helper.pop<std::unique_ptr<Context> *>();
        bool result = false;

        switch (renderer.current_backend) {
        case Backend::OpenGL: {
            result = gl::create(*ctx);
            break;
        }

        default: {
            REPORT_MISSING(renderer.current_backend);
            break;
        }
        }

        complete_command(renderer, helper, result);
    }

    COMMAND(handle_create_render_target) {
        std::unique_ptr<RenderTarget> *render_target = helper.pop<std::unique_ptr<RenderTarget>*>();
        SceGxmRenderTargetParams *params = helper.pop<SceGxmRenderTargetParams*>();

        bool result = false;

        switch (renderer.current_backend) {
        case Backend::OpenGL: {
            result = gl::create(*render_target, *params, features);
            break;
        }

        default: {
            REPORT_MISSING(renderer.current_backend);
            break;
        }
        }

        complete_command(renderer, helper, result);
    }

    // Client
    bool create(std::unique_ptr<FragmentProgram> &fp, State &state, const SceGxmProgram &program, const emu::SceGxmBlendInfo *blend, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id) {
        switch (state.current_backend) {
        case Backend::OpenGL: {
            return gl::create(fp, static_cast<gl::GLState&>(state), program, blend, gxp_ptr_map, base_path, title_id);
        }

        default: {
            REPORT_MISSING(state.current_backend);
            break;
        }
        }

        return false;
    }

    bool create(std::unique_ptr<VertexProgram> &vp, State &state, const SceGxmProgram &program, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id) {
        switch (state.current_backend) {
        case Backend::OpenGL: {
            return gl::create(vp, static_cast<gl::GLState&>(state), program, gxp_ptr_map, base_path, title_id);
        }

        default: {
            REPORT_MISSING(state.current_backend);
            break;
        }
        }

        return false;
    }

    bool init(std::unique_ptr<State> &state, Backend backend) {
        switch (backend) {
        case Backend::OpenGL: {
            state = std::make_unique<gl::GLState>();
            break;
        }

        default: {
            REPORT_MISSING(backend);
            break;
        }
        }
        
        state->current_backend = backend;

        // Can change this
        state->command_buffer_queue.maxPendingCount_ = 30;

        return true;
    }
}