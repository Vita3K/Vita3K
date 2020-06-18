#include <pvf/state.h>

bool init(PvfState &state) {
    return true;
}

ScePvfLibID create_pvf_library(PvfState &state) {
    ++state.next_lib_id;
    auto id = state.next_lib_id;
    auto lib = std::shared_ptr<PvfLibrary>(new PvfLibrary);
    if (lib->init()) {
        return 0;
    }

    lib->lib_id = id;
    state.libs.emplace(id, lib);
    return id;
}

std::shared_ptr<PvfLibrary> get_pvf_library(PvfState &state, ScePvfLibID lib_id) {
    return state.libs[lib_id];
}

std::shared_ptr<PvfFont> get_pvf_font(PvfState &state, ScePvfFontId font_id) {
    auto lib_id = state.font_to_lib[font_id];
    return state.libs[lib_id]->get_font(font_id);
}

void clean_pvf_library(PvfState &state, ScePvfLibID lib_id) {
    auto lib = state.libs[lib_id];
    for (const auto &[font_id, _] : lib->fonts) {
        clean_pvf_font(state, font_id);
    }
    state.libs.erase(lib_id);
}

void clean_pvf_font(PvfState &state, ScePvfFontId font_id) {
    auto lib_id = state.font_to_lib[font_id];
    state.libs[lib_id]->fonts.erase(font_id);
    state.font_to_lib.erase(font_id);
}
