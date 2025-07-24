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

#include <config/state.h>
#include <ctrl/ctrl.h>
#include <dialog/state.h>
#include <gui/functions.h>
#include <io/state.h>
#include <kernel/state.h>
#include <packages/license.h>
#include <renderer/state.h>

#include <io/VitaIoDevice.h>
#include <io/vfs.h>
#include <util/log.h>

#include <pugixml.hpp>

#include <chrono>
#include <stb_image.h>

namespace gui {

bool get_sys_apps_state(GuiState &gui) {
    return !gui.vita_area.content_manager && !gui.vita_area.settings && !gui.vita_area.trophy_collection && !gui.vita_area.manual;
}

struct FRAME {
    std::string id;
    std::string multi;
    SceUInt64 autoflip;
};

struct STR {
    std::string color;
    float size;
    std::string text;
};

static std::map<std::string, std::vector<FRAME>> frames;
static std::map<std::string, std::map<std::string, std::vector<STR>>> str;
static std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::pair<SceInt32, std::string>>>>> liveitem;
static std::map<std::string, std::map<std::string, std::map<std::string, ImVec2>>> items_size;
static std::map<std::string, std::map<std::string, std::string>> target;
static std::map<std::string, std::map<std::string, uint64_t>> current_item, last_time;
static std::map<std::string, std::string> type;

struct Items {
    ImVec2 gate_pos;
    struct Frames {
        ImVec2 pos;
        ImVec2 size;
    };
    std::map<std::string, Frames> frames;
};

// clang-format off
static std::map<std::string, Items> items_styles = {
    // A1
    { "a1", { 
        ImVec2(620.f, 361.f), { 
            { "frame1", { ImVec2(900.f, 414.f), ImVec2(260.f, 260.f) } },
            { "frame2", { ImVec2(320.f, 414.f), ImVec2(260.f, 260.f) } },
            { "frame3", { ImVec2(900.f, 154.f), ImVec2(840.f, 150.f) } }
        }
    } },
    // A2
    { "a2", {
        ImVec2(620.f, 395.f), {
            { "frame1", { ImVec2(900.f, 404.f), ImVec2(260.f, 400.f) } },
            { "frame2", { ImVec2(320.f, 404.f), ImVec2(260.f, 400.f) } },
            { "frame3", { ImVec2(640.f, 204.f), ImVec2(320.f, 200.f) } }
        }
    } },
    // A3
    { "a3", {
        ImVec2(620.f, 395.f), {
            { "frame1", { ImVec2(900.f, 414.f), ImVec2(260.f, 200.f) } },
            { "frame2", { ImVec2(320.f, 414.f), ImVec2(260.f, 200.f) } },
            { "frame3", { ImVec2(900.f, 214.f), ImVec2(260.f, 210.f) } },
            { "frame4", { ImVec2(640.f, 214.f), ImVec2(320.f, 210.f) } },
            { "frame5", { ImVec2(320.f, 214.f), ImVec2(260.f, 210.f) } }
		}
    } },
    // A4
    { "a4", { 
        ImVec2(620.f, 395.f), {
            { "frame1", { ImVec2(900.f, 414.f), ImVec2(260.f, 200.f) } },
            { "frame2", { ImVec2(320.f, 414.f), ImVec2(260.f, 200.f) } },
            { "frame3", { ImVec2(900.f, 214.f), ImVec2(840.f, 70.f) } },
            { "frame4", { ImVec2(900.f, 144.f), ImVec2(840.f, 70.f) } },
            { "frame5", { ImVec2(900.f, 74.f), ImVec2(840.f, 70.f) } }
        }
    } },
    // A5
    { "a5", {
        ImVec2(380.f, 395.f), {
            { "frame1", { ImVec2(900.f, 412.f), ImVec2(480.f, 68.f) } },
            { "frame2", { ImVec2(900.f, 344.f), ImVec2(480.f, 68.f) } },
            { "frame3", { ImVec2(900.f, 276.f), ImVec2(480.f, 68.f) } },
            { "frame4", { ImVec2(900.f, 208.f), ImVec2(480.f, 68.f) } },
            { "frame5", { ImVec2(900.f, 140.f), ImVec2(480.f, 68.f) } },
            { "frame6", { ImVec2(900.f, 72.f), ImVec2(480.f, 68.f) } },
            { "frame7", { ImVec2(420.f, 214.f), ImVec2(360.f, 210.f) } }
        }
    } },
    // Browser
    { "browser", {
        ImVec2(381.f, 395.f), {
            { "frame1", { ImVec2(890.f, 405.f), ImVec2(472.f, 50.f) } },
            { "frame2", { ImVec2(890.f, 355.f), ImVec2(472.f, 50.f) } },
            { "frame3", { ImVec2(890.f, 305.f), ImVec2(472.f, 50.f) } },
            { "frame4", { ImVec2(890.f, 255.f), ImVec2(472.f, 50.f) } },
            { "frame5", { ImVec2(890.f, 205.f), ImVec2(472.f, 50.f) } },
            { "frame6", { ImVec2(890.f, 155.f), ImVec2(472.f, 50.f) } },
            { "frame7", { ImVec2(890.f, 105.f), ImVec2(472.f, 50.f) } },
            { "frame8", { ImVec2(890.f, 55.f), ImVec2(472.f, 50.f) } },
            { "frame9", { ImVec2(381.f, 223.f), ImVec2(282.f, 108.f) } },
            { "frame10", { ImVec2(381.f, 115.f), ImVec2(282.f, 108.f) } }
        }
    } },
    // Content manager
    { "content_manager", { ImVec2(620.f, 361.f), {} } },
    // PSMobile
    { "psmobile", {
        ImVec2(380.f, 345.f), {
            { "frame1", { ImVec2(866.f, 414.f), ImVec2(446.f, 154.f) } }, 
            { "frame2", { ImVec2(866.f, 249.5f), ImVec2(446.f, 109.f) } },
            { "frame3", { ImVec2(866.f, 119.f), ImVec2(196.f, 58.f) } },
            { "frame4", { ImVec2(866.f, 34.f), ImVec2(772.f, 30.f) } }
        }
    } },
};
// clang-format on

void init_live_area(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const auto &live_area_lang = gui.lang.user_lang[LIVE_AREA];
    const auto is_sys_app = app_path.starts_with("NPXS") && (app_path != "NPXS10007");
    const auto is_ps_app = app_path.starts_with("PCS") || (app_path == "NPXS10007");
    const VitaIoDevice app_device = is_sys_app ? VitaIoDevice::vs0 : VitaIoDevice::ux0;
    const auto APP_INDEX = get_app_index(gui, app_path);
    const auto TITLE_ID = APP_INDEX->title_id;

    get_license(emuenv, APP_INDEX->title_id, APP_INDEX->content_id);

    if (!gui.live_area_contents.contains(app_path)) {
        auto default_contents = false;
        const auto fw_path{ emuenv.pref_path / "vs0" };
        const auto default_fw_contents{ fw_path / "data/internal/livearea/default/sce_sys/livearea/contents/template.xml" };
        const auto APP_PATH{ emuenv.pref_path / app_device._to_string() / "app" / app_path };
        const auto live_area_path{ fs::path("sce_sys") / ((emuenv.license.rif[TITLE_ID].sku_flag == 3) && fs::exists(APP_PATH / "sce_sys/retail/livearea") ? "retail/livearea" : "livearea") };
        auto template_xml{ APP_PATH / live_area_path / "contents/template.xml" };

        pugi::xml_document doc;

        if (!doc.load_file(template_xml.c_str())) {
            if (is_ps_app || is_sys_app)
                LOG_WARN("Live Area Contents is corrupted or missing for title: {} '{}' in path: {}.", APP_INDEX->title_id, APP_INDEX->title, template_xml);
            if (doc.load_file(default_fw_contents.c_str())) {
                template_xml = default_fw_contents;
                default_contents = true;
                LOG_INFO("Using default firmware contents.");
            } else {
                type[app_path] = "a1";
                LOG_WARN("Default firmware contents is corrupted or missing, install firmware for fix it.");
                return;
            }
        }

        if (doc.load_file(template_xml.c_str())) {
            std::map<std::string, std::string> name;

            type[app_path].clear();
            type[app_path] = doc.child("livearea").attribute("style").as_string();

            if (!doc.child("livearea").child("livearea-background").child("image").child("lang").text().empty()) {
                for (const auto &livearea_background : doc.child("livearea").child("livearea-background")) {
                    if (livearea_background.child("lang").text().as_string() == live_area_lang) {
                        name["livearea-background"] = livearea_background.text().as_string();
                        break;
                    } else if (!livearea_background.child("exclude-lang").empty() && livearea_background.child("exclude-lang").text().as_string() != live_area_lang) {
                        name["livearea-background"] = livearea_background.text().as_string();
                        break;
                    } else if (livearea_background.attribute("default").as_string() == std::string("on")) {
                        name["livearea-background"] = livearea_background.text().as_string();
                        break;
                    }
                }
            } else
                name["livearea-background"] = doc.child("livearea").child("livearea-background").child("image").text().as_string();

            if (name["livearea-background"].empty())
                name["livearea-background"] = doc.child("livearea").child("livearea-background").child("image").text().as_string();

            if (!default_contents) {
                for (const auto &gate : doc.child("livearea").child("gate")) {
                    if (gate.child("lang")) {
                        if (gate.child("lang").text().as_string() == live_area_lang) {
                            name["gate"] = gate.text().as_string();
                            break;
                        } else if (gate.attribute("default").as_string() == std::string("on"))
                            name["gate"] = gate.text().as_string();
                        else
                            for (const auto &lang : gate)
                                if (lang.text().as_string() == live_area_lang) {
                                    name["gate"] = gate.text().as_string();
                                    break;
                                }
                    } else if (gate.child("cntry")) {
                        if (gate.child("cntry").attribute("lang").as_string() == live_area_lang) {
                            name["gate"] = gate.text().as_string();
                            break;
                        } else
                            name["gate"] = gate.text().as_string();
                    } else
                        name["gate"] = gate.text().as_string();
                }
                if (name["gate"].find('\n') != std::string::npos)
                    name["gate"].erase(remove(name["gate"].begin(), name["gate"].end(), '\n'), name["gate"].end());
                name["gate"].erase(remove_if(name["gate"].begin(), name["gate"].end(), isspace), name["gate"].end());
            }

            if (name["livearea-background"].find('\n') != std::string::npos)
                name["livearea-background"].erase(remove(name["livearea-background"].begin(), name["livearea-background"].end(), '\n'), name["livearea-background"].end());
            name["livearea-background"].erase(remove_if(name["livearea-background"].begin(), name["livearea-background"].end(), isspace), name["livearea-background"].end());

            for (const auto &contents : name) {
                int32_t width = 0;
                int32_t height = 0;
                vfs::FileBuffer buffer;

                if (contents.second.empty()) {
                    LOG_WARN("Content '{}' is empty for title {} [{}].", contents.first, app_path, APP_INDEX->title);
                    continue;
                }

                if (default_contents)
                    vfs::read_file(VitaIoDevice::vs0, buffer, emuenv.pref_path, "data/internal/livearea/default/sce_sys/livearea/contents/" + contents.second);
                else if (app_device == VitaIoDevice::vs0)
                    vfs::read_file(VitaIoDevice::vs0, buffer, emuenv.pref_path, "app/" + app_path + "/sce_sys/livearea/contents/" + contents.second);
                else
                    vfs::read_app_file(buffer, emuenv.pref_path, app_path, live_area_path / "contents" / contents.second);

                if (buffer.empty()) {
                    if (is_ps_app || is_sys_app)
                        LOG_WARN("Contents {} '{}' Not found for title {} [{}].", contents.first, contents.second, app_path, APP_INDEX->title);
                    continue;
                }
                stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                if (!data) {
                    LOG_ERROR("Invalid Live Area Contents for title {} [{}].", app_path, APP_INDEX->title);
                    continue;
                }

                gui.live_area_contents[app_path][contents.first].init(gui.imgui_state.get(), data, width, height);
                stbi_image_free(data);
            }

            std::map<std::string, std::map<std::string, std::vector<std::string>>> items_name;

            frames[app_path].clear();
            liveitem[app_path].clear();
            str[app_path].clear();
            target[app_path].clear();

            for (const auto &livearea : doc.child("livearea")) {
                if (!livearea.attribute("id").empty()) {
                    frames[app_path].push_back({ livearea.attribute("id").as_string(), livearea.attribute("multi").as_string(), livearea.attribute("autoflip").as_uint() });

                    std::string frame = frames[app_path].back().id;

                    // Position background
                    if (!livearea.child("liveitem").child("background").empty()) {
                        if (!livearea.child("liveitem").child("background").attribute("x").empty())
                            liveitem[app_path][frame]["background"]["x"].first = livearea.child("liveitem").child("background").attribute("x").as_int();
                        if (!livearea.child("liveitem").child("background").attribute("y").empty())
                            liveitem[app_path][frame]["background"]["y"].first = livearea.child("liveitem").child("background").attribute("y").as_int();
                        if (!livearea.child("liveitem").child("background").attribute("align").empty())
                            liveitem[app_path][frame]["background"]["align"].second = livearea.child("liveitem").child("background").attribute("align").as_string();
                        if (!livearea.child("liveitem").child("background").attribute("valign").empty())
                            liveitem[app_path][frame]["background"]["valign"].second = livearea.child("liveitem").child("background").attribute("valign").as_string();
                    }

                    // Position image
                    if (!livearea.child("liveitem").child("image").empty()) {
                        if (!livearea.child("liveitem").child("image").attribute("x").empty())
                            liveitem[app_path][frame]["image"]["x"].first = livearea.child("liveitem").child("image").attribute("x").as_int();
                        if (!livearea.child("liveitem").child("image").attribute("y").empty())
                            liveitem[app_path][frame]["image"]["y"].first = livearea.child("liveitem").child("image").attribute("y").as_int();
                        if (!livearea.child("liveitem").child("image").attribute("align").empty())
                            liveitem[app_path][frame]["image"]["align"].second = livearea.child("liveitem").child("image").attribute("align").as_string();
                        if (!livearea.child("liveitem").child("image").attribute("valign").empty())
                            liveitem[app_path][frame]["image"]["valign"].second = livearea.child("liveitem").child("image").attribute("valign").as_string();
                        if (!livearea.child("liveitem").child("image").attribute("origin").empty())
                            liveitem[app_path][frame]["image"]["origin"].second = livearea.child("liveitem").child("image").attribute("origin").as_string();
                    }

                    if (!livearea.child("liveitem").child("text").empty()) {
                        // SceInt32
                        if (!livearea.child("liveitem").child("text").attribute("width").empty())
                            liveitem[app_path][frame]["text"]["width"].first = livearea.child("liveitem").child("text").attribute("width").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("height").empty())
                            liveitem[app_path][frame]["text"]["height"].first = livearea.child("liveitem").child("text").attribute("height").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("x").empty())
                            liveitem[app_path][frame]["text"]["x"].first = livearea.child("liveitem").child("text").attribute("x").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("y").empty())
                            liveitem[app_path][frame]["text"]["y"].first = livearea.child("liveitem").child("text").attribute("y").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("margin-top").empty())
                            liveitem[app_path][frame]["text"]["margin-top"].first = livearea.child("liveitem").child("text").attribute("margin-top").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("margin-bottom").empty())
                            liveitem[app_path][frame]["text"]["margin-bottom"].first = livearea.child("liveitem").child("text").attribute("margin-bottom").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("margin-left").empty())
                            liveitem[app_path][frame]["text"]["margin-left"].first = livearea.child("liveitem").child("text").attribute("margin-left").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("margin-right").empty())
                            liveitem[app_path][frame]["text"]["margin-right"].first = livearea.child("liveitem").child("text").attribute("margin-right").as_int();

                        // String
                        if (!livearea.child("liveitem").child("text").attribute("align").empty())
                            liveitem[app_path][frame]["text"]["align"].second = livearea.child("liveitem").child("text").attribute("align").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("valign").empty())
                            liveitem[app_path][frame]["text"]["valign"].second = livearea.child("liveitem").child("text").attribute("valign").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("origin").empty())
                            liveitem[app_path][frame]["text"]["origin"].second = livearea.child("liveitem").child("text").attribute("origin").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("line-align").empty())
                            liveitem[app_path][frame]["text"]["line-align"].second = livearea.child("liveitem").child("text").attribute("line-align").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("text-align").empty())
                            liveitem[app_path][frame]["text"]["text-align"].second = livearea.child("liveitem").child("text").attribute("text-align").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("text-valign").empty())
                            liveitem[app_path][frame]["text"]["text-valign"].second = livearea.child("liveitem").child("text").attribute("text-valign").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("word-wrap").empty())
                            liveitem[app_path][frame]["text"]["word-wrap"].second = livearea.child("liveitem").child("text").attribute("word-wrap").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("word-scroll").empty())
                            liveitem[app_path][frame]["text"]["word-scroll"].second = livearea.child("liveitem").child("text").attribute("word-scroll").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("ellipsis").empty())
                            liveitem[app_path][frame]["text"]["ellipsis"].second = livearea.child("liveitem").child("text").attribute("ellipsis").as_string();
                    }

                    for (const auto &frame_id : livearea) {
                        if (frame_id.name() == std::string("liveitem")) {
                            if (!frame_id.child("cntry").empty()) {
                                for (const auto &cntry : frame_id) {
                                    if ((cntry.name() == std::string("cntry")) && (cntry.attribute("lang").as_string() == live_area_lang)) {
                                        if (!frame_id.child("background").text().empty())
                                            items_name[frame]["background"].push_back({ frame_id.child("background").text().as_string() });
                                        if (!frame_id.child("image").text().empty())
                                            items_name[frame]["image"].push_back({ frame_id.child("image").text().as_string() });
                                        if (!frame_id.child("target").text().empty())
                                            target[app_path][frame] = frame_id.child("target").text().as_string();
                                        if (!frame_id.child("text").child("str").text().empty()) {
                                            for (const auto &str_child : frame_id.child("text"))
                                                str[app_path][frame].push_back({ str_child.attribute("color").as_string(), str_child.attribute("size").as_float(), str_child.text().as_string() });
                                        }
                                        break;
                                    }
                                }
                            } else if (!frame_id.child("exclude-lang").empty()) {
                                bool exclude_lang = false;
                                for (const auto &liveitem : frame_id)
                                    if ((liveitem.name() == std::string("exclude-lang")) && (liveitem.text().as_string() == live_area_lang)) {
                                        exclude_lang = true;
                                        break;
                                    }
                                if (!exclude_lang) {
                                    if (!frame_id.child("background").text().empty())
                                        items_name[frame]["background"].push_back({ frame_id.child("background").text().as_string() });
                                    if (!frame_id.child("image").text().empty())
                                        items_name[frame]["image"].push_back({ frame_id.child("image").text().as_string() });
                                    if (!frame_id.child("target").text().empty())
                                        target[app_path][frame] = frame_id.child("target").text().as_string();
                                    if (!frame_id.child("text").child("str").text().empty()) {
                                        for (const auto &str_child : frame_id.child("text"))
                                            str[app_path][frame].push_back({ str_child.attribute("color").as_string(), str_child.attribute("size").as_float(), str_child.text().as_string() });
                                    }
                                    break;
                                }
                            } else if (frame_id.child("lang").text().as_string() == live_area_lang) {
                                if (!frame_id.child("background").text().empty())
                                    items_name[frame]["background"].push_back({ frame_id.child("background").text().as_string() });
                                if (!frame_id.child("image").text().empty())
                                    items_name[frame]["image"].push_back({ frame_id.child("image").text().as_string() });
                                if (!frame_id.child("target").text().empty())
                                    target[app_path][frame] = frame_id.child("target").text().as_string();
                                if (!frame_id.child("text").child("str").text().empty()) {
                                    for (const auto &str_child : frame_id.child("text"))
                                        str[app_path][frame].push_back({ str_child.attribute("color").as_string(), str_child.attribute("size").as_float(), str_child.text().as_string() });
                                }
                                break;
                            }
                        }
                    }

                    // if empty
                    const auto liveitem = livearea.child("liveitem");
                    if (items_name[frame]["background"].empty() && !liveitem.child("background").text().empty())
                        items_name[frame]["background"].push_back({ liveitem.child("background").text().as_string() });
                    if (items_name[frame]["image"].empty() && !liveitem.child("image").text().empty())
                        items_name[frame]["image"].push_back({ liveitem.child("image").text().as_string() });
                    if (target[app_path][frame].empty() && !liveitem.child("target").text().empty())
                        target[app_path][frame] = liveitem.child("target").text().as_string();
                    if (str[app_path][frame].empty() && !liveitem.child("text").child("str").text().empty()) {
                        for (const auto &str_child : liveitem.child("text"))
                            str[app_path][frame].push_back({ str_child.attribute("color").as_string(), str_child.attribute("size").as_float(), str_child.text().as_string() });
                    }

                    // Remove space or return line if exist on target
                    if (!target[app_path][frame].empty()) {
                        if (target[app_path][frame].find('\n') != std::string::npos)
                            target[app_path][frame].erase(remove(target[app_path][frame].begin(), target[app_path][frame].end(), '\n'), target[app_path][frame].end());
                        target[app_path][frame].erase(remove_if(target[app_path][frame].begin(), target[app_path][frame].end(), isspace), target[app_path][frame].end());
                    }
                }
            }

            for (auto &item : items_name) {
                current_item[app_path][item.first] = 0;
                if (!item.second.empty()) {
                    for (auto &bg_name : item.second["background"]) {
                        if (!bg_name.empty()) {
                            if (bg_name.find('\n') != std::string::npos)
                                bg_name.erase(remove(bg_name.begin(), bg_name.end(), '\n'), bg_name.end());
                            bg_name.erase(remove_if(bg_name.begin(), bg_name.end(), isspace), bg_name.end());

                            int32_t width = 0;
                            int32_t height = 0;
                            vfs::FileBuffer buffer;

                            if (app_device == VitaIoDevice::vs0)
                                vfs::read_file(VitaIoDevice::vs0, buffer, emuenv.pref_path, fmt::format("app/{}/sce_sys/livearea/contents/{}", app_path, bg_name));
                            else
                                vfs::read_app_file(buffer, emuenv.pref_path, app_path, live_area_path / "contents" / bg_name);

                            if (buffer.empty()) {
                                if (is_ps_app || is_sys_app)
                                    LOG_WARN("background, Id: {}, Name: '{}', Not found for title: {} [{}].", item.first, bg_name, app_path, APP_INDEX->title);
                                continue;
                            }
                            stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                            if (!data) {
                                LOG_ERROR("Frame: {}, Invalid Live Area Contents for title: {} [{}].", item.first, app_path, APP_INDEX->title);
                                continue;
                            }

                            items_size[app_path][item.first]["background"] = ImVec2(static_cast<float>(width), static_cast<float>(height));
                            gui.live_items[app_path][item.first]["background"].emplace_back(gui.imgui_state.get(), data, width, height);
                            stbi_image_free(data);
                        }
                    }

                    for (auto &img_name : item.second["image"]) {
                        if (!img_name.empty()) {
                            if (img_name.find('\n') != std::string::npos)
                                img_name.erase(remove(img_name.begin(), img_name.end(), '\n'), img_name.end());
                            img_name.erase(remove_if(img_name.begin(), img_name.end(), isspace), img_name.end());

                            int32_t width = 0;
                            int32_t height = 0;
                            vfs::FileBuffer buffer;

                            if (app_device == VitaIoDevice::vs0)
                                vfs::read_file(VitaIoDevice::vs0, buffer, emuenv.pref_path, fmt::format("app/{}/sce_sys/livearea/contents/{}", app_path, img_name));
                            else
                                vfs::read_app_file(buffer, emuenv.pref_path, app_path, live_area_path / "contents" / img_name);

                            if (buffer.empty()) {
                                if (is_ps_app || is_sys_app)
                                    LOG_WARN("Image, Id: {} Name: '{}', Not found for title {} [{}].", item.first, img_name, app_path, APP_INDEX->title);
                                continue;
                            }
                            stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                            if (!data) {
                                LOG_ERROR("Frame: {}, Invalid Live Area Contents for title: {} [{}].", item.first, app_path, APP_INDEX->title);
                                continue;
                            }

                            items_size[app_path][item.first]["image"] = ImVec2(static_cast<float>(width), static_cast<float>(height));
                            gui.live_items[app_path][item.first]["image"].emplace_back(gui.imgui_state.get(), data, width, height);
                            stbi_image_free(data);
                        }
                    }
                }
            }
        }
    }
    if (type[app_path].empty())
        type[app_path] = "a1";
}

inline static uint64_t current_time() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

void open_search(const std::string &title) {
    auto search_url = "https://www.google.com/search?q=" + title;
    std::replace(search_url.begin(), search_url.end(), ' ', '+');
    open_path(search_url);
}

void update_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    if (gui.live_area_contents.contains(app_path))
        gui.live_area_contents.erase(app_path);
    if (gui.live_items.contains(app_path))
        gui.live_items.erase(app_path);

    init_user_app(gui, emuenv, app_path);
    save_apps_cache(gui, emuenv);

    if (get_live_area_current_open_apps_list_index(gui, app_path) != gui.live_area_current_open_apps_list.end())
        init_live_area(gui, emuenv, app_path);
}

void close_live_area_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    if (app_path == emuenv.io.app_path) {
        update_time_app_used(gui, emuenv, app_path);
        emuenv.kernel.exit_delete_all_threads();
        emuenv.load_exec = true;
        // make sure we are not stuck waiting for a gpu command
        emuenv.renderer->should_display = true;
    } else {
        gui.live_area_current_open_apps_list.erase(get_live_area_current_open_apps_list_index(gui, app_path));
        if (gui.live_area_app_current_open == 0) {
            gui.vita_area.live_area_screen = false;
            gui.vita_area.home_screen = true;
        }
        --gui.live_area_app_current_open;
    }
}

enum LiveAreaType {
    GATE,
    SEARCH,
    MANUAL,
};

static constexpr ImU32 ARROW_COLOR = 0xFFFFFFFF; // White
static LiveAreaType live_area_type_selected = GATE;

void browse_live_area_apps_list(GuiState &gui, EmuEnvState &emuenv, const uint32_t button) {
    const auto manual_path{ emuenv.pref_path / "ux0/app" / gui.live_area_current_open_apps_list[gui.live_area_app_current_open] / "sce_sys/manual/" };
    const auto manual_found = fs::exists(manual_path) && !fs::is_empty(manual_path);

    if (!gui.is_nav_button) {
        if ((live_area_type_selected == MANUAL) && !manual_found)
            live_area_type_selected = GATE;
        gui.is_nav_button = true;
        return;
    }

    const auto live_area_current_open_apps_list_size = static_cast<int32_t>(gui.live_area_current_open_apps_list.size() - 1);

    const auto cancel = [&]() {
        close_live_area_app(gui, emuenv, gui.live_area_current_open_apps_list[gui.live_area_app_current_open]);
    };
    const auto confirm = [&]() {
        switch (live_area_type_selected) {
        case GATE:
            pre_run_app(gui, emuenv, gui.live_area_current_open_apps_list[gui.live_area_app_current_open]);
            break;
        case SEARCH:
            open_search(get_app_index(gui, gui.live_area_current_open_apps_list[gui.live_area_app_current_open])->title);
            break;
        case MANUAL:
            open_manual(gui, emuenv, gui.live_area_current_open_apps_list[gui.live_area_app_current_open]);
            break;
        default:
            break;
        }
    };

    switch (button) {
    case SCE_CTRL_UP: {
        if (manual_found)
            live_area_type_selected = MANUAL;
        break;
    }
    case SCE_CTRL_DOWN:
        live_area_type_selected = GATE;
        break;
    case SCE_CTRL_LEFT:
    case SCE_CTRL_L1:
        if ((button & SCE_CTRL_LEFT) && live_area_type_selected == MANUAL)
            live_area_type_selected = SEARCH;
        else {
            gui.live_area_app_current_open = std::max(gui.live_area_app_current_open - 1, -1);
            gui.vita_area.live_area_screen = gui.live_area_app_current_open >= 0;
            gui.vita_area.home_screen = !gui.vita_area.live_area_screen;
            live_area_type_selected = GATE;
        }
        break;
    case SCE_CTRL_RIGHT:
    case SCE_CTRL_R1:
        if ((button & SCE_CTRL_RIGHT) && live_area_type_selected == SEARCH)
            live_area_type_selected = MANUAL;
        else if (gui.live_area_app_current_open < live_area_current_open_apps_list_size) {
            ++gui.live_area_app_current_open;
            live_area_type_selected = GATE;
        }
        break;
    case SCE_CTRL_CIRCLE:
        if (emuenv.cfg.sys_button == 1)
            cancel();
        else
            confirm();
        break;
    case SCE_CTRL_CROSS:
        if (emuenv.cfg.sys_button == 1)
            confirm();
        else
            cancel();
        break;
    default:
        break;
    }
}

void draw_live_area_screen(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    const auto &app_path = gui.live_area_current_open_apps_list[gui.live_area_app_current_open];
    const VitaIoDevice app_device = app_path.starts_with("NPXS") ? VitaIoDevice::vs0 : VitaIoDevice::ux0;

    const auto INFO_BAR_HEIGHT = 32.f * SCALE.y;

    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INFO_BAR_HEIGHT);
    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INFO_BAR_HEIGHT);
    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
    if (gui.is_nav_button)
        flags |= ImGuiWindowFlags_NoMouseInputs;
    ImGui::Begin("##live_area", &gui.vita_area.live_area_screen, flags);

    // Draw background
    draw_background(gui, emuenv);

    const auto background_pos = ImVec2(900.0f * SCALE.x, 500.0f * SCALE.y);
    const auto pos_bg = ImVec2(WINDOW_POS.x + WINDOW_SIZE.x - background_pos.x, WINDOW_POS.y + WINDOW_SIZE.y - background_pos.y);
    const auto background_size = ImVec2(840.0f * SCALE.x, 500.0f * SCALE.y);

    const auto window_draw_list = ImGui::GetWindowDrawList();

    if (gui.live_area_contents[app_path].contains("livearea-background"))
        window_draw_list->AddImage(gui.live_area_contents[app_path]["livearea-background"], pos_bg, ImVec2(pos_bg.x + background_size.x, pos_bg.y + background_size.y));
    else
        window_draw_list->AddRectFilled(pos_bg, ImVec2(pos_bg.x + background_size.x, pos_bg.y + background_size.y), IM_COL32(148.f, 164.f, 173.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    const auto &app_type = type[app_path];
    LOG_WARN_IF(!items_styles.contains(app_type), "Type of style {} no found for: {}", app_type, app_path);

    for (const auto &frame : frames[app_path]) {
        if (frame.autoflip != 0) {
            if (last_time[app_path][frame.id] == 0)
                last_time[app_path][frame.id] = current_time();

            while (last_time[app_path][frame.id] + frame.autoflip < current_time()) {
                last_time[app_path][frame.id] += frame.autoflip;

                if (gui.live_items[app_path][frame.id].contains("background")) {
                    if (current_item[app_path][frame.id] != gui.live_items[app_path][frame.id]["background"].size() - 1)
                        ++current_item[app_path][frame.id];
                    else
                        current_item[app_path][frame.id] = 0;
                } else if (gui.live_items[app_path][frame.id].contains("image")) {
                    if (current_item[app_path][frame.id] != gui.live_items[app_path][frame.id]["image"].size() - 1)
                        ++current_item[app_path][frame.id];
                    else
                        current_item[app_path][frame.id] = 0;
                }
            }
        }

        if (!items_styles[app_type].frames.contains(frame.id))
            continue;

        const auto FRAME = items_styles[app_type].frames[frame.id];
        const auto FRAME_SIZE = FRAME.size;

        auto FRAME_POS = ImVec2(FRAME.pos.x * SCALE.x, FRAME.pos.y * SCALE.y);

        auto bg_size = items_size[app_path][frame.id]["background"];

        // Resize items
        const auto bg_resize = ImVec2(bg_size.x / FRAME_SIZE.x, bg_size.y / FRAME_SIZE.y);
        if (bg_size.x > FRAME_SIZE.x)
            bg_size.x /= bg_resize.x;
        if (bg_size.y > FRAME_SIZE.y)
            bg_size.y /= bg_resize.y;

        auto img_size = items_size[app_path][frame.id]["image"];
        const auto img_resize = ImVec2(img_size.x / FRAME_SIZE.x, img_size.y / FRAME_SIZE.y);
        if (img_size.x > FRAME_SIZE.x)
            img_size.x /= img_resize.x;
        if (img_size.y > FRAME_SIZE.y)
            img_size.y /= img_resize.y;

        // Items pos init (Center)
        auto bg_pos_init = ImVec2((FRAME_SIZE.x - bg_size.x) / 2.f, (FRAME_SIZE.y - bg_size.y) / 2.f);
        auto img_pos_init = ImVec2((FRAME_SIZE.x - img_size.x) / 2.f, (FRAME_SIZE.y - img_size.y) / 2.f);

        // Align items
        if ((liveitem[app_path][frame.id]["background"]["align"].second == "left") && (liveitem[app_path][frame.id]["background"]["x"].first >= 0))
            bg_pos_init.x = 0.0f;
        else if ((liveitem[app_path][frame.id]["background"]["align"].second == "right") && (liveitem[app_path][frame.id]["background"]["x"].first <= 0))
            bg_pos_init.x = FRAME_SIZE.x - bg_size.x;
        else
            bg_pos_init.x += liveitem[app_path][frame.id]["background"]["x"].first;

        if ((liveitem[app_path][frame.id]["image"]["align"].second == "left") && (liveitem[app_path][frame.id]["image"]["x"].first >= 0))
            img_pos_init.x = 0.0f;
        else if ((liveitem[app_path][frame.id]["image"]["align"].second == "right") && (liveitem[app_path][frame.id]["image"]["x"].first <= 0))
            img_pos_init.x = FRAME_SIZE.x - img_size.x;
        else
            img_pos_init.x += liveitem[app_path][frame.id]["image"]["x"].first;

        // Valign items
        if ((liveitem[app_path][frame.id]["background"]["valign"].second == "top") && (liveitem[app_path][frame.id]["background"]["y"].first <= 0))
            bg_pos_init.y = 0.0f;
        else if ((liveitem[app_path][frame.id]["background"]["valign"].second == "bottom") && (liveitem[app_path][frame.id]["background"]["y"].first >= 0))
            bg_pos_init.y = FRAME_SIZE.y - bg_size.y;
        else
            bg_pos_init.y -= liveitem[app_path][frame.id]["background"]["y"].first;

        if ((liveitem[app_path][frame.id]["image"]["valign"].second == "top") && (liveitem[app_path][frame.id]["image"]["y"].first <= 0))
            img_pos_init.y = 0.0f;
        else if ((liveitem[app_path][frame.id]["image"]["valign"].second == "bottom") && (liveitem[app_path][frame.id]["image"]["y"].first >= 0))
            img_pos_init.y = FRAME_SIZE.y - img_size.y;
        else
            img_pos_init.y -= liveitem[app_path][frame.id]["image"]["y"].first;

        // Set items pos
        auto bg_pos = ImVec2((WINDOW_SIZE.x - FRAME_POS.x) + (bg_pos_init.x * SCALE.x), (WINDOW_SIZE.y - FRAME_POS.y) + (bg_pos_init.y * SCALE.y));

        auto img_pos = ImVec2((WINDOW_SIZE.x - FRAME_POS.x) + (img_pos_init.x * SCALE.x), (WINDOW_SIZE.y - FRAME_POS.y) + (img_pos_init.y * SCALE.y));

        if (bg_size.x == FRAME_SIZE.x)
            bg_pos.x = WINDOW_SIZE.x - FRAME_POS.x;
        if (bg_size.y == FRAME_SIZE.y)
            bg_pos.y = WINDOW_SIZE.y - FRAME_POS.y;

        if (img_size.x == FRAME_SIZE.x)
            img_pos.x = WINDOW_SIZE.x - FRAME_POS.x;
        if (img_size.y == FRAME_SIZE.y)
            img_pos.y = WINDOW_SIZE.y - FRAME_POS.y;

        // Scale size items
        const auto bg_scal_size = ImVec2(bg_size.x * SCALE.x, bg_size.y * SCALE.y);
        const auto img_scal_size = ImVec2(img_size.x * SCALE.x, img_size.y * SCALE.y);

        const auto pos_frame = ImVec2(WINDOW_SIZE.x - FRAME_POS.x, WINDOW_SIZE.y - FRAME_POS.y);

        // Scale size frame
        const auto scal_size_frame = ImVec2(FRAME_SIZE.x * SCALE.x, FRAME_SIZE.y * SCALE.y);

        // Reset position if get outside frame
        if ((bg_pos.x + bg_scal_size.x) > (pos_frame.x + scal_size_frame.x))
            bg_pos.x += (pos_frame.x + scal_size_frame.x) - (bg_pos.x + bg_scal_size.x);
        if (bg_pos.x < pos_frame.x)
            bg_pos.x += pos_frame.x - bg_pos.x;

        if ((bg_pos.y + bg_scal_size.y) > (pos_frame.y + scal_size_frame.y))
            bg_pos.y += (pos_frame.y + scal_size_frame.y) - (bg_pos.y + bg_scal_size.y);
        if (bg_pos.y < pos_frame.y)
            bg_pos.y += pos_frame.y - bg_pos.y;

        if ((img_pos.x + img_scal_size.x) > (pos_frame.x + scal_size_frame.x))
            img_size.x += (pos_frame.x + scal_size_frame.x) - (img_pos.x + img_scal_size.x);
        if (img_pos.x < pos_frame.x)
            img_pos.x += pos_frame.x - img_pos.x;

        if ((img_pos.y + img_scal_size.y) > (pos_frame.y + scal_size_frame.y))
            img_pos.y += (pos_frame.y + scal_size_frame.y) - (img_pos.y + img_scal_size.y);
        if (img_pos.y < pos_frame.y)
            img_pos.y += pos_frame.y - img_pos.y;

        // Display items
        if (gui.live_items[app_path][frame.id].contains("background")) {
            ImGui::SetCursorPos(bg_pos);
            ImGui::Image(gui.live_items[app_path][frame.id]["background"][current_item[app_path][frame.id]], bg_scal_size);
        }
        if (gui.live_items[app_path][frame.id].contains("image")) {
            ImGui::SetCursorPos(img_pos);
            ImGui::Image(gui.live_items[app_path][frame.id]["image"][current_item[app_path][frame.id]], img_scal_size);
        }

        // Target link
        if (!target[app_path][frame.id].empty() && (target[app_path][frame.id].find("psts:") == std::string::npos)) {
            ImGui::SetCursorPos(pos_frame);
            ImGui::PushID(frame.id.c_str());
            if (ImGui::Selectable("##target_link", false, ImGuiSelectableFlags_None, scal_size_frame))
                open_path(target[app_path][frame.id]);
            ImGui::PopID();
        }

        // Text
        for (const auto &str_tag : str[app_path][frame.id]) {
            if (!str_tag.text.empty()) {
                std::vector<ImVec4> str_color;

                if (!str_tag.color.empty()) {
                    uint32_t color = 0xFF'FF'FF'FF;

                    if (frame.autoflip)
                        std::istringstream{ str[app_path][frame.id][current_item[app_path][frame.id]].color }.ignore(1, '#') >> std::hex >> color;
                    else
                        std::istringstream{ str_tag.color }.ignore(1, '#') >> std::hex >> color;

                    str_color.emplace_back(((color >> 16) & 0xFF) / 255.f, ((color >> 8) & 0xFF) / 255.f, ((color >> 0) & 0xFF) / 255.f, 1.f);
                } else
                    str_color.emplace_back(1.f, 1.f, 1.f, 1.f);

                auto str_size = scal_size_frame, text_pos = pos_frame;

                // Origin
                if (liveitem[app_path][frame.id]["text"]["origin"].second.empty() || (liveitem[app_path][frame.id]["text"]["origin"].second == "background")) {
                    if (gui.live_items[app_path][frame.id].contains("background")) {
                        str_size = bg_scal_size;
                        text_pos = bg_pos;
                    } else if (!liveitem[app_path][frame.id]["text"]["origin"].second.empty() && gui.live_items[app_path][frame.id].contains("image")) {
                        str_size = img_scal_size;
                        text_pos = img_pos;
                    }
                } else if (liveitem[app_path][frame.id]["text"]["origin"].second == "image") {
                    if (gui.live_items[app_path][frame.id].contains("image")) {
                        str_size = img_scal_size;
                        text_pos = img_pos;
                    } else if (gui.live_items[app_path][frame.id].contains("background")) {
                        str_size = bg_scal_size;
                        text_pos = bg_pos;
                    }
                }

                auto str_wrap = scal_size_frame.x;
                if (liveitem[app_path][frame.id]["text"]["align"].second == "outside-right")
                    str_wrap = str_size.x;

                if (liveitem[app_path][frame.id]["text"]["width"].first > 0) {
                    if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                        str_wrap = static_cast<float>(liveitem[app_path][frame.id]["text"]["width"].first) * SCALE.x;
                    text_pos.x += (str_size.x - (static_cast<float>(liveitem[app_path][frame.id]["text"]["width"].first) * SCALE.x)) / 2.f;
                    str_size.x = static_cast<float>(liveitem[app_path][frame.id]["text"]["width"].first) * SCALE.x;
                }

                if ((liveitem[app_path][frame.id]["text"]["height"].first > 0)
                    && (liveitem[app_path][frame.id]["text"]["word-scroll"].second == "on" || liveitem[app_path][frame.id]["text"]["height"].first <= FRAME_SIZE.y)) {
                    text_pos.y += (str_size.y - (static_cast<float>(liveitem[app_path][frame.id]["text"]["height"].first) * SCALE.y)) / 2.f;
                    str_size.y = static_cast<float>(liveitem[app_path][frame.id]["text"]["height"].first) * SCALE.y;
                }

                const auto size_text_scale = str_tag.size != 0 ? str_tag.size / 19.2f : 1.f;
                ImGui::SetWindowFontScale(size_text_scale);

                // Calculate text pixel size
                ImVec2 calc_text_size;
                if (frame.autoflip > 0) {
                    if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                        calc_text_size = ImGui::CalcTextSize(str[app_path][frame.id][current_item[app_path][frame.id]].text.c_str(), 0, false, str_wrap);
                    else
                        calc_text_size = ImGui::CalcTextSize(str[app_path][frame.id][current_item[app_path][frame.id]].text.c_str());
                } else {
                    if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                        calc_text_size = ImGui::CalcTextSize(str_tag.text.c_str(), 0, false, str_wrap);
                    else
                        calc_text_size = ImGui::CalcTextSize(str_tag.text.c_str());
                }

                /*if (liveitem[app_path][frame.id]["text"]["ellipsis"].second == "on") {
                    // TODO ellipsis
                }*/

                ImVec2 str_pos_init;

                // Align
                if (liveitem[app_path][frame.id]["text"]["align"].second.empty()) {
                    if (liveitem[app_path][frame.id]["text"]["text-align"].second.empty()) {
                        if (liveitem[app_path][frame.id]["text"]["line-align"].second == "left")
                            str_pos_init.x = 0.f;
                        else if (liveitem[app_path][frame.id]["text"]["line-align"].second == "right")
                            str_pos_init.x = str_size.x - calc_text_size.x;
                        else if (liveitem[app_path][frame.id]["text"]["line-align"].second == "outside-right") {
                            text_pos.x += str_size.x;
                            if (liveitem[app_path][frame.id]["text"]["origin"].second == "image") {
                                if (gui.live_items[app_path][frame.id].contains("background"))
                                    str_size.x = bg_scal_size.x - img_scal_size.x - (img_pos.x - bg_pos.x);
                                else {
                                    str_size = scal_size_frame;
                                    text_pos = pos_frame;
                                }
                            }
                        } else
                            str_pos_init.x = (str_size.x - calc_text_size.x) / 2.0f;
                    } else {
                        if ((liveitem[app_path][frame.id]["text"]["text-align"].second == "center")
                            || ((liveitem[app_path][frame.id]["text"]["text-align"].second == "left") && (liveitem[app_path][frame.id]["text"]["x"].first < 0)))
                            str_pos_init.x = (str_size.x - calc_text_size.x) / 2.0f;
                        else if (liveitem[app_path][frame.id]["text"]["text-align"].second == "left")
                            str_pos_init.x = 0.f;
                        else if (liveitem[app_path][frame.id]["text"]["text-align"].second == "right")
                            str_pos_init.x = str_size.x - calc_text_size.x;
                        else if (liveitem[app_path][frame.id]["text"]["text-align"].second == "outside-right") {
                            text_pos.x += str_size.x;
                            if (liveitem[app_path][frame.id]["text"]["origin"].second == "image") {
                                if (gui.live_items[app_path][frame.id].contains("background"))
                                    str_size.x = bg_scal_size.x - img_scal_size.x - (img_pos.x - bg_pos.x);
                                else {
                                    str_size = scal_size_frame;
                                    text_pos = pos_frame;
                                }
                            }
                        }
                    }
                } else {
                    if (liveitem[app_path][frame.id]["text"]["align"].second == "center")
                        str_pos_init.x = (str_size.x - calc_text_size.x) / 2.0f;
                    else if (liveitem[app_path][frame.id]["text"]["align"].second == "left")
                        str_pos_init.x = 0.f;
                    else if (liveitem[app_path][frame.id]["text"]["align"].second == "right")
                        str_pos_init.x = str_size.x - calc_text_size.x;
                    else if (liveitem[app_path][frame.id]["text"]["align"].second == "outside-right") {
                        text_pos.x += str_size.x;
                        if (liveitem[app_path][frame.id]["text"]["origin"].second == "image") {
                            if (gui.live_items[app_path][frame.id].contains("background"))
                                str_size.x = bg_scal_size.x - img_scal_size.x - (img_pos.x - bg_pos.x);
                            else {
                                str_size = scal_size_frame;
                                text_pos = pos_frame;
                            }
                        }
                    }
                }

                // Valign
                if (liveitem[app_path][frame.id]["text"]["valign"].second.empty()) {
                    if (liveitem[app_path][frame.id]["text"]["text-valign"].second.empty() || (liveitem[app_path][frame.id]["text"]["text-valign"].second == "center"))
                        str_pos_init.y = (str_size.y - calc_text_size.y) / 2.f;
                    else if (liveitem[app_path][frame.id]["text"]["text-valign"].second == "bottom")
                        str_pos_init.y = str_size.y - calc_text_size.y;
                    else if (liveitem[app_path][frame.id]["text"]["text-valign"].second == "top")
                        str_pos_init.y = 0.f;
                } else {
                    if ((liveitem[app_path][frame.id]["text"]["valign"].second == "center")
                        || ((liveitem[app_path][frame.id]["text"]["valign"].second == "bottom") && (liveitem[app_path][frame.id]["text"]["y"].first != 0))
                        || ((liveitem[app_path][frame.id]["text"]["valign"].second == "top") && (liveitem[app_path][frame.id]["text"]["y"].first != 0)))
                        str_pos_init.y = (str_size.y - calc_text_size.y) / 2.f;
                    else if (liveitem[app_path][frame.id]["text"]["valign"].second == "bottom")
                        str_pos_init.y = str_size.y - calc_text_size.y;
                    else if (liveitem[app_path][frame.id]["text"]["valign"].second == "top") {
                        str_pos_init.y = 0.f;
                    } else if (liveitem[app_path][frame.id]["text"]["valign"].second == "outside-top") {
                        str_pos_init.y = 0.f;
                    }
                }

                auto pos_str = ImVec2(str_pos_init.x, str_pos_init.y - (liveitem[app_path][frame.id]["text"]["y"].first * SCALE.y));

                if (liveitem[app_path][frame.id]["text"]["x"].first > 0) {
                    text_pos.x += liveitem[app_path][frame.id]["text"]["x"].first * SCALE.x;
                    str_size.x -= liveitem[app_path][frame.id]["text"]["x"].first * SCALE.x;
                }

                if ((liveitem[app_path][frame.id]["text"]["margin-left"].first > 0) && !liveitem[app_path][frame.id]["text"]["width"].first) {
                    text_pos.x += liveitem[app_path][frame.id]["text"]["margin-left"].first * SCALE.x;
                    str_size.x -= liveitem[app_path][frame.id]["text"]["margin-left"].first * SCALE.x;
                }

                if (liveitem[app_path][frame.id]["text"]["margin-right"].first > 0)
                    str_size.x -= liveitem[app_path][frame.id]["text"]["margin-right"].first * SCALE.x;

                if (liveitem[app_path][frame.id]["text"]["margin-top"].first > 0) {
                    text_pos.y += liveitem[app_path][frame.id]["text"]["margin-top"].first * SCALE.y;
                    str_size.y -= liveitem[app_path][frame.id]["text"]["margin-top"].first * SCALE.y;
                }

                // Text Display
                // TODO Multiple color support on same frame, used by eg: Asphalt: Injection
                // TODO Correct display few line on same frame, used by eg: Asphalt: Injection
                ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + text_pos.x, WINDOW_POS.y + text_pos.y));
                ImGui::BeginChild(frame.id.c_str(), str_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
                if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                    ImGui::PushTextWrapPos(str_wrap);
                if (liveitem[app_path][frame.id]["text"]["word-scroll"].second == "on") {
                    static std::map<std::string, std::map<std::string, std::pair<bool, float>>> scroll;
                    if (liveitem[app_path][frame.id]["text"]["word-wrap"].second == "off") {
                        if (scroll[app_path][frame.id].first) {
                            if (scroll[app_path][frame.id].second >= 0.f)
                                scroll[app_path][frame.id].second -= 0.5f;
                            else
                                scroll[app_path][frame.id].first = false;
                        } else {
                            if (scroll[app_path][frame.id].second <= ImGui::GetScrollMaxX())
                                scroll[app_path][frame.id].second += 0.5f;
                            else
                                scroll[app_path][frame.id].first = true;
                        }
                        ImGui::SetScrollX(scroll[app_path][frame.id].second);
                    } else {
                        if (scroll[app_path][frame.id].first) {
                            if (scroll[app_path][frame.id].second >= 0.f)
                                scroll[app_path][frame.id].second -= 0.5f;
                            else
                                scroll[app_path][frame.id].first = false;
                        } else {
                            if (scroll[app_path][frame.id].second <= ImGui::GetScrollMaxY())
                                scroll[app_path][frame.id].second += 0.5f;
                            else
                                scroll[app_path][frame.id].first = true;
                        }
                        ImGui::SetScrollY(scroll[app_path][frame.id].second);
                    }
                }
                ImGui::SetCursorPos(pos_str);
                if (frame.autoflip > 0)
                    ImGui::TextColored(str_color[0], "%s", str[app_path][frame.id][current_item[app_path][frame.id]].text.c_str());
                else
                    ImGui::TextColored(str_color[0], "%s", str_tag.text.c_str());
                if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                    ImGui::PopTextWrapPos();
                ImGui::EndChild();
                ImGui::SetWindowFontScale(1.f);
            }
        }
    }

    ImGui::SetWindowFontScale(1.f);
    const auto default_font_scale = (25.f * emuenv.manual_dpi_scale) * (ImGui::GetFontSize() / (19.2f * emuenv.manual_dpi_scale));
    const auto font_size_scale = default_font_scale / ImGui::GetFontSize();

    const auto gate_pos = items_styles[app_type].gate_pos;
    const std::string BUTTON_STR = app_path == emuenv.io.title_id ? gui.lang.live_area.main["continue"] : gui.lang.live_area.main["start"];

    const auto GATE_SIZE = ImVec2(280.0f * SCALE.x, 158.0f * SCALE.y);
    const auto GATE_POS = ImVec2(WINDOW_SIZE.x - (gate_pos.x * SCALE.x), WINDOW_SIZE.y - (gate_pos.y * SCALE.y));
    const ImVec2 GATE_POS_MIN(WINDOW_POS.x + GATE_POS.x, WINDOW_POS.y + GATE_POS.y);
    const ImVec2 GATE_POS_MAX(GATE_POS_MIN.x + GATE_SIZE.x, GATE_POS_MIN.y + GATE_SIZE.y);

    const auto START_SIZE = ImVec2((ImGui::CalcTextSize(BUTTON_STR.c_str()).x * font_size_scale), (ImGui::CalcTextSize(BUTTON_STR.c_str()).y * font_size_scale));
    const auto START_BUTTON_SIZE = ImVec2(START_SIZE.x + 26.0f * SCALE.x, START_SIZE.y + 5.0f * SCALE.y);
    const auto POS_BUTTON = ImVec2((GATE_POS.x + (GATE_SIZE.x - START_BUTTON_SIZE.x) / 2.0f), (GATE_POS.y + (GATE_SIZE.y - START_BUTTON_SIZE.y) / 1.08f));
    const auto POS_START = ImVec2(WINDOW_POS.x + POS_BUTTON.x + (START_BUTTON_SIZE.x - START_SIZE.x) / 2.f, WINDOW_POS.y + POS_BUTTON.y + (START_BUTTON_SIZE.y - START_SIZE.y) / 2.f);
    const auto SELECT_SIZE = ImVec2(GATE_SIZE.x - (10.f * SCALE.x), GATE_SIZE.y - (5.f * SCALE.y));
    const auto SELECT_POS = ImVec2(GATE_POS.x + (5.f * SCALE.y), GATE_POS.y + (2.f * SCALE.y));

    const auto BUTTON_SIZE = ImVec2(72.f * SCALE.x, 30.f * SCALE.y);

    if (gui.live_area_contents[app_path].contains("gate")) {
        ImGui::SetCursorPos(GATE_POS);
        ImGui::Image(gui.live_area_contents[app_path]["gate"], GATE_SIZE);
    } else {
        // Draw background of gate
        window_draw_list->AddRectFilled(GATE_POS_MIN, GATE_POS_MAX, IM_COL32(47, 51, 50, 255), 10.0f * SCALE.x, ImDrawFlags_RoundCornersAll);

        const auto ICON_SIZE_SCALE = 94.f * SCALE.x;
        const auto ICON_CENTER_POS = ImVec2(GATE_POS_MIN.x + (GATE_SIZE.x / 2.f), GATE_POS_MIN.y + (15.5f * SCALE.y) + (ICON_SIZE_SCALE / 2.f));
        const auto ICON_POS_MINI_SCALE = ImVec2(ICON_CENTER_POS.x - (ICON_SIZE_SCALE / 2.f), ICON_CENTER_POS.y - (ICON_SIZE_SCALE / 2.f));
        const auto ICON_POS_MAX_SCALE = ImVec2(ICON_POS_MINI_SCALE.x + ICON_SIZE_SCALE, ICON_POS_MINI_SCALE.y + ICON_SIZE_SCALE);

        // check if app icon exist
        auto &APP_ICON_TYPE = app_path.starts_with("NPXS") && (app_path != "NPXS10007") ? gui.app_selector.sys_apps_icon : gui.app_selector.user_apps_icon;
        if (APP_ICON_TYPE.contains(app_path)) {
            window_draw_list->AddImageRounded(APP_ICON_TYPE[app_path], ICON_POS_MINI_SCALE, ICON_POS_MAX_SCALE,
                ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 75.f * SCALE.x, ImDrawFlags_RoundCornersAll);
        } else
            window_draw_list->AddCircleFilled(ICON_CENTER_POS, ICON_SIZE_SCALE / 2.f, IM_COL32_WHITE);
    }
    ImGui::PushID(app_path.c_str());
    const ImVec2 BUTTON_POS_MIN(WINDOW_POS.x + POS_BUTTON.x, WINDOW_POS.y + POS_BUTTON.y);
    window_draw_list->AddRectFilled(BUTTON_POS_MIN, ImVec2(BUTTON_POS_MIN.x + START_BUTTON_SIZE.x, BUTTON_POS_MIN.y + START_BUTTON_SIZE.y), IM_COL32(20, 168, 222, 255), 10.0f * SCALE.x, ImDrawFlags_RoundCornersAll);
    window_draw_list->AddText(gui.vita_font, default_font_scale, POS_START, IM_COL32(255, 255, 255, 255), BUTTON_STR.c_str());
    ImGui::SetCursorPos(SELECT_POS);
    if (ImGui::Selectable("##gate", gui.is_nav_button && (live_area_type_selected == GATE), ImGuiSelectableFlags_None, SELECT_SIZE))
        pre_run_app(gui, emuenv, app_path);
    ImGui::PopID();
    window_draw_list->AddRect(GATE_POS_MIN, GATE_POS_MAX, IM_COL32(192, 192, 192, 255), 10.f * SCALE.x, ImDrawFlags_RoundCornersAll, 12.f * SCALE.x);

    if (app_device == VitaIoDevice::ux0) {
        const auto widget_scal_size = ImVec2(80.0f * SCALE.x, 80.f * SCALE.y);
        const auto manual_path{ emuenv.pref_path / "ux0/app" / app_path / "sce_sys/manual/" };
        const auto scal_widget_font_size = 23.0f / ImGui::GetFontSize();

        const auto manual_exist = fs::exists(manual_path) && !fs::is_empty(manual_path);
        const auto search_pos = ImVec2((manual_exist ? 633.f : 578.f) * SCALE.x, 505.0f * SCALE.y);
        const auto pos_scal_search = ImVec2(WINDOW_SIZE.x - search_pos.x, WINDOW_SIZE.y - search_pos.y);
        const ImVec2 SEARCH_WIDGET_POS_MIN(WINDOW_POS.x + pos_scal_search.x, WINDOW_POS.y + pos_scal_search.y);
        const char *SEARCH_STR = "Search";
        const auto SEARCH_SCAL_SIZE = ImVec2((ImGui::CalcTextSize(SEARCH_STR).x * scal_widget_font_size) * SCALE.x, (ImGui::CalcTextSize(SEARCH_STR).y * scal_widget_font_size) * SCALE.y);
        const auto POS_STR_SEARCH = ImVec2(SEARCH_WIDGET_POS_MIN.x + ((widget_scal_size.x / 2.f) - (SEARCH_SCAL_SIZE.x / 2.f)),
            SEARCH_WIDGET_POS_MIN.y + ((widget_scal_size.x / 2.f) - (SEARCH_SCAL_SIZE.y / 2.f)));
        window_draw_list->AddRectFilled(SEARCH_WIDGET_POS_MIN, ImVec2(SEARCH_WIDGET_POS_MIN.x + widget_scal_size.x, SEARCH_WIDGET_POS_MIN.y + widget_scal_size.y), IM_COL32(10, 169, 246, 255), 12.0f * SCALE.x, ImDrawFlags_RoundCornersAll);
        window_draw_list->AddText(gui.vita_font, 23.0f * SCALE.x, POS_STR_SEARCH, IM_COL32(255, 255, 255, 255), SEARCH_STR);
        ImGui::SetCursorPos(pos_scal_search);
        if (ImGui::Selectable("##Search", gui.is_nav_button && (live_area_type_selected == SEARCH), ImGuiSelectableFlags_None, widget_scal_size))
            open_search(get_app_index(gui, app_path)->title);

        if (manual_exist) {
            const auto manual_pos = ImVec2(520.f * SCALE.x, 505.0f * SCALE.y);
            const auto pos_scal_manual = ImVec2(WINDOW_SIZE.x - manual_pos.x, WINDOW_SIZE.y - manual_pos.y);

            const char *MANUAL_STR = "Manual";
            const auto MANUAL_STR_SCAL_SIZE = ImVec2((ImGui::CalcTextSize(MANUAL_STR).x * scal_widget_font_size) * SCALE.x, (ImGui::CalcTextSize(MANUAL_STR).y * scal_widget_font_size) * SCALE.y);
            const ImVec2 MANUAL_WIDGET_POS_MIN(WINDOW_POS.x + pos_scal_manual.x, WINDOW_POS.y + pos_scal_manual.y);
            const auto MANUAL_STR_POS = ImVec2(MANUAL_WIDGET_POS_MIN.x + ((widget_scal_size.x / 2.f) - (MANUAL_STR_SCAL_SIZE.x / 2.f)),
                MANUAL_WIDGET_POS_MIN.y + ((widget_scal_size.x / 2.f) - (MANUAL_STR_SCAL_SIZE.y / 2.f)));
            window_draw_list->AddRectFilled(MANUAL_WIDGET_POS_MIN, ImVec2(MANUAL_WIDGET_POS_MIN.x + widget_scal_size.x, MANUAL_WIDGET_POS_MIN.y + widget_scal_size.y), IM_COL32(202, 0, 106, 255), 12.0f * SCALE.x, ImDrawFlags_RoundCornersAll);
            window_draw_list->AddText(gui.vita_font, 23.0f * SCALE.x, MANUAL_STR_POS, IM_COL32(255, 255, 255, 255), MANUAL_STR);
            ImGui::SetCursorPos(pos_scal_manual);
            if (ImGui::Selectable("##manual", gui.is_nav_button && (live_area_type_selected == MANUAL), ImGuiSelectableFlags_None, widget_scal_size))
                open_manual(gui, emuenv, app_path);
        }

        const auto update_pos = ImVec2((manual_exist ? 408.f : 463.f) * SCALE.x, 505.0f * SCALE.y);
        const auto pos_scal_update = ImVec2(WINDOW_SIZE.x - update_pos.x, WINDOW_SIZE.y - update_pos.y);

        const auto UPDATE_STR = "Update";
        const auto UPDATE_STR_SCAL_SIZE = ImVec2((ImGui::CalcTextSize(UPDATE_STR).x * scal_widget_font_size) * SCALE.x, (ImGui::CalcTextSize(UPDATE_STR).y * scal_widget_font_size) * SCALE.y);
        const ImVec2 UPDATE_WIDGET_POS_MIN(WINDOW_POS.x + pos_scal_update.x, WINDOW_POS.y + pos_scal_update.y);
        const auto UPDATE_STR_POS = ImVec2(UPDATE_WIDGET_POS_MIN.x + ((widget_scal_size.x / 2.f) - (UPDATE_STR_SCAL_SIZE.x / 2.f)),
            UPDATE_WIDGET_POS_MIN.y + ((widget_scal_size.x / 2.f) - (UPDATE_STR_SCAL_SIZE.y / 2.f)));
        window_draw_list->AddRectFilled(UPDATE_WIDGET_POS_MIN, ImVec2(UPDATE_WIDGET_POS_MIN.x + widget_scal_size.x, UPDATE_WIDGET_POS_MIN.y + widget_scal_size.y), IM_COL32(3, 187, 250, 255), 12.0f * SCALE.x, ImDrawFlags_RoundCornersAll);
        window_draw_list->AddText(gui.vita_font, 23.0f * SCALE.x, UPDATE_STR_POS, IM_COL32(255, 255, 255, 255), UPDATE_STR);
        ImGui::SetCursorPos(pos_scal_update);
        if (ImGui::Selectable("##update", ImGuiSelectableFlags_None, false, widget_scal_size))
            update_app(gui, emuenv, app_path);
    }

    auto &lang = gui.lang.live_area.help;
    auto &common = emuenv.common_dialog.lang.common;

    if (!gui.vita_area.content_manager && !gui.vita_area.manual) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f * SCALE.x);
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - (60.0f * SCALE.x) - BUTTON_SIZE.x, 12.0f * SCALE.y));
        if (ImGui::Button("Esc", BUTTON_SIZE))
            close_live_area_app(gui, emuenv, app_path);
        ImGui::SetCursorPos(ImVec2(60.f * SCALE.x, 12.0f * SCALE.y));
        if (ImGui::Button("Help", BUTTON_SIZE))
            ImGui::OpenPopup("Live Area Help");
        ImGui::SetNextWindowPos(ImVec2(WINDOW_SIZE.x / 2.f, WINDOW_SIZE.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Live Area Help", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
            TextColoredCentered(GUI_COLOR_TEXT_TITLE, gui.lang.main_menubar.help["title"].c_str());
            ImGui::Spacing();
            auto TextColoredCenteredEx = [](GuiState &gui, const ImVec4 &col, const char *text_id) {
                TextColoredCentered(col, gui.lang.live_area.help[text_id].c_str());
                ImGui::Spacing();
            };
            TextColoredCenteredEx(gui, GUI_COLOR_TEXT, "control_setting");
            if (gui.modules.empty())
                TextColoredCenteredEx(gui, GUI_COLOR_TEXT, "firmware_not_detected");
            if (!gui.fw_font)
                TextColoredCenteredEx(gui, GUI_COLOR_TEXT, "firmware_font_not_detected");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["live_area_help"].c_str());
            ImGui::Spacing();
            auto draw_help_table = [](GuiState &gui, const char *const *items, size_t items_count) {
                auto &lang = gui.lang.live_area.help;
                // here I just need any unique value
                if (ImGui::BeginTable(log_hex(reinterpret_cast<uintptr_t>(items)).c_str(), 2)) {
                    ImGui::TableSetupColumn("01");
                    ImGui::TableSetupColumn("02");
                    for (int i = 0; i < items_count; i++) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang[items[i]].c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang[std::string(items[i]) + "_control"].c_str());
                    }
                    ImGui::EndTable();
                }
            };
            constexpr std::array<const char *, 4> help_items_01{ "browse_app", "start_app", "show_hide", "exit_livearea" };
            draw_help_table(gui, help_items_01.data(), help_items_01.size());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["manual_help"].c_str());
            ImGui::Spacing();
            constexpr std::array<const char *, 3> help_items_02{ "browse_page", "hide_show", "exit_manual" };
            draw_help_table(gui, help_items_02.data(), help_items_02.size());

            ImGui::Spacing();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2.f - (BUTTON_SIZE.x / 2.f));
            if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(ImGui_ImplSdl_ScancodeToImGuiKey(emuenv.cfg.keyboard_button_cross)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
    }

    const auto SELECTABLE_SIZE = ImVec2(50.f * SCALE.x, 60.f * SCALE.y);

    const auto ARROW_HEIGHT_POS = WINDOW_SIZE.y - (250.f * SCALE.y);
    const auto ARROW_HEIGHT_DRAW_POS = WINDOW_POS.y + ARROW_HEIGHT_POS;
    const auto ARROW_WIDTH_POS = (30.f * SCALE.x);
    const auto ARROW_SELECT_HEIGHT_POS = ARROW_HEIGHT_POS - (SELECTABLE_SIZE.y / 2.f);

    // Draw left arrow
    const auto ARROW_LEFT_CENTER = ImVec2(WINDOW_POS.x + ARROW_WIDTH_POS, ARROW_HEIGHT_DRAW_POS);
    window_draw_list->AddTriangleFilled(
        ImVec2(ARROW_LEFT_CENTER.x + (16.f * SCALE.x), ARROW_LEFT_CENTER.y - (20.f * SCALE.y)),
        ImVec2(ARROW_LEFT_CENTER.x - (16.f * SCALE.x), ARROW_LEFT_CENTER.y),
        ImVec2(ARROW_LEFT_CENTER.x + (16.f * SCALE.x), ARROW_LEFT_CENTER.y + (20.f * SCALE.y)), ARROW_COLOR);
    ImGui::SetCursorPos(ImVec2(ARROW_WIDTH_POS - (SELECTABLE_SIZE.x / 2.f), ARROW_SELECT_HEIGHT_POS));
    if (ImGui::Selectable("##left", false, ImGuiSelectableFlags_None, SELECTABLE_SIZE)) {
        if (gui.live_area_app_current_open == 0) {
            gui.vita_area.live_area_screen = false;
            gui.vita_area.home_screen = true;
        }
        --gui.live_area_app_current_open;
    }

    // Draw right arrow
    if (gui.live_area_app_current_open < gui.live_area_current_open_apps_list.size() - 1) {
        const auto ARROW_RIGHT_CENTER = ImVec2(WINDOW_POS.x + WINDOW_SIZE.x - ARROW_WIDTH_POS, ARROW_HEIGHT_DRAW_POS);
        window_draw_list->AddTriangleFilled(
            ImVec2(ARROW_RIGHT_CENTER.x - (16.f * SCALE.x), ARROW_RIGHT_CENTER.y - (20.f * SCALE.y)),
            ImVec2(ARROW_RIGHT_CENTER.x + (16.f * SCALE.x), ARROW_RIGHT_CENTER.y),
            ImVec2(ARROW_RIGHT_CENTER.x - (16.f * SCALE.x), ARROW_RIGHT_CENTER.y + (20.f * SCALE.y)), ARROW_COLOR);
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - ARROW_WIDTH_POS - (SELECTABLE_SIZE.x / 2.f), ARROW_SELECT_HEIGHT_POS));
        if (ImGui::Selectable("##right", false, ImGuiSelectableFlags_None, SELECTABLE_SIZE))
            ++gui.live_area_app_current_open;
    }
    ImGui::SetWindowFontScale(1.f);
    ImGui::End();
    ImGui::PopStyleVar(3);
}

} // namespace gui
