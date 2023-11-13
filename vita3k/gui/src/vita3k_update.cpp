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

#include <boost/algorithm/string.hpp>

#include <config/state.h>
#include <config/version.h>

#include <gui/functions.h>
#include <util/net_utils.h>
#include <util/string_utils.h>

#include <SDL.h>

#ifdef WIN32
#include <combaseapi.h>
#endif

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

struct GitCommitDesc {
    std::string author;
    std::string sha;
    std::string msg;
};

static int git_version;
static std::vector<GitCommitDesc> git_commit_desc_list;
bool init_vita3k_update(GuiState &gui) {
    state = NO_UPDATE;
    git_commit_desc_list.clear();
    git_version = 0;
    vita_area_state = {};
    const auto latest_link = "https://api.github.com/repos/Vita3K/Vita3K/releases/latest";

    // Get Build number of latest release
    const auto version = net_utils::get_web_regex_result(latest_link, std::regex("Vita3K Build: (\\d+)"));
    if (!version.empty() && std::all_of(version.begin(), version.end(), ::isdigit))
        git_version = string_utils::stoi_def(version, 0, "git version");
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
                auto commits = net_utils::get_web_response(continuous_link);

                // Check if response is not empty
                if (!commits.empty()) {
                    std::string author, msg, sha;
                    std::smatch match;

                    // Replace \" to &quot; for help regex search message
                    boost::replace_all(commits, "\\\"", "&quot;");

                    // Using regex to get sha, author and message from commits
                    const std::regex commit_regex("\"sha\":\"([a-f0-9]{40})\".*?\"author\":\\{\"name\":\"([^\"]+)\".*?\"message\":\"([^\"]+)\"");
                    while (std::regex_search(commits, match, commit_regex)) {
                        // Get sha and message from regex match result
                        sha = match[1];
                        author = match[2];
                        msg = match[3];

                        if (!sha.empty() && !author.empty() && !msg.empty()) {
                            // Replace &quot; to \" for get back original message
                            boost::replace_all(msg, "&quot;", "\"");

                            // Replace \r and \n to new line
                            boost::replace_all(msg, "\\r\\n", "\n");
                            boost::replace_all(msg, "\\n", "\n");

                            // Add commit to list
                            git_commit_desc_list.push_back({ author, sha, msg });
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

static std::atomic<float> progress(0);
static std::atomic<uint64_t> remaining(0);
static net_utils::ProgressState progress_state{};

static void download_update(const std::string &base_path) {
    progress_state.download = true;
    progress_state.pause = false;
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

        const std::string version = std::to_string(git_version);

        // Download latest Vita3K version
        LOG_INFO("Attempting to download and extract the latest Vita3K version {} in progress...", git_version);

        static const auto progress_callback = [](float updated_progress, uint64_t updated_remaining) {
            progress = updated_progress;
            remaining = updated_remaining;
            return &progress_state;
        };

        if (net_utils::download_file(download_continuous_link, vita3k_latest_path, progress_callback)) {
            SDL_Event event;
            event.type = SDL_QUIT;
            SDL_PushEvent(&event);

#ifdef WIN32
            const auto vita3K_batch = fmt::format("\"{}/update-vita3k.bat\"", base_path);
            FreeConsole();
#elif defined(__APPLE__)
            const auto vita3K_batch = fmt::format("sh \"{}/update-vita3k.sh\"", base_path);
#else
            const auto vita3K_batch = fmt::format("chmod +x \"{}/update-vita3k.sh\" && \"{}/update-vita3k.sh\"", base_path, base_path);
#endif
            std::system(vita3K_batch.c_str());
        } else {
            if (progress_state.download) {
                LOG_WARN("Download failed, please try again later.");
                state = NOT_COMPLETE_UPDATE;
            } else
                state = HAS_UPDATE;
        }
    });
    download.detach();
}

static std::string get_remaining_str(LangState &lang, const uint64_t remaining) {
    if (remaining > 60)
        return fmt::format(fmt::runtime(lang.vita3k_update["minutes_left"]), remaining / 60);
    else
        return fmt::format(fmt::runtime(lang.vita3k_update["seconds_left"]), remaining > 0 ? remaining : 1);
}

void draw_vita3k_update(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 display_size(emuenv.viewport_size.x, emuenv.viewport_size.y);
    const auto RES_SCALE = ImVec2(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);
    const ImVec2 WINDOW_POS(emuenv.viewport_pos.x, emuenv.viewport_pos.y);

    const auto BUTTON_SIZE = ImVec2(150.f * SCALE.x, 46.f * SCALE.y);
    const auto is_background = gui.apps_background.contains("NPXS10015");

    auto common = emuenv.common_dialog.lang.common;
    auto lang = gui.lang.vita3k_update;

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::Begin("##vita3k_update", &gui.help_menu.vita3k_update, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    if (is_background)
        ImGui::GetBackgroundDrawList()->AddImage(gui.apps_background["NPXS10015"], WINDOW_POS, display_size);
    else
        ImGui::GetBackgroundDrawList()->AddRectFilled(WINDOW_POS, display_size, IM_COL32(36.f, 120.f, 12.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

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
        ImGui::Columns(2, "commit_columns", true);
        ImGui::SetColumnWidth(0, 200 * SCALE.x);
        const auto space_margin = ImGui::GetStyle().ItemSpacing.x * 2.f;
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["authors"].c_str());
        ImGui::NextColumn();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["comments"].c_str());
        ImGui::Separator();
        ImGui::NextColumn();
        for (const auto &desc : git_commit_desc_list) {
            const auto pos = ImGui::GetCursorPosY();
            const auto author_height = ImGui::CalcTextSize(desc.author.c_str(), nullptr, false, ImGui::GetColumnWidth(0) - space_margin).y;
            const auto comment_height = ImGui::CalcTextSize(desc.msg.c_str(), nullptr, false, ImGui::GetColumnWidth(1) - space_margin).y;
            const auto max_height = std::max(author_height, comment_height);
            ImGui::SetCursorPosY(pos + (max_height / 2.f) - (author_height / 2.f));
            ImGui::TextWrapped("%s", desc.author.c_str());
            ImGui::NextColumn();
            ImGui::SetCursorPosY(pos + (max_height / 2.f) - (comment_height / 2.f));
            ImGui::TextWrapped("%s", desc.msg.c_str());
            ImGui::NextColumn();
            ImGui::SetCursorPosY(pos);
            ImGui::PushID(desc.sha.c_str());
            if (ImGui::Selectable("##commit_link", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(ImGui::GetWindowWidth(), max_height))) {
                open_path(fmt::format("https://github.com/Vita3K/Vita3K/commit/{}", desc.sha));
            }
            ImGui::PopID();
            ImGui::Separator();
        }
        ImGui::Columns(1);
        ImGui::EndChild();
        ImGui::PopStyleVar(3);

        break;
    }
    case UPDATE_VITA3K:
        ImGui::SetCursorPos(ImVec2(display_size.x / 2.f - (ImGui::CalcTextSize(lang["update_vita3k"].c_str()).x / 2.f), (display_size.y / 2.f) - ImGui::GetFontSize()));
        ImGui::Text("%s", lang["update_vita3k"].c_str());

        break;
    case DOWNLOAD: {
        ImGui::SetWindowFontScale(1.25f * RES_SCALE.x);
        ImGui::SetCursorPos(ImVec2(102.f * SCALE.x, ImGui::GetCursorPosY() + (44 * SCALE.y)));
        ImGui::PushTextWrapPos(WINDOW_POS.x + (858.f * SCALE.x));
        ImGui::Text("%s", lang["downloading"].c_str());
        ImGui::PopTextWrapPos();
        ImGui::SetWindowFontScale(1.04f * RES_SCALE.x);
        const auto remaining_str = get_remaining_str(gui.lang, remaining);
        ImGui::SetCursorPos(ImVec2(display_size.x - (90 * SCALE.x) - (ImGui::CalcTextSize(remaining_str.c_str()).x), display_size.y - (196.f * SCALE.y) - ImGui::GetFontSize()));
        ImGui::Text("%s", remaining_str.c_str());
        const float PROGRESS_BAR_WIDTH = 780.f * SCALE.x;
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2) - (PROGRESS_BAR_WIDTH / 2.f), display_size.y - (186.f * SCALE.y)));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
        ImGui::ProgressBar(progress / 100.f, ImVec2(PROGRESS_BAR_WIDTH, 15.f * SCALE.y), "");
        const auto progress_str = std::to_string(uint32_t(progress)).append("%");
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(progress_str.c_str()).x / 2.f), ImGui::GetCursorPosY() + 16.f * emuenv.dpi_scale));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", progress_str.c_str());
        ImGui::PopStyleColor();

        break;
    }
    default:
        break;
    }

    ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
    ImGui::SetCursorPosY(display_size.y - BUTTON_SIZE.y - (20.f * SCALE.y));
    ImGui::Separator();
    ImGui::SetCursorPos(ImVec2(WINDOW_POS.x + (10.f * SCALE.x), (display_size.y - BUTTON_SIZE.y - (12.f * SCALE.y))));
    if (ImGui::Button((state < DESCRIPTION) || (state == DOWNLOAD) ? common["cancel"].c_str() : lang["back"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
        if (state < DESCRIPTION) {
            gui.vita_area = vita_area_state;
            gui.help_menu.vita3k_update = false;
        } else if (state == DOWNLOAD) {
            progress_state.pause = true;
            ImGui::OpenPopup("cancel_update_popup");
        } else
            state = (Vita3kUpdate)(state - 1);
    }

    if (state > NO_UPDATE && state < DOWNLOAD) {
        ImGui::SetCursorPos(ImVec2(display_size.x - WINDOW_POS.x - BUTTON_SIZE.x - (10.f * SCALE.x), (display_size.y - BUTTON_SIZE.y - (12.f * SCALE.y))));
        if (ImGui::Button(state < UPDATE_VITA3K ? lang["next"].c_str() : lang["update"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle))) {
            state = (Vita3kUpdate)(state + 1);
            if (state == DOWNLOAD)
                download_update(emuenv.base_path.string());
        }
    }

    // Draw Cancel popup
    const auto POPUP_SIZE = ImVec2(760.0f * SCALE.x, 436.0f * SCALE.y);
    ImGui::SetNextWindowSize(POPUP_SIZE, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + (display_size.x / 2.f) - (POPUP_SIZE.x / 2.f), WINDOW_POS.y + (display_size.y / 2.f) - (POPUP_SIZE.y / 2.f)), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f * SCALE.x);
    if (ImGui::BeginPopupModal("cancel_update_popup", &progress_state.pause, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration)) {
        const auto LARGE_BUTTON_SIZE = ImVec2(310.f * SCALE.x, 46.f * SCALE.y);
        auto common = emuenv.common_dialog.lang.common;
        const auto str_size = ImGui::CalcTextSize(lang["cancel_update"].c_str(), 0, false, POPUP_SIZE.x - (120.f * SCALE.x));
        ImGui::SetCursorPos(ImVec2(60.f * SCALE.x, (ImGui::GetWindowHeight() / 2.f) - (str_size.y / 2.f)));
        ImGui::TextWrapped("%s", lang["cancel_update"].c_str());
        ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - LARGE_BUTTON_SIZE.x - (20.f * SCALE.x), POPUP_SIZE.y - LARGE_BUTTON_SIZE.y - (22.0f * SCALE.y)));
        if (ImGui::Button(common["no"].c_str(), LARGE_BUTTON_SIZE)) {
            std::unique_lock<std::mutex> lock(progress_state.mutex);
            progress_state.pause = false;
            progress_state.cv.notify_one();

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(common["yes"].c_str(), LARGE_BUTTON_SIZE)) {
            std::unique_lock<std::mutex> lock(progress_state.mutex);
            progress_state.download = false;
            progress_state.pause = false; // Unpause so it gets handled by the callback
            progress_state.cv.notify_one();

            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::End();
}

} // namespace gui
