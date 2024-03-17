// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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
#include <host/dialog/filesystem.hpp>
#include <packages/sfo.h>

#include <util/string_utils.h>

#include <fmt/core.h>

#include <thread>

namespace gui {

static bool delete_archive_file;
static std::string state, type, title;
static std::map<fs::path, std::vector<ContentInfo>> contents_archives;
static std::vector<fs::path> invalid_archives;
static std::filesystem::path archive_path = "";
static float global_progress = 0.f;
static int archives_count = 0;

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

void draw_archive_install_dialog(GuiState &gui, EmuEnvState &emuenv) {
    static std::atomic<float> count(0.f);
    static std::atomic<float> current(0.f);
    static std::atomic<float> progress(0.f);
    static std::mutex install_mutex;
    static const auto progress_callback = [&](const ArchiveContents &progress_value) {
        if (progress_value.count.has_value())
            count = progress_value.count.value();
        if (progress_value.current.has_value())
            current = progress_value.current.value();
        if (progress_value.progress.has_value())
            progress = progress_value.progress.value();
    };
    std::lock_guard<std::mutex> lock(install_mutex);

    auto lang = gui.lang.install_dialog.archive_install;
    auto indicator = gui.lang.indicator;
    auto common = emuenv.common_dialog.lang.common;

    const ImVec2 display_size(emuenv.viewport_size.x, emuenv.viewport_size.y);
    const auto RES_SCALE = ImVec2(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);
    const auto BUTTON_SIZE = ImVec2(190.f * SCALE.x, 45.f * SCALE.y);
    const auto WINDOW_SIZE = ImVec2(616.f * SCALE.x, (state.empty() ? 240.f : 340.f) * SCALE.y);

    ImGui::SetNextWindowPos(ImVec2(emuenv.viewport_pos.x, emuenv.viewport_pos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size);
    ImGui::Begin("archive_install", &gui.file_menu.archive_install_dialog, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowPos(ImVec2(emuenv.viewport_pos.x + (display_size.x / 2.f) - (WINDOW_SIZE.x / 2.f), emuenv.viewport_pos.y + (display_size.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
    ImGui::BeginChild("##archive_Install_child", WINDOW_SIZE, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    const auto POS_BUTTON = (ImGui::GetWindowWidth() / 2.f) - (BUTTON_SIZE.x / 2.f) + (10.f * SCALE.x);
    ImGui::SetWindowFontScale(RES_SCALE.x);
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(title.c_str()).x / 2.f));
    ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", title.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (type.empty()) {
        ImGui::SetCursorPosX(POS_BUTTON);
        title = lang["select_install_type"];
        if (ImGui::Button(lang["select_file"].c_str(), BUTTON_SIZE))
            type = "file";
        ImGui::Spacing();
        ImGui::SetCursorPosX(POS_BUTTON);
        if (ImGui::Button(lang["select_directory"].c_str(), BUTTON_SIZE))
            type = "directory";
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX(POS_BUTTON);
        if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE))
            gui.file_menu.archive_install_dialog = false;
    } else {
        if (state.empty()) {
            host::dialog::filesystem::Result result = host::dialog::filesystem::Result::CANCEL;
            if (type == "file") {
                // Set file filters for the file picking dialog
                std::vector<host::dialog::filesystem::FileFilter> file_filters = {
                    { "PlayStation Vita commercial software package (NoNpDrm/FAGDec) / PlayStation Vita homebrew software package", { "zip", "vpk" } },
                    { "PlayStation Vita commercial software package (NoNpDrm/FAGDec)", { "zip" } },
                    { "PlayStation Vita homebrew software package", { "vpk" } },
                };
                // Call file picking dialog from the native file browser
                result = host::dialog::filesystem::open_file(archive_path, file_filters);
            } else {
                result = host::dialog::filesystem::pick_folder(archive_path);
            }
            if (result == host::dialog::filesystem::Result::SUCCESS) {
                state = "install";
            } else {
                if (result == host::dialog::filesystem::Result::ERROR)
                    LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
                type.clear();
            }
        } else if (state == "install") {
            std::thread installation([&emuenv, &gui]() {
                global_progress = 1.f;
                const auto install_archive_contents = [&](const fs::path &archive_path) {
                    const auto result = install_archive(emuenv, &gui, archive_path, progress_callback);
                    std::lock_guard<std::mutex> lock(install_mutex);
                    if (!result.empty()) {
                        contents_archives[archive_path] = result;
                        std::sort(contents_archives[archive_path].begin(), contents_archives[archive_path].end(), [&](const ContentInfo &lhs, const ContentInfo &rhs) {
                            return lhs.state < rhs.state;
                        });
                    } else
                        invalid_archives.push_back(archive_path);
                };
                const auto contents_path = fs::path(archive_path.native());
                if (type == "directory") {
                    const auto archives_path = get_path_of_archives(contents_path);
                    archives_count = archives_path.size();
                    for (const auto &archive : archives_path) {
                        install_archive_contents(archive);
                        ++global_progress;
                        progress = 0.f;
                    }
                } else {
                    archives_count = 1;
                    global_progress = 1.f;
                    install_archive_contents(contents_path);
                    progress = 0.f;
                }
                archives_count = 0;
                global_progress = 0.f;
                state = "finished";
            });
            installation.detach();
            state = "installing";
        } else if (state == "installing") {
            title = indicator["installing"];
            ImGui::SetCursorPos(ImVec2(178.f * SCALE.x, ImGui::GetCursorPosY() + (20.f * SCALE.y)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", emuenv.app_info.app_title.c_str());
            ImGui::SetCursorPos(ImVec2(178.f * SCALE.x, ImGui::GetCursorPosY() + (20.f * SCALE.y)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", indicator["installing"].c_str());
            const float PROGRESS_BAR_WIDTH = 502.f * SCALE.x;
            const auto PROGRESS_BAR_POS = (WINDOW_SIZE.x / 2) - (PROGRESS_BAR_WIDTH / 2.f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            // Draw Global Progress bar
            ImGui::SetCursorPos(ImVec2(PROGRESS_BAR_POS, ImGui::GetCursorPosY() + (14.f * SCALE.y)));
            ImGui::ProgressBar(global_progress / archives_count, ImVec2(PROGRESS_BAR_WIDTH, 15.f * SCALE.y), "");
            const auto global_progress_str = fmt::format("{}/{}", global_progress, archives_count);
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(global_progress_str.c_str()).x / 2.f), ImGui::GetCursorPosY() + (6.f * SCALE.y)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", global_progress_str.c_str());
            // Draw Progress bar of count contents
            ImGui::SetCursorPos(ImVec2(PROGRESS_BAR_POS, ImGui::GetCursorPosY() + (14.f * SCALE.y)));
            ImGui::ProgressBar(current / count, ImVec2(PROGRESS_BAR_WIDTH, 15.f * SCALE.y), "");
            const auto count_progress_str = fmt::format("{}/{}", current.load(), count.load());
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(count_progress_str.c_str()).x / 2.f), ImGui::GetCursorPosY() + (6.f * SCALE.y)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", count_progress_str.c_str());
            // Draw Progress bar
            ImGui::SetCursorPos(ImVec2(PROGRESS_BAR_POS, ImGui::GetCursorPosY() + (14.f * SCALE.y)));
            ImGui::ProgressBar(progress / 100.f, ImVec2(PROGRESS_BAR_WIDTH, 15.f * SCALE.y), "");
            const auto progress_str = std::to_string(progress).append("%");
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(progress_str.c_str()).x / 2.f), ImGui::GetCursorPosY() + (6.f * SCALE.y)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", progress_str.c_str());
            ImGui::PopStyleColor();
        } else if (state == "finished") {
            title = indicator["install_complete"];
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowPos().x + (5.f * SCALE.x), ImGui::GetWindowPos().y + BUTTON_SIZE.y));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
            ImGui::BeginChild("##content_installed_list", ImVec2(WINDOW_SIZE.x - (10.f * SCALE.x), WINDOW_SIZE.y - (BUTTON_SIZE.y * 2.f) - (25 * SCALE.y)), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
            if (!contents_archives.empty()) {
                const auto count_content_state = [&](const fs::path &path, const bool state) {
                    return std::count_if(contents_archives[path].begin(), contents_archives[path].end(), [&](const ContentInfo &c) {
                        return c.state == state;
                    });
                };
                ImGui::Spacing();
                const auto compatible_content_str = fmt::format(fmt::runtime(lang["compatible_content"].c_str()), contents_archives.size());
                ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", compatible_content_str.c_str());
                ImGui::Spacing();
                ImGui::Separator();
                for (const auto &archive : contents_archives) {
                    ImGui::TextWrapped("%s", fs_utils::path_to_utf8(archive.first.filename()).c_str());
                    ImGui::Spacing();
                    const auto count_contents_successed = count_content_state(archive.first, true);
                    if (count_contents_successed) {
                        const auto successed_install_archive_str = fmt::format(fmt::runtime(lang["successed_install_archive"].c_str()), count_contents_successed);
                        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", successed_install_archive_str.c_str());
                        for (const auto &content : archive.second) {
                            if (content.state) {
                                ImGui::TextWrapped("%s [%s]", content.title.c_str(), content.title_id.c_str());
                                if (content.category.find("gp") != std::string::npos)
                                    ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", lang["update_app"].c_str(), emuenv.app_info.app_version.c_str());
                            }
                        }
                    }
                    const auto count_contents_failed = count_content_state(archive.first, false);
                    if (count_contents_failed) {
                        ImGui::Spacing();
                        const auto failed_install_archive_str = fmt::format(fmt::runtime(lang["failed_install_archive"].c_str()), count_contents_failed);
                        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", failed_install_archive_str.c_str());
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
                const auto not_compatible_content_str = fmt::format(fmt::runtime(lang["not_compatible_content"].c_str()), invalid_archives.size());
                ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", not_compatible_content_str.c_str());
                ImGui::Spacing();
                for (const auto &archive : invalid_archives)
                    ImGui::TextWrapped("%s", fs_utils::path_to_utf8(archive.filename()).c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Checkbox(lang["delete_archive"].c_str(), &delete_archive_file);
            ImGui::SetCursorPos(ImVec2(POS_BUTTON, WINDOW_SIZE.y - BUTTON_SIZE.y - (12.f * SCALE.y)));
            if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
                for (const auto &archive : contents_archives) {
                    for (const auto &content : archive.second) {
                        if (content.state) {
                            emuenv.app_info.app_category = content.category;
                            emuenv.app_info.app_title_id = content.title_id;
                            emuenv.app_info.app_content_id = content.content_id;
                            if (emuenv.app_info.app_category != "theme")
                                update_notice_info(gui, emuenv, "content");
                            if (content.category == "gd") {
                                init_user_app(gui, emuenv, content.title_id);
                                save_apps_cache(gui, emuenv);
                            }
                        }
                    }
                    if (delete_archive_file)
                        fs::remove(archive.first);
                }
                if (delete_archive_file && !invalid_archives.empty()) {
                    for (const auto &archive : invalid_archives)
                        fs::remove(archive);
                }
                archive_path = "";
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
