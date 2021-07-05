// Vita3K emulator project
// Copyright (C) 2019 Vita3K team
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

struct ContentInfo {
    std::string title;
    std::string title_id;
    std::string category;
    std::string content_id;
    fs::path path;
};

static bool delete_archive_file;
static std::string state, type, title;
static std::vector<ContentInfo> content_installed;
static std::vector<fs::path> content_failed;
static nfdchar_t *archive_path;
static float global_progress = 0;
static float content_count = 0;

static float number_of_content(const fs::path &path) {
    float count = 0;
    for (const auto &content : fs::directory_iterator(path)) {
        if ((content.path().extension() == ".zip") || (content.path().extension() == ".vpk"))
            ++count;
    }
    return count;
}

void draw_archive_install_dialog(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);
    const auto BUTTON_SIZE = ImVec2(160.f * SCALE.x, 45.f * SCALE.y);
    const auto WINDOW_SIZE = ImVec2(616.f * SCALE.x, 284.f * SCALE.y);
    static std::atomic<float> progress(0);
    static std::mutex install_mutex;
    static const auto progress_callback = [&](float updated_progress) {
        progress = updated_progress;
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
                global_progress = 1;
                const auto content_path = fs::path(string_utils::utf_to_wide(archive_path));
                if (type == "directory") {
                    content_count = number_of_content(content_path);
                    for (const auto &content : fs::directory_iterator(content_path)) {
                        if ((content.path().extension() == std::string(".zip")) || (content.path().extension() == std::string(".vpk"))) {
                            if (install_archive(host, &gui, content, progress_callback)) {
                                std::lock_guard<std::mutex> lock(install_mutex);
                                content_installed.push_back({ host.app_title, host.app_title_id, host.app_category, host.app_content_id, content });
                            } else
                                content_failed.push_back(content);
                            ++global_progress;
                            progress = 0;
                        }
                    }
                } else {
                    content_count = 1.f;
                    if (install_archive(host, &gui, content_path, progress_callback)) {
                        std::lock_guard<std::mutex> lock(install_mutex);
                        content_installed.push_back({ host.app_title, host.app_title_id, host.app_category, host.app_content_id, content_path });
                    } else
                        content_failed.push_back(content_path);
                    progress = 0;
                }
                content_count = 0;
                global_progress = 0;
                state = "finished";
            });
            installation.detach();
            state = "installing";
        } else if (state == "installing") {
            title = "Installing";
            ImGui::SetCursorPos(ImVec2(178.f * host.dpi_scale, ImGui::GetCursorPosY() + (20.f * host.dpi_scale)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", host.app_title.c_str());
            const auto installing = !indicator["installing"].empty() ? indicator["installing"].c_str() : "Installing...";
            ImGui::SetCursorPos(ImVec2(178.f * host.dpi_scale, ImGui::GetCursorPosY() + (20.f * host.dpi_scale)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", installing);
            const float PROGRESS_BAR_WIDTH = 502.f * host.dpi_scale;
            const auto PROGRESS_BAR_POS = (ImGui::GetWindowWidth() / 2) - (PROGRESS_BAR_WIDTH / 2.f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            ImGui::SetCursorPos(ImVec2(PROGRESS_BAR_POS, ImGui::GetCursorPosY() + (14.f * host.dpi_scale)));
            ImGui::ProgressBar(global_progress / content_count, ImVec2(PROGRESS_BAR_WIDTH, 15.f * host.dpi_scale), "");
            const auto global_progress_str = fmt::format("{}/{}", global_progress, content_count);
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(global_progress_str.c_str()).x / 2.f), ImGui::GetCursorPosY() + (6.f * host.dpi_scale)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", global_progress_str.c_str());
            ImGui::SetCursorPos(ImVec2(PROGRESS_BAR_POS, ImGui::GetCursorPosY() + (14.f * host.dpi_scale)));
            ImGui::ProgressBar(progress / 100.f, ImVec2(PROGRESS_BAR_WIDTH, 15.f * host.dpi_scale), "");
            const auto progress_str = std::to_string(uint32_t(progress)).append("%");
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(progress_str.c_str()).x / 2.f), ImGui::GetCursorPosY() + (6.f * host.dpi_scale)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", progress_str.c_str());
            ImGui::PopStyleColor();
        } else if (state == "finished") {
            title = !indicator["install_complete"].empty() ? indicator["install_complete"] : "Installation complete.";
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowPos().x + (5.f * SCALE.x), ImGui::GetWindowPos().y + BUTTON_SIZE.y));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
            ImGui::BeginChild("##content_installed_list", ImVec2(WINDOW_SIZE.x - (10.f * SCALE.x), WINDOW_SIZE.y - (BUTTON_SIZE.y * 2.f) - (25 * SCALE.y)), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
            if (!content_installed.empty()) {
                ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%lu content have success installed:", content_installed.size());
                for (const auto &content : content_installed)
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s [%s]", content.title_id.c_str(), content.title.c_str());
            }
            if (!content_failed.empty()) {
                ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%lu content have failed install:", content_failed.size());
                for (const auto &content : content_failed)
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", content.string().c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Checkbox("Delete archive?", &delete_archive_file);
            ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (12.f * SCALE.y)));
            if (ImGui::Button("OK", BUTTON_SIZE)) {
                for (const auto &content : content_installed) {
                    host.app_category = content.category;
                    host.app_title_id = content.title_id;
                    host.app_content_id = content.content_id;
                    update_notice_info(gui, host, "content");
                    if (content.category == "gd")
                        init_user_app(gui, host, content.title_id);
                    if (delete_archive_file)
                        fs::remove(content.path);
                }
                save_apps_cache(gui, host);
                archive_path = nullptr;
                gui.file_menu.archive_install_dialog = false;
                delete_archive_file = false;
                content_installed.clear();
                content_failed.clear();
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
