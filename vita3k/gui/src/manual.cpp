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
        gui.vita_area.home_screen = false;
        gui.vita_area.live_area_screen = false;
        gui.vita_area.manual = true;
    } else
        LOG_ERROR("Error opening Manual");
}

static int32_t scroll_page = 0;
static int32_t current_page = 0;
static float scroll = 0.f;
static std::map<uint32_t, uint32_t> max_scroll{};
static bool hidden_button = false;
static bool is_page_animating = false;
static float target_scroll_page;
static bool is_book_mode = false;

static void change_page(EmuEnvState &emuenv, int32_t target_page) {
    scroll = 0.f;
    current_page = target_page;
    target_scroll_page = current_page * (is_book_mode ? emuenv.logical_viewport_size.x / 2 : emuenv.logical_viewport_size.x);
    is_page_animating = true;
}

void browse_pages_manual(GuiState &gui, EmuEnvState &emuenv, const uint32_t button) {
    const auto RES_HEIGHT_SCALE = emuenv.gui_scale.y;
    const auto SCALE = RES_HEIGHT_SCALE * emuenv.manual_dpi_scale;

    const auto manual_size = static_cast<int32_t>(gui.manuals.size() - 1);

    switch (button) {
    case SCE_CTRL_RIGHT:
        change_page(emuenv, std::min(current_page + 1, manual_size));
        break;
    case SCE_CTRL_LEFT:
        change_page(emuenv, std::max(current_page - 1, 0));
        break;
    case SCE_CTRL_UP:
        scroll -= std::min(40.f * SCALE, scroll);
        break;
    case SCE_CTRL_DOWN:
        scroll += std::min(40.f * SCALE, max_scroll.at(current_page) - scroll);
        break;
    case SCE_CTRL_TRIANGLE:
        hidden_button = !hidden_button;
        break;
    default:
        break;
    }
}

static std::vector<ImVec2> manual_pages_size{};

bool init_manual(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    // Reset manual variables
    constexpr uint32_t MAX_MANUAL_PAGES = 999;
    current_page = 0;
    scroll = 0.f;
    max_scroll.clear();
    is_book_mode = false;
    manual_pages_size.clear();

    const auto APP_INDEX = get_app_index(gui, app_path);
    const auto APP_PATH{ emuenv.pref_path / "ux0/app" / app_path };
    auto manual_path{ fs::path("sce_sys/manual") };

    const auto lang = fmt::format("{:0>2d}", emuenv.cfg.sys_lang);
    if (fs::exists(APP_PATH / manual_path / lang))
        manual_path /= lang;

    // Load manual images
    for (uint32_t i = 0; i < MAX_MANUAL_PAGES; i++) {
        // Get manual page path
        const auto page_path = manual_path / fmt::format("{:0>3d}.png", i + 1);

        // Read manual image from file buffer and check if it exists
        vfs::FileBuffer buffer;
        if (!vfs::read_app_file(buffer, emuenv.pref_path, app_path, page_path))
            break;

        // Load image data from memory buffer and check if it's valid
        int32_t width, height;
        stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data) {
            LOG_ERROR("Invalid manual image for title: {} [{}] in path: {}.", app_path, APP_INDEX->title, page_path);
            return false;
        }

        // Add manual image to vector
        gui.manuals.emplace_back(gui.imgui_state.get(), data, width, height);

        // Store manual page size
        manual_pages_size.emplace_back(static_cast<float>(width), static_cast<float>(height));

        // Free image data
        stbi_image_free(data);
    }

    return !gui.manuals.empty();
}

void draw_manual(GuiState &gui, EmuEnvState &emuenv) {
    // Set settings and begin window for manual
    ImGui::SetNextWindowBgAlpha(0.999f);
    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const ImVec2 WINDOW_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 WINDOW_SIZE(display_size.x * gui.manuals.size(), display_size.y);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::SetNextWindowContentSize(WINDOW_SIZE);
    auto flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##manual", &gui.vita_area.manual, flags);

    // Draw black background for full screens
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0.f, 0.f), ImGui::GetIO().DisplaySize, IM_COL32(0.f, 0.f, 0.f, 255.f));

    const auto SCREEN_POS = ImGui::GetCursorScreenPos();
    const auto WIDTH_PAGE = is_book_mode ? display_size.x / 2.f : display_size.x;
    static float scroll_x = 0.f; // Current scroll position
    if (is_page_animating)
        is_page_animating = set_scroll_animation(scroll_x, target_scroll_page, std::to_string(current_page), ImGui::SetScrollX);

    // Recalibrate scroll position if display size changed
    const float correct_scroll_x = current_page * WIDTH_PAGE;
    if (!is_page_animating && fabs(scroll_x - correct_scroll_x) > 1.0f) {
        scroll_x = correct_scroll_x;
        ImGui::SetScrollX(scroll_x);
    }

    // Set button size
    const auto BUTTON_SIZE = ImVec2(65.f * SCALE.x, 30.f * SCALE.y);

    static float scroll_y_speed = 0.08f; // Speed of the scroll animation
    static float scroll_y = 0.f; // Current scroll position
    const uint32_t total_pages = static_cast<uint32_t>(gui.manuals.size());
    const uint32_t max_pages_index = total_pages - 1u;

    // Set range for manual pages to be displayed, based on the current page
    const int32_t range = 3;
    const uint32_t start_page = std::max(current_page - range, 0);
    const uint32_t end_page = std::min(current_page + range, static_cast<int32_t>(max_pages_index));

    // Draw manual image if exists and is valid
    for (auto p = 0; p < gui.manuals.size(); p++) {
        // Skip pages outside the range of the current page
        if ((p < start_page) || (p > end_page))
            continue;

        // Set size and position for each page
        const ImVec2 MANUAL_PAGE_SIZE(manual_pages_size.at(p).x * SCALE.x, manual_pages_size.at(p).y * SCALE.y);
        const ImVec2 CHILD_SIZE(is_book_mode ? display_size.x / 2.f : display_size.x, display_size.y);
        const auto ratio = is_book_mode ? std::min(CHILD_SIZE.x / MANUAL_PAGE_SIZE.x, CHILD_SIZE.y / MANUAL_PAGE_SIZE.y) : CHILD_SIZE.x / MANUAL_PAGE_SIZE.x;
        const ImVec2 PAGE_SIZE(MANUAL_PAGE_SIZE.x * ratio, MANUAL_PAGE_SIZE.y * ratio);
        const ImVec2 PAGE_POS(std::max((CHILD_SIZE.x - PAGE_SIZE.x) / 2.f, 0.f), std::max((CHILD_SIZE.y - PAGE_SIZE.y) / 2.f, 0.f));
        const ImVec2 CHILD_POS(SCREEN_POS.x + (p * CHILD_SIZE.x), SCREEN_POS.y);

        //  Set settings and begin child window for manual pages
        ImGui::SetNextWindowPos(CHILD_POS, ImGuiCond_Always);
        ImGui::SetNextWindowContentSize(PAGE_SIZE);
        ImGui::BeginChild(fmt::format("##manual_page_{}", p + 1).c_str(), CHILD_SIZE, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetCursorPos(PAGE_POS);
        ImGui::Image(gui.manuals[p], PAGE_SIZE);

        // Set max scroll for each page
        max_scroll[p] = ImGui::GetScrollMaxY();

        // Set scroll with mouse wheel or keyboard up/down keys
        const auto wheel_counter = ImGui::GetIO().MouseWheel;
        if ((wheel_counter == 1.f) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_up)))
            scroll -= std::min(40.f * SCALE.y, scroll);
        else if ((wheel_counter == -1.f) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_down)))
            scroll += std::min(40.f * SCALE.y, max_scroll.at(current_page) - scroll);

        // Set scroll y position
        ImGui::SetScrollY(scroll);

        ImGui::EndChild();
    }

    // Set pos and begin child window for manual buttons
    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::BeginChild("##manual_buttons", display_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    // Draw manual scroll bar when is available
    if (max_scroll.at(current_page)) {
        ImGui::SetCursorPos(ImVec2(display_size.x - (18.f * SCALE.x), 10.f * SCALE.y));
        ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 174.f * SCALE.y);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 40.f * SCALE.y);
        const auto old_scroll = scroll;
        ImGui::VSliderFloat("##manual_scroll_bar", ImVec2(8.f * SCALE.x, 460.f * SCALE.y), &scroll, max_scroll.at(current_page), 0, "");
        ImGui::PopStyleVar(2);
    }

    // Set window font scale for buttons
    ImGui::SetWindowFontScale(RES_SCALE.x);

    // Hide button when right click is pressed on mouse
    if (!ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(0))
        hidden_button = !hidden_button;

    // Draw esc button
    ImGui::SetCursorPos(ImVec2(5.0f * SCALE.x, 10.0f * SCALE.y));
    if ((!hidden_button && ImGui::Button("Esc", BUTTON_SIZE)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_psbutton)))
        gui::close_system_app(gui, emuenv);

    const auto BUTTON_POS = ImVec2(10.f * SCALE.x, display_size.y - (10.f * SCALE.y) - BUTTON_SIZE.y);

    // Draw left button
    if (current_page > 0) {
        ImGui::SetCursorPos(BUTTON_POS);
        if ((!hidden_button && ImGui::Button("<", BUTTON_SIZE)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_left)))
            change_page(emuenv, current_page - 1);
    }

    // Draw browser page button
    if (!hidden_button) {
        ImGui::SetCursorPos(ImVec2(display_size.x / 2.f - (BUTTON_SIZE.x / 2.f), display_size.y - (40.f * SCALE.y)));
        const std::string slider = fmt::format("{:0>2d}/{:0>2d}", current_page + 1, total_pages);
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
            const auto previous_page = current_page;
            ImGui::SliderInt("##slider_current_manual", &current_page, 0, total_pages, slider.c_str());
            if (current_page != previous_page)
                change_page(emuenv, current_page);
            ImGui::PopStyleVar(2);
            ImGui::PopItemWidth();
            if (ImGui::IsItemDeactivated())
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    ImGui::SetCursorPos(ImVec2(display_size.x - BUTTON_POS.x - (BUTTON_SIZE.x * 2.f) - (36.f * SCALE.x), BUTTON_POS.y));
    if ((!hidden_button && ImGui::Button("Book", BUTTON_SIZE)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_right)))
        is_book_mode = !is_book_mode;

    // Draw right button
    if (current_page < max_pages_index) {
        ImGui::SetCursorPos(ImVec2(display_size.x - BUTTON_POS.x - BUTTON_SIZE.x, BUTTON_POS.y));
        if ((!hidden_button && ImGui::Button(">", BUTTON_SIZE)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_right)))
            change_page(emuenv, current_page + 1);
    }
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace gui
