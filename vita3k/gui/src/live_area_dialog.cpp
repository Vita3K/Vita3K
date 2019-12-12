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

#include <gui/functions.h>

#include <io/device.h>
#include <io/vfs.h>
#include <util/log.h>

#include <pugixml.hpp>

#include <SDL_keycode.h>

#include <chrono>
#include <sstream>
#include <stb_image.h>

namespace gui {

#ifdef _WIN32
static const char OS_PREFIX[] = "start ";
#elif __APPLE__
static const char OS_PREFIX[] = "open ";
#else
static const char OS_PREFIX[] = "xdg-open ";
#endif

enum LiveAreaContentsZone {
    LIVEAREA_BACKGROUND,
    FRAME1,
    FRAME2,
    FRAME3,
    FRAME4,
    FRAME5,
    FRAME6,
    FRAME7,
    BACKGROUND,
    IMAGE,
    GATE,
    SEARCH,
    MANUAL /*,
    BUTTON,
    REFRESH*/
};

static std::map<std::string, std::map<int, std::pair<std::vector<std::string>, std::pair<std::uint32_t, std::string>>>> str;
static std::map<std::string, std::map<int, std::pair<ImVec2, std::pair<std::string, std::string>>>> str_pos;
static std::map<std::string, std::map<int, std::map<int, std::pair<ImVec2, std::pair<ImVec2, std::pair<std::string, std::string>>>>>> items;
static std::map<std::string, std::map<int, std::string>> target;
static std::map<std::string, std::map<int, uint64_t>> current_item, last_time, frame_autoflip;
static std::map<std::string, std::string> type;
static std::size_t current_game;

void init_live_area(GuiState &gui, HostState &host) {
    if (gui.live_area_contents.find(host.io.title_id) == gui.live_area_contents.end()) {
        auto default_contents = false;
        const auto fw_path{ fs::path(host.pref_path) / "vs0" };
        const auto default_fw_contents{ fw_path / "data/internal/livearea/default/sce_sys/livearea/contents/template.xml" };
        const auto game_contents{ fs::path(host.pref_path) / "ux0/app" / host.io.title_id / "sce_sys/livearea/contents/template.xml" };
        auto template_xml = game_contents;

        pugi::xml_document doc;

        if (!doc.load_file(game_contents.c_str())) {
            LOG_WARN("Live Area Contents is corrupted or missing for title: {} '{}'.", host.io.title_id, host.game_title);
            if (doc.load_file(default_fw_contents.c_str())) {
                template_xml = default_fw_contents;
                default_contents = true;
                LOG_INFO("Using default firmware contents.");
            } else {
                LOG_ERROR("Default firmware contents is corrupted or missing.");
                return;
            }
        }

        if (doc.load_file(template_xml.c_str())) {
            std::map<int, std::string> name;

            type[host.io.title_id].clear();
            type[host.io.title_id] = doc.child("livearea").attribute("style").as_string();

            std::string user_lang;
            const auto sys_lang = static_cast<SceSystemParamLang>(host.cfg.sys_lang);
            switch (sys_lang) {
            case SCE_SYSTEM_PARAM_LANG_JAPANESE: user_lang = "ja"; break;
            case SCE_SYSTEM_PARAM_LANG_ENGLISH_US: user_lang = "en"; break;
            case SCE_SYSTEM_PARAM_LANG_FRENCH: user_lang = "fr"; break;
            case SCE_SYSTEM_PARAM_LANG_SPANISH: user_lang = "es"; break;
            case SCE_SYSTEM_PARAM_LANG_GERMAN: user_lang = "de"; break;
            case SCE_SYSTEM_PARAM_LANG_ITALIAN: user_lang = "it"; break;
            case SCE_SYSTEM_PARAM_LANG_DUTCH: user_lang = "nl"; break;
            case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT: user_lang = "pt"; break;
            case SCE_SYSTEM_PARAM_LANG_RUSSIAN: user_lang = "ru"; break;
            case SCE_SYSTEM_PARAM_LANG_KOREAN: user_lang = "ko"; break;
            case SCE_SYSTEM_PARAM_LANG_CHINESE_T: user_lang = "ch"; break;
            case SCE_SYSTEM_PARAM_LANG_CHINESE_S: user_lang = "zh"; break;
            case SCE_SYSTEM_PARAM_LANG_FINNISH: user_lang = "fi"; break;
            case SCE_SYSTEM_PARAM_LANG_SWEDISH: user_lang = "sv"; break;
            case SCE_SYSTEM_PARAM_LANG_DANISH: user_lang = "da"; break;
            case SCE_SYSTEM_PARAM_LANG_NORWEGIAN: user_lang = "no"; break;
            case SCE_SYSTEM_PARAM_LANG_POLISH: user_lang = "pl"; break;
            case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR: user_lang = "pt-br"; break;
            case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB: user_lang = "en-gb"; break;
            case SCE_SYSTEM_PARAM_LANG_TURKISH: user_lang = "tr"; break;
            default: break;
            }

            if (doc.child("livearea").child("livearea-background").child("image").child("lang")) {
                for (const auto &livearea_background : doc.child("livearea").child("livearea-background")) {
                    if (livearea_background.child("lang").text().as_string() == user_lang) {
                        name[LIVEAREA_BACKGROUND] = livearea_background.text().as_string();
                        break;
                    } else if (livearea_background.attribute("default").as_string() == std::string("on")) {
                        name[LIVEAREA_BACKGROUND] = livearea_background.text().as_string();
                        break;
                    }
                }
            } else
                name[LIVEAREA_BACKGROUND] = doc.child("livearea").child("livearea-background").child("image").text().as_string();

            if (name[LIVEAREA_BACKGROUND].empty())
                name[LIVEAREA_BACKGROUND] = doc.child("livearea").child("livearea-background").child("image").text().as_string();

            for (const auto &gate : doc.child("livearea").child("gate")) {
                if ((strncmp(gate.name(), "startup-image", 13) == 0) && (gate.child("lang").text().as_string() == user_lang)) {
                    name[GATE] = gate.text().as_string();
                    break;
                } else if ((strncmp(gate.name(), "startup-image", 13) == 0) && (gate.child("exclude-lang"))) {
                    auto exclude_lang = false;
                    for (const auto &exclude : gate)
                        if ((strncmp(exclude.name(), "exclude-lang", 12) == 0) && (exclude.text().as_string() == user_lang)) {
                            exclude_lang = true;
                            break;
                        }
                    if (!exclude_lang)
                        name[GATE] = gate.text().as_string();
                } else if (!gate.child("lang") || (gate.attribute("default").as_string() == std::string("on"))) {
                    name[GATE] = gate.text().as_string();
                    break;
                }
            }

            if (fs::exists(fw_path / "vsh/shell/6df5c7c2.png"))
                name[SEARCH] = "6df5c7c2.png";
            if (fs::exists(fw_path / "vsh/shell/2f88f589.png"))
                name[MANUAL] = "2f88f589.png";
            /*if (fs::exists(fw_path / "vsh/shell/b4a6dc.png"))
                name[button] = "b4a6dc.png";
            if (fs::exists(fw_path / "vsh/shell/4d4d9ca0.png"))
                name[refresh] = "4d4d9ca0.png";*/

            if (name[LIVEAREA_BACKGROUND].find('\n') != std::string::npos)
                name[LIVEAREA_BACKGROUND].erase(remove(name[LIVEAREA_BACKGROUND].begin(), name[LIVEAREA_BACKGROUND].end(), '\n'), name[LIVEAREA_BACKGROUND].end());
            name[LIVEAREA_BACKGROUND].erase(remove_if(name[LIVEAREA_BACKGROUND].begin(), name[LIVEAREA_BACKGROUND].end(), isspace), name[LIVEAREA_BACKGROUND].end());

            if (name[GATE].find('\n') != std::string::npos)
                name[GATE].erase(remove(name[GATE].begin(), name[GATE].end(), '\n'), name[GATE].end());
            name[GATE].erase(remove_if(name[GATE].begin(), name[GATE].end(), isspace), name[GATE].end());

            for (const auto &contents : name) {
                int32_t width = 0;
                int32_t height = 0;
                vfs::FileBuffer buffer;

                if (default_contents)
                    vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, "data/internal/livearea/default/sce_sys/livearea/contents/" + contents.second);

                if ((contents.first == SEARCH) || (contents.first == MANUAL)) //|| (contents.first == button) || (contents.first == refresh))
                    vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, "vsh/shell/" + contents.second);
                else
                    vfs::read_app_file(buffer, host.pref_path, host.io.title_id, "sce_sys/livearea/contents/" + contents.second);

                if (buffer.empty() && ((contents.first != SEARCH) && (contents.first != MANUAL))) {
                    LOG_WARN("Contents {} '{}' Not found for title {} [{}].", contents.first, contents.second, host.io.title_id, host.game_title);
                    continue;
                }
                stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                if (!data) {
                    LOG_ERROR("Invalid Live Area Contents for title {}.", host.io.title_id);
                    continue;
                }

                gui.live_area_contents[host.io.title_id][contents.first].init(gui.imgui_state.get(), data, width, height);
                stbi_image_free(data);
            }

            std::map<int, std::map<int, std::vector<std::string>>> frame_name;

            frame_autoflip[host.io.title_id].clear();
            items[host.io.title_id].clear();
            str[host.io.title_id].clear();
            str_pos[host.io.title_id].clear();
            target[host.io.title_id].clear();

            for (const auto &livearea : doc.child("livearea")) {
                auto frame = 0;
                if (livearea.attribute("id").as_string() == std::string("frame1"))
                    frame = FRAME1;
                else if (livearea.attribute("id").as_string() == std::string("frame2"))
                    frame = FRAME2;
                else if (livearea.attribute("id").as_string() == std::string("frame3"))
                    frame = FRAME3;
                else if (livearea.attribute("id").as_string() == std::string("frame4"))
                    frame = FRAME4;
                else if (livearea.attribute("id").as_string() == std::string("frame5"))
                    frame = FRAME5;
                else if (livearea.attribute("id").as_string() == std::string("frame6"))
                    frame = FRAME6;
                else if (livearea.attribute("id").as_string() == std::string("frame7"))
                    frame = FRAME7;

                if (frame != 0) {
                    frame_autoflip[host.io.title_id][frame] = livearea.attribute("autoflip").as_uint();

                    if (livearea.child("liveitem").child("background")) {
                        items[host.io.title_id][frame][BACKGROUND].second.first.x = livearea.child("liveitem").child("background").attribute("x").as_float();
                        items[host.io.title_id][frame][BACKGROUND].second.first.y = livearea.child("liveitem").child("background").attribute("y").as_float();
                        items[host.io.title_id][frame][BACKGROUND].second.second.first = livearea.child("liveitem").child("background").attribute("align").as_string();
                        items[host.io.title_id][frame][BACKGROUND].second.second.second = livearea.child("liveitem").child("background").attribute("valign").as_string();
                    }
                    if (livearea.child("liveitem").child("image")) {
                        items[host.io.title_id][frame][IMAGE].second.first.x = livearea.child("liveitem").child("image").attribute("x").as_float();
                        items[host.io.title_id][frame][IMAGE].second.first.y = livearea.child("liveitem").child("image").attribute("y").as_float();
                        items[host.io.title_id][frame][IMAGE].second.second.first = livearea.child("liveitem").child("image").attribute("align").as_string();
                        items[host.io.title_id][frame][IMAGE].second.second.second = livearea.child("liveitem").child("image").attribute("valign").as_string();
                    }
                    if (livearea.child("liveitem").child("text")) {
                        str_pos[host.io.title_id][frame].first.x = livearea.child("liveitem").child("text").attribute("x").as_float();
                        str_pos[host.io.title_id][frame].first.y = livearea.child("liveitem").child("text").attribute("y").as_float();
                        str_pos[host.io.title_id][frame].second.first = livearea.child("liveitem").child("text").attribute("align").as_string();
                        str_pos[host.io.title_id][frame].second.second = livearea.child("liveitem").child("text").attribute("valign").as_string();
                        str[host.io.title_id][frame].second.first = livearea.child("liveitem").child("text").child("str").attribute("size").as_uint();
                        str[host.io.title_id][frame].second.second = livearea.child("liveitem").child("text").child("str").attribute("color").as_string();
                    }

                    for (const auto &frame_id : livearea) {
                        bool exclude_lang = false;
                        if (frame_id.child("exclude-lang")) {
                            for (const auto &liveitem : frame_id)
                                if ((strncmp(liveitem.name(), "exclude-lang", 12) == 0) && (liveitem.text().as_string() == user_lang)) {
                                    exclude_lang = true;
                                    break;
                                }
                            if (!exclude_lang) {
                                if (frame_id.child("background"))
                                    frame_name[frame][BACKGROUND].push_back({ frame_id.child("background").text().as_string() });
                                if (frame_id.child("image"))
                                    frame_name[frame][IMAGE].push_back({ frame_id.child("image").text().as_string() });
                                if (frame_id.child("target"))
                                    target[host.io.title_id][frame] = frame_id.child("target").text().as_string();
                                if (frame_id.child("text").child("str"))
                                    str[host.io.title_id][frame].first.push_back({ frame_id.child("text").child("str").text().as_string() });
                            }
                        } else if ((strncmp(frame_id.name(), "liveitem", 8) == 0) && (frame_id.child("lang").text().as_string() == user_lang)) {
                            if (frame_id.child("background"))
                                frame_name[frame][BACKGROUND].push_back({ frame_id.child("background").text().as_string() });
                            if (frame_id.child("image"))
                                frame_name[frame][IMAGE].push_back({ frame_id.child("image").text().as_string() });
                            if (frame_id.child("target"))
                                target[host.io.title_id][frame] = frame_id.child("target").text().as_string();
                            if (frame_id.child("text").child("str"))
                                str[host.io.title_id][frame].first.push_back({ frame_id.child("text").child("str").text().as_string() });
                        } else if (!frame_id.child("exclude-lang") && !frame_id.child("lang")) {
                            if (frame_id.child("background"))
                                frame_name[frame][BACKGROUND].push_back({ frame_id.child("background").text().as_string() });
                            if (frame_id.child("image"))
                                frame_name[frame][IMAGE].push_back({ frame_id.child("image").text().as_string() });
                            if (frame_id.child("target"))
                                target[host.io.title_id][frame] = frame_id.child("target").text().as_string();
                            if (frame_id.child("text").child("str"))
                                str[host.io.title_id][frame].first.push_back({ frame_id.child("text").child("str").text().as_string() });
                        }
                    }
                    if (target[host.io.title_id][frame].find('\n') != std::string::npos)
                        target[host.io.title_id][frame].erase(remove(target[host.io.title_id][frame].begin(), target[host.io.title_id][frame].end(), '\n'), target[host.io.title_id][frame].end());
                    target[host.io.title_id][frame].erase(remove_if(target[host.io.title_id][frame].begin(), target[host.io.title_id][frame].end(), isspace), target[host.io.title_id][frame].end());
                }
            }

            for (auto &frame : frame_name) {
                current_item[host.io.title_id][frame.first] = 0;
                if (!frame.second[BACKGROUND].empty()) {
                    for (auto &bg_name : frame.second[BACKGROUND])
                        gui.live_items[host.io.title_id][frame.first][BACKGROUND].push_back({});
                }
                if (!frame.second[IMAGE].empty()) {
                    for (auto &img_name : frame.second[IMAGE])
                        gui.live_items[host.io.title_id][frame.first][IMAGE].push_back({});
                }
            }

            auto pos = 0;
            std::map<int, int> current_frame;
            for (auto &frame : frame_name) {
                if (!frame.second.empty()) {
                    for (auto &bg_name : frame.second[BACKGROUND]) {
                        if (!bg_name.empty()) {
                            if (current_frame[BACKGROUND] != frame.first) {
                                current_frame[BACKGROUND] = frame.first;
                                pos = 0;
                            } else
                                ++pos;

                            if (bg_name.find('\n') != std::string::npos)
                                bg_name.erase(remove(bg_name.begin(), bg_name.end(), '\n'), bg_name.end());
                            bg_name.erase(remove_if(bg_name.begin(), bg_name.end(), isspace), bg_name.end());

                            int32_t width = 0;
                            int32_t height = 0;
                            vfs::FileBuffer buffer;

                            vfs::read_app_file(buffer, host.pref_path, host.io.title_id, "sce_sys/livearea/contents/" + bg_name);

                            if (buffer.empty()) {
                                LOG_WARN("Frame: {}, Name: '{}', Not found for title: {} [{}].", frame.first, bg_name, host.io.title_id, host.game_title);
                                continue;
                            }
                            stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                            if (!data) {
                                LOG_ERROR("Frame: {}, Invalid Live Area Contents for title: {} [{}].", frame.first, host.io.title_id, host.game_title);
                                continue;
                            }

                            items[host.io.title_id][frame.first][BACKGROUND].first = ImVec2(float(width), float(height));
                            gui.live_items[host.io.title_id][frame.first][BACKGROUND][pos].init(gui.imgui_state.get(), data, width, height);
                            stbi_image_free(data);
                        }
                    }

                    for (auto &img_name : frame.second[IMAGE]) {
                        if (!img_name.empty()) {
                            if (current_frame[IMAGE] != frame.first) {
                                current_frame[IMAGE] = frame.first;
                                pos = 0;
                            } else
                                ++pos;

                            if (img_name.find('\n') != std::string::npos)
                                img_name.erase(remove(img_name.begin(), img_name.end(), '\n'), img_name.end());
                            img_name.erase(remove_if(img_name.begin(), img_name.end(), isspace), img_name.end());

                            int32_t width = 0;
                            int32_t height = 0;
                            vfs::FileBuffer buffer;

                            vfs::read_app_file(buffer, host.pref_path, host.io.title_id, "sce_sys/livearea/contents/" + img_name);

                            if (buffer.empty()) {
                                LOG_WARN("Frame: {} Name: '{}', Not found for title {} [{}].", frame.first, img_name, host.io.title_id, host.game_title);
                                continue;
                            }
                            stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                            if (!data) {
                                LOG_ERROR("Frame: {}, Invalid Live Area Contents for title: {} [{}].", frame.first, host.io.title_id, host.game_title);
                                continue;
                            }

                            items[host.io.title_id][frame.first][IMAGE].first = ImVec2(float(width), float(height));
                            gui.live_items[host.io.title_id][frame.first][IMAGE][pos].init(gui.imgui_state.get(), data, width, height);
                            stbi_image_free(data);
                        }
                    }
                }
            }
        }
    }
    for (const auto &game : gui.game_selector.games) {
        const auto games_index = std::find_if(gui.game_selector.games.begin(), gui.game_selector.games.end(), [&](const Game &g) {
            return g.title_id == host.io.title_id;
        });
        current_game = std::distance(gui.game_selector.games.begin(), games_index);
    }
}

inline uint64_t current_time() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

void draw_live_area_dialog(GuiState &gui, HostState &host) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::Begin("Live Area", &gui.live_area.live_area_dialog, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

    const auto scal = ImVec2(display_size.x / 960.0f, display_size.y / 544.0f);

    const auto background_pos = ImVec2(900.0f * scal.x, 500.0f * scal.y);
    const auto pos_bg = ImVec2(display_size.x - background_pos.x, display_size.y - background_pos.y);
    const auto background_size = ImVec2(840.0f * scal.x, 500.0f * scal.y);

    if (gui.live_area_contents[host.io.title_id].find(LIVEAREA_BACKGROUND) != gui.live_area_contents[host.io.title_id].end())
        ImGui::GetWindowDrawList()->AddImage(gui.live_area_contents[host.io.title_id][LIVEAREA_BACKGROUND],
            pos_bg, ImVec2(pos_bg.x + background_size.x, pos_bg.y + background_size.y));

    ImVec2 GATE_POS, FRAME1_POS, FRAME1_SIZE, FRAME2_POS, FRAME2_SIZE, FRAME3_POS, FRAME3_SIZE, FRAME4_POS, FRAME4_SIZE, FRAME5_POS, FRAME5_SIZE;
    if (type[host.io.title_id] == "a1") {
        GATE_POS = ImVec2(620.0f * scal.x, 364.0f * scal.y);
        FRAME1_POS = ImVec2(900.0f * scal.x, 415.0f * scal.y);
        FRAME1_SIZE = ImVec2(260.0f, 260.0f);
        FRAME2_POS = ImVec2(320.0f * scal.x, 415.0f * scal.y);
        FRAME2_SIZE = ImVec2(260.0f, 260.0f);
        FRAME3_POS = ImVec2(900.0f * scal.x, 155.0f * scal.y);
        FRAME3_SIZE = ImVec2(840.0f, 150.0f);
    } else if (type[host.io.title_id] == "a2") {
        GATE_POS = ImVec2(620.0f * scal.x, 391.0f * scal.y);
        FRAME1_POS = ImVec2(900.0f * scal.x, 405.0f * scal.y);
        FRAME1_SIZE = ImVec2(260.0f, 400.0f);
        FRAME2_POS = ImVec2(320.0f * scal.x, 405.0f * scal.y);
        FRAME2_SIZE = ImVec2(260.0f, 400.0f);
        FRAME3_POS = ImVec2(640.0f * scal.x, 205.0f * scal.y);
        FRAME3_SIZE = ImVec2(320.0f, 200.0f);
    } else if (type[host.io.title_id] == "a3") {
        GATE_POS = ImVec2(620.0f * scal.x, 394.0f * scal.y);
        FRAME1_POS = ImVec2(900.0f * scal.x, 415.0f * scal.y);
        FRAME1_SIZE = ImVec2(260.0f, 200.0f);
        FRAME2_POS = ImVec2(320.0f * scal.x, 415.0f * scal.y);
        FRAME2_SIZE = ImVec2(260.0f, 200.0f);
        FRAME3_POS = ImVec2(900.0f * scal.x, 215.0f * scal.y);
        FRAME3_SIZE = ImVec2(260.0f, 210.0f);
        FRAME4_POS = ImVec2(640.0f * scal.x, 215.0f * scal.y);
        FRAME4_SIZE = ImVec2(320.0f, 210.0f);
        FRAME5_POS = ImVec2(320.0f * scal.x, 215.0f * scal.y);
        FRAME5_SIZE = ImVec2(260.0f, 210.0f);
    } else if (type[host.io.title_id] == "a4") {
        GATE_POS = ImVec2(620.0f * scal.x, 394.0f * scal.y);
        FRAME1_POS = ImVec2(900.0f * scal.x, 415.0f * scal.y);
        FRAME1_SIZE = ImVec2(260.0f, 200.0f);
        FRAME2_POS = ImVec2(320.0f * scal.x, 415.0f * scal.y);
        FRAME2_SIZE = ImVec2(260.0f, 200.0f);
        FRAME3_POS = ImVec2(900.0f * scal.x, 215.0f * scal.y);
        FRAME3_SIZE = ImVec2(840.0f, 70.0f);
        FRAME4_POS = ImVec2(900.0f * scal.x, 145.0f * scal.y);
        FRAME4_SIZE = ImVec2(840.0f, 70.0f);
        FRAME5_POS = ImVec2(900.0f * scal.x, 75.0f * scal.y);
        FRAME5_SIZE = ImVec2(840.0f, 70.0f);
    } else {
        GATE_POS = ImVec2(380.0f * scal.x, 388.0f * scal.y);
        FRAME1_POS = ImVec2(900.0f * scal.x, 413.0f * scal.y);
        FRAME1_SIZE = ImVec2(480.0f, 68.0f);
        FRAME2_POS = ImVec2(900.0f * scal.x, 345.0f * scal.y);
        FRAME2_SIZE = ImVec2(480.0f, 68.0f);
        FRAME3_POS = ImVec2(900 * scal.x, 277.0f * scal.y);
        FRAME3_SIZE = ImVec2(480.0f, 68.0f);
        FRAME4_POS = ImVec2(900.0f * scal.x, 209.0f * scal.y);
        FRAME4_SIZE = ImVec2(480.0f, 68.0f);
        FRAME5_POS = ImVec2(900.0f * scal.x, 141.0f * scal.y);
        FRAME5_SIZE = ImVec2(480.0f, 68.0f);
    }

    for (auto &frame : gui.live_items[host.io.title_id]) {
        ImVec2 FRAME_SIZE, FRAME_POS;

        if (frame.first == FRAME1)
            FRAME_POS = FRAME1_POS, FRAME_SIZE = FRAME1_SIZE;
        else if (frame.first == FRAME2)
            FRAME_POS = FRAME2_POS, FRAME_SIZE = FRAME2_SIZE;
        else if (frame.first == FRAME3)
            FRAME_POS = FRAME3_POS, FRAME_SIZE = FRAME3_SIZE;
        else if (frame.first == FRAME4)
            FRAME_POS = FRAME4_POS, FRAME_SIZE = FRAME4_SIZE;
        else if (frame.first == FRAME5)
            FRAME_POS = FRAME5_POS, FRAME_SIZE = FRAME5_SIZE;
        else if (frame.first == FRAME6)
            FRAME_POS = ImVec2(900.0f * scal.x, 73.0f * scal.y), FRAME_SIZE = ImVec2(480.0f, 68.0f);
        else if (frame.first == FRAME7)
            FRAME_POS = ImVec2(420.0f * scal.x, 220.0f * scal.y), FRAME_SIZE = ImVec2(360.0f, 210.0f);

        if (frame.first != 0) {
            auto bg_size = items[host.io.title_id][frame.first][BACKGROUND].first;
            const auto bg_resize = ImVec2(bg_size.x / FRAME_SIZE.x, bg_size.y / FRAME_SIZE.y);
            if (bg_size.x > FRAME_SIZE.x)
                bg_size.x /= bg_resize.x;
            if (bg_size.y > FRAME_SIZE.y)
                bg_size.y /= bg_resize.y;

            auto img_size = items[host.io.title_id][frame.first][IMAGE].first;
            const auto img_resize = ImVec2(img_size.x / FRAME_SIZE.x, img_size.y / FRAME_SIZE.y);
            if (img_size.x > FRAME_SIZE.x)
                img_size.x /= img_resize.x;
            if (img_size.y > FRAME_SIZE.y)
                img_size.y /= img_resize.y;

            auto bg_pos_init = ImVec2((FRAME_SIZE.x - bg_size.x) / 2, (FRAME_SIZE.y - bg_size.y) / 2);

            auto img_pos_init = ImVec2((FRAME_SIZE.x - img_size.x) / 2, (FRAME_SIZE.y - img_size.y) / 2);

            if (items[host.io.title_id][frame.first][BACKGROUND].second.second.first == "left")
                bg_pos_init.x = 0.0f;
            else if (items[host.io.title_id][frame.first][BACKGROUND].second.second.first == "right")
                bg_pos_init.x = FRAME_SIZE.x - bg_size.x;

            if (items[host.io.title_id][frame.first][IMAGE].second.second.first == "left")
                img_pos_init.x = 0.0f;
            else if (items[host.io.title_id][frame.first][IMAGE].second.second.first == "right")
                img_pos_init.x = FRAME_SIZE.x - img_size.x;

            if (items[host.io.title_id][frame.first][BACKGROUND].second.second.second == "top")
                bg_pos_init.y = 0.0f;
            else if (items[host.io.title_id][frame.first][BACKGROUND].second.second.second == "bottom")
                bg_pos_init.y = FRAME_SIZE.y - bg_size.y;

            if (items[host.io.title_id][frame.first][IMAGE].second.second.second == "top")
                img_pos_init.y = 0.0f;
            else if (items[host.io.title_id][frame.first][IMAGE].second.second.second == "bottom")
                img_pos_init.y = FRAME_SIZE.y - img_size.y;

            auto bg_pos = ImVec2(display_size.x - FRAME_POS.x + (bg_pos_init.x * scal.x) + (items[host.io.title_id][frame.first][BACKGROUND].second.first.x * scal.x),
                display_size.y - FRAME_POS.y + (bg_pos_init.y * scal.y) - (items[host.io.title_id][frame.first][BACKGROUND].second.first.y * scal.y));

            auto img_pos = ImVec2(display_size.x - FRAME_POS.x + (img_pos_init.x * scal.x) + (items[host.io.title_id][frame.first][IMAGE].second.first.x * scal.x),
                display_size.y - FRAME_POS.y + (img_pos_init.y * scal.y) - (items[host.io.title_id][frame.first][IMAGE].second.first.y * scal.y));

            if (bg_size.x == FRAME_SIZE.x)
                bg_pos.x = display_size.x - FRAME_POS.x;
            if (bg_size.y == FRAME_SIZE.y)
                bg_pos.y = display_size.y - FRAME_POS.y;

            if (img_size.x == FRAME_SIZE.x)
                img_pos.x = display_size.x - FRAME_POS.x;
            if (img_size.y == FRAME_SIZE.y)
                img_pos.y = display_size.y - FRAME_POS.y;

            const auto bg_scal_size = ImVec2(bg_size.x * scal.x, bg_size.y * scal.y);

            const auto img_scal_size = ImVec2(img_size.x * scal.x, img_size.y * scal.y);

            const auto pos_frame = ImVec2(display_size.x - FRAME_POS.x, display_size.y - FRAME_POS.y);
            const auto size_frame = ImVec2(FRAME_SIZE.x * scal.x, FRAME_SIZE.y * scal.y);

            if ((bg_size.x + bg_scal_size.x) > (pos_frame.x + size_frame.x))
                bg_size.x += (pos_frame.x + size_frame.x) - (bg_pos.x + bg_scal_size.x);
            if ((bg_size.y + bg_scal_size.y) > (pos_frame.y + size_frame.y))
                bg_pos.y += (pos_frame.y + size_frame.y) - (bg_pos.y + bg_scal_size.y);
            if (bg_pos.y < pos_frame.y)
                bg_pos.y += pos_frame.y - bg_pos.y;

            if ((img_size.x + img_scal_size.x) > (pos_frame.x + size_frame.x))
                img_size.x += (pos_frame.x + size_frame.x) - (img_pos.x + img_scal_size.x);
            if ((img_size.y + img_scal_size.y) > (pos_frame.y + size_frame.y))
                img_pos.y += (pos_frame.y + size_frame.y) - (img_pos.y + img_scal_size.y);
            if (img_pos.y < pos_frame.y)
                img_pos.y += pos_frame.y - img_pos.y;

            if (frame.second.find(BACKGROUND) != frame.second.end()) {
                ImGui::SetCursorPos(bg_pos);
                ImGui::Image(frame.second[BACKGROUND][current_item[host.io.title_id][frame.first]], bg_scal_size);
            }

            if (frame.second.find(IMAGE) != frame.second.end()) {
                ImGui::SetCursorPos(img_pos);
                ImGui::Image(frame.second[IMAGE][current_item[host.io.title_id][frame.first]], img_scal_size);
            }

            if (!target[host.io.title_id][frame.first].empty() && ImGui::IsItemClicked(0)) {
                std::ostringstream target_link;
                target_link << OS_PREFIX << target[host.io.title_id][frame.first];
                system(target_link.str().c_str());
            }

            if (!str[host.io.title_id][frame.first].first.empty()) {
                int R = 255, G = 255, B = 255;

                if (!str[host.io.title_id][frame.first].second.second.empty()) {
                    int color;
                    sscanf(str[host.io.title_id][frame.first].second.second.c_str(), "#%x", &color);

                    R = (color >> 16) & 0xFF;
                    G = (color >> 8) & 0xFF;
                    B = (color >> 0) & 0xFF;
                }

                auto size_text = 1.37f;
                if (str[host.io.title_id][frame.first].second.first != 0)
                    size_text = str[host.io.title_id][frame.first].second.first / 18.0f;
                const auto scal_str = size_text / 1.37f;
                const auto scal_size_str = size_text * scal_str;
                const auto str_size = 12.0f * scal_str;

                auto size_str = ImVec2((str[host.io.title_id][frame.first].first[current_item[host.io.title_id][frame.first]].size() * 11.9f) * scal_str, str_size);

                if (size_str.x > FRAME_SIZE.x)
                    size_str.x = FRAME_SIZE.x - 32.0f;

                auto str_pos_init = ImVec2((FRAME_SIZE.x - size_str.x) / 2.0f, (FRAME_SIZE.y - size_str.y) / 2.0f);

                if (str_pos[host.io.title_id][frame.first].second.second == "left")
                    str_pos_init.x = 0.0f;
                else if (str_pos[host.io.title_id][frame.first].second.second == "right")
                    str_pos_init.x = FRAME_SIZE.x - size_str.x;

                if (str_pos[host.io.title_id][frame.first].second.second == "top")
                    str_pos_init.y = 0.0f;
                else if (str_pos[host.io.title_id][frame.first].second.second == "bottom")
                    str_pos_init.y = FRAME_SIZE.y - size_str.y;

                const auto pos_str = ImVec2(pos_frame.x + (str_pos_init.x * scal.x) + (str_pos[host.io.title_id][frame.first].first.x * scal.x),
                    pos_frame.y + (str_pos_init.y * scal.y) + (str_pos[host.io.title_id][frame.first].first.y * scal.y));

                if (!gui.live_area.manual_dialog)
                    ImGui::GetForegroundDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize() * (scal_size_str * scal.x),
                        pos_str, IM_COL32(R, G, B, 255), str[host.io.title_id][frame.first].first[current_item[host.io.title_id][frame.first]].c_str(),
                        nullptr, size_frame.x - ((32.f * scal_str) * scal.x));
            }
        }
    }

    for (auto &flip : frame_autoflip[host.io.title_id])
        if (flip.second != 0) {
            if (last_time[host.io.title_id][flip.first] == 0)
                last_time[host.io.title_id][flip.first] = current_time();

            while (last_time[host.io.title_id][flip.first] + flip.second < current_time()) {
                last_time[host.io.title_id][flip.first] += flip.second;

                if (gui.live_items[host.io.title_id][flip.first].find(BACKGROUND) != gui.live_items[host.io.title_id][flip.first].end()) {
                    if (current_item[host.io.title_id][flip.first] != int(gui.live_items[host.io.title_id][flip.first][BACKGROUND].size()) - 1)
                        ++current_item[host.io.title_id][flip.first];
                    else
                        current_item[host.io.title_id][flip.first] = 0;
                } else if (gui.live_items[host.io.title_id][flip.first].find(IMAGE) != gui.live_items[host.io.title_id][flip.first].end()) {
                    if (current_item[host.io.title_id][flip.first] != int(gui.live_items[host.io.title_id][flip.first][IMAGE].size()) - 1)
                        ++current_item[host.io.title_id][flip.first];
                    else
                        current_item[host.io.title_id][flip.first] = 0;
                }
            }
        }

    const auto BUTTON_SIZE = ImVec2(80.f * scal.x, 30.f * scal.y);

    static const auto GATE_SIZE = ImVec2(280.0f, 158.0f);
    const auto pos_GATE = ImVec2(display_size.x - GATE_POS.x, display_size.y - GATE_POS.y);
    const auto scal_GATE_size = ImVec2(GATE_SIZE.x * scal.x, GATE_SIZE.y * scal.y);

    if (gui.live_area_contents[host.io.title_id].find(GATE) != gui.live_area_contents[host.io.title_id].end())
        ImGui::GetWindowDrawList()->AddImage(gui.live_area_contents[host.io.title_id][GATE],
            pos_GATE, ImVec2(pos_GATE.x + scal_GATE_size.x, pos_GATE.y + scal_GATE_size.y));

    ImGui::GetWindowDrawList()->AddRect(pos_GATE, ImVec2(pos_GATE.x + scal_GATE_size.x, pos_GATE.y + scal_GATE_size.y), IM_COL32(192, 192, 192, 255), 10.0f, ImDrawCornerFlags_All, 10.0f);

    ImGui::SetCursorPos(ImVec2(pos_GATE.x + (100.0f * scal.x), pos_GATE.y + (120.0f * scal.y)));
    if (ImGui::Button("Start", BUTTON_SIZE)) {
        gui.game_selector.selected_title_id = host.io.title_id;
        gui.live_area.live_area_dialog = false;
    }

    const auto icon_scal_size = ImVec2(32.0f * scal.x, 32.f * scal.y);
    const auto icon_pos = ImVec2(496.0f * scal.x, 544.f * scal.y);
    const auto pos_scal_icon = ImVec2(display_size.x - icon_pos.x, display_size.y - icon_pos.y);

    if (gui.game_selector.icons.find(host.io.title_id) != gui.game_selector.icons.end()) {
        ImGui::SetCursorPos(pos_scal_icon);
        ImGui::Image(gui.game_selector.icons[host.io.title_id], icon_scal_size);
    }

    const auto widget_scal_size = ImVec2(80.0f * scal.x, 80.f * scal.y);
    const auto manual_path{ fs::path(host.pref_path) / "ux0/app" / host.io.title_id / "sce_sys/manual/" };

    auto search_pos = ImVec2(578.0f * scal.x, 505.0f * scal.y);
    if (!fs::exists(manual_path) || fs::is_empty(manual_path))
        search_pos = ImVec2(520.0f * scal.x, 505.0f * scal.y);

    const auto pos_scal_search = ImVec2(display_size.x - search_pos.x, display_size.y - search_pos.y);
    ImGui::SetCursorPos(pos_scal_search);
    if (gui.live_area_contents[host.io.title_id].find(SEARCH) != gui.live_area_contents[host.io.title_id].end())
        ImGui::Image(gui.live_area_contents[host.io.title_id][SEARCH], widget_scal_size);
    else
        ImGui::Button("Search", BUTTON_SIZE);
    if (ImGui::IsItemClicked(0)) {
        std::ostringstream search_link;
        std::replace(host.game_title.begin(), host.game_title.end(), ' ', '+');
        std::string search_url = "http://www.google.com/search?q=" + host.game_title;
        search_link << OS_PREFIX << search_url;
        system(search_link.str().c_str());
    }

    if (fs::exists(manual_path) && !fs::is_empty(manual_path)) {
        const auto manaul_pos = ImVec2(463.0f * scal.x, 505.0f * scal.y);
        const auto pos_scal_manual = ImVec2(display_size.x - manaul_pos.x, display_size.y - manaul_pos.y);

        ImGui::SetCursorPos(pos_scal_manual);
        if (gui.live_area_contents[host.io.title_id].find(MANUAL) != gui.live_area_contents[host.io.title_id].end())
            ImGui::Image(gui.live_area_contents[host.io.title_id][MANUAL], widget_scal_size);
        else
            ImGui::Button("Manual", BUTTON_SIZE);
        if (ImGui::IsItemClicked(0)) {
            if (init_manual(gui, host))
                gui.live_area.manual_dialog = true;
            else
                LOG_ERROR("Error opening Manual");
        }
    }

    /*const auto button_pos = ImVec2(355.0f * scal.x, 505.0f * scal.y);
    const auto pos_scal_button = ImVec2(display_size.x - button_pos.x, display_size.y - button_pos.y);

    const auto refresh_scal_size = ImVec2(64.0f * scal.x, 64.f * scal.y);
    const auto refresh_pos = ImVec2(457.0f * scal.x, 501.0f * scal.y);
    const auto pos_scal_refresh = ImVec2(display_size.x - refresh_pos.x, display_size.y - refresh_pos.y);

    ImGui::SetCursorPos(pos_scal_button);
    if ((gui.live_area_contents[host.io.title_id].find(BUTTON) != gui.live_area_contents[host.io.title_id].end()) && (gui.live_area_contents[host.io.title_id].find(REFRESH) != gui.live_area_contents[host.io.title_id].end())) {
        ImGui::Image(gui.live_area_contents[host.io.title_id][BUTTON], widget_scal_size);
        ImGui::SetCursorPos(pos_scal_refresh);
        ImGui::Image(gui.live_area_contents[host.io.title_id][REFRESH], refresh_scal_size);
    } else
        ImGui::Button("Refresh", BUTTON_SIZE);
    if (ImGui::IsItemClicked(0)) {
        gui.live_area_contents.erase(host.io.title_id);
        gui.live_items.erase(host.io.title_id);
        init_live_area(gui, host);
    }*/

    if (!gui.live_area.manual_dialog) {
        const auto wheel_counter = ImGui::GetIO().MouseWheel;

        if (ImGui::IsKeyPressed(SDL_SCANCODE_LEFT) || (wheel_counter == 1)) {
            if (current_game > 0)
                --current_game;
            else
                current_game = int(gui.game_selector.games.size()) - 1;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT) || (wheel_counter == -1)) {
            if (current_game < int(gui.game_selector.games.size()) - 1)
                ++current_game;
            else
                current_game = 0;
        }

        if (host.io.title_id != gui.game_selector.games[current_game].title_id) {
            host.io.title_id = gui.game_selector.games[current_game].title_id;
            host.game_title = gui.game_selector.games[current_game].title;
            init_live_area(gui, host);
        }

        ImGui::SetCursorPos(ImVec2(display_size.x - (140.0f * scal.x), 14.0f * scal.y));
        if (ImGui::Button("X", BUTTON_SIZE) || ImGui::IsKeyPressed(SDL_SCANCODE_H))
            gui.live_area.live_area_dialog = false;
    }

    ImGui::End();
}

} // namespace gui
