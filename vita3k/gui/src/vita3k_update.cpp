// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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
#include <dialog/state.h>
#include <gui/functions.h>
#include <util/net_utils.h>
#include <util/string_utils.h>

#include <SDL3/SDL_events.h>

#ifdef _WIN32
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
static std::atomic<bool> cancel_thread{ false };
static std::atomic<bool> thread_running{ false };
static std::mutex commit_desc_list_mutex;
bool init_vita3k_update(GuiState &gui) {
    // Reset all variables
    state = NO_UPDATE;
    git_commit_desc_list.clear();
    git_version = 0;
    vita_area_state = {};

    // Get Build number of latest release
    const auto version = net_utils::get_web_regex_result("https://api.github.com/repos/Vita3K/Vita3K/releases/latest", std::regex("Vita3K Build: (\\d+)"));

    // Check if version is empty or not all digit
    if (version.empty() || !std::all_of(version.begin(), version.end(), ::isdigit)) {
        LOG_WARN("Failed to get current git version, try again later\n{}", version);
        gui.help_menu.vita3k_update = false;
        return false;
    }

    // Set latest git version number
    git_version = string_utils::stoi_def(version, 0, "git version");

    // Get difference between current app number and git version
    const auto dif_from_current = static_cast<uint32_t>(std::max(git_version - app_number, 0));
    const auto has_update = dif_from_current > 0;
    if (has_update) {
        std::thread get_commit_desc([dif_from_current]() {
            // Reset cancel and thread running variable
            cancel_thread = false;
            thread_running = true;

            // Lambda function for get commits from github api
            const auto get_commits = [dif_from_current]() {
                // Set constant for get commits from github api
                static const uint32_t max_per_page = 100;
                static const std::regex commit_regex(R"lit("sha":"([a-f0-9]{40})".*?"author":\{"name":"([^"]+)".*?"message":"([^"]+)")lit");
                static const std::regex end_commit(R"(\s*"parents":\[\{"sha":"[a-f0-9]{40}","url":"[^"]+","html_url":"[^"]+"\}\]\})");

                // Reserve memory for commit description list
                git_commit_desc_list.reserve(dif_from_current);

                // Calculate Page and Last Per Page Get
                const uint32_t page_count = static_cast<uint32_t>(std::ceil(static_cast<double>(dif_from_current) / max_per_page));
                const uint32_t last_per_page = dif_from_current % max_per_page;

                // Browse all page
                for (auto p = 0; p < page_count; p++) {
                    // Check if thread is requested to stop
                    if (cancel_thread)
                        return;

                    // Create link for get commits
                    const auto continuous_link = fmt::format(R"(https://api.github.com/repos/Vita3K/Vita3K/commits?sha=continuous&page={}&per_page={})", p + 1, std::min(dif_from_current, max_per_page));

                    // Get response from github api
                    auto commits = net_utils::get_web_response(continuous_link);

                    // Check if response is empty
                    if (commits.empty()) {
                        LOG_WARN("Failed to get commits from page: {}", p + 1);
                        return;
                    }

                    // Replace \" to &quot; for help regex search message
                    boost::replace_all(commits, "\\\"", "&quot;");

                    const uint32_t commit_count = std::min(dif_from_current - (p * max_per_page), max_per_page);
                    for (uint32_t c = 0; c < commit_count; c++) {
                        // Check if thread is requested to stop
                        if (cancel_thread)
                            return;

                        std::smatch match;

                        // Using regex to get sha, author and message from commits
                        if (!std::regex_search(commits, match, commit_regex) || (match.size() != 4)) {
                            LOG_WARN("Failed to find commit: {}, on page: {}", c + 1, p + 1);
                            return;
                        }

                        // Get sha, Author and message from regex match result
                        const std::string sha = match[1].str();
                        const std::string author = match[2].str();
                        std::string msg = match[3].str();

                        // Replace &quot; to \" for get back original message
                        boost::replace_all(msg, "&quot;", "\"");

                        // Replace \r and \n to new line
                        boost::replace_all(msg, "\\r\\n", "\n");
                        boost::replace_all(msg, "\\n", "\n");

                        {
                            // Add commit description to list with lock mutex for thread safe
                            std::lock_guard<std::mutex> lock(commit_desc_list_mutex);
                            git_commit_desc_list.push_back({ author, sha, msg });
                        }

                        // Find end of commit
                        if (!std::regex_search(commits, match, end_commit)) {
                            LOG_WARN("Failed to find end of commit: {}, on page: {}", c + 1, p + 1);
                            return;
                        }

                        // Remove current commit for next search
                        commits = match.suffix();
                    }
                }
            };

            // Get commits descriptions from github api
            get_commits();

            thread_running = false;
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

static void download_update(const fs::path &base_path) {
    progress_state.download = true;
    progress_state.pause = false;
    std::thread download([base_path]() {
        std::string download_continuous_link = "https://github.com/Vita3K/Vita3K/releases/download/continuous";
#ifdef _WIN32
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

        const auto vita3k_latest_ver_path = base_path / "vita3k-latest-ver";
        const auto vita3k_latest_path = base_path / ("vita3k-latest" + archive_ext);

        const std::string vita3k_version = std::to_string(git_version);

        // check if vita3k_latest_path exist
        if (fs::exists(vita3k_latest_path)) {
            // read latest ver file if exist
            if (fs::ifstream vita3k_latest_ver{ vita3k_latest_ver_path, std::ios::binary }) {
                std::string vita3k_latest_version;
                std::getline(vita3k_latest_ver, vita3k_latest_version);

                // check if latest version of Vita3K is same with current git version for can resume download
                if (vita3k_latest_version == vita3k_version)
                    LOG_INFO("Resume download of version: {}", vita3k_latest_version);
                else {
                    fs::remove(vita3k_latest_ver_path);
                    fs::remove(vita3k_latest_path);
                    LOG_INFO("Start download of new version: {}", vita3k_version);
                }
            }
        }

        // check if latest_ver file exist
        if (!fs::exists(vita3k_latest_ver_path)) {
            // write latest_info file
            if (fs::ofstream vita3k_latest_ver{ vita3k_latest_ver_path, std::ios::binary }) {
                vita3k_latest_ver.write(vita3k_version.c_str(), vita3k_version.length());
            } else {
                LOG_ERROR("Failed to write latest_ver file at {}", vita3k_latest_ver_path.string());
            }
        }

        // Download latest Vita3K version
        LOG_INFO("Attempting to download and extract the latest Vita3K version {} in progress...", git_version);

        static const auto progress_callback = [](float updated_progress, uint64_t updated_remaining) {
            progress = updated_progress;
            remaining = updated_remaining;
            return &progress_state;
        };

        if (net_utils::download_file(download_continuous_link, fs_utils::path_to_utf8(vita3k_latest_path), progress_callback)) {
            SDL_Event event;
            event.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&event);

#ifdef _WIN32
            const auto vita3K_batch = fmt::format("\"{}\\update-vita3k.bat\"", base_path);
            FreeConsole();
#elif defined(__APPLE__)
            const auto vita3K_batch = fmt::format("sh \"{}/update-vita3k.sh\"", base_path);
#else
            const auto vita3K_batch = fmt::format("chmod +x \"{}/update-vita3k.sh\" && \"{}/update-vita3k.sh\"", base_path, base_path);
#endif

            // When success finish download, remove latest ver file
            fs::remove(vita3k_latest_ver_path);

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
    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const ImVec2 WINDOW_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);

    const auto BUTTON_SIZE = ImVec2(150.f * SCALE.x, 46.f * SCALE.y);
    const auto is_background = gui.apps_background.contains("NPXS10015");

    auto &common = emuenv.common_dialog.lang.common;
    auto &lang = gui.lang.vita3k_update;

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::Begin("##vita3k_update", &gui.help_menu.vita3k_update, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    if (is_background)
        ImGui::GetBackgroundDrawList()->AddImage(gui.apps_background["NPXS10015"], WINDOW_POS, display_size);
    else
        ImGui::GetBackgroundDrawList()->AddRectFilled(WINDOW_POS, display_size, IM_COL32(36.f, 120.f, 12.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
    ImGui::SetCursorPosY(44.f * SCALE.y);
    TextCentered(lang["title"].c_str());
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (6.f * SCALE.y));
    ImGui::Separator();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
    switch (state) {
    case NOT_COMPLETE_UPDATE:
        ImGui::SetCursorPosY((display_size.y / 2.f) - ImGui::GetFontSize());
        TextCentered(lang["not_complete_update"].c_str());

        break;
    case NO_UPDATE:
        ImGui::SetCursorPosY(display_size.y / 2.f);
        TextCentered(app_number > git_version ? lang["later_version_already_installed"].c_str() : lang["latest_version_already_installed"].c_str());
        break;
    case HAS_UPDATE: {
        ImGui::SetCursorPosY((display_size.y / 2.f) - ImGui::GetFontSize());
        TextCentered(lang["new_version_available"].c_str());
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (40.f * SCALE.x));
        TextCentered(fmt::format(fmt::runtime(lang["version"]), git_version).c_str());
        break;
    }
    case DESCRIPTION: {
        ImGui::Spacing();
        ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
        TextCentered(fmt::format(fmt::runtime(lang["new_features"]), git_version).c_str());
        ImGui::Spacing();
        ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, 136.0f * SCALE.y), ImGuiCond_Always, ImVec2(0.5f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 4.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 15.f * SCALE.x);
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::BeginChild("##description_child", ImVec2(860 * SCALE.x, 334.f * SCALE.y), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetWindowFontScale(0.8f);
        ImGui::Columns(2, "commit_columns", true);
        ImGui::SetColumnWidth(0, 200 * SCALE.x);
        const auto space_margin = ImGui::GetStyle().ItemSpacing.x * 2.f;
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["authors"].c_str());
        ImGui::NextColumn();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["comments"].c_str());
        ImGui::Separator();
        ImGui::NextColumn();

        // Draw commit description list with lock mutex for thread safe
        std::lock_guard<std::mutex> lock(commit_desc_list_mutex);
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
        ImGui::SetCursorPosY((display_size.y / 2.f) - ImGui::GetFontSize());
        TextCentered(lang["update_vita3k"].c_str());

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
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 16.f * emuenv.manual_dpi_scale);
        TextColoredCentered(GUI_COLOR_TEXT, std::to_string(static_cast<uint32_t>(progress)).append("%").c_str());
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
            if (thread_running)
                cancel_thread = true;
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
        const auto str_size = ImGui::CalcTextSize(lang["cancel_update_resume"].c_str(), 0, false, POPUP_SIZE.x - (120.f * SCALE.y));
        ImGui::SetCursorPos(ImVec2(60.f * SCALE.x, (ImGui::GetWindowHeight() / 2.f) - (str_size.y / 2.f)));
        ImGui::PushTextWrapPos(POPUP_SIZE.x - (120.f * SCALE.x));
        ImGui::Text("%s", lang["cancel_update_resume"].c_str());
        ImGui::PopTextWrapPos();
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
