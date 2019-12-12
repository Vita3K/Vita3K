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

static int current_page;

bool init_manual(GuiState &gui, HostState &host) {
    current_page = 0;
    if (gui.manuals.find(host.io.title_id) == gui.manuals.end()) {
        std::vector<std::string> manual_page_list;
        const auto game_path{ fs::path(host.pref_path) / "ux0/app" / host.io.title_id };
        auto manual_path{ fs::path("sce_sys/manual/") };

        if (fs::exists(game_path / manual_path / fmt::format("{:0>2d}", host.cfg.sys_lang)))
            manual_path /= fmt::format("{:0>2d}/", host.cfg.sys_lang);

        if (fs::exists(game_path / manual_path) && !fs::is_empty(game_path / manual_path)) {
            fs::directory_iterator end;
            const std::string ext = ".png";
            for (fs::directory_iterator m(game_path / manual_path); m != end; ++m) {
                const fs::path page_manual = *m;
                if (m->path().extension() == ext) {
                    manual_page_list.push_back({ page_manual.filename().string() });
                    gui.manuals[host.io.title_id].push_back({});
                }
            }
        }

        for (auto p = 0; p < manual_page_list.size(); p++) {
            int32_t width = 0;
            int32_t height = 0;
            vfs::FileBuffer buffer;
            std::string app_manual_path = manual_path.string();

            app_manual_path += manual_page_list[p];

            vfs::read_app_file(buffer, host.pref_path, host.io.title_id, app_manual_path);

            if (buffer.empty()) {
                LOG_WARN("Manual not found for title: {} [{}].", host.io.title_id, host.game_title);
                return false;
            }
            stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
            if (!data) {
                LOG_ERROR("Invalid manual image for title: {} [{}].", host.io.title_id, host.game_title);
                return false;
            }

            gui.manuals[host.io.title_id][p].init(gui.imgui_state.get(), data, width, height);
            stbi_image_free(data);
        }
    }
    return gui.manuals.find(host.io.title_id) != gui.manuals.end();
}

static auto hiden_button = false;

void draw_manual_dialog(GuiState &gui, HostState &host) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(-5.0f, -5.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_size.x + 10.0f, display_size.y + 10.0f), ImGuiCond_Always);
    ImGui::Begin("Manual", &gui.live_area.manual_dialog, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    const auto scal = ImVec2(display_size.x / 960.0f, display_size.y / 544.0f);

    if (gui.manuals.find(host.io.title_id) != gui.manuals.end())
        ImGui::GetWindowDrawList()->AddImage(gui.manuals[host.io.title_id][current_page], ImVec2(0, 0), display_size);

    if (ImGui::IsMouseDoubleClicked(0)) {
        if (!hiden_button)
            hiden_button = true;
        else
            hiden_button = false;
    }

    const auto BUTTON_SIZE = ImVec2(80.f * scal.x, 30.f * scal.y);

    ImGui::SetCursorPos(ImVec2(display_size.x - (80.0f * scal.x), 10.0f * scal.y));
    if (!hiden_button && ImGui::Button("X", BUTTON_SIZE) || ImGui::IsKeyPressed(SDL_SCANCODE_H))
        gui.live_area.manual_dialog = false;

    const auto wheel_counter = ImGui::GetIO().MouseWheel;
    if (current_page > 0) {
        ImGui::SetCursorPos(ImVec2(10.0f * scal.x, display_size.y - (40.0f * scal.y)));
        if ((!hiden_button && ImGui::Button("<", BUTTON_SIZE)) || ImGui::IsKeyPressed(SDL_SCANCODE_LEFT) || (wheel_counter == 1))
            --current_page;
    }
    if (!hiden_button) {
        ImGui::SameLine();
        ImGui::SetCursorPos(ImVec2(display_size.x / 2 - (40.0f * scal.x), display_size.y - (40.0f * scal.y)));
        const std::string slider = fmt::format("{}/{}", current_page + 1, (int)gui.manuals[host.io.title_id].size());
        if (ImGui::Button(slider.c_str(), BUTTON_SIZE))
            ImGui::OpenPopup("Manual Slider");
        ImGui::SetNextWindowPos(ImVec2(-5.0f, display_size.y - 50.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(display_size.x + 10.0f, 40.0f), ImGuiCond_Always);
        if (ImGui::BeginPopupModal("Manual Slider", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::PushItemWidth(display_size.x - 10.0f);
            ImGui::SliderInt("##slider_current_manual", &current_page, 0, (int)gui.manuals[host.io.title_id].size() - 1);
            ImGui::PopItemWidth();
            if (ImGui::IsItemDeactivated())
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    if (current_page < (int)gui.manuals[host.io.title_id].size() - 1) {
        ImGui::SameLine();
        ImGui::SetCursorPos(ImVec2(display_size.x - (80.0f * scal.x), display_size.y - (40.0f * scal.y)));
        if ((!hiden_button && ImGui::Button(">", BUTTON_SIZE)) || ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT) || (wheel_counter == -1))
            ++current_page;
    }

    ImGui::End();
}

} // namespace gui
