// Vita3K emulator project
// Copyright (C) 2020 Vita3K team
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

#include <io/vfs.h>

#include <util/log.h>

#include <SDL_keycode.h>

#include <stb_image.h>

namespace gui {

static int32_t current_page;
static std::map<std::string, std::map<std::string, ImVec2>> size_page;
static std::map<std::string, std::pair<bool, bool>> zoom;

bool init_manual(GuiState &gui, HostState &host) {
    current_page = 0;
    const auto title_id = gui.apps_list_opened[gui.current_app_selected];
    if (gui.manuals.find(title_id) == gui.manuals.end()) {
        std::vector<std::string> manual_page_list;
        const auto app_path{ fs::path(host.pref_path) / "ux0/app" / title_id };
        auto manual_path{ fs::path("sce_sys/manual/") };

        if (fs::exists(app_path / manual_path / fmt::format("{:0>2d}", host.cfg.sys_lang)))
            manual_path /= fmt::format("{:0>2d}/", host.cfg.sys_lang);

        if (fs::exists(app_path / manual_path) && !fs::is_empty(app_path / manual_path)) {
            for (const auto &manual : fs::directory_iterator(app_path / manual_path)) {
                if (manual.path().extension() == ".png") {
                    manual_page_list.push_back({ manual.path().filename().string() });
                    gui.manuals[title_id].push_back({});
                }
            }
        }

        int32_t width, height;
        for (auto p = 0; p < manual_page_list.size(); p++) {
            vfs::FileBuffer buffer;
            std::string app_manual_path = manual_path.string();

            app_manual_path += manual_page_list[p];

            vfs::read_app_file(buffer, host.pref_path, title_id, app_manual_path);

            if (buffer.empty()) {
                LOG_WARN("Manual not found for title: {} [{}].", title_id, host.app_title);
                return false;
            }
            stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
            if (!data) {
                LOG_ERROR("Invalid manual image for title: {} [{}].", title_id, host.app_title);
                return false;
            }
            gui.manuals[title_id][p].init(gui.imgui_state.get(), data, width, height);
            stbi_image_free(data);
        }
        size_page[title_id]["mini"] = size_page[title_id]["current"] = height < width ? ImVec2(float(width), float(height)) : ImVec2(float(width * (width / 960.f)), float(height / (height / 544.f)));
        size_page[title_id]["max"] = ImVec2(float(width), float(height));
        if (height > width)
            zoom[title_id].first = true;
    }
    return gui.manuals.find(title_id) != gui.manuals.end();
}

static auto hiden_button = false;

void draw_manual(GuiState &gui, HostState &host) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(-5.f, -1.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_size.x + 10.f, display_size.y + 2.f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.999f);
    ImGui::Begin("##manual", &gui.live_area.manual, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    const auto title_id = gui.apps_list_opened[gui.current_app_selected];
    if (zoom[title_id].first && ImGui::IsMouseDoubleClicked(0)) {
        zoom[title_id].second ? size_page[title_id]["current"] = size_page[title_id]["mini"] : size_page[title_id]["current"] = size_page[title_id]["max"];
        zoom[title_id].second = !zoom[title_id].second;
    }
    const auto scal = ImVec2(display_size.x / 960.0f, display_size.y / 544.0f);
    const auto size_child = ImVec2(size_page[title_id]["current"].x * scal.x, size_page[title_id]["mini"].y * scal.y);
    ImGui::BeginChild("##manual_child", size_child, false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | (zoom[title_id].second ? ImGuiWindowFlags_AlwaysVerticalScrollbar : ImGuiWindowFlags_NoScrollbar));

    if (gui.manuals.find(title_id) != gui.manuals.end())
        ImGui::Image(gui.manuals[title_id][current_page], ImVec2(size_page[title_id]["current"].x * scal.x, size_page[title_id]["current"].y * scal.y));

    if (ImGui::IsMouseClicked(1))
        hiden_button = !hiden_button;

    const auto BUTTON_SIZE = ImVec2(65.f * scal.x, 30.f * scal.y);

    ImGui::SetCursorPos(ImVec2(size_child.x - ((!zoom[title_id].second ? 70.0f : 85.f) * scal.x), 10.0f * scal.y));
    if (!hiden_button && ImGui::Button("Esc", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_psbutton)) {
        gui.live_area.manual = false;
        gui.live_area.information_bar = true;
        gui.live_area.live_area_screen = true;
    }

    const auto wheel_counter = ImGui::GetIO().MouseWheel;
    if (current_page > 0) {
        ImGui::SetCursorPos(ImVec2(5.0f * scal.x, size_child.y - (40.0f * scal.y)));
        if ((!hiden_button && !zoom[title_id].second && ImGui::Button("<", BUTTON_SIZE)) || ImGui::IsKeyPressed(host.cfg.keyboard_leftstick_left) || ImGui::IsKeyPressed(host.cfg.keyboard_button_left) || (!zoom[title_id].second && wheel_counter == 1))
            --current_page;
    }
    if (!hiden_button && !zoom[title_id].second) {
        ImGui::SetCursorPos(ImVec2(size_child.x / 2.f - ((BUTTON_SIZE.x / 2.f) * scal.x), display_size.y - (40.f * scal.y)));
        const std::string slider = fmt::format("{:0>2d}/{:0>2d}", current_page + 1, (int32_t)gui.manuals[title_id].size());
        if (ImGui::Button(slider.c_str(), BUTTON_SIZE))
            ImGui::OpenPopup("Manual Slider");
        ImGui::SetNextWindowPos(ImVec2(-5.0f, size_child.y - 50.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(display_size.x + 10.0f, 40.0f), ImGuiCond_Always);
        if (ImGui::BeginPopupModal("Manual Slider", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::PushItemWidth(display_size.x - 10.0f);
            ImGui::SliderInt("##slider_current_manual", &current_page, 0, (int32_t)gui.manuals[title_id].size() - 1, slider.c_str());
            ImGui::PopItemWidth();
            if (ImGui::IsItemDeactivated())
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    if (current_page < (int)gui.manuals[title_id].size() - 1) {
        ImGui::SetCursorPos(ImVec2(size_child.x - (70.f * scal.x), display_size.y - (40.0f * scal.y)));
        if ((!hiden_button && !zoom[title_id].second && ImGui::Button(">", BUTTON_SIZE)) || ImGui::IsKeyPressed(host.cfg.keyboard_leftstick_right) || ImGui::IsKeyPressed(host.cfg.keyboard_button_right) || (!zoom[title_id].second && (wheel_counter == -1)))
            ++current_page;
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace gui
