// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <gui/functions.h>
#include <host/functions.h>
#include <host/pkg.h>
#include <misc/cpp/imgui_stdlib.h>
#include <rif2zrif.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <nfd.h>

#include <thread>

namespace gui {

static nfdchar_t *pkg_path, *work_path;
static std::string state, title, zRIF;
static bool draw_file_dialog = true;
static bool delete_pkg_file, delete_work_file;

void draw_pkg_install_dialog(GuiState &gui, HostState &host) {
    nfdresult_t result = NFD_CANCEL;
    static std::atomic<float> progress(0);
    static std::mutex install_mutex;
    static const auto progress_callback = [&](float updated_progress) {
        progress = updated_progress;
    };
    std::lock_guard<std::mutex> lock(install_mutex);

    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);

    const auto BUTTON_SIZE = ImVec2(160.f * SCALE.x, 45.f * SCALE.y);

    if (draw_file_dialog) {
        result = NFD_OpenDialog("pkg", nullptr, &pkg_path);
        draw_file_dialog = false;
        if (result == NFD_OKAY)
            ImGui::OpenPopup("install");
        else if (result == NFD_CANCEL) {
            gui.file_menu.pkg_install_dialog = false;
            draw_file_dialog = true;
        } else {
            LOG_ERROR("Error initializing file dialog: {}", NFD_GetError());
            gui.file_menu.pkg_install_dialog = false;
            draw_file_dialog = true;
        }
    }

    auto indicator = gui.lang.indicator;
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(616.f * SCALE.x, 264.f * SCALE.y));
    if (ImGui::BeginPopupModal("install", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration)) {
        ImGui::SetWindowFontScale(RES_SCALE.x);
        const auto POS_BUTTON = (ImGui::GetWindowWidth() / 2.f) - (BUTTON_SIZE.x / 2.f) + (10.f * SCALE.x);
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (ImGui::CalcTextSize(title.c_str()).x / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (state.empty()) {
            ImGui::SetCursorPosX(POS_BUTTON);
            title = "Select key type";
            if (ImGui::Button("Select Work.bin", BUTTON_SIZE))
                state = "work";
            ImGui::SetCursorPosX(POS_BUTTON);
            if (ImGui::Button("Enter zRIF", BUTTON_SIZE))
                state = "zrif";
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::SetCursorPosX(POS_BUTTON);
            if (ImGui::Button("Cancel", BUTTON_SIZE)) {
                gui.file_menu.pkg_install_dialog = false;
                draw_file_dialog = true;
            }
        } else if (state == "work") {
            result = NFD_OpenDialog("bin", nullptr, &work_path);
            if (result == NFD_OKAY) {
                fs::ifstream binfile(string_utils::utf_to_wide(std::string(work_path)), std::ios::in | std::ios::binary | std::ios::ate);
                zRIF = rif2zrif(binfile);
                state = "install";
            } else
                state.clear();
        } else if (state == "zrif") {
            title = "Enter zRIF key";
            ImGui::PushItemWidth(640.f * SCALE.x);
            ImGui::InputTextWithHint("##enter_zrif", "Please input your zRIF here", &zRIF);
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Ctrl(Cmd) + C for copy, Ctrl(Cmd) + V to paste.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::SetCursorPos(ImVec2(POS_BUTTON - (BUTTON_SIZE.x / 2) - 10.f * host.dpi_scale, ImGui::GetWindowSize().y / 2));
            if (ImGui::Button("Cancel", BUTTON_SIZE)) {
                state.clear();
                zRIF.clear();
            }
            ImGui::SameLine(0, 20.f * SCALE.x);
            if (ImGui::Button("OK", BUTTON_SIZE) && !zRIF.empty())
                state = "install";
        } else if (state == "install") {
            std::thread installation([&host]() {
                if (install_pkg(std::string(pkg_path), host, zRIF, progress_callback)) {
                    std::lock_guard<std::mutex> lock(install_mutex);
                    state = "success";
                } else {
                    state = "fail";
                }
                zRIF.clear();
            });
            installation.detach();
            state = "installing";
        } else if (state == "success") {
            title = !indicator["install_complete"].empty() ? indicator["install_complete"] : "Installation complete.";
            ImGui::TextColored(GUI_COLOR_TEXT, "%s [%s]", host.app_title_id.c_str(), host.app_title.c_str());
            if (host.app_category == std::string("gd"))
                ImGui::TextColored(GUI_COLOR_TEXT, "App version: %s", host.app_version.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Checkbox("Delete the pkg file?", &delete_pkg_file);
            if (work_path)
                ImGui::Checkbox("Delete the work.bin file?", &delete_work_file);
            ImGui::Spacing();
            ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
            if (ImGui::Button("OK", BUTTON_SIZE)) {
                if (delete_pkg_file) {
                    fs::remove(fs::path(string_utils::utf_to_wide(std::string(pkg_path))));
                    delete_pkg_file = false;
                }
                if (delete_work_file) {
                    fs::remove(fs::path(string_utils::utf_to_wide(std::string(work_path))));
                    delete_work_file = false;
                }
                if (host.app_category == "gd") {
                    init_user_app(gui, host, host.app_title_id);
                    save_apps_cache(gui, host);
                }
                update_notice_info(gui, host, "content");
                pkg_path = nullptr;
                work_path = nullptr;
                gui.file_menu.pkg_install_dialog = false;
                draw_file_dialog = true;
                state.clear();
            }
        } else if (state == "fail") {
            title = !indicator["install_failed"].empty() ? gui.lang.indicator["install_failed"].c_str() : "Could not install.";
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowSize().x / 2.f) - (ImGui ::CalcTextSize("Please check log for more details.").x / 2.f), ImGui::GetWindowSize().y / 2.f - 20.f * host.dpi_scale));
            ImGui::TextColored(GUI_COLOR_TEXT, "Please check log for more details.");
            ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
            if (ImGui::Button("OK", BUTTON_SIZE)) {
                refresh_app_list(gui, host);
                gui.file_menu.pkg_install_dialog = false;
                pkg_path = nullptr;
                draw_file_dialog = true;
                work_path = nullptr;
                state.clear();
            }
        } else if (state == "installing") {
            title = "Installing";
            ImGui::SetCursorPos(ImVec2(178.f * host.dpi_scale, ImGui::GetCursorPosY() + 30.f * host.dpi_scale));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", host.app_title.c_str());
            const auto installing = !indicator["installing"].empty() ? indicator["installing"].c_str() : "Installing...";
            ImGui::SetCursorPos(ImVec2(178.f * host.dpi_scale, ImGui::GetCursorPosY() + 30.f * host.dpi_scale));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", installing);
            const float PROGRESS_BAR_WIDTH = 502.f * host.dpi_scale;
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2) - (PROGRESS_BAR_WIDTH / 2.f), ImGui::GetCursorPosY() + 30.f * host.dpi_scale));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            ImGui::ProgressBar(progress / 100.f, ImVec2(PROGRESS_BAR_WIDTH, 15.f * host.dpi_scale), "");
            const auto progress_str = std::to_string(uint32_t(progress)).append("%");
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(progress_str.c_str()).x / 2.f), ImGui::GetCursorPosY() + 16.f * host.dpi_scale));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", progress_str.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndPopup();
    }
}
} // namespace gui
