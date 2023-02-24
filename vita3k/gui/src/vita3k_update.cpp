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

#include "private.h"

#include <config/state.h>
#include <config/version.h>
#include <gui/functions.h>

#include <SDL.h>
#include <boost/algorithm/string.hpp>

#include <https/functions.h>

#include <regex>

namespace gui {

enum Vita3kUpdate {
    NOT_COMPLETE_UPDATE,
    NO_UPDATE,
    HAS_UPDATE,
    DESCRIPTION,
    UPDATE_VITA3K,
    DOWNLOAD,
};

static Vita3kUpdate state;
static VitaAreaState vita_area_state;

static int git_version;
static std::vector<std::pair<std::string, std::string>> git_commit_desc_list;
bool init_vita3k_update(GuiState &gui) {
    state = NO_UPDATE;
    git_commit_desc_list.clear();
    git_version = 0;
    vita_area_state = {};
    const auto latest_link = "https://api.github.com/repos/Vita3K/Vita3K/releases/latest";

    // Get Build number of latest release
    const auto version = https::get_web_regex_result(latest_link, std::regex("Vita3K Build: (\\d+)"));
    if (!version.empty() && std::isdigit(version[0]))
        git_version = std::stoi(version);
    else {
        LOG_WARN("Failed to get current git version, try again later\n{}", version);
        gui.help_menu.vita3k_update = false;
        return false;
    }

    const auto dif_from_current = git_version - app_number;
    const auto has_update = dif_from_current > 0;
    if (has_update) {
        std::thread get_commit_desc([dif_from_current]() {
            // Calculate Page and Per Page Get
            std::vector<std::pair<uint32_t, uint32_t>> page_count;
            for (int32_t i = 0; i < dif_from_current; i += 100) {
                const auto page = i / 100 + 1;
                const auto per_page = std::min(dif_from_current - i, int32_t(100));
                page_count.push_back({ page, per_page });
            }

            // Browse all page
            for (const auto &page : page_count) {
                const auto continuous_link = fmt::format(R"(https://api.github.com/repos/Vita3K/Vita3K/commits?sha=continuous&page={}&per_page={})", page.first, dif_from_current < 100 ? dif_from_current : 100);

                // Get response from github api
                auto response = https::get_web_response(continuous_link);

                // Check if response is not empty
                if (!response.empty()) {
                    // Get commits from response with remove HTTP header
                    std::string commits = response.substr(response.find("\r\n\r\n") + 4);
                    std::string msg, sha;
                    std::smatch match;

                    // Replace \" to &quot; for help regex search message
                    boost::replace_all(commits, "\\\"", "&quot;");

                    // Using regex to get sha from commits
                    const std::regex commit_regex("\"sha\":\"([a-f0-9]{40})\".*?\"message\":\"([^\"]+)\"");
                    while (std::regex_search(commits, match, commit_regex)) {
                        // Get sha and message from regex match result
                        sha = match[1];
                        msg = match[2];

                        if (!sha.empty() && !msg.empty()) {
                            // Replace &quot; to \" for get back original message
                            boost::replace_all(msg, "&quot;", "\"");
                            // Replace \r and \n to new line
                            boost::replace_all(msg, "\\r\\n", "\n");
                            boost::replace_all(msg, "\\n", "\n");

                            // Add commit to list
                            git_commit_desc_list.push_back({ sha, msg });
                        }

                        // Remove current commit for next search
                        const std::regex end_commit(R"(\s*"parents":\[\{"sha":"[a-f0-9]{40}","url":"[^"]+","html_url":"[^"]+"\}\]\})");
                        if (std::regex_search(commits, match, end_commit))
                            commits = match.suffix();
                    }
                }
            }
        });
        get_commit_desc.detach();

        state = HAS_UPDATE;
    }

    if (has_update || gui.help_menu.vita3k_update) {
        vita_area_state = gui.vita_area;
        gui.vita_area.home_screen = false;
        gui.vita_area.live_area_screen = false;
        gui.vita_area.start_screen = false;
        gui.vita_area.information_bar = true;
    }

    return has_update;
}

static void download_update(const std::string base_path) {
    std::thread download([base_path]() {
        std::string download_continuous_link = "https://github.com/Vita3K/Vita3K/releases/download/continuous";
#ifdef WIN32
        download_continuous_link += "/windows-latest.zip";
#elif defined(__APPLE__)
        download_continuous_link += "/macos-latest.dmg";
#else
        download_continuous_link += "/ubuntu-latest.zip";
#endif

#ifdef __APPLE__
        const std::string archive_ext = ".dmg";
#else
        const std::string archive_ext = ".zip";
#endif

        const auto vita3k_latest_path = base_path + "vita3k-latest" + archive_ext;

        LOG_INFO("Attempting to download and extract the latest Vita3K version {} in progress...", git_version);
        if (https::download_file(download_continuous_link, vita3k_latest_path)) {
            SDL_Event event;
            event.type = SDL_QUIT;
            SDL_PushEvent(&event);

#ifdef WIN32
            const auto vita3K_batch = fmt::format("{}/update-vita3k.bat", base_path);
#elif defined(__APPLE__)
            const auto vita3K_batch = fmt::format("sh {}/update-vita3k.sh", base_path);
#else
            const auto vita3K_batch = fmt::format("chmod +x {}/update-vita3k.sh && {}/update-vita3k.sh", base_path, base_path);
#endif

            std::system(vita3K_batch.c_str());
        } else {
            state = NOT_COMPLETE_UPDATE;
            LOG_WARN("Download failed, please try again later.");
        }
    });
    download.detach();
}

void draw_vita3k_update(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);

    const auto BUTTON_SIZE = ImVec2(150.f * SCALE.x, 46.f * SCALE.y);
    const auto is_background = gui.apps_background.find("NPXS10015") != gui.apps_background.end();

    auto common = emuenv.common_dialog.lang.common;
    auto lang = gui.lang.vita3k_update;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::Begin("##vita3k_update", &gui.help_menu.vita3k_update, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    if (is_background)
        ImGui::GetBackgroundDrawList()->AddImage(gui.apps_background["NPXS10015"], ImVec2(0.f, 0.f), display_size);
    else
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, 0.f), display_size, IM_COL32(36.f, 120.f, 12.f, 255.f), 0.f, ImDrawCornerFlags_All);

    ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
    ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - (ImGui::CalcTextSize(lang["title"].c_str()).x / 2.f), 44.f * SCALE.y));
    ImGui::Text("%s", lang["title"].c_str());
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (6.f * SCALE.y));
    ImGui::Separator();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
    switch (state) {
    case NOT_COMPLETE_UPDATE:
        ImGui::SetCursorPos(ImVec2(display_size.x / 2.f - (ImGui::CalcTextSize(lang["not_complete_update"].c_str()).x / 2.f), (display_size.y / 2.f) - ImGui::GetFontSize()));
        ImGui::Text("%s", lang["not_complete_update"].c_str());

        break;
    case NO_UPDATE: {
        const auto no_update_str = app_number > git_version ? lang["later_version_already_installed"].c_str() : lang["latest_version_already_installed"].c_str();
        ImGui::SetCursorPos(ImVec2(display_size.x / 2.f - (ImGui::CalcTextSize(no_update_str).x / 2.f), display_size.y / 2.f));
        ImGui::Text("%s", no_update_str);

        break;
    }
    case HAS_UPDATE: {
        ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - (ImGui::CalcTextSize(lang["new_version_available"].c_str()).x / 2.f), (display_size.y / 2.f) - ImGui::GetFontSize()));
        ImGui::Text("%s", lang["new_version_available"].c_str());
        const auto version_str = fmt::format(fmt::runtime(lang["version"].c_str()), git_version);
        ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - (ImGui::CalcTextSize(version_str.c_str()).x / 2.f), ImGui::GetCursorPosY() + (40.f * SCALE.x)));
        ImGui::Text("%s", version_str.c_str());

        break;
    }
    case DESCRIPTION: {
        ImGui::Spacing();
        ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
        const auto new_features_str = fmt::format(fmt::runtime(lang["new_features"]), git_version);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2) - (ImGui::CalcTextSize(new_features_str.c_str()).x / 2.f));
        ImGui::Text("%s", new_features_str.c_str());
        ImGui::Spacing();
        ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, 136.0f * SCALE.y), ImGuiCond_Always, ImVec2(0.5f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 4.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 15.f * SCALE.x);
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::BeginChild("##description_child", ImVec2(860 * SCALE.x, 334.f * SCALE.y), true, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetWindowFontScale(0.8f);
        for (const auto &desc : git_commit_desc_list) {
            const auto pos = ImGui::GetCursorPosY();
            ImGui::Spacing();
            ImGui::TextWrapped("%s", !desc.second.empty() ? desc.second.c_str() : "Loading...");
            ImGui::Spacing();
            ImGui::SetCursorPosY(pos);
            ImGui::PushID(desc.first.c_str());
            if (ImGui::Selectable("##commit_link", false, ImGuiSelectableFlags_None, ImVec2(ImGui::GetWindowWidth(), ImGui::CalcTextSize(desc.second.c_str(), nullptr, false, ImGui::GetWindowWidth() - (20.f * SCALE.x) - (15.f * SCALE.x)).y + 20.f))) {
                open_path(fmt::format("https://github.com/Vita3K/Vita3K/commit/{}", desc.first));
            }
            ImGui::PopID();
            ImGui::Separator();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(3);

        break;
    }
    case UPDATE_VITA3K:
        ImGui::SetCursorPos(ImVec2(display_size.x / 2.f - (ImGui::CalcTextSize(lang["update_vita3k"].c_str()).x / 2.f), (display_size.y / 2.f) - ImGui::GetFontSize()));
        ImGui::Text("%s", lang["update_vita3k"].c_str());

        break;
    case DOWNLOAD: {
        const auto calc_str = ImGui::CalcTextSize(lang["downloading"].c_str(), 0, false, 776.f * SCALE.x);
        ImGui::SetCursorPos(ImVec2(92.f * SCALE.x, (display_size.y / 2.f) - (calc_str.y / 2.f)));
        ImGui::PushTextWrapPos(868.f * SCALE.x);
        ImGui::Text("%s", lang["downloading"].c_str());
        ImGui::PopTextWrapPos();

        break;
    }
    default:
        break;
    }

    ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
    ImGui::SetCursorPosY(display_size.y - BUTTON_SIZE.y - (20.f * SCALE.y));
    ImGui::Separator();
    if (state == DOWNLOAD)
        ImGui::BeginDisabled();
    ImGui::SetCursorPos(ImVec2(10.f * SCALE.x, (display_size.y - BUTTON_SIZE.y - (12.f * SCALE.y))));
    if (ImGui::Button(state < DESCRIPTION ? common["cancel"].c_str() : lang["back"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(emuenv.cfg.keyboard_button_cross)) {
        if (state < DESCRIPTION) {
            if (fs::exists("windows-latest.zip"))
                fs::remove("windows-latest.zip");
            gui.vita_area = vita_area_state;
            gui.help_menu.vita3k_update = false;
        } else
            state = (Vita3kUpdate)(state - 1);
    }
    if (state == DOWNLOAD)
        ImGui::EndDisabled();

    if (state > NO_UPDATE && state < DOWNLOAD) {
        ImGui::SetCursorPos(ImVec2(display_size.x - BUTTON_SIZE.x - (10.f * SCALE.x), (display_size.y - BUTTON_SIZE.y - (12.f * SCALE.y))));
        if (ImGui::Button(state < UPDATE_VITA3K ? lang["next"].c_str() : lang["update"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(emuenv.cfg.keyboard_button_circle)) {
            state = (Vita3kUpdate)(state + 1);
            if (state == DOWNLOAD)
                download_update(emuenv.base_path);
        }
        if (state == DOWNLOAD)
            ImGui::EndDisabled();
    }
    ImGui::PopStyleVar();
    ImGui::End();
}

} // namespace gui
