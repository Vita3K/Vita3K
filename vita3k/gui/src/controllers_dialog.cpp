// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <config/functions.h>
#include <config/state.h>
#include <ctrl/state.h>
#include <emuenv/state.h>
#include <gui/functions.h>

#include <SDL_events.h>

namespace gui {

bool rebinds_is_open = false;
bool is_capturing_buttons = false;

void reset_controller_binding(EmuEnvState &emuenv) {
    emuenv.cfg.controller_binds = {
        SDL_CONTROLLER_BUTTON_A,
        SDL_CONTROLLER_BUTTON_B,
        SDL_CONTROLLER_BUTTON_X,
        SDL_CONTROLLER_BUTTON_Y,
        SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_GUIDE,
        SDL_CONTROLLER_BUTTON_START,
        SDL_CONTROLLER_BUTTON_LEFTSTICK,
        SDL_CONTROLLER_BUTTON_RIGHTSTICK,
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
        SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
        SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT
    };
    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
}

static void add_bind_to_table(GuiState &gui, EmuEnvState &emuenv, const SDL_GameControllerType type, const SDL_GameControllerButton btn) {
    auto &controls = gui.lang.controls;
    std::map<SDL_GameControllerButton, std::string> buttons_name = {
        { SDL_CONTROLLER_BUTTON_BACK, controls["select_button"].c_str() },
        { SDL_CONTROLLER_BUTTON_START, controls["start_button"].c_str() },
        { SDL_CONTROLLER_BUTTON_LEFTSTICK, controls["l3_button"].c_str() },
        { SDL_CONTROLLER_BUTTON_RIGHTSTICK, controls["r3_button"].c_str() },
        { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, controls["l1_button"].c_str() },
        { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, controls["r1_button"].c_str() },
        { SDL_CONTROLLER_BUTTON_A, controls["cross_button"].c_str() },
        { SDL_CONTROLLER_BUTTON_B, controls["circle_button"].c_str() },
        { SDL_CONTROLLER_BUTTON_X, controls["square_button"].c_str() },
        { SDL_CONTROLLER_BUTTON_Y, controls["triangle_button"].c_str() },
        { SDL_CONTROLLER_BUTTON_DPAD_UP, controls["d_pad_up"].c_str() },
        { SDL_CONTROLLER_BUTTON_DPAD_DOWN, controls["d_pad_down"].c_str() },
        { SDL_CONTROLLER_BUTTON_DPAD_LEFT, controls["d_pad_left"].c_str() },
        { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, controls["d_pad_right"].c_str() },
        { SDL_CONTROLLER_BUTTON_GUIDE, controls["ps_button"].c_str() },
    };

    const auto get_mapped_button_name = [type](const SDL_GameControllerButton btn) -> std::string {
        switch (btn) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return "D-Pad Up";
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "D-Pad Down";
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "D-Pad Left";
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "D-Pad Right";
        default: break;
        }

        switch (type) {
        case SDL_CONTROLLER_TYPE_XBOX360:
        case SDL_CONTROLLER_TYPE_XBOXONE:
            switch (btn) {
            case SDL_CONTROLLER_BUTTON_BACK: return "Back";
            case SDL_CONTROLLER_BUTTON_START: return "Start";
            case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "LS";
            case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "RS";
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "LB";
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "RB";
            case SDL_CONTROLLER_BUTTON_A: return "A";
            case SDL_CONTROLLER_BUTTON_B: return "B";
            case SDL_CONTROLLER_BUTTON_X: return "X";
            case SDL_CONTROLLER_BUTTON_Y: return "Y";
            case SDL_CONTROLLER_BUTTON_GUIDE: return "Guide";
            default: break;
            }
            break;
        case SDL_CONTROLLER_TYPE_PS3:
        case SDL_CONTROLLER_TYPE_PS4:
        case SDL_CONTROLLER_TYPE_PS5:
            switch (btn) {
            case SDL_CONTROLLER_BUTTON_BACK: return "Select";
            case SDL_CONTROLLER_BUTTON_START: return "Start";
            case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "L3";
            case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "R3";
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "L1";
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "R1";
            case SDL_CONTROLLER_BUTTON_A: return "Cross";
            case SDL_CONTROLLER_BUTTON_B: return "Circle";
            case SDL_CONTROLLER_BUTTON_X: return "Square";
            case SDL_CONTROLLER_BUTTON_Y: return "Triangle";
            case SDL_CONTROLLER_BUTTON_GUIDE: return "PS";
            default: break;
            }
            break;
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
            switch (btn) {
            case SDL_CONTROLLER_BUTTON_BACK: return "-";
            case SDL_CONTROLLER_BUTTON_START: return "+";
            case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "LS";
            case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "RS";
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "L";
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "R";
            case SDL_CONTROLLER_BUTTON_A: return "B";
            case SDL_CONTROLLER_BUTTON_B: return "A";
            case SDL_CONTROLLER_BUTTON_X: return "Y";
            case SDL_CONTROLLER_BUTTON_Y: return "X";
            case SDL_CONTROLLER_BUTTON_GUIDE: return "Home";
            default: break;
            }
            break;
        default: break;
        }

        return std::to_string(btn);
    };

    ImGui::PushID(btn);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", buttons_name[btn].c_str());
    ImGui::TableSetColumnIndex(1);
    if (ImGui::Button(get_mapped_button_name(SDL_GameControllerButton(emuenv.cfg.controller_binds[btn])).c_str())) {
        is_capturing_buttons = true;
        while (is_capturing_buttons) {
            // query SDL for button events
            SDL_Event get_controller_events;
            while (SDL_PollEvent(&get_controller_events)) {
                switch (get_controller_events.type) {
                case SDL_KEYDOWN:
                    is_capturing_buttons = false;
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                    emuenv.cfg.controller_binds[btn] = get_controller_events.cbutton.button;
                    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
                    is_capturing_buttons = false;
                    break;
                default: break;
                }
            }
        }
    }
    ImGui::PopID();
}

void draw_controllers_dialog(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_POS(emuenv.viewport_pos.x, emuenv.viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.viewport_size.x, emuenv.viewport_size.y);
    const ImVec2 RES_SCALE(VIEWPORT_SIZE.x / emuenv.res_width_dpi_scale, VIEWPORT_SIZE.y / emuenv.res_height_dpi_scale);

    auto &ctrl = emuenv.ctrl;
    auto &lang = gui.lang.controllers;

    const auto has_controllers = ctrl.controllers_num > 0;

    if (has_controllers)
        ImGui::SetNextWindowSize(ImVec2(VIEWPORT_SIZE.x / 2.5f, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + (VIEWPORT_SIZE.x / 2.f), VIEWPORT_POS.y + (VIEWPORT_SIZE.y / 2.f)), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin(lang["title"].c_str(), &gui.controls_menu.controllers_dialog, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(RES_SCALE.x);
    if (has_controllers) {
        const auto connected_str = fmt::format(fmt::runtime(lang["connected"].c_str()), ctrl.controllers_num);
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", connected_str.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::BeginTable("main", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("num");
            ImGui::TableSetupColumn("name");
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["num"].c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["name"].c_str());
            ImGui::Spacing();
            for (auto i = 0; i < ctrl.controllers_num; i++) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%-8d", i);
                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button(ctrl.controllers_name[i]))
                    rebinds_is_open = true;
                if (rebinds_is_open) {
                    const SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(i);
                    if (!ctrl.controllers.contains(guid)) {
                        rebinds_is_open = false;
                        continue;
                    }
                    ImGui::SetNextWindowSize(ImVec2(VIEWPORT_SIZE.x / 1.4f, 0.f), ImGuiCond_Always);
                    ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + (VIEWPORT_SIZE.x / 2.f), VIEWPORT_POS.y + (VIEWPORT_SIZE.y / 2.f)), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                    ImGui::Begin(lang["rebind_controls"].c_str(), &rebinds_is_open, ImGuiWindowFlags_NoSavedSettings);
                    ImGui::SetWindowFontScale(RES_SCALE.x);
                    auto &controls = gui.lang.controls;
                    const auto type = SDL_GameControllerTypeForIndex(i);
                    ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", ctrl.controllers_name[i]);
                    ImGui::Separator();
                    if (ImGui::BeginTable("rebindControl", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersInnerV)) {
                        ImGui::TableSetupColumn("leftButtons");
                        ImGui::TableSetupColumn("rightButtons");
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);

                        if (ImGui::BeginTable("rebindLeft", 2, ImGuiTableFlags_NoSavedSettings)) {
                            ImGui::TableSetupColumn("vitaControl");
                            ImGui::TableSetupColumn("controllerControl");
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", controls["button"].c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", controls["mapped_button"].c_str());

                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_DPAD_UP);
                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_BACK);
                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_GUIDE);

                            ImGui::EndTable();
                        }

                        ImGui::TableSetColumnIndex(1);
                        if (ImGui::BeginTable("rebindRight", 2, ImGuiTableFlags_NoSavedSettings)) {
                            ImGui::TableSetupColumn("vitaControl");
                            ImGui::TableSetupColumn("controllerControl");
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", controls["button"].c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", controls["mapped_button"].c_str());

                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_Y);
                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_A);
                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_X);
                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_B);
                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_START);

                            ImGui::EndTable();
                        }
                        ImGui::EndTable();
                    }

                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", controls["ps_tv_mode"].c_str());
                    if (ImGui::BeginTable("rebindExt", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersInnerV)) {
                        ImGui::TableSetupColumn("leftButtons");
                        ImGui::TableSetupColumn("rightButtons");
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        if (ImGui::BeginTable("PSTV_modeL", 2, ImGuiTableFlags_NoSavedSettings)) {
                            ImGui::TableSetupColumn("button");
                            ImGui::TableSetupColumn("mapped_button");
                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_LEFTSTICK);
                            ImGui::EndTable();
                        }
                        ImGui::TableSetColumnIndex(1);
                        if (ImGui::BeginTable("PSTV_modeR", 2, ImGuiTableFlags_NoSavedSettings)) {
                            ImGui::TableSetupColumn("button");
                            ImGui::TableSetupColumn("mapped_button");
                            add_bind_to_table(gui, emuenv, type, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
                            ImGui::EndTable();
                        }
                        ImGui::EndTable();
                    }

                    if (ctrl.controllers[guid].has_led) {
                        const auto set_led_color = [&](const std::vector<int> &led) {
                            SDL_GameControllerSetLED(ctrl.controllers[guid].controller.get(), led[0], led[1], led[2]);
                            config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
                        };
                        ImGui::Spacing();
                        const auto led_color_str = lang["led_color"].c_str();
                        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(led_color_str).x / 2.f));
                        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", led_color_str);
                        auto &color = emuenv.cfg.controller_led_color;
                        bool has_custom_color = !color.empty();
                        if (ImGui::Checkbox(lang["use_custom_color"].c_str(), &has_custom_color)) {
                            const std::vector<int> default_color = { 0, 0, 65 };
                            if (color.empty())
                                color = default_color;
                            else
                                color.clear();
                            set_led_color(default_color);
                        }
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s", lang["use_custom_color_description"].c_str());
                        if (has_custom_color) {
                            ImGui::Spacing();
                            if (ImGui::BeginTable("setColor", 3, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersInnerV)) {
                                ImGui::TableSetupColumn("red");
                                ImGui::TableSetupColumn("green");
                                ImGui::TableSetupColumn("blue");
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("%s", lang["red"].c_str());
                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text("%s", lang["green"].c_str());
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Text("%s", lang["blue"].c_str());
                                ImGui::TableNextRow();
                                const auto tab_size = (ImGui::GetWindowWidth() / 3.f) - ImGui::GetStyle().WindowPadding.x - ImGui::GetStyle().FramePadding.x;
                                for (auto l = 0; l < color.size(); l++) {
                                    ImGui::PushID(l);
                                    ImGui::TableSetColumnIndex(l);
                                    ImGui::PushItemWidth(tab_size);
                                    if (ImGui::SliderInt("##color", &color[l], 0, 255, std::to_string((color[l] * 100) / 255).append("%%").c_str()))
                                        set_led_color(color);
                                    ImGui::PopItemWidth();
                                    ImGui::PopID();
                                }
                                ImGui::EndTable();
                            }
                        }
                    }
                    ImGui::End();
                }
            }
            ImGui::EndTable();
        }
    } else
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", lang["not_connected"].c_str());

    if (emuenv.ctrl.has_motion_support) {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - (ImGui::GetStyle().WindowPadding.x * 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["motion_support"].c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::Spacing();
    if (ImGui::Button(lang["reset_controller_binding"].c_str()))
        reset_controller_binding(emuenv);

    ImGui::End();
}

} // namespace gui
