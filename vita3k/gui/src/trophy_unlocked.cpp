#include "private.h"

#include <gui/functions.h>
#include <gui/state.h>
#include <gui/imgui_impl_sdl.h>

#include <host/state.h>

namespace gui {

static constexpr float TROPHY_MOVE_DELTA = 12.0f;
static constexpr float TROPHY_WINDOW_ICON_SIZE = 60.0f;
static constexpr float TROPHY_WINDOW_MARGIN_PADDING = 10.0f;
static const ImVec2 TROPHY_WINDOW_SIZE = ImVec2(360, TROPHY_WINDOW_ICON_SIZE + TROPHY_WINDOW_MARGIN_PADDING * 2);
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, GUI_SMOOTH_GRAY);
    ImGui::SetNextWindowSize(TROPHY_WINDOW_SIZE);
    ImGui::Begin("##NoName", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::SetWindowFontScale(1.3f);
    ImGui::SetCursorPos(ImVec2(TROPHY_WINDOW_MARGIN_PADDING, TROPHY_WINDOW_MARGIN_PADDING));
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, TROPHY_WINDOW_ICON_SIZE + TROPHY_WINDOW_MARGIN_PADDING * 2);
    ImGui::Image((ImTextureID)gui.trophy_window_icon, ImVec2(TROPHY_WINDOW_ICON_SIZE, TROPHY_WINDOW_ICON_SIZE));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));
    ImGui::SetColumnWidth(1, ImGui::GetIO().DisplaySize.x - ImGui::GetCursorPosX() - TROPHY_WINDOW_MARGIN_PADDING);
    ImGui::NextColumn();

    std::string trophy_kind_s = "?";

    switch (callback_data.trophy_kind) {
    case emu::np::trophy::TrophyType::PLATINUM: {
        trophy_kind_s = "Platinum";
        break;
    }

    case emu::np::trophy::TrophyType::BRONZE: {
        trophy_kind_s = "Bronze";
        break;
    }

    case emu::np::trophy::TrophyType::SILVER: {
        trophy_kind_s = "Silver";
        break;
    }

    case emu::np::trophy::TrophyType::GOLD: {
        trophy_kind_s = "Gold";
        break;
    }

    default:
        break;
    }

    ImGui::TextWrapped("%s (%s)", callback_data.trophy_name.c_str(), trophy_kind_s.c_str());

    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextWrapped("%s", callback_data.description.c_str());

    ImGui::PopStyleColor();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

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
