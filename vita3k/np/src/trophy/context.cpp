#include <io/device.h>
#include <io/functions.h>
#include <io/state.h>
#include <np/functions.h>
#include <np/state.h>
#include <np/trophy/context.h>

#include <pugixml.hpp>
#include <spdlog/fmt/fmt.h>

namespace np {
namespace trophy {
Context::Context(const CommunicationID &comm_id, IOState &io, const SceUID trophy_stream,
    const std::string &output_progress_path)
    : comm_id(comm_id)
    , trophy_file_stream(trophy_stream)
    , trophy_progress_output_file_path(output_progress_path)
    , io(io) {
    trophy_file.read_func = [&](void *dest, std::uint32_t amount) -> bool {
        return read_file(dest, this->io, this->trophy_file_stream, amount, "parse_trophy_file");
    };

    trophy_file.seek_func = [&](std::uint32_t amount) -> bool {
        return seek_file(this->trophy_file_stream, amount, SCE_SEEK_SET, this->io, "parse_trophy_file");
    };
}

#define SET_TROPHY_BIT(arr, bit) arr[bit >> 5] |= (1 << (bit & 31))

static bool read_trophy_entry_to_buffer(np::trophy::TRPFile &trophy_file, const char *fname, std::string &buffer) {
    // Read the trophy config
    const std::int32_t eidx = trophy_file.search_file(fname);

    if (eidx == -1) {
        return false;
    }

    buffer.resize(trophy_file.entries[eidx].size);

    // Read
    std::uint32_t pointee = 0;
    trophy_file.get_entry_data(static_cast<std::uint32_t>(eidx), [&](void *source, std::uint32_t amount) {
        std::memcpy(&buffer[pointee], source, amount);
        pointee += amount;

        return true;
    });

    return true;
}

bool Context::init_info_from_trp() {
    // Read the trophy config
    std::string tropcfg;
    if (!read_trophy_entry_to_buffer(trophy_file, "TROPCONF.SFM", tropcfg)) {
        return false;
    }

    pugi::xml_document conf_file_doc;
    pugi::xml_parse_result parse_result = conf_file_doc.load_string(tropcfg.c_str());

    if (!parse_result) {
        return false;
    }

    std::fill(trophy_progress, trophy_progress + (MAX_TROPHIES >> 5), 0);
    std::fill(trophy_availability, trophy_availability + (MAX_TROPHIES >> 5), 0);
    std::fill(trophy_kinds.begin(), trophy_kinds.end(), TrophyType::INVALID);
    std::fill(unlock_timestamps.begin(), unlock_timestamps.end(), 0);
    platinum_trophy_id = -1;

    trophy_count = 0;

    // Get parental of all
    for (auto trop : conf_file_doc.child("trophyconf")) {
        if (strncmp(trop.name(), "trophy", 6) == 0) {
            // Get ID
            const std::uint32_t id = trop.attribute("id").as_uint();
            const std::string hidden = trop.attribute("hidden").as_string();
            const std::string type = trop.attribute("ttype").as_string();

            // Set the bit
            if (hidden == "yes") {
                SET_TROPHY_BIT(trophy_availability, id);
            }

            if (type == "P") {
                trophy_kinds[id] = np::trophy::TrophyType::PLATINUM;
                platinum_trophy_id = id;
            }

            if (type == "G") {
                trophy_kinds[id] = np::trophy::TrophyType::GOLD;
            }

            if (type == "S") {
                trophy_kinds[id] = np::trophy::TrophyType::SILVER;
            }

            if (type == "B") {
                trophy_kinds[id] = np::trophy::TrophyType::BRONZE;
            }

            trophy_count++;
        }
    }

    save_trophy_progress_file();
    return true;
}

static constexpr std::uint32_t TROPHY_USR_MAGIC = 0x12D5819A;

void Context::save_trophy_progress_file() {
    // Open the file
    const SceUID output = open_file(io, trophy_progress_output_file_path.c_str(), SCE_O_WRONLY, pref_path, "save_trophy_progress");

    auto write_stuff = [&](const void *data, std::uint32_t amount) -> int {
        return write_file(output, data, amount, io, "save_trophy_progress_file");
    };

    write_stuff(&TROPHY_USR_MAGIC, 4);
    write_stuff(trophy_progress, sizeof(trophy_progress));
    write_stuff(trophy_availability, sizeof(trophy_availability));
    write_stuff(&trophy_count, 4);
    write_stuff(&platinum_trophy_id, 4);

    write_stuff(&unlock_timestamps[0], (std::uint32_t)unlock_timestamps.size() * 8);
    write_stuff(&trophy_kinds[0], (std::uint32_t)trophy_kinds.size());

    close_file(io, output, "save_trophy_progress_file");
}

bool Context::load_trophy_progress_file(const SceUID &progress_input_file) {
    // Check magic
    std::uint32_t magic;
    auto read_stuff = [&](void *data, std::uint32_t amount) -> int {
        return read_file(data, io, progress_input_file, amount, "load_trophy_progress_file");
    };

    if (read_stuff(&magic, 4) != 4 || magic != TROPHY_USR_MAGIC) {
        return false;
    }

    std::fill(trophy_progress, trophy_progress + (MAX_TROPHIES >> 5), 0);
    if (read_stuff(trophy_progress, sizeof(trophy_progress)) != sizeof(trophy_progress)) {
        return false;
    }

    if (read_stuff(trophy_availability, sizeof(trophy_availability)) != sizeof(trophy_availability)) {
        return false;
    }

    // Read trophy count
    if (read_stuff(&trophy_count, 4) != 4) {
        return false;
    }

    // Read platinum trophy ID
    if (read_stuff(&platinum_trophy_id, 4) != 4) {
        return false;
    }

    // Read timestamps
    if (read_stuff(&unlock_timestamps[0], (std::uint32_t)unlock_timestamps.size() * 8) != (int)unlock_timestamps.size() * 8) {
        return false;
    }

    // Read trophy type (shinyyyy!! *yes this is lord of the ring reference*)
    if (read_stuff(&trophy_kinds[0], (std::uint32_t)trophy_kinds.size()) != (int)trophy_kinds.size()) {
        return false;
    }

    return true;
}

bool Context::unlock_trophy(std::int32_t id, np::NpTrophyError *err, const bool force_unlock) {
    if (id < 0 || id >= np::trophy::MAX_TROPHIES || trophy_kinds[id] == np::trophy::TrophyType::INVALID) {
        if (err) {
            *err = np::NpTrophyError::TROPHY_ID_INVALID;
        }

        return false;
    }

    if (trophy_kinds[id] == np::trophy::TrophyType::PLATINUM && !force_unlock) {
        if (err) {
            *err = np::NpTrophyError::TROPHY_PLATINUM_IS_UNBREAKABLE;
        }

        return false;
    }

    if (trophy_progress[id >> 5] & (1 << (id & 31))) {
        if (err) {
            *err = np::NpTrophyError::TROPHY_ALREADY_UNLOCKED;
        }

        return false;
    }

    SET_TROPHY_BIT(trophy_progress, id);

    if (err) {
        *err = np::NpTrophyError::TROPHY_ERROR_NONE;
    }

    save_trophy_progress_file();

    return true;
}

const int Context::total_trophy_unlocked() {
    int total = 0;

    for (int i = 0; i<MAX_TROPHIES>> 5; i++) {
        if (trophy_progress[i] != 0) {
            if (trophy_progress[i] == 0xFFFFFFFF) {
                total += 32;
            } else {
                for (int j = 0; j < 32; j++) {
                    if (trophy_progress[i] & (1 << j)) {
                        total++;
                    }
                }
            }
        }
    }

    return total;
}

bool Context::get_trophy_description(const std::int32_t id, std::string &name, std::string &detail) {
    if (id < 0 || id >= MAX_TROPHIES) {
        return false;
    }

    if (trophy_detail_xml.empty()) {
        const std::string fname = fmt::format("TROP_{:0>2d}.SFM", lang);

        if (!read_trophy_entry_to_buffer(trophy_file, fname.c_str(), trophy_detail_xml)) {
            if (!read_trophy_entry_to_buffer(trophy_file, "TROP.SFM", trophy_detail_xml)) {
                return false;
            }
        }
    }

    // Parse it
    pugi::xml_document doc;
    const auto result = doc.load_string(trophy_detail_xml.c_str());

    if (!result) {
        return false;
    }

    // Try to find the description for the id
    for (auto trop : doc.child("trophyconf")) {
        if ((strncmp(trop.name(), "trophy", 6) == 0) && (trop.attribute("id").as_uint() == id)) {
            name = trop.child("name").text().as_string();
            detail = trop.child("detail").text().as_string();

            return true;
        }
    }

    return false;
}
} // namespace trophy
} // namespace np

np::trophy::ContextHandle create_trophy_context(NpState &np, IOState &io, const std::string &pref_path,
    const np::CommunicationID *custom_comm, const std::uint32_t lang, np::NpTrophyError *error) {
    if (!custom_comm) {
        custom_comm = &np.comm_id;
    }

    if (error)
        *error = np::NpTrophyError::TROPHY_ERROR_NONE;

#define TROPHY_RET_ERROR(err)            \
    if (error)                           \
        *error = np::NpTrophyError::err; \
    return np::trophy::INVALID_CONTEXT_HANDLE

    // Check if a context has already been created for this communication ID
    for (std::size_t i = 0; i < np.trophy_state.contexts.size(); i++) {
        if (np.trophy_state.contexts[i].valid && np.trophy_state.contexts[i].comm_id == *custom_comm) {
            TROPHY_RET_ERROR(TROPHY_CONTEXT_EXIST);
        }
    }

    // Initialize the stream
    const auto unique_trophy_folder = fmt::format("{}_{:0>2d}/", custom_comm->data, custom_comm->num);
    const auto trophy_file_path = device::construct_normalized_path(VitaIoDevice::app0, "sce_sys/trophy/" + unique_trophy_folder + "TROPHY.TRP");

    // Try to open the file
    const SceUID trophy_file = open_file(io, trophy_file_path.c_str(), SCE_O_RDONLY, pref_path, "create_trophy_context");
    if (trophy_file < 0) {
        TROPHY_RET_ERROR(TROPHY_CONTEXT_FILE_NON_EXIST);
    }

    // Try to open the trophy save file. The context will automatically took the default profile to perform trophy
    // operations on.
    auto trophy_progress_save_file = device::construct_normalized_path(VitaIoDevice::ux0, "user/" + io.user_id + "/trophy/data/" + unique_trophy_folder);

    create_dir(io, trophy_progress_save_file.c_str(), 0, pref_path, "create_trophy_context", true);
    trophy_progress_save_file += "TROPUSR.DAT";
    const SceUID trophy_progress_file_inp = open_file(io, trophy_progress_save_file.c_str(), SCE_O_RDONLY, pref_path, "create_trophy_context");

    np::trophy::Context *new_context = nullptr;

    // Yesss, we can now go fully creative
    // Check for free slot first
    for (auto &context : np.trophy_state.contexts) {
        if (!context.valid) {
            context.comm_id = *custom_comm;
            context.trophy_file_stream = trophy_file;
            context.trophy_progress_output_file_path = trophy_progress_save_file;
            context.valid = true;

            new_context = &context;
        }
    }

    if (!new_context) {
        // Yikes. Create new one.
        np.trophy_state.contexts.emplace_back(*custom_comm, io, trophy_file, trophy_progress_save_file);
        np.trophy_state.contexts.back().id = static_cast<np::trophy::ContextHandle>(np.trophy_state.contexts.size());

        new_context = &np.trophy_state.contexts.back();
        new_context->pref_path = pref_path;
    }

    new_context->lang = lang;
    new_context->trophy_file.header_parse();

    if (trophy_progress_file_inp > 0) {
        new_context->load_trophy_progress_file(trophy_progress_file_inp);
        close_file(io, trophy_progress_file_inp, "create_trophy_context");
    } else {
        new_context->init_info_from_trp();
    }

#undef TROPHY_RET_ERROR

    return new_context->id;
}

np::trophy::Context *get_trophy_context(NpTrophyState &state, const np::trophy::ContextHandle handle) {
    if (handle < 0 || handle > static_cast<np::trophy::ContextHandle>(state.contexts.size())) {
        return nullptr;
    }

    return &state.contexts[handle - 1];
}

bool destroy_trophy_context(NpTrophyState &state, const np::trophy::ContextHandle handle) {
    if (handle < 0 || handle > static_cast<np::trophy::ContextHandle>(state.contexts.size())) {
        return false;
    }

    if (!state.contexts[handle - 1].valid) {
        return false;
    }

    close_file(state.contexts[handle - 1].io, state.contexts[handle - 1].trophy_file_stream, "destroy_trophy_context");
    state.contexts[handle - 1].valid = false;

    return true;
}
