// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include "interface.h"
#include "private.h"

#include <gui/functions.h>

#include <util/string_utils.h>

#include <nfd.h>

#include <thread>

namespace gui {

static bool delete_archive_file;
static std::string state, type, title;
static std::map<fs::path, std::vector<ContentInfo>> contents_archives;
static std::vector<fs::path> invalid_archives;
static nfdchar_t *archive_path;
static float global_progress = 0.f;
static float archives_count = 0.f;

static std::vector<fs::path> get_path_of_archives(const fs::path &path) {
    std::vector<fs::path> archives_path;
    for (const auto &archive : fs::directory_iterator(path)) {
        const auto extension = string_utils::tolower(archive.path().extension().string());
        if ((extension == ".zip") || (extension == ".vpk")) {
            archives_path.push_back(archive);
        }
    }

    return archives_path;
}

void draw_archive_install_dialog(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);
    const auto BUTTON_SIZE = ImVec2(160.f * SCALE.x, 45.f * SCALE.y);
    const auto WINDOW_SIZE = ImVec2(616.f * SCALE.x, (state.empty() ? 240.f : 340.f) * SCALE.y);

    static std::atomic<float> count(0.f);
    static std::atomic<float> current(0.f);
    static std::atomic<float> progress(0.f);
    static std::mutex install_mutex;
    static const auto progress_callback = [&](ArchiveContents progress_value) {
        if (progress_value.count.has_value())
            count = progress_value.count.value();
        if (progress_value.current.has_value())
            current = progress_value.current.value();
        if (progress_value.progress.has_value())
            progress = progress_value.progress.value();
    };
    std::lock_guard<std::mutex> lock(install_mutex);

    auto indicator = gui.lang.indicator;
    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size);
    ImGui::Begin("archive_install", &gui.file_menu.archive_install_dialog, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetWindowFontScale(RES_SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
    ImGui::BeginChild("##archive_Install_child", WINDOW_SIZE, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    const auto POS_BUTTON = (ImGui::GetWindowWidth() / 2.f) - (BUTTON_SIZE.x / 2.f) + (10.f * SCALE.x);
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (ImGui::CalcTextSize(title.c_str()).x / 2.f));
    ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", title.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (type.empty()) {
        ImGui::SetCursorPosX(POS_BUTTON);
        title = "Select install type";
        if (ImGui::Button("Select File", BUTTON_SIZE))
            type = "file";
        ImGui::Spacing();
        ImGui::SetCursorPosX(POS_BUTTON);
        if (ImGui::Button("Select Directory", BUTTON_SIZE))
            type = "directory";
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX(POS_BUTTON);
        if (ImGui::Button("Cancel", BUTTON_SIZE))
            gui.file_menu.archive_install_dialog = false;
    } else {
        if (state.empty()) {
            nfdresult_t result = NFD_CANCEL;
            if (type == "file")
                result = NFD_OpenDialog("zip,vpk", nullptr, &archive_path);
            else
                result = NFD_PickFolder(nullptr, &archive_path);
            if (result == NFD_OKAY) {
                state = "install";
            } else
                type.clear();
        } else if (state == "install") {
            std::thread installation([&host, &gui]() {
                global_progress = 1.f;
                const auto install_archive_contents = [&](const fs::path &archive_path) {
                    const auto result = install_archive(host, &gui, archive_path, progress_callback);
                    std::lock_guard<std::mutex> lock(install_mutex);
                    if (!result.empty()) {
                        contents_archives[archive_path] = result;
                        std::sort(contents_archives[archive_path].begin(), contents_archives[archive_path].end(), [&](const ContentInfo &lhs, const ContentInfo &rhs) {
                            return lhs.state < rhs.state;
                        });
                    } else
                        invalid_archives.push_back(archive_path);
                };
                const auto contents_path = fs::path(string_utils::utf_to_wide(archive_path));
                if (type == "directory") {
                    const auto archives_path = get_path_of_archives(contents_path);
                    archives_count = float(archives_path.size());
                    for (const auto &archive : archives_path) {
                        install_archive_contents(archive);
                        ++global_progress;
                        progress = 0.f;
                    }
                } else {
                    archives_count = global_progress = 1.f;
                    install_archive_contents(contents_path);
                    progress = 0.f;
                }
                archives_count = 0.f;
                global_progress = 0.f;
                state = "finished";
            });
            installation.detach();
            state = "installing";
        } else if (state == "installing") {
            title = "Installing";
            ImGui::SetCursorPos(ImVec2(178.f * host.dpi_scale, ImGui::GetCursorPosY() + (20.f * host.dpi_scale)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", host.app_title.c_str());
            ImGui::SetCursorPos(ImVec2(178.f * host.dpi_scale, ImGui::GetCursorPosY() + (20.f * host.dpi_scale)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", indicator["installing"].c_str());
            const float PROGRESS_BAR_WIDTH = 502.f * host.dpi_scale;
            const auto PROGRESS_BAR_POS = (ImGui::GetWindowWidth() / 2) - (PROGRESS_BAR_WIDTH / 2.f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            // Draw Global Progress bar
            ImGui::SetCursorPos(ImVec2(PROGRESS_BAR_POS, ImGui::GetCursorPosY() + (14.f * host.dpi_scale)));
            ImGui::ProgressBar(global_progress / archives_count, ImVec2(PROGRESS_BAR_WIDTH, 15.f * host.dpi_scale), "");
            const auto global_progress_str = fmt::format("{}/{}", global_progress, archives_count);
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(global_progress_str.c_str()).x / 2.f), ImGui::GetCursorPosY() + (6.f * host.dpi_scale)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", global_progress_str.c_str());
            // Draw Progress bar of count contents
            ImGui::SetCursorPos(ImVec2(PROGRESS_BAR_POS, ImGui::GetCursorPosY() + (14.f * host.dpi_scale)));
            ImGui::ProgressBar(current / count, ImVec2(PROGRESS_BAR_WIDTH, 15.f * host.dpi_scale), "");
            const auto count_progress_str = fmt::format("{}/{}", current, count);
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(count_progress_str.c_str()).x / 2.f), ImGui::GetCursorPosY() + (6.f * host.dpi_scale)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", count_progress_str.c_str());
            // Draw Progress bar
            ImGui::SetCursorPos(ImVec2(PROGRESS_BAR_POS, ImGui::GetCursorPosY() + (14.f * host.dpi_scale)));
            ImGui::ProgressBar(progress / 100.f, ImVec2(PROGRESS_BAR_WIDTH, 15.f * host.dpi_scale), "");
            const auto progress_str = std::to_string(uint32_t(progress)).append("%");
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(progress_str.c_str()).x / 2.f), ImGui::GetCursorPosY() + (6.f * host.dpi_scale)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", progress_str.c_str());
            ImGui::PopStyleColor();
        } else if (state == "finished") {
            title = indicator["install_complete"];
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowPos().x + (5.f * SCALE.x), ImGui::GetWindowPos().y + BUTTON_SIZE.y));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
            ImGui::BeginChild("##content_installed_list", ImVec2(WINDOW_SIZE.x - (10.f * SCALE.x), WINDOW_SIZE.y - (BUTTON_SIZE.y * 2.f) - (25 * SCALE.y)), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
            if (!contents_archives.empty()) {
                const auto count_content_state = [&](const fs::path path, const bool state) {
                    return std::count_if(contents_archives[path].begin(), contents_archives[path].end(), [&](const ContentInfo &c) {
                        return c.state == state;
                    });
                };
                ImGui::Spacing();
                ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%lu archive(s) found with compatible contents.", contents_archives.size());
                ImGui::Spacing();
                ImGui::Separator();
                for (const auto &archive : contents_archives) {
                    ImGui::TextWrapped("%s", string_utils::wide_to_utf(archive.first.filename().wstring()).c_str());
                    ImGui::Spacing();
                    const auto count_contents_successed = count_content_state(archive.first, true);
                    if (count_contents_successed) {
                        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%ld contents of this archive have successed installed:", count_contents_successed);
                        for (const auto &content : archive.second) {
                            if (content.state) {
                                ImGui::TextWrapped("%s [%s]", content.title.c_str(), content.title_id.c_str());
                                if (content.category.find("gp") != std::string::npos)
                                    ImGui::TextColored(GUI_COLOR_TEXT, "Update App to: %s", host.app_version.c_str());
                            }
                        }
                    }
                    const auto count_contents_failed = count_content_state(archive.first, false);
                    if (count_contents_failed) {
                        ImGui::Spacing();
                        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%ld content have failed installed:", count_contents_failed);
                        ImGui::Spacing();
                        for (const auto &content : archive.second) {
                            if (!content.state)
                                ImGui::TextWrapped("%s [%s] in path: %s", content.title.c_str(), content.title_id.c_str(), content.path.c_str());
                        }
                    }
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }
            }
            if (!invalid_archives.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%lu archive(s) did no found compatible contents:", invalid_archives.size());
                ImGui::Spacing();
                for (const auto &archive : invalid_archives)
                    ImGui::TextWrapped("%s", archive.filename().string().c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Checkbox("Delete archive?", &delete_archive_file);
            ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (12.f * SCALE.y)));
            if (ImGui::Button("OK", BUTTON_SIZE)) {
                for (const auto &archive : contents_archives) {
                    for (const auto &content : archive.second) {
                        if (content.state) {
                            host.app_category = content.category;
                            host.app_title_id = content.title_id;
                            host.app_content_id = content.content_id;
                            if (host.app_category != "theme")
                                update_notice_info(gui, host, "content");
                            if (content.category == "gd") {
                                init_user_app(gui, host, content.title_id);
                                save_apps_cache(gui, host);
                            }
                        }
                    }
                    if (delete_archive_file)
                        fs::remove(archive.first);
                }
                if (delete_archive_file && !invalid_archives.empty()) {
                    for (const auto archive : invalid_archives)
                        fs::remove(archive);
                }
                archive_path = nullptr;
                gui.file_menu.archive_install_dialog = false;
                delete_archive_file = false;
                contents_archives.clear();
                invalid_archives.clear();
                type.clear();
                state.clear();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
}

} // namespace gui
