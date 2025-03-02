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
// 51 Franklin Street, Fifth Floor, 1Boston, MA 02110-1301 USA.

#include "private.h"

#include <dialog/state.h>
#include <gui/functions.h>
#include <host/dialog/filesystem.h>
#include <misc/cpp/imgui_stdlib.h>
#include <packages/license.h>
#include <packages/sce_types.h>
#include <util/log.h>

namespace gui {

void draw_self_decrypt_dialog(GuiState &gui, EmuEnvState &emuenv) {
    host::dialog::filesystem::Result result = host::dialog::filesystem::Result::CANCEL;
    static std::filesystem::path self_path = "";
    static std::filesystem::path license_path = "";
    static std::string title;
    static bool draw_file_dialog = true;

    enum class State {
        UNDEFINED,
        DECRYPT,
        NO_ENCRYPTED,
        SUCCESS,
        FAIL
    };

    static State state = State::UNDEFINED;
    static std::vector<uint8_t> fself{};
    static SceNpDrmLicense license{};
    static std::string self_file{};
    static fs::path decrypted_path{};

    if (draw_file_dialog) {
        result = host::dialog::filesystem::open_file(self_path, { { "Self file", { "bin", "self", "suprx" } } });
        draw_file_dialog = false;
        if (result == host::dialog::filesystem::Result::SUCCESS) {
            fs::ifstream infile(self_path.native(), std::ios::binary);
            self_file = fs::path(self_path).filename().string();
            fself = std::vector<uint8_t>(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>());
            if (is_fself_encrypted(fself)) {
                if (is_fself_app(fself)) {
                    result = host::dialog::filesystem::open_file(license_path, { { "PlayStation Vita software license file", { "bin", "rif" } } });
                    if (result == host::dialog::filesystem::Result::SUCCESS) {
                        if (open_license(license_path.native(), license))
                            state = State::DECRYPT;
                        else {
                            LOG_ERROR("Error opening license file: {}", license_path.string());
                            state = State::FAIL;
                        }
                    } else {
                        if (result == host::dialog::filesystem::Result::ERROR)
                            LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
                        state = State::UNDEFINED;
                    }
                } else
                    state = State::DECRYPT;
            } else {
                state = State::NO_ENCRYPTED;
                fself.clear();
            }
            if (result == host::dialog::filesystem::Result::SUCCESS)
                ImGui::OpenPopup("decrypt");
        } else if (result == host::dialog::filesystem::Result::CANCEL) {
            gui.file_menu.self_decrypt_dialog = false;
            draw_file_dialog = true;
        } else {
            LOG_ERROR("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
            gui.file_menu.self_decrypt_dialog = false;
            draw_file_dialog = true;
        }
    }

    auto &indicator = gui.lang.indicator;
    auto &common = emuenv.common_dialog.lang.common;

    const ImVec2 display_size(emuenv.viewport_size.x, emuenv.viewport_size.y);
    const ImVec2 RES_SCALE(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);
    const ImVec2 WINDOW_SIZE(616.f * SCALE.x, 264.f * SCALE.y);
    const ImVec2 BUTTON_SIZE(180.f * SCALE.x, 45.f * SCALE.y);

    ImGui::SetNextWindowPos(ImVec2(emuenv.viewport_pos.x + (display_size.x / 2.f) - (WINDOW_SIZE.x / 2), emuenv.viewport_pos.y + (display_size.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE);
    if (ImGui::BeginPopupModal("decrypt", &gui.file_menu.self_decrypt_dialog, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration)) {
        ImGui::SetWindowFontScale(RES_SCALE.x);
        const auto POS_BUTTON = (WINDOW_SIZE.x / 2.f) - (BUTTON_SIZE.x / 2.f) + (10.f * SCALE.x);
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, title.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        switch (state) {
        case State::DECRYPT: {
            fself = decrypt_fself(std::move(fself), license.key);
            if (!fself.empty()) {
                decrypted_path = emuenv.cache_path / "decrypted_self" / self_file;
                fs::create_directories(decrypted_path.stem());
                fs::ofstream outfile(decrypted_path, std::ios::binary);
                outfile.write(reinterpret_cast<const char *>(fself.data()), fself.size());
                outfile.close();
                state = State::SUCCESS;
            } else {
                state = State::FAIL;
            }
            fself.clear();
            license = {};
            break;
        }
        case State::NO_ENCRYPTED: {
            title = "No Encrypted";
            ImGui::TextColored(GUI_COLOR_TEXT, "This Self '%s' is not encrypted!", self_file.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
            if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
                self_file.clear();
                self_path = "";
                license_path = "";
                gui.file_menu.self_decrypt_dialog = false;
                draw_file_dialog = true;
                state = State::UNDEFINED;
            }
            break;
        }
        case State::SUCCESS: {
            title = "Decrypt Complete";
            ImGui::TextColored(GUI_COLOR_TEXT, "Succes Decrypt: %s", self_file.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextWrapped("Decrypted in: %s", decrypted_path.parent_path().string().c_str());
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
            if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
                decrypted_path.clear();
                self_file.clear();
                self_path = "";
                license_path = "";
                gui.file_menu.self_decrypt_dialog = false;
                draw_file_dialog = true;
                state = State::UNDEFINED;
            }
            break;
        }
        case State::FAIL: {
            title = "Decrypt Failed";
            ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) - (20.f * SCALE.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "Failed Decrypt: %s", self_file.c_str());
            ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
            if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
                gui.file_menu.self_decrypt_dialog = false;
                decrypted_path.clear();
                self_file.clear();
                self_path = "";
                draw_file_dialog = true;
                license_path = "";
                state = State::UNDEFINED;
            }
            break;
        }
        }
        ImGui::EndPopup();
    }
}
} // namespace gui
