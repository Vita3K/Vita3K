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

#include <config/functions.h>

#include <config/state.h>
#include <dialog/state.h>
#include <gui/functions.h>

#include "private.h"

namespace gui {

void draw_welcome_dialog(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    static const auto BUTTON_SIZE = ImVec2(120.f * emuenv.manual_dpi_scale, 0.f);

    auto &lang = gui.lang.welcome;
    auto &common = emuenv.common_dialog.lang.common;

    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    ImGui::Begin("##welcome", &gui.help_menu.welcome_dialog, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowFontScale(RES_SCALE.x);
    TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang["title"].c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["vita3k"].c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped("%s", lang["about_vita3k"].c_str());
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["development_stage"].c_str());
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["about_firmware"].c_str());
    if (ImGui::Button(lang["download_firmware"].c_str()))
        get_firmware_file(emuenv);
    ImGui::SameLine();
    if (ImGui::Button(gui.lang.install_dialog.firmware_install["download_firmware_font_package"].c_str()))
        open_path("https://bit.ly/2P2rb0r");
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["vita3k_quickstart"].c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6.f);
    if (ImGui::Button(lang["quickstart"].c_str()))
        open_path("https://vita3k.org/quickstart.html");
    ImGui::SameLine();
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["page"].c_str());
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["check_compatibility"].c_str());
    if (ImGui::Button(lang["commercial_compatibility_list"].c_str()))
        open_path("https://vita3k.org/compatibility.html");
    ImGui::SameLine();
    if (ImGui::Button(lang["homebrew_compatibility_list"].c_str()))
        open_path("https://vita3k.org/compatibility-homebrew.html");
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["welcome_contribution"].c_str());
    ImGui::SameLine();
    if (ImGui::Button("GitHub"))
        open_path("https://github.com/Vita3K/Vita3K");
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["discord_help"].c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6.f);
    if (ImGui::Button("Discord"))
        open_path("https://discord.gg/6aGwQzh");
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["no_piracy"].c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Checkbox(lang["show_next_time"].c_str(), &emuenv.cfg.show_welcome))
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    ImGui::Spacing();
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (BUTTON_SIZE.x / 2.f));
    if (ImGui::Button(common["close"].c_str(), BUTTON_SIZE))
        gui.help_menu.welcome_dialog = false;

    ImGui::End();
}

} // namespace gui
