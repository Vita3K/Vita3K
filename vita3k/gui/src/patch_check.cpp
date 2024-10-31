// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include "private.h"

#include <config/state.h>
#include <dialog/state.h>

#include <gui/functions.h>

#include <patch_check/functions.h>

#include <regex>

namespace gui {

namespace {

enum class HeadingLevel {
    None = 0,
    H1,
    H2,
    H3,
    H4,
    H5,
    H6
};

struct UpdateLine {
    ImVec4 font_color;
    uint32_t font_size;
    HeadingLevel heading_level;
    bool in_list;
    bool is_bullet;
    std::string text;
};

static std::map<std::string, std::vector<UpdateLine>> update_history_infos;
static const std::string BULLET = "\u30FB";
static bool is_new_version = false;
static std::string downloading_update_msg;

static patch_check::RemoteUpdateInfo *ensure_remote_update_info(GuiState &gui, EmuEnvState &emuenv, const std::string &id) {
    const auto &APP_INDEX = get_app_index(gui, id);
    if (!APP_INDEX) {
        LOG_ERROR("App index not found for id: {}", id);
        return nullptr;
    }

    return patch_check::ensure_cached_remote_update_info(emuenv.pref_path, id, APP_INDEX->title_id, APP_INDEX->app_ver, emuenv.cfg.sys_lang);
}

ImVec4 get_history_line_color(const std::string &color_code) {
    if (color_code.empty())
        return GUI_COLOR_TEXT;

    const uint32_t color = std::strtoul(color_code.c_str(), nullptr, 16);
    return ImVec4((float((color >> 16) & 0xFF)) / 255.f, (float((color >> 8) & 0xFF)) / 255.f, (float((color >> 0) & 0xFF)) / 255.f, 1.f);
}

HeadingLevel to_gui_heading_level(patch_check::HistoryHeadingLevel level) {
    return static_cast<HeadingLevel>(level);
}

const std::map<HeadingLevel, float> HEADING_SIZES = {
    { HeadingLevel::H1, 30.0f },
    { HeadingLevel::H2, 24.0f },
    { HeadingLevel::H3, 20.0f },
    { HeadingLevel::H4, 16.0f },
    { HeadingLevel::H5, 14.0f },
    { HeadingLevel::H6, 12.0f }
};

const std::map<int, float> FONT_SIZE_PX = {
    { 1, 13.3f },
    { 2, 16.7f },
    { 3, 20.0f },
    { 4, 23.3f },
    { 5, 30.0f },
    { 6, 40.0f },
    { 7, 60.0f }
};

bool cache_update_history(const std::map<std::string, std::vector<patch_check::UpdateHistoryLine>> &history_lines, const bool new_version) {
    update_history_infos.clear();
    is_new_version = new_version;
    for (const auto &[version, lines] : history_lines) {
        auto &gui_lines = update_history_infos[version];
        for (const auto &line : lines)
            gui_lines.push_back({ get_history_line_color(line.color), line.font_size, to_gui_heading_level(line.heading_level), line.in_list, line.is_bullet, line.text });
    }

    return !update_history_infos.empty();
}

} // namespace

void process_app_update(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    auto &PATCH_CHECK_STATE = patch_check::get_state();
    const auto ID = fs::path(app_path).stem().string();
    const auto ERASE_UPDATE_INSTALL_STATE = [&]() {
        PATCH_CHECK_STATE.erase_update_install(ID);
        if (app_path != ID)
            PATCH_CHECK_STATE.erase_update_install(app_path);
    };

    auto UPDATE_INSTALL_INFO = PATCH_CHECK_STATE.find_update_install(ID);
    if (!UPDATE_INSTALL_INFO && (app_path != ID))
        UPDATE_INSTALL_INFO = PATCH_CHECK_STATE.find_update_install(app_path);

    if (UPDATE_INSTALL_INFO && (UPDATE_INSTALL_INFO->state == patch_check::UpdateState::WAITING_INSTALL)
        && (UPDATE_INSTALL_INFO->pkg_path.empty() || !fs::exists(UPDATE_INSTALL_INFO->pkg_path))) {
        ERASE_UPDATE_INSTALL_STATE();
        UPDATE_INSTALL_INFO = nullptr;
    }

    if (!UPDATE_INSTALL_INFO || (UPDATE_INSTALL_INFO->state == patch_check::UpdateState::NONE)) {
        const auto UPDATE_INFO = ensure_remote_update_info(gui, emuenv, ID);
        if (!UPDATE_INFO)
            return;
        if ((UPDATE_INFO->size * 2) > fs::space(emuenv.pref_path).available) {
            downloading_update_msg = fmt::format(fmt::runtime(gui.lang.patch_check["not_enough_space"]), get_unit_size(UPDATE_INFO->size * 2));
            gui.vita_area.update_history_info = true;
        } else {
            if (cache_remote_update_history(gui, emuenv, app_path))
                gui.vita_area.update_history_info = true;
        }
    } else if (UPDATE_INSTALL_INFO->state == patch_check::UpdateState::DOWNLOADING) {
        downloading_update_msg = gui.lang.patch_check["downloading_app_update"];
        gui.vita_area.update_history_info = true;
    } else if (UPDATE_INSTALL_INFO->state == patch_check::UpdateState::WAITING_INSTALL) {
        update_install(gui, emuenv, ID);
    }
}

bool cache_remote_update_history(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const auto &APP_INDEX = get_app_index(gui, app_path);
    if (!APP_INDEX)
        return false;

    std::map<std::string, std::vector<patch_check::UpdateHistoryLine>> history_lines;
    bool new_version = false;
    if (!patch_check::get_update_history(emuenv.pref_path, APP_INDEX->title_id, APP_INDEX->app_ver, emuenv.cfg.sys_lang, history_lines, new_version)) {
        LOG_WARN("Failed to get update history for app: {}", app_path);
        return false;
    }

    return cache_update_history(history_lines, new_version);
}

bool cache_local_update_history(EmuEnvState &emuenv, const std::string &app_path) {
    std::map<std::string, std::vector<patch_check::UpdateHistoryLine>> history_lines;
    bool new_version = false;
    if (!patch_check::get_local_update_history(emuenv.pref_path, app_path, emuenv.cfg.sys_lang, history_lines, new_version)) {
        LOG_WARN("Failed to get local update history for app: {}", app_path);
        return false;
    }

    return cache_update_history(history_lines, new_version);
}

void draw_update_infos(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);

    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const ImVec2 WINDOW_SIZE(756.0f * SCALE.x, 436.0f * SCALE.y);
    const ImVec2 BUTTON_SIZE(320.f * SCALE.x, 46.f * SCALE.y);

    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(VIEWPORT_SIZE, ImGuiCond_Always);
    ImGui::Begin("##update_infos", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowBgAlpha(0.999f * SCALE.y);
    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x + (VIEWPORT_SIZE.x / 2.f) - (WINDOW_SIZE.x / 2.f), emuenv.logical_viewport_pos.y + (VIEWPORT_SIZE.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
    auto &lang = gui.lang.app_context;
    auto &patch_check = gui.lang.patch_check;
    auto &common = emuenv.common_dialog.lang.common;
    const auto &UPDATE_APP_PATH = gui.live_area_app_current_open >= 0 ? gui.live_area_current_open_apps_list[gui.live_area_app_current_open] : emuenv.app_path;
    const auto ID = is_new_version ? fs::path(UPDATE_APP_PATH).stem().string() : "";
    const auto SHOW_MESSAGE_ONLY = !downloading_update_msg.empty();

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
    ImGui::BeginChild("##update_infos_child", WINDOW_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);

    const ImVec2 UPDATE_LIST_PADDING(24.f * SCALE.x, 56.f * SCALE.y);
    const auto BUTTON_BOTTOM_PADDING = 22.f * SCALE.y;

    if (SHOW_MESSAGE_ONLY) {
        ImGui::SetWindowFontScale(1.34f * RES_SCALE.y);
        const auto STR_SIZE = ImGui::CalcTextSize(downloading_update_msg.c_str(), 0, false, WINDOW_SIZE.x - (172.f * SCALE.x));
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (STR_SIZE.x / 2.f), (WINDOW_SIZE.y / 2.f) - (STR_SIZE.y / 2.f) - (BUTTON_SIZE.y / 2.f) - (22.f * SCALE.y)));
        ImGui::PushTextWrapPos(WINDOW_SIZE.x - (86.f * SCALE.x));
        ImGui::Text("%s", downloading_update_msg.c_str());
        ImGui::PopTextWrapPos();
    } else {
        const ImVec2 UPDATE_LIST_SIZE(WINDOW_SIZE.x - (UPDATE_LIST_PADDING.x * 2.f), WINDOW_SIZE.y - UPDATE_LIST_PADDING.y - (74.f * SCALE.y));
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowPos().x + UPDATE_LIST_PADDING.x, ImGui::GetWindowPos().y + UPDATE_LIST_PADDING.y));
        ImGui::BeginChild("##info_update_list", UPDATE_LIST_SIZE, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetWindowFontScale(1.48f * RES_SCALE.y);
        if (is_new_version) {
            ImGui::TextWrapped("%s", patch_check["new_app_version_available"].c_str());
            const auto UPDATE_INFOS = patch_check::get_state().find_cached_update_info(ID);
            if (!UPDATE_INFOS)
                return;
            auto SIZE_STR = get_unit_size(UPDATE_INFOS->size);
            SIZE_STR = std::regex_replace(SIZE_STR, std::regex("\\s+"), "");
            ImGui::NewLine();
            ImGui::TextColored(GUI_COLOR_TEXT, "%s(%s)", fmt::format(fmt::runtime(patch_check["version"]), UPDATE_INFOS->version).c_str(), SIZE_STR.c_str());
            ImGui::NewLine();
        }

        const auto WRAP_WIDTH = UPDATE_LIST_SIZE.x - ImGui::GetStyle().ScrollbarSize;

        for (auto it = update_history_infos.rbegin(); it != update_history_infos.rend(); ++it) {
            ImGui::SetWindowFontScale(1.48f * RES_SCALE.y);
            const auto VERSION_STR = is_new_version ? fmt::format("{}", it->first) : fmt::format(fmt::runtime(lang.main["history_version"]), it->first);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", VERSION_STR.c_str());

            for (const auto &line : it->second) {
                const float FONT_SCALE = (line.heading_level != HeadingLevel::None ? HEADING_SIZES.at(line.heading_level) : FONT_SIZE_PX.at(line.font_size)) / ImGui::GetFont()->FontSize;
                ImGui::SetWindowFontScale(FONT_SCALE * RES_SCALE.y);

                const auto BULLET_SIZE = ImGui::CalcTextSize(BULLET.c_str());
                if (line.in_list || line.is_bullet || line.text.find("*") != std::string::npos) {
                    const auto TEXT_POS = ImGui::GetCursorPosY();
                    if (line.is_bullet)
                        ImGui::TextUnformatted(BULLET.c_str());

                    ImGui::SetCursorPos(ImVec2(BULLET_SIZE.x + ImGui::GetStyle().ItemSpacing.x, TEXT_POS));
                }

                ImGui::PushTextWrapPos(WRAP_WIDTH);
                ImGui::PushStyleColor(ImGuiCol_Text, line.font_color);
                ImGui::TextWrapped("%s", line.text.c_str());
                ImGui::PopStyleColor();
                ImGui::PopTextWrapPos();
                ImGui::ScrollWhenDragging();
            }

            if (std::next(it) != update_history_infos.rend())
                ImGui::NewLine();
        }
        ImGui::EndChild();
    }
    ImGui::SetWindowFontScale(1.25f * RES_SCALE.y);
    if (is_new_version && !SHOW_MESSAGE_ONLY) {
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - BUTTON_SIZE.x - (10.f * SCALE.x), WINDOW_SIZE.y - BUTTON_SIZE.y - BUTTON_BOTTOM_PADDING));
        if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle)))
            gui.vita_area.update_history_info = false;
        ImGui::SameLine(0, 20.f * SCALE.x);
        if (ImGui::Button(patch_check["download"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
            gui.vita_area.update_history_info = false;
            update_notice_info(gui, emuenv, "downloading", ID);
        }
    } else {
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (BUTTON_SIZE.x / 2.f), WINDOW_SIZE.y - BUTTON_SIZE.y - BUTTON_BOTTOM_PADDING));
        if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
            if (SHOW_MESSAGE_ONLY)
                downloading_update_msg.clear();
            gui.vita_area.update_history_info = false;
        }
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
}

} // namespace gui
