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

#include "private.h"

#include <dialog/state.h>
#include <gui/functions.h>
#include <host/dialog/filesystem.h>
#include <misc/cpp/imgui_stdlib.h>
#include <packages/pkg.h>
#include <packages/sfo.h>
#include <rif2zrif.h>
#include <util/log.h>

#include <thread>

namespace gui {

static std::filesystem::path pkg_path = "";
static std::filesystem::path license_path = "";
static std::string state, title, zRIF;
static bool draw_file_dialog = true;
static bool delete_pkg_file, delete_license_file;

void draw_pkg_install_dialog(GuiState &gui, EmuEnvState &emuenv) {
    host::dialog::filesystem::Result result = host::dialog::filesystem::Result::CANCEL;
    static std::atomic<float> progress(0);
    static std::mutex install_mutex;
    static const auto progress_callback = [&](float updated_progress) {
        progress = updated_progress;
    };
    std::lock_guard<std::mutex> lock(install_mutex);

    if (draw_file_dialog) {
        result = host::dialog::filesystem::open_file(pkg_path, { { "PlayStation Store Downloaded Package", { "pkg" } } });
        draw_file_dialog = false;
        if (result == host::dialog::filesystem::Result::SUCCESS) {
            fs::ifstream infile(pkg_path.native(), std::ios::binary);
            PkgHeader pkg_header{};
            infile.read(reinterpret_cast<char *>(&pkg_header), sizeof(PkgHeader));
            std::string title_id_str(pkg_header.content_id);
            std::string title_id = title_id_str.substr(7, 9);
            const auto work_path{ emuenv.pref_path / fmt::format("ux0/license/{}/{}.rif", title_id, pkg_header.content_id) };
            if (fs::exists(work_path)) {
                LOG_INFO("Found license file: {}", work_path);
                fs::ifstream binfile(work_path, std::ios::in | std::ios::binary | std::ios::ate);
                zRIF = rif2zrif(binfile);
                ImGui::OpenPopup("install");
                gui::state = "install";
            } else {
                ImGui::OpenPopup("install");
            }
        } else if (result == host::dialog::filesystem::Result::CANCEL) {
            gui.file_menu.pkg_install_dialog = false;
            draw_file_dialog = true;
        } else {
            LOG_ERROR("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
            gui.file_menu.pkg_install_dialog = false;
            draw_file_dialog = true;
        }
    }

    auto lang = gui.lang.install_dialog.pkg_install;
    auto indicator = gui.lang.indicator;
    auto common = emuenv.common_dialog.lang.common;

    const ImVec2 display_size(emuenv.viewport_size.x, emuenv.viewport_size.y);
    const ImVec2 RES_SCALE(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);
    const ImVec2 WINDOW_SIZE(616.f * SCALE.x, 264.f * SCALE.y);
    const ImVec2 BUTTON_SIZE(180.f * SCALE.x, 45.f * SCALE.y);

    ImGui::SetNextWindowPos(ImVec2(emuenv.viewport_pos.x + (display_size.x / 2.f) - (WINDOW_SIZE.x / 2), emuenv.viewport_pos.y + (display_size.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE);
    if (ImGui::BeginPopupModal("install", &gui.file_menu.pkg_install_dialog, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration)) {
        ImGui::SetWindowFontScale(RES_SCALE.x);
        const auto POS_BUTTON = (WINDOW_SIZE.x / 2.f) - (BUTTON_SIZE.x / 2.f) + (10.f * SCALE.x);
        ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(title.c_str()).x / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (state.empty()) {
            ImGui::SetCursorPosX(POS_BUTTON);
            title = lang["select_license_type"];
            if (ImGui::Button(lang["select_bin_rif"].c_str(), BUTTON_SIZE))
                state = "license";
            ImGui::SetCursorPosX(POS_BUTTON);
            if (ImGui::Button(lang["enter_zrif"].c_str(), BUTTON_SIZE))
                state = "zrif";
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::SetCursorPosX(POS_BUTTON);
            if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE)) {
                gui.file_menu.pkg_install_dialog = false;
                draw_file_dialog = true;
            }
        } else if (state == "license") {
            result = host::dialog::filesystem::open_file(license_path, { { "PlayStation Vita software license file", { "bin", "rif" } } });
            if (result == host::dialog::filesystem::Result::SUCCESS) {
                fs::ifstream binfile(license_path.native(), std::ios::in | std::ios::binary | std::ios::ate);
                zRIF = rif2zrif(binfile);
                state = "install";
            } else {
                if (result == host::dialog::filesystem::Result::ERROR)
                    LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
                state.clear();
            }
        } else if (state == "zrif") {
            title = lang["enter_zrif_key"];
            ImGui::PushItemWidth(640.f * SCALE.x);
            ImGui::InputTextWithHint("##enter_zrif", lang["input_zrif"].c_str(), &zRIF);
            ImGui::PopItemWidth();
            SetTooltipEx(lang["copy_paste_zrif"].c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::SetCursorPos(ImVec2(POS_BUTTON - (BUTTON_SIZE.x / 2) - (10.f * SCALE.x), WINDOW_SIZE.y / 2));
            if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE)) {
                state.clear();
                zRIF.clear();
            }
            ImGui::SameLine(0, 20.f * SCALE.x);
            if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) && !zRIF.empty())
                state = "install";
        } else if (state == "install") {
            std::thread installation([&emuenv]() {
                if (install_pkg(pkg_path.native(), emuenv, zRIF, progress_callback)) {
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
            title = indicator["install_complete"];
            ImGui::TextColored(GUI_COLOR_TEXT, "%s [%s]", emuenv.app_info.app_title.c_str(), emuenv.app_info.app_title_id.c_str());
            if (emuenv.app_info.app_category.find("gp") != std::string::npos)
                ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", lang["update_app"].c_str(), emuenv.app_info.app_version.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Checkbox(lang["delete_pkg"].c_str(), &delete_pkg_file);
            if (license_path != "")
                ImGui::Checkbox(lang["delete_bin_rif"].c_str(), &delete_license_file);
            ImGui::Spacing();
            ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
            if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
                if (delete_pkg_file) {
                    fs::remove(fs::path(pkg_path.native()));
                    delete_pkg_file = false;
                }
                if (delete_license_file) {
                    fs::remove(fs::path(license_path.native()));
                    delete_license_file = false;
                }
                if ((emuenv.app_info.app_category.find("gd") != std::string::npos) || (emuenv.app_info.app_category.find("gp") != std::string::npos)) {
                    init_user_app(gui, emuenv, emuenv.app_info.app_title_id);
                    save_apps_cache(gui, emuenv);
                    select_app(gui, emuenv.app_info.app_title_id);
                }
                update_notice_info(gui, emuenv, "content");
                pkg_path = "";
                license_path = "";
                gui.file_menu.pkg_install_dialog = false;
                draw_file_dialog = true;
                state.clear();
            }
        } else if (state == "fail") {
            title = indicator["install_failed"];
            auto FAILED_INSTALL_STR = lang["failed_install_package"].c_str();
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowSize().x / 2.f) - (ImGui ::CalcTextSize(FAILED_INSTALL_STR).x / 2.f), (WINDOW_SIZE.y / 2.f) - (20.f * SCALE.y)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", FAILED_INSTALL_STR);
            ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
            if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
                gui.file_menu.pkg_install_dialog = false;
                pkg_path = "";
                draw_file_dialog = true;
                license_path = "";
                state.clear();
            }
        } else if (state == "installing") {
            title = indicator["installing"];
            ImGui::SetCursorPos(ImVec2(178.f * SCALE.x, ImGui::GetCursorPosY() + 30.f * SCALE.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", emuenv.app_info.app_title.c_str());
            ImGui::SetCursorPos(ImVec2(178.f * SCALE.x, ImGui::GetCursorPosY() + 30.f * SCALE.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", indicator["installing"].c_str());
            const float PROGRESS_BAR_WIDTH = 502.f * SCALE.x;
            ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (PROGRESS_BAR_WIDTH / 2.f), ImGui::GetCursorPosY() + 30.f * SCALE.y));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            ImGui::ProgressBar(progress / 100.f, ImVec2(PROGRESS_BAR_WIDTH, 15.f * SCALE.x), "");
            const auto progress_str = std::to_string(uint32_t(progress)).append("%");
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(progress_str.c_str()).x / 2.f), ImGui::GetCursorPosY() + 16.f * SCALE.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", progress_str.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndPopup();
    }
}
} // namespace gui
