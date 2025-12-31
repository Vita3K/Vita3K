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

static bool get_zrif(EmuEnvState &emuenv, const std::string &content_id, std::string &zrif) {
    const std::string title_id = content_id.substr(7, 9);
    const auto work_path{ emuenv.pref_path / fmt::format("ux0/license/{}/{}.rif", title_id, content_id) };
    if (fs::exists(work_path)) {
        LOG_INFO("Found license file: {}", work_path);
        fs::ifstream binfile(work_path, std::ios::in | std::ios::binary | std::ios::ate);
        zrif = rif2zrif(binfile);
    }

    return !zrif.empty();
}

void update_install(GuiState &gui, EmuEnvState &emuenv, const std::string &id) {
    if (!gui.updates_install.contains(id))
        return;

    auto &update_install = gui.updates_install[id];
    if (update_install.state != UpdateState::WAITING_INSTALL)
        return;

    std::string zRif;
    if (!get_zrif(emuenv, update_install.content_id, zRif)) {
        gui.file_menu.license_install_dialog = true;
        return;
    }

    update_install.state = UpdateState::INSTALLING;
    pkg_install(gui, emuenv, update_install.pkg_path, zRif, id);
}

enum class State {
    UNDEFINED,
    LICENSE,
    ZRIF,
    INSTALL,
    INSTALLING,
    SUCCESS,
    FAIL
};

static State state = State::UNDEFINED;

static std::mutex install_mutex;
static std::atomic<float> progress(0);
void pkg_install(GuiState &gui, EmuEnvState &emuenv, const fs::path &pkg_path, const std::string &zrif, const std::string &id) {
    static const auto progress_callback = [&](float updated_progress) {
        progress = updated_progress;
    };

    auto zRif = std::make_shared<std::string>(zrif);

    progress = 0;
    std::thread installation([&gui, &emuenv, pkg_path, zRif, id]() {
        if (!id.empty()) {
            std::lock_guard<std::mutex> lock(install_mutex);
            auto &updates_install = gui.updates_install[emuenv.app_info.app_title_id];
            gui.vita_area.pkg_install = true;
        }

        if (install_pkg(pkg_path, emuenv, *zRif, progress_callback)) {
            if ((emuenv.app_info.app_category.find("gd") != std::string::npos) || (emuenv.app_info.app_category.find("gp") != std::string::npos)) {
                init_vita_app(gui, emuenv, "ux0:app/" + emuenv.app_info.app_title_id);
                save_apps_cache(gui, emuenv);
                select_app(gui, emuenv.app_info.app_title_id);
            } else if (emuenv.app_info.app_category.find("theme") != std::string::npos)
                update_themes(gui, emuenv, emuenv.app_info.app_content_id);
            update_notice_info(gui, emuenv, "content");
            std::lock_guard<std::mutex> lock(install_mutex);
            if (!id.empty()) {
                gui.new_update_infos.erase(id);
                const auto &app_path = fmt::format("ux0/app/{}", id);
                gui.app_has_update[app_path] = false;
                gui.updates_install[id].state = UpdateState::SUCCESS;
            } else
                state = State::SUCCESS;
        } else {
            std::lock_guard<std::mutex> lock(install_mutex);
            if (!id.empty()) {
                update_notice_info(gui, emuenv, "failed_install", id);
                if (gui.updates_install.contains(id)) {
                    if (gui.vita_area.home_screen)
                        gui.updates_install.erase(id);
                    else
                        gui.updates_install[id].state = UpdateState::FAILED;
                }
            } else
                state = State::FAIL;
        }
        if (!id.empty()) {
            fs::remove_all(pkg_path.parent_path());
            std::lock_guard<std::mutex> lock(install_mutex);
            gui.vita_area.pkg_install = false;
            state = State::UNDEFINED;
            emuenv.app_info = {};
        }
    });
    installation.detach();
}

void draw_pkg_install(GuiState &gui, EmuEnvState &emuenv) {
    auto &indicator = gui.lang.indicator;

    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);

    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    if (gui.vita_area.pkg_install) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::SetNextWindowPos(VIEWPORT_POS, ImGuiCond_Always);
        ImGui::SetNextWindowSize(VIEWPORT_SIZE, ImGuiCond_Always);
        ImGui::Begin("##pkg_install", &gui.vita_area.pkg_install, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

        const ImVec2 WINDOW_SIZE(616.f * SCALE.x, 236.f * SCALE.y);
        ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + (VIEWPORT_SIZE.x / 2.f) - (WINDOW_SIZE.x / 2), VIEWPORT_POS.y + (VIEWPORT_SIZE.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.f * SCALE.x);

        ImGui::BeginChild("##pkg_install_child", WINDOW_SIZE, ImGuiChildFlags_Borders | ImGuiWindowFlags_NoDecoration);
    }
    ImGui::SetCursorPos(ImVec2(178.f * SCALE.x, ImGui::GetCursorPosY() + 30.f * SCALE.y));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", emuenv.app_info.app_title.c_str());
    ImGui::SetCursorPos(ImVec2(178.f * SCALE.x, ImGui::GetCursorPosY() + 30.f * SCALE.y));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", indicator["installing"].c_str());
    const float PROGRESS_BAR_WIDTH = 502.f * SCALE.x;
    ImGui::SetCursorPos(ImVec2((ImGui::GetWindowSize().x / 2.f) - (PROGRESS_BAR_WIDTH / 2.f), ImGui::GetCursorPosY() + 30.f * SCALE.y));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
    ImGui::ProgressBar(progress / 100.f, ImVec2(PROGRESS_BAR_WIDTH, 15.f * SCALE.x), "");
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 16.f * SCALE.y);
    TextColoredCentered(GUI_COLOR_TEXT, std::to_string(static_cast<uint32_t>(progress)).append("%").c_str());
    ImGui::PopStyleColor();
    if (gui.vita_area.pkg_install) {
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::End();
        ImGui::PopStyleVar(2);
    }
}

void draw_pkg_install_dialog(GuiState &gui, EmuEnvState &emuenv) {
    host::dialog::filesystem::Result result = host::dialog::filesystem::Result::CANCEL;
    static fs::path pkg_path{};
    static fs::path license_path{};
    static std::string title, zRIF;
    static bool draw_file_dialog = true;
    static bool delete_pkg_file, delete_license_file;

    std::lock_guard<std::mutex> lock(install_mutex);

    if (draw_file_dialog) {
        result = host::dialog::filesystem::open_file(pkg_path, { { "PlayStation Store Downloaded Package", { "pkg" } } });
        draw_file_dialog = false;
        if (result == host::dialog::filesystem::Result::SUCCESS) {
            FILE *infile = host::dialog::filesystem::resolve_host_handle(pkg_path);
            PkgHeader pkg_header{};
            fread(&pkg_header, sizeof(PkgHeader), 1, infile);
            fclose(infile);
            std::string title_id_str(pkg_header.content_id);
            if (get_zrif(emuenv, title_id_str, zRIF))
                state = State::INSTALL;

            ImGui::OpenPopup("install");
        } else if (result == host::dialog::filesystem::Result::CANCEL) {
            gui.file_menu.pkg_install_dialog = false;
            draw_file_dialog = true;
        } else {
            LOG_ERROR("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
            gui.file_menu.pkg_install_dialog = false;
            draw_file_dialog = true;
        }
    }

    auto &lang = gui.lang.install_dialog.pkg_install;
    auto &indicator = gui.lang.indicator;
    auto &common = emuenv.common_dialog.lang.common;

    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const ImVec2 WINDOW_SIZE(616.f * SCALE.x, 264.f * SCALE.y);
    const ImVec2 BUTTON_SIZE(180.f * SCALE.x, 45.f * SCALE.y);

    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x + (display_size.x / 2.f) - (WINDOW_SIZE.x / 2), emuenv.logical_viewport_pos.y + (display_size.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 15.f * SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 2.f * SCALE.x);
    if (ImGui::BeginPopupModal("install", &gui.file_menu.pkg_install_dialog, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration)) {
        ImGui::SetWindowFontScale(RES_SCALE.x);
        const auto POS_BUTTON = (WINDOW_SIZE.x / 2.f) - (BUTTON_SIZE.x / 2.f) + (10.f * SCALE.x);
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, title.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        switch (state) {
        case State::UNDEFINED: {
            ImGui::SetCursorPosX(POS_BUTTON);
            title = lang["select_license_type"];
            if (ImGui::Button(lang["select_bin_rif"].c_str(), BUTTON_SIZE))
                state = State::LICENSE;
            ImGui::SetCursorPosX(POS_BUTTON);
            if (ImGui::Button(lang["enter_zrif"].c_str(), BUTTON_SIZE))
                state = State::ZRIF;
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::SetCursorPosX(POS_BUTTON);
            if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE)) {
                gui.file_menu.pkg_install_dialog = false;
                draw_file_dialog = true;
            }
            break;
        }
        case State::LICENSE: {
            result = host::dialog::filesystem::open_file(license_path, { { "PlayStation Vita software license file", { "bin", "rif" } } });
            if (result == host::dialog::filesystem::Result::SUCCESS) {
                fs::ifstream binfile(license_path, std::ios::in | std::ios::binary | std::ios::ate);
                zRIF = rif2zrif(binfile);
                state = State::INSTALL;
            } else {
                if (result == host::dialog::filesystem::Result::ERROR)
                    LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
                state = State::UNDEFINED;
            }
            break;
        }
        case State::ZRIF: {
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
                state = State::UNDEFINED;
                zRIF.clear();
            }
            ImGui::SameLine(0, 20.f * SCALE.x);
            if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) && !zRIF.empty())
                state = State::INSTALL;
            break;
        }
        case State::INSTALL: {
            draw_file_dialog = false;
            pkg_install(gui, emuenv, pkg_path.native(), zRIF);
            state = State::INSTALLING;
            break;
        }
        case State::INSTALLING: {
            title = indicator["installing"];
            draw_pkg_install(gui, emuenv);
            break;
        }
        case State::SUCCESS: {
            title = indicator["install_complete"];
            ImGui::TextColored(GUI_COLOR_TEXT, "%s [%s]", emuenv.app_info.app_title.c_str(), emuenv.app_info.app_title_id.c_str());
            if (emuenv.app_info.app_category.find("gp") != std::string::npos)
                ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", lang["update_app"].c_str(), emuenv.app_info.app_version.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
#ifndef __ANDROID__
            ImGui::Checkbox(lang["delete_pkg"].c_str(), &delete_pkg_file);
            if (license_path != "")
                ImGui::Checkbox(lang["delete_bin_rif"].c_str(), &delete_license_file);
            ImGui::Spacing();
#endif
            ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
            if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
                if (delete_pkg_file) {
                    fs::remove(pkg_path);
                    delete_pkg_file = false;
                }
                if (delete_license_file) {
                    fs::remove(license_path);
                    delete_license_file = false;
                }
                pkg_path.clear();
                license_path.clear();
                gui.file_menu.pkg_install_dialog = false;
                draw_file_dialog = true;
                zRIF.clear();
                emuenv.app_info = {};
                state = State::UNDEFINED;
            }
            break;
        }
        case State::FAIL: {
            title = indicator["install_failed"];
            ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) - (20.f * SCALE.y));
            TextColoredCentered(GUI_COLOR_TEXT, lang["failed_install_package"].c_str());
            ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
            if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
                gui.file_menu.pkg_install_dialog = false;
                pkg_path = "";
                draw_file_dialog = true;
                license_path = "";
                state = State::UNDEFINED;
                zRIF.clear();
                emuenv.app_info = {};
            }
            break;
        }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}
} // namespace gui
