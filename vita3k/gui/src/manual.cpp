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

#include <ctrl/ctrl.h>

#include <gui/functions.h>

#include <config/state.h>
#include <io/vfs.h>

#include <util/log.h>

#include <stb_image.h>

namespace gui {

void open_manual(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    if (init_manual(gui, emuenv, app_path)) {
        emuenv.app_path = app_path;
        gui.vita_area.information_bar = false;
        gui.vita_area.live_area_screen = false;
        gui.vita_area.manual = true;
    } else
        LOG_ERROR("Error opening Manual");
}

static int32_t current_page;
static float scroll = 0.f, max_scroll = 0.f;
static auto hidden_button = false;

void browse_pages_manual(GuiState &gui, EmuEnvState &emuenv, const uint32_t button) {
    const auto RES_HEIGHT_SCALE = emuenv.gui_scale.y;
    const auto SCALE = RES_HEIGHT_SCALE * emuenv.manual_dpi_scale;

    const auto manual_size = static_cast<int32_t>(gui.manuals.size() - 1);

    switch (button) {
    case SCE_CTRL_RIGHT:
        current_page = std::min(current_page + 1, manual_size);
        scroll = 0.f;
        break;
    case SCE_CTRL_LEFT:
        current_page = std::max(current_page - 1, 0);
        scroll = 0.f;
        break;
    case SCE_CTRL_UP:
        scroll -= std::min(40.f * SCALE, scroll);
        break;
    case SCE_CTRL_DOWN:
        scroll += std::min(40.f * SCALE, max_scroll - scroll);
        break;
    case SCE_CTRL_TRIANGLE:
        hidden_button = !hidden_button;
        break;
    default:
        break;
    }
}

static std::vector<uint32_t> height_manual_pages;

bool init_manual(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    // Reset manual variables
    current_page = 0;
    scroll = 0.f;
    height_manual_pages.clear();

    const auto APP_INDEX = get_app_index(gui, app_path);
    const auto APP_PATH{ emuenv.pref_path / "ux0/app" / app_path };
    auto manual_path{ fs::path("sce_sys/manual/") };

    const auto lang = fmt::format("{:0>2d}", emuenv.cfg.sys_lang);
    if (fs::exists(APP_PATH / manual_path / lang))
        manual_path /= lang;

    if (fs::exists(APP_PATH / manual_path) && !fs::is_empty(APP_PATH / manual_path)) {
        int32_t width, height;
        for (const auto &manual : fs::directory_iterator(APP_PATH / manual_path)) {
            if (manual.path().extension() == ".png") {
                const auto page_path = manual_path / manual.path().filename();

                vfs::FileBuffer buffer;
                vfs::read_app_file(buffer, emuenv.pref_path, app_path, page_path);

                if (buffer.empty()) {
                    LOG_WARN("Manual not found for title: {} [{}].", app_path, APP_INDEX->title);
                    return false;
                }

                // Load image data from memory buffer and check if it's valid
                stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                if (!data) {
                    LOG_ERROR("Invalid manual image for title: {} [{}] in path: {}.", app_path, APP_INDEX->title, page_path);
                    return false;
                }

                // Add manual image to vector
                gui.manuals.emplace_back(gui.imgui_state.get(), data, width, height);

                // Calculate height of the page
                const auto ratio = static_cast<float>(width) / static_cast<float>(height);
                height = static_cast<int32_t>(960.f / ratio);
                height_manual_pages.push_back(height);

                // Free image data
                stbi_image_free(data);
            }
        }
    }

    return !gui.manuals.empty();
}

void draw_manual(GuiState &gui, EmuEnvState &emuenv) {
    // Set settings and begin window for manual
    ImGui::SetNextWindowBgAlpha(0.999f);
    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("##manual", &gui.vita_area.manual, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    // Set settings and begin child window for manual pages
    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const ImVec2 WINDOW_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::BeginChild("##manual_page", display_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);

    // Draw manual image if exists and is valid
    if (!gui.manuals.empty() && gui.manuals[current_page])
        ImGui::Image(gui.manuals[current_page], ImVec2(display_size.x, height_manual_pages[current_page] * SCALE.y));

    // Set max scroll
    max_scroll = ImGui::GetScrollMaxY();

    // Set scroll with mouse wheel or keyboard up/down keys
    const auto wheel_counter = ImGui::GetIO().MouseWheel;
    if ((wheel_counter == 1.f) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_up)))
        scroll -= std::min(40.f * SCALE.y, scroll);
    else if ((wheel_counter == -1.f) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_down)))
        scroll += std::min(40.f * SCALE.y, max_scroll - scroll);

    // Set scroll y position
    ImGui::SetScrollY(scroll);

    // Set pos and begin child window for manual buttons
    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::BeginChild("##manual_buttons", display_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    // Set window font scale for buttons
    ImGui::SetWindowFontScale(RES_SCALE.x);

    // Hide button when right click is pressed on mouse
    if (!ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(0))
        hidden_button = !hidden_button;

    // Set button size
    const auto BUTTON_SIZE = ImVec2(65.f * SCALE.x, 30.f * SCALE.y);

    // Draw esc button
    ImGui::SetCursorPos(ImVec2(5.0f * SCALE.x, 10.0f * SCALE.y));
    if ((!hidden_button && ImGui::Button("Esc", BUTTON_SIZE)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_psbutton)))
        gui::close_system_app(gui, emuenv);

    // Draw manual scroll bar when is available
    if (max_scroll) {
        ImGui::SetCursorPos(ImVec2(display_size.x - (18.f * SCALE.x), 10.f * SCALE.y));
        ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 174.f * SCALE.y);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 40.f * SCALE.y);
        ImGui::VSliderFloat("##manual_scroll_bar", ImVec2(8.f * SCALE.x, 460.f * SCALE.y), &scroll, max_scroll, 0, "");
        ImGui::PopStyleVar(2);
    }

    // Draw left button
    if (current_page > 0) {
        ImGui::SetCursorPos(ImVec2(5.0f * SCALE.x, display_size.y - (40.0f * SCALE.y)));
        if ((!hidden_button && ImGui::Button("<", BUTTON_SIZE)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_left))) {
            --current_page;
            scroll = 0.f;
        }
    }

    // Draw browser page button
    if (!hidden_button) {
        ImGui::SetCursorPos(ImVec2(display_size.x / 2.f - ((BUTTON_SIZE.x / 2.f)), display_size.y - (40.f * SCALE.y)));
        const std::string slider = fmt::format("{:0>2d}/{:0>2d}", current_page + 1, (int32_t)gui.manuals.size());
        if (ImGui::Button(slider.c_str(), BUTTON_SIZE))
            ImGui::OpenPopup("Manual Slider");
        const auto POPUP_HEIGHT = 64.f * SCALE.y;
        ImGui::SetNextWindowPos(ImVec2(0.f, display_size.y - POPUP_HEIGHT), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(display_size.x, POPUP_HEIGHT), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.7f);
        if (ImGui::BeginPopupModal("Manual Slider", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
            const auto SLIDER_WIDTH = 800.f * SCALE.x;
            ImGui::PushItemWidth(SLIDER_WIDTH);
            ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 26.f * SCALE.y);
            ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 50.f * SCALE.y);
            ImGui::SetCursorPos(ImVec2((display_size.x / 2) - (SLIDER_WIDTH / 2.f), (POPUP_HEIGHT / 2) - (15.f * SCALE.x)));
            ImGui::SliderInt("##slider_current_manual", &current_page, 0, (int32_t)gui.manuals.size() - 1, slider.c_str());
            ImGui::PopStyleVar(2);
            ImGui::PopItemWidth();
            if (ImGui::IsItemDeactivated())
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    // Draw right button
    if (current_page < (int)gui.manuals.size() - 1) {
        ImGui::SetCursorPos(ImVec2(display_size.x - (70.f * SCALE.x), display_size.y - (40.0f * SCALE.y)));
        if ((!hidden_button && ImGui::Button(">", BUTTON_SIZE)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_right))) {
            scroll = 0.f;
            ++current_page;
        }
    }
    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
