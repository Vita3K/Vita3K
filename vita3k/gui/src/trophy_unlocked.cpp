#include "private.h"

#include <gui/functions.h>
#include <gui/imgui_impl_sdl.h>
#include <gui/state.h>

#include <host/state.h>

namespace gui {

static constexpr float TROPHY_MOVE_DELTA = 12.0f;
static constexpr float TROPHY_WINDOW_ICON_SIZE = 48.0f;
static constexpr float TROPHY_ICON_MARGIN_PADDING = 6.f;
static constexpr float TROPHY_WINDOW_MARGIN_PADDING = 12.f;
static const ImVec2 TROPHY_WINDOW_SIZE = ImVec2(400.f, TROPHY_WINDOW_ICON_SIZE + TROPHY_WINDOW_MARGIN_PADDING);
static constexpr int TROPHY_WINDOW_STATIC_FRAME_COUNT = 250;
static constexpr float TROPHY_WINDOW_Y_POS = 20.0f;

static void draw_trophy_unlocked(GuiState &gui, HostState &host, NpTrophyUnlockCallbackData &callback_data) {
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
    ImGui::Begin("##NoName", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, TROPHY_WINDOW_ICON_SIZE + TROPHY_WINDOW_MARGIN_PADDING * 2);
    ImGui::SetCursorPos(ImVec2(TROPHY_WINDOW_MARGIN_PADDING, TROPHY_ICON_MARGIN_PADDING));
    ImGui::Image((ImTextureID)gui.trophy_window_icon, ImVec2(TROPHY_WINDOW_ICON_SIZE, TROPHY_WINDOW_ICON_SIZE));
    ImGui::NextColumn();

    auto common = gui.lang.common.common;
    std::string trophy_kind_s = "?";

    switch (callback_data.trophy_kind) {
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM: {
        trophy_kind_s = !common["platinium"].empty() ? common["platinum"] : "Platinum";
        break;
    }

    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD: {
        trophy_kind_s = !common["gold"].empty() ? common["gold"] : "Gold";
        break;
    }

    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER: {
        trophy_kind_s = !common["silver"].empty() ? common["silver"] : "Silver";
        break;
    }

    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE: {
        trophy_kind_s = !common["bronze"].empty() ? common["bronze"] : "Bronze";
        break;
    }

    default:
        break;
    }

    ImGui::SetWindowFontScale(1.f);
    ImGui::SetCursorPosY(TROPHY_WINDOW_MARGIN_PADDING);
    ImGui::TextColored(ImVec4(0.24f, 0.24f, 0.24f, 1.0f), "(%s) %s", trophy_kind_s.c_str(), callback_data.trophy_name.c_str());
    ImGui::SetWindowFontScale(0.8f);
    const auto trophy_earned = !gui.lang.indicator["trophy_earned"].empty() ? gui.lang.indicator["trophy_earned"].c_str() : "You have earned a trophy!";
    ImGui::TextColored(ImVec4(0.24f, 0.24f, 0.24f, 1.0f), trophy_earned);
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
            gui.trophy_unlock_display_requests.pop();

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
