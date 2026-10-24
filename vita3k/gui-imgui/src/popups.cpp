// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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
#include <ctrl/ctrl.h>
#include <dialog/state.h>
#include <gui-imgui/functions.h>
#include <gui-imgui/popups.h>
#include <io/state.h>
#include <util/log.h>

#include <mutex>
#include <utility>
#include <vector>

namespace gui {

static std::mutex info_messages_mutex;
static std::vector<InfoMessage> info_messages;

void init_info_message(InfoMessage message) {
    std::lock_guard lock(info_messages_mutex);
    info_messages.emplace_back(std::move(message));
}

void draw_info_message(GuiState &gui, EmuEnvState &emuenv) {
    if (emuenv.io.title_id.empty() && emuenv.cfg.display_info_message) {
        const ImVec2 display_size(gui.logical_viewport_size.x, gui.logical_viewport_size.y);
        const ImVec2 RES_SCALE(gui.gui_scale.x, gui.gui_scale.y);
        const ImVec2 SCALE(RES_SCALE.x * gui.manual_dpi_scale, RES_SCALE.y * gui.manual_dpi_scale);

        const ImVec2 WINDOW_SIZE(680.0f * SCALE.x, 320.0f * SCALE.y);
        const ImVec2 BUTTON_SIZE(160.f * SCALE.x, 46.f * SCALE.y);

        ImGui::SetNextWindowPos(ImVec2(gui.logical_viewport_pos.x, gui.logical_viewport_pos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
        ImGui::Begin("##information", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration);
        ImGui::SetNextWindowPos(ImVec2(gui.logical_viewport_pos.x + (display_size.x / 2) - (WINDOW_SIZE.x / 2.f), gui.logical_viewport_pos.y + (display_size.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
        ImGui::BeginChild("##info", WINDOW_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration);
        const auto &title = gui.info_message.title;
        ImGui::SetWindowFontScale(RES_SCALE.x);
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, title.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        const auto text_size = ImGui::CalcTextSize(gui.info_message.msg.c_str(), 0, false, WINDOW_SIZE.x - (24.f * SCALE.x));
        const auto text_pos = ImVec2((WINDOW_SIZE.x / 2.f) - (text_size.x / 2.f), (WINDOW_SIZE.y / 2.f) - (text_size.y / 2.f) - (24 * SCALE.y));
        ImGui::SetCursorPos(text_pos);
        ImGui::TextWrapped("%s", gui.info_message.msg.c_str());
        ImGui::SetCursorPosY(WINDOW_SIZE.y - BUTTON_SIZE.y - (42.0f * SCALE.y));
        ImGui::Separator();
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2.f) - (BUTTON_SIZE.x / 2.f), WINDOW_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
        if (ImGui::Button(::lang::get(::lang::str::ok).c_str(), BUTTON_SIZE) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_cross))
            gui.info_message = {};
        ImGui::EndChild();

        ImGui::PopStyleVar();
        ImGui::End();
    } else {
        spdlog::log(gui.info_message.level, "[{}] {}", gui.info_message.function, gui.info_message.msg);
        gui.info_message = {};
    }
}

static void draw_app_close(GuiState &gui, EmuEnvState &emuenv) {
    const auto button = std::exchange(gui.vita_area.app_close_button, 0U);
    const auto confirm_button = emuenv.cfg.sys_button == 1 ? SCE_CTRL_CROSS : SCE_CTRL_CIRCLE;
    const auto cancel_button = emuenv.cfg.sys_button == 1 ? SCE_CTRL_CIRCLE : SCE_CTRL_CROSS;
    const ImVec2 display_size(gui.logical_viewport_size.x, gui.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(gui.gui_scale.x, gui.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * gui.manual_dpi_scale, RES_SCALE.y * gui.manual_dpi_scale);

    const auto WINDOW_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);
    const auto BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);

    ImGui::SetNextWindowPos(ImVec2(gui.logical_viewport_pos.x, gui.logical_viewport_pos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::Begin("##app_close", &gui.vita_area.app_close, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowPos(ImVec2(gui.logical_viewport_pos.x + (display_size.x / 2.f) - (WINDOW_SIZE.x / 2.f), gui.logical_viewport_pos.y + (display_size.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
    ImGui::BeginChild("##app_close_child", WINDOW_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);

    const auto ICON_SIZE = ImVec2(64.f * SCALE.x, 64.f * SCALE.y);

    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
    ImGui::SetCursorPos(ImVec2(50.f * SCALE.x, 108.f * SCALE.y));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.lang.game_data["app_close"].c_str());
    if (const auto app_icon = get_app_icon(gui, emuenv.io.app_path)) {
        const auto ICON_POS_SCALE = ImVec2(50.f * SCALE.x, (WINDOW_SIZE.y / 2.f) - (ICON_SIZE.y / 2.f) - (10.f * SCALE.y));
        ImGui::SetCursorPos(ICON_POS_SCALE);
        const auto POS_MIN = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddImageRounded(*app_icon, POS_MIN, ImVec2(POS_MIN.x + ICON_SIZE.x, POS_MIN.y + ICON_SIZE.y), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_SIZE.x, ImDrawFlags_RoundCornersAll);
    }
    ImGui::SetCursorPos(ImVec2(ICON_SIZE.x + (72.f * SCALE.x), (WINDOW_SIZE.y / 2.f) - ImGui::CalcTextSize(emuenv.current_app_title.c_str()).y + (4.f * SCALE.y)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", emuenv.current_app_title.c_str());
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), WINDOW_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
    const bool cancelled = ImGui::Button(::lang::get(::lang::str::cancel).c_str(), BUTTON_SIZE) || button == cancel_button;
    if (cancelled)
        gui.vita_area.app_close = false;
    ImGui::SameLine(0, 20.f * SCALE.x);
    if ((ImGui::Button(::lang::get(::lang::str::ok).c_str(), BUTTON_SIZE) || button == confirm_button) && !cancelled) {
        gui.vita_area.app_close = false;
        const auto &app_path = gui.vita_area.live_area_screen ? gui.live_area_current_open_apps_list[gui.live_area_app_current_open] : emuenv.app_path;
        close_and_run_new_app(emuenv, app_path);
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
}

static void draw_connecting_please_wait(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_SIZE(gui.logical_viewport_size.x, gui.logical_viewport_size.y);
    const ImVec2 VIEWPORT_POS(gui.logical_viewport_pos.x, gui.logical_viewport_pos.y);
    const auto RES_SCALE = ImVec2(gui.gui_scale.x, gui.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * gui.manual_dpi_scale, RES_SCALE.y * gui.manual_dpi_scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowPos(VIEWPORT_POS);
    ImGui::SetNextWindowSize(VIEWPORT_SIZE);
    ImGui::Begin("please_wait", &gui.vita_area.connecting_please_wait, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration);
    const ImVec2 WINDOW_SIZE(520.f * SCALE.x, 120.f * SCALE.y);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.f * SCALE.x);
    ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + ((VIEWPORT_SIZE.x - WINDOW_SIZE.x) / 2.f), VIEWPORT_POS.y + ((VIEWPORT_SIZE.y - WINDOW_SIZE.y) / 2.f)));
    ImGui::BeginChild("please_wait_child", WINDOW_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    const auto &text = gui.lang.common.main.at("connecting_please_wait");
    const auto text_size = ImGui::CalcTextSize(text.c_str());
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x - text_size.x) / 2.f, (WINDOW_SIZE.y - text_size.y) / 2.f));
    ImGui::Text("%s", text.c_str());
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

static void draw_please_wait(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_SIZE(gui.logical_viewport_size.x, gui.logical_viewport_size.y);
    const ImVec2 VIEWPORT_POS(gui.logical_viewport_pos.x, gui.logical_viewport_pos.y);
    const auto RES_SCALE = ImVec2(gui.gui_scale.x, gui.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * gui.manual_dpi_scale, RES_SCALE.y * gui.manual_dpi_scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowPos(VIEWPORT_POS);
    ImGui::SetNextWindowSize(VIEWPORT_SIZE);
    ImGui::Begin("please_wait", &gui.vita_area.please_wait, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration);
    const ImVec2 WINDOW_SIZE(520.f * SCALE.x, 120.f * SCALE.y);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.f * SCALE.x);
    ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + ((VIEWPORT_SIZE.x - WINDOW_SIZE.x) / 2.f), VIEWPORT_POS.y + ((VIEWPORT_SIZE.y - WINDOW_SIZE.y) / 2.f)));
    ImGui::BeginChild("please_wait_child", WINDOW_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    const auto text = ::lang::get(::lang::str::please_wait);
    const auto text_size = ImGui::CalcTextSize(text.c_str());
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x - text_size.x) / 2.f, (WINDOW_SIZE.y - text_size.y) / 2.f));
    ImGui::Text("%s", text.c_str());
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void draw_popups(GuiState &gui, EmuEnvState &emuenv) {
    if (gui.info_message.msg.empty()) {
        std::lock_guard lock(info_messages_mutex);
        if (!info_messages.empty()) {
            gui.info_message = std::move(info_messages.front());
            info_messages.erase(info_messages.begin());
        }
    }

    if (gui.vita_area.app_close)
        draw_app_close(gui, emuenv);

    if (!gui.info_message.msg.empty())
        draw_info_message(gui, emuenv);

    if (gui.vita_area.connecting_please_wait)
        draw_connecting_please_wait(gui, emuenv);
    if (gui.vita_area.please_wait)
        draw_please_wait(gui, emuenv);
}

} // namespace gui