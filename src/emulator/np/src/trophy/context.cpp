#include <np/trophy/context.h>
#include <np/state.h>
#include <np/functions.h>
#include <io/state.h>
#include <io/functions.h>

#include <spdlog/fmt/fmt.h>
#include <psp2/types.h>

namespace emu::np::trophy {
Context::Context(const CommunicationID &comm_id, IOState *io, const SceUID trophy_stream)
    : comm_id(comm_id)
    , io(io)
    , trophy_file_stream(trophy_stream) {
    trophy_file.read_func = [&](void *dest, std::uint32_t amount) -> bool {
        return read_file(dest, *this->io, this->trophy_file_stream, amount, "parse_trophy_file");
    };

    trophy_file.seek_func = [&](std::uint32_t amount) -> bool {
        return seek_file(this->trophy_file_stream, amount, SCE_SEEK_SET, *this->io, "parse_trophy_file");
    };
}
}

emu::np::trophy::ContextHandle create_trophy_context(NpState &np, IOState &io, const std::string &pref_path, const emu::np::CommunicationID *custom_comm,
    NpTrophyError *error) {
    if (!custom_comm) {
        custom_comm = &np.comm_id;
    }

    if (error)
        *error = NpTrophyError::TROPHY_ERROR_NONE;

    #define TROPHY_RET_ERROR(err)                                                   \
        if (error) *error = NpTrophyError::err;                                     \
        return emu::np::trophy::INVALID_CONTEXT_HANDLE

    // Check if a context has already been created for this communication ID 
    for (std::size_t i = 0; i < np.trophy_state.contexts.size(); i++) {
        if (np.trophy_state.contexts[i].valid && np.trophy_state.contexts[i].comm_id == *custom_comm) {
            TROPHY_RET_ERROR(TROPHY_CONTEXT_EXIST);
        }
    }

    // Initialize the stream
    const std::string trophy_file_path = fmt::format("app0:sce_sys/trophy/{}_{:0>2d}/TROPHY.TRP", custom_comm->data,
        custom_comm->num);

    // Try to open the file
    const SceUID trophy_file = open_file(io, trophy_file_path, SCE_O_RDONLY, pref_path.c_str(), "create_trophy_context");

    if (trophy_file < 0) {
        TROPHY_RET_ERROR(TROPHY_CONTEXT_FILE_NON_EXIST);
    }
    
    // Yesss, we can now go fully creative
    // Check for free slot first
    for (auto &context: np.trophy_state.contexts) {
        if (!context.valid) {
            context.comm_id = *custom_comm;
            context.trophy_file_stream = trophy_file;
            context.valid = true;
                    
            context.trophy_file.header_parse();

            return context.id;
        }
    }

    // Yikes. Create new one.
    np.trophy_state.contexts.emplace_back(*custom_comm, &io, trophy_file);
    np.trophy_state.contexts.back().id = static_cast<emu::np::trophy::ContextHandle>(np.trophy_state.contexts.size());

    // Parse the header
    np.trophy_state.contexts.back().trophy_file.header_parse();

    #undef TROPHY_RET_ERROR

    return static_cast<emu::np::trophy::ContextHandle>(np.trophy_state.contexts.size());
}

emu::np::trophy::Context *get_trophy_context(NpTrophyState &state, const emu::np::trophy::ContextHandle handle) {
    if (handle < 0 || handle > static_cast<emu::np::trophy::ContextHandle>(state.contexts.size())) {
        return nullptr;
    }

    return &state.contexts[handle - 1];
}

bool destroy_trophy_context(NpTrophyState &state, const emu::np::trophy::ContextHandle handle) {
    if (handle < 0 || handle > static_cast<emu::np::trophy::ContextHandle>(state.contexts.size())) {
        return false;
    }

    if (!state.contexts[handle - 1].valid) {
        return false;
    }

    state.contexts[handle - 1].valid = false;
    return true;
}