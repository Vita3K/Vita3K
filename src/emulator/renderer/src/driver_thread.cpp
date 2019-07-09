#include <renderer/driver_thread.h>
#include <renderer/types.h>
#include <renderer/commands.h>
#include <renderer/state.h>

#include <functional>

namespace renderer {
    static void gpu_handle_context_change(renderer::State &state, void *context_to_bind) {
        // Context switching is costly, so we should try to avoid it
        if (context_to_bind != state.active_context) {
            // Bind this context
            state.active_context = context_to_bind;
            
            // TUTURU, bind it
        }
    }

    static void gpu_do_begin_scene(renderer::State &state, std::unique_ptr<Command> &command) {
        Context *ctx = command->get<renderer::Context*>(0);

        if (!ctx) {
            command->complete(COMMAND_RESULT_ARGUMENT_INVALID);
            return;
        }

        gpu_handle_context_change(state, ctx->gl.get());

        switch (state.current_backend) {
        case Backend::OpenGL: {
            break;
        }

        case Backend::Vulkan: {
            break;
        }
        }

        command->complete(COMMAND_RESULT_NO_ERROR);
    }

    int gpu_driver_thread(renderer::State &state) {
        using CommandHandlerFunc = std::function<void(renderer::State&, std::unique_ptr<Command>&)>;
        static std::map<CommandOpcode, CommandHandlerFunc> handlers = {
            { CommandOpcode::BEGIN_SCENE, gpu_do_begin_scene }
        };
        
        // Don't handle hustle API change yet....
        while (true) {
            std::unique_ptr<Command> cmd = state.command_queue.pop();
            auto handler_iterator = handlers.find(cmd->opcode);

            if (handler_iterator == handlers.end()) {
                assert(false);
            }

            handler_iterator->second(state, cmd);

            // Finish one. Signal all who wait. The one that has its status code changed should
            // be the one waken up.
            state.command_queue_finish_one.notify_all();
        }
    }
}