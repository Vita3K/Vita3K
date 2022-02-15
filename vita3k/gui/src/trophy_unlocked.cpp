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
#include <gui/imgui_impl_sdl.h>
#include <gui/state.h>

#include <host/state.h>

namespace gui {

static constexpr int TROPHY_WINDOW_STATIC_FRAME_COUNT = 250;

static void draw_trophy_unlocked(GuiState &gui, HostState &host, NpTrophyUnlockCallbackData &callback_data) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);

    const auto TROPHY_WINDOW_MARGIN_PADDING = 12.f * SCALE.x;
    const auto TROPHY_WINDOW_ICON_SIZE = 48.f * SCALE.x;
    const auto TROPHY_ICON_MARGIN_PADDING = 6.f * SCALE.y;
    const auto TROPHY_WINDOW_SIZE = ImVec2(400.f * SCALE.x, TROPHY_WINDOW_ICON_SIZE + TROPHY_WINDOW_MARGIN_PADDING);
    const auto TROPHY_WINDOW_Y_POS = 20.f * SCALE.y;
    const auto TROPHY_MOVE_DELTA = 12.f * SCALE.x;

    if (gui.trophy_window_frame_stage == TrophyAnimationStage::SLIDE_IN
        || gui.trophy_window_frame_stage == TrophyAnimationStage::SLIDE_OUT) {
        ImVec2 target_window_pos = ImVec2(0.0f, 0.0f);

        if (gui.trophy_window_frame_stage == TrophyAnimationStage::SLIDE_IN)
            target_window_pos = ImVec2(ImGui::GetIO().DisplaySize.x - TROPHY_WINDOW_SIZE.x - TROPHY_WINDOW_MARGIN_PADDING,
                TROPHY_WINDOW_Y_POS);
        else
            target_window_pos = ImVec2(ImGui::GetIO().DisplaySize.x + TROPHY_WINDOW_MARGIN_PADDING, TROPHY_WINDOW_Y_POS);

        if (gui.trophy_window_frame_stage == TrophyAnimationStage::SLIDE_IN && gui.trophy_window_frame_count == 0) {
            gui.trophy_window_pos = ImVec2(ImGui::GetIO().DisplaySize.x + TROPHY_WINDOW_MARGIN_PADDING, TROPHY_WINDOW_Y_POS);

            // Load icon
            gui.trophy_window_icon = load_image(gui, (const char *)callback_data.icon_buf.data(),
                static_cast<std::uint32_t>(callback_data.icon_buf.size()));
        } else if (gui.trophy_window_frame_stage == TrophyAnimationStage::SLIDE_IN && gui.trophy_window_pos.x > target_window_pos.x) {
            gui.trophy_window_pos.x -= TROPHY_MOVE_DELTA;
        } else if (gui.trophy_window_frame_stage == TrophyAnimationStage::SLIDE_OUT && gui.trophy_window_pos.x < target_window_pos.x) {
            gui.trophy_window_pos.x += TROPHY_MOVE_DELTA;
        } else {
            gui.trophy_window_frame_stage = static_cast<TrophyAnimationStage>(static_cast<int>(gui.trophy_window_frame_stage) + 1);
            gui.trophy_window_frame_count = 0;
        }

        gui.trophy_window_frame_count++;
    }

    ImGui::SetNextWindowBgAlpha(0.9f);
    ImGui::SetNextWindowPos(gui.trophy_window_pos);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 15.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, GUI_SMOOTH_GRAY);
    ImGui::SetNextWindowSize(TROPHY_WINDOW_SIZE);
    ImGui::Begin("##trophy", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, TROPHY_WINDOW_ICON_SIZE + TROPHY_WINDOW_MARGIN_PADDING * 2);
    ImGui::SetCursorPos(ImVec2(TROPHY_WINDOW_MARGIN_PADDING, TROPHY_ICON_MARGIN_PADDING));
    ImGui::Image((ImTextureID)gui.trophy_window_icon, ImVec2(TROPHY_WINDOW_ICON_SIZE, TROPHY_WINDOW_ICON_SIZE));
    ImGui::NextColumn();

    auto common = gui.lang.common.main;
    std::string trophy_kind_s = "?";

    switch (callback_data.trophy_kind) {
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM: {
        trophy_kind_s = common["platinum"];
        break;
    }

    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD: {
        trophy_kind_s = common["gold"];
        break;
    }

    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER: {
        trophy_kind_s = common["silver"];
        break;
    }

    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE: {
        trophy_kind_s = common["bronze"];
        break;
    }

    default:
        break;
    }

    ImGui::SetWindowFontScale(1.f * RES_SCALE.x);
    ImGui::SetCursorPosY(TROPHY_WINDOW_MARGIN_PADDING);
    ImGui::TextColored(ImVec4(0.24f, 0.24f, 0.24f, 1.0f), "(%s) %s", trophy_kind_s.c_str(), callback_data.trophy_name.c_str());
    ImGui::SetWindowFontScale(0.8f * RES_SCALE.x);
    ImGui::TextColored(ImVec4(0.24f, 0.24f, 0.24f, 1.0f), "%s", gui.lang.indicator["trophy_earned"].c_str());
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    if (gui.trophy_window_frame_stage == TrophyAnimationStage::STATIC) {
        gui.trophy_window_frame_count++;

        if (gui.trophy_window_frame_count == TROPHY_WINDOW_STATIC_FRAME_COUNT) {
            gui.trophy_window_frame_stage = TrophyAnimationStage::SLIDE_OUT;
            gui.trophy_window_frame_count = 0;
        }
    }
}

void draw_trophies_unlocked(GuiState &gui, HostState &host) {
    const std::lock_guard<std::mutex> guard(gui.trophy_unlock_display_requests_access_mutex);

    if (!gui.trophy_unlock_display_requests.empty()) {
        if (gui.trophy_window_frame_stage == TrophyAnimationStage::END) {
            update_notice_info(gui, host, "trophy");
            gui.trophy_unlock_display_requests.pop_back();

            // Destroy the texture
            if (gui.trophy_window_frame_count != 0xFFFFFFFF)
                ImGui_ImplSdl_DeleteTexture(gui.imgui_state.get(), gui.trophy_window_icon);

            gui.trophy_window_frame_stage = TrophyAnimationStage::SLIDE_IN;
            gui.trophy_window_frame_count = 0;
        } else {
            draw_trophy_unlocked(gui, host, gui.trophy_unlock_display_requests.back());
        }
    }
}

} // namespace gui
