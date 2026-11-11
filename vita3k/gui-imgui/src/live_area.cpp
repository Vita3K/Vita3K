// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <app/functions.h>
#include <config/state.h>
#include <ctrl/ctrl.h>
#include <dialog/state.h>
#include <gui-imgui/functions.h>

#include <io/device.h>
#include <io/state.h>

#include <kernel/state.h>

#include <packages/license.h>
#include <packages/sce_types.h>

#include <renderer/state.h>

#include <io/VitaIoDevice.h>
#include <io/vfs.h>

#include <util/log.h>
#include <util/net_utils.h>
#include <vfs/appmeta.h>

#include <pugixml.hpp>

#include <chrono>
#include <cmath>
#include <imgui_internal.h>
#include <mutex>
#include <numbers>
#include <sstream>
#include <stb_image.h>

void GateAnimation::start(GateAnimationState anim_state) {
    state = anim_state;
    progress = 0.f;
    launch_request.reset();
    start_time = std::chrono::steady_clock::now();
}

void GateAnimation::update() {
    if (state == GateAnimationState::None)
        return;

    float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - start_time).count();

    progress = std::min(elapsed / duration, 1.0f);

    if (progress >= 1.0f) {
        if (state == GateAnimationState::EnterApp)
            state = GateAnimationState::PreRunApp;
        else
            state = GateAnimationState::None;
    }
}

namespace gui {

bool get_sys_apps_state(GuiState &gui) {
    return !gui.vita_area.content_manager && !gui.vita_area.settings && !gui.vita_area.trophy_collection && !gui.vita_area.friends && !gui.vita_area.friend_profile && !gui.vita_area.friend_activity && !gui.vita_area.messages && !gui.vita_area.manual;
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
    const auto is_sys_app = is_emu_app(app_path);
    const auto is_shell = app_path == "emu:vsh/shell";
    const auto APP_INDEX = get_app_index(gui, emuenv, app_path);
    if (!APP_INDEX)
        return;
    const auto TITLE_ID = APP_INDEX->title_id;
    const auto is_ps_app = TITLE_ID.starts_with("PCS") || TITLE_ID == "NPXS10007";
    const auto asset_path = app_asset_path(app_path);

    get_license(emuenv, APP_INDEX->title_id, APP_INDEX->content_id);

    if (!gui.live_area_contents.contains(app_path)) {
        auto default_contents = false;
        const auto fw_path{ emuenv.vita_fs_path / "vs0" };
        const auto default_fw_contents{ fw_path / "data/internal/livearea/default/sce_sys/livearea/contents/template.xml" };
        const auto emu_path = is_shell ? emuenv.vita_fs_path / "vs0/vsh/shell" : emuenv.vita_fs_path / "vs0/app" / TITLE_ID;
        pugi::xml_document doc;
        vfs::FileBuffer template_data;
        bool retail = false;
        if (emuenv.license.rif[TITLE_ID].sku_flag == 3) {
            if (is_sys_app)
                retail = fs::exists(emu_path / "sce_sys/retail/livearea");
            else
                retail = vfs::read_app_livearea_file(template_data, emuenv.vita_fs_path, asset_path, "sce_sys/retail/livearea/contents/template.xml", TITLE_ID);
        }
        const auto live_area_path = fs::path("sce_sys") / (retail ? "retail/livearea" : "livearea");
        auto template_xml = emu_path / live_area_path / "contents/template.xml";
        bool loaded = false;
        if (is_sys_app) {
            loaded = doc.load_file(template_xml.c_str());
        } else {
            if (!retail)
                vfs::read_app_livearea_file(template_data, emuenv.vita_fs_path, asset_path, live_area_path / "contents/template.xml", TITLE_ID);
            loaded = !template_data.empty() && doc.load_buffer(template_data.data(), template_data.size());
        }
        if (!loaded) {
            if (!is_shell && (is_ps_app || is_sys_app))
                LOG_WARN("Live Area Contents is corrupted or missing for title: {} '{}' at {}.", APP_INDEX->title_id, APP_INDEX->title, live_area_path / "contents/template.xml");
            if (doc.load_file(default_fw_contents.c_str())) {
                template_xml = default_fw_contents;
                default_contents = true;
                LOG_INFO_IF(!is_shell, "Using default firmware contents.");
            } else {
                type[app_path] = "a1";
                LOG_WARN("Default firmware contents is corrupted or missing, install firmware for fix it.");
                return;
            }
        }

        {
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
                    vfs::read_file(VitaIoDevice::vs0, buffer, emuenv.vita_fs_path, "data/internal/livearea/default/sce_sys/livearea/contents/" + contents.second);
                else if (is_sys_app)
                    fs_utils::read_data(template_xml.parent_path() / contents.second, buffer);
                else
                    vfs::read_app_livearea_file(buffer, emuenv.vita_fs_path, asset_path, live_area_path / "contents" / contents.second, TITLE_ID);

                if (buffer.empty()) {
                    if (is_ps_app || is_sys_app)
                        LOG_WARN("Contents {} '{}' Not found for title {} [{}].", contents.first, contents.second, app_path, APP_INDEX->title);
                    continue;
                }
                stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                if (!data) {
                    LOG_ERROR("Invalid Live Area Contents '{}' for title {} [{}].", contents.second, app_path, APP_INDEX->title);
                    continue;
                }

                gui.live_area_contents[app_path][contents.first] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
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

                            if (is_sys_app || default_contents)
                                fs_utils::read_data(template_xml.parent_path() / bg_name, buffer);
                            else
                                vfs::read_app_livearea_file(buffer, emuenv.vita_fs_path, asset_path, live_area_path / "contents" / bg_name, TITLE_ID);

                            if (buffer.empty()) {
                                if (is_ps_app || is_sys_app)
                                    LOG_WARN("background, Id: {}, Name: '{}', Not found for title: {} [{}].", item.first, bg_name, app_path, APP_INDEX->title);
                                continue;
                            }
                            stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
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

                            if (is_sys_app || default_contents)
                                fs_utils::read_data(template_xml.parent_path() / img_name, buffer);
                            else
                                vfs::read_app_livearea_file(buffer, emuenv.vita_fs_path, asset_path, live_area_path / "contents" / img_name, TITLE_ID);

                            if (buffer.empty()) {
                                if (is_ps_app || is_sys_app)
                                    LOG_WARN("Image, Id: {} Name: '{}', Not found for title {} [{}].", item.first, img_name, app_path, APP_INDEX->title);
                                continue;
                            }
                            stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
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
    if (type[app_path].empty() || !items_styles.contains(type[app_path])) {
        LOG_WARN("Type of style {} not found for: {}", type[app_path], app_path);
        type[app_path] = "a1";
    }

    if (app_path.starts_with("ux0:app/")) {
        const bool has_update = check_has_patch(emuenv, APP_INDEX->title_id, APP_INDEX->app_ver).value_or(false);
        std::lock_guard lock(gui.update_state_mutex);
        gui.app_has_update[app_path] = has_update;
    }
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

std::mutex refresh_animation_mutex;
struct RefreshAnimationInfo {
    std::chrono::steady_clock::time_point start_time;
    bool stop_requested = false;
    float stop_after_turns = 0.f;
};

std::map<std::string, RefreshAnimationInfo> refresh_animation_states;

void refresh_and_check_patch(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const auto app = get_app_index(gui, emuenv, app_path);
    if (!app) {
        LOG_ERROR("Cannot check updates: app index not found for '{}'.", app_path);
        return;
    }

    destroy_update_check_thread(gui);
    {
        std::lock_guard lock(gui.update_state_mutex);
        gui.app_has_update.erase(app_path);
    }
    {
        std::lock_guard lock(refresh_animation_mutex);
        refresh_animation_states[app_path] = { std::chrono::steady_clock::now(), false, 0.f };
    }

    const auto title_id = app->title_id;
    const auto title = app->title;
    const auto version = app->app_ver;
    gui.update_check_thread = std::jthread([&gui, &emuenv, app_path, title_id, title, version](std::stop_token stop_token) {
        const auto stop_refresh_animation = [&]() {
            std::lock_guard lock(refresh_animation_mutex);
            const auto refresh_it = refresh_animation_states.find(app_path);
            if (refresh_it == refresh_animation_states.end())
                return;

            const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - refresh_it->second.start_time).count();
            refresh_it->second.stop_requested = true;
            refresh_it->second.stop_after_turns = std::max(1.f, std::ceil(elapsed));
        };

        const auto url = resolve_ver_xml_url(title_id);
        if (url.empty()) {
            LOG_ERROR("Failed to resolve update metadata URL for {} [{}].", title, title_id);
            stop_refresh_animation();
            return;
        }

        const auto bgdl_path = emuenv.vita_fs_path / "ux0/bgdl";
        std::error_code error;
        fs::create_directories(bgdl_path, error);
        if (error) {
            LOG_ERROR("Failed to create update metadata directory '{}': {}", bgdl_path, error.message());
            stop_refresh_animation();
            return;
        }
        const auto version_xml_path = bgdl_path / fs::path(url).filename();
        fs::remove(version_xml_path, error);
        if (error) {
            LOG_ERROR("Failed to remove stale update metadata '{}': {}", version_xml_path, error.message());
            stop_refresh_animation();
            return;
        }
        net_utils::ProgressState progress_state;
        const net_utils::ProgressCallback callback = [&](float, uint64_t, uint64_t) {
            if (stop_token.stop_requested()) {
                std::lock_guard lock(progress_state.mutex);
                progress_state.canceled = true;
                progress_state.pause = false;
                progress_state.cv.notify_all();
            }
            return &progress_state;
        };
        const auto downloaded = net_utils::download_file_ex(url, fs_utils::path_to_utf8(version_xml_path), callback);
        if (stop_token.stop_requested()) {
            stop_refresh_animation();
            return;
        }
        if (downloaded.http_code == 404 || downloaded.http_code == 410) {
            stop_refresh_animation();
            return;
        }
        if (!downloaded.is_success()) {
            LOG_WARN("Failed to download update metadata for {} [{}] (HTTP {}, curl {}).", title, title_id, downloaded.http_code, downloaded.curl_res);
            stop_refresh_animation();
            return;
        }

        const auto has_update = check_has_patch(emuenv, title_id, version);
        if (!has_update) {
            stop_refresh_animation();
            return;
        }
        {
            std::lock_guard lock(gui.update_state_mutex);
            gui.app_has_update[app_path] = *has_update;
        }
        push_update_notification(gui, title_id, title, *has_update ? gui.lang.patch_check["new_app_version_available"] : gui.lang.patch_check["latest_version_installed"]);
        stop_refresh_animation();
    });
}

void destroy_update_check_thread(GuiState &gui) {
    if (gui.update_check_thread.joinable()) {
        gui.update_check_thread.request_stop();
        gui.update_check_thread.join();
    }
}

void refresh_live_area(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    gui.live_area_contents.erase(app_path);
    gui.live_items.erase(app_path);
    if (get_live_area_current_open_apps_list_index(gui, app_path) != gui.live_area_current_open_apps_list.end())
        init_live_area(gui, emuenv, app_path);

    const auto app = get_app_index(gui, emuenv, app_path);
    const auto id = app ? app->title_id : fs::path(app_path).filename().string();
    const auto install = gui.updates_install.find(id);
    if (install != gui.updates_install.end() && install->second.state == UpdateState::SUCCESS)
        gui.updates_install.erase(install);
    {
        std::lock_guard lock(gui.update_state_mutex);
        gui.app_has_update.erase(app_path);
    }
}

void close_live_area_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const std::string closed_app_path = app_path;
    const bool is_current_app = app_launch_path(closed_app_path) == emuenv.io.app_path;
    if (is_current_app)
        app::update_app_time_used(emuenv, emuenv.io.app_path);

    if (!gui.vita_area.online_storage) {
        const auto app_index = get_live_area_current_open_apps_list_index(gui, closed_app_path);
        if (app_index != gui.live_area_current_open_apps_list.end()) {
            const auto removed_index = static_cast<int32_t>(std::distance(gui.live_area_current_open_apps_list.begin(), app_index));
            const bool defer_removal_for_close_animation = gui.live_area_app_current_open == removed_index;
            if (defer_removal_for_close_animation) {
                gui.live_area_app_closing = closed_app_path;
                gui.live_area_app_current_open = removed_index - 1;
                gui.vita_area.home_screen = true;
                gui.vita_area.information_bar = true;
            } else {
                gui.live_area_current_open_apps_list.erase(app_index);
                if (gui.live_area_current_open_apps_list.empty())
                    gui.live_area_app_current_open = -1;
                else {
                    if (gui.live_area_app_current_open > removed_index)
                        --gui.live_area_app_current_open;
                    if (gui.live_area_app_current_open < 0 || gui.live_area_app_current_open >= static_cast<int32_t>(gui.live_area_current_open_apps_list.size()))
                        gui.live_area_app_current_open = 0;
                }
                gui.vita_area.home_screen = true;
                gui.vita_area.information_bar = true;
            }
        }
    }

    if (is_current_app) {
        app::request_in_process_launch(emuenv, AppLaunchRequest{
                                                   .app_path = app_launch_path(closed_app_path),
                                                   .self_path = emuenv.self_path,
                                                   .reason = AppLaunchReason::ProcessExit,
                                               });
        // make sure we are not stuck waiting for a gpu command
        emuenv.renderer->should_display = true;
    }
}

enum LiveAreaType {
    GATE,
    UPDATE,
    SEARCH,
    MANUAL,
    REFRESH,
    CLOUD
};

struct ButtonEntry {
    std::string title;
    LiveAreaType type;
    std::function<void()> callback;
};

static constexpr ImU32 ARROW_COLOR = 0xFFFFFFFF; // White
static LiveAreaType live_area_type_selected = GATE;

bool has_live_area_update(GuiState &gui, const std::string &app_path, const std::string &title_id) {
    {
        std::lock_guard lock(gui.update_state_mutex);
        const auto update = gui.app_has_update.find(app_path);
        if (update != gui.app_has_update.end() && update->second)
            return true;
    }
    if (title_id.empty())
        return false;
    const auto install = gui.updates_install.find(title_id);
    return install != gui.updates_install.end()
        && (install->second.state == UpdateState::DOWNLOADING || install->second.state == UpdateState::WAITING_INSTALL || install->second.state == UpdateState::FAILED);
}

void browse_live_area_apps_list(GuiState &gui, EmuEnvState &emuenv, const uint32_t button, app::AppSessionController *session) {
    if (!gui.live_area_app_closing.empty())
        return;

    if (gui.live_area_app_current_open < 0 || gui.live_area_app_current_open >= static_cast<int32_t>(gui.live_area_current_open_apps_list.size())) {
        LOG_ERROR("Cannot navigate LiveArea: invalid open app index {}.", gui.live_area_app_current_open);
        return;
    }

    const auto &APP_PATH = gui.live_area_current_open_apps_list[gui.live_area_app_current_open];
    const auto app_index = get_app_index(gui, emuenv, APP_PATH);
    const auto manual_found = !is_emu_app(APP_PATH) && vfs::app_dir_has_entries(emuenv.vita_fs_path, APP_PATH, "sce_sys/manual", app_index ? app_index->title_id : APP_PATH);
    const bool has_update = has_live_area_update(gui, APP_PATH, app_index ? app_index->title_id : std::string{});
    const bool has_refresh = app_index && app_index->title_id.starts_with("PCS");
    const bool has_cloud = has_refresh;
    std::vector<LiveAreaType> nav_widgets;
    if (has_update)
        nav_widgets.push_back(UPDATE);
    nav_widgets.push_back(SEARCH);
    if (manual_found)
        nav_widgets.push_back(MANUAL);
    if (has_refresh)
        nav_widgets.push_back(REFRESH);
    if (has_cloud)
        nav_widgets.push_back(CLOUD);

    if (!gui.is_nav_button) {
        if ((live_area_type_selected == MANUAL) && !manual_found)
            live_area_type_selected = GATE;
        if (live_area_type_selected != GATE && std::find(nav_widgets.begin(), nav_widgets.end(), live_area_type_selected) == nav_widgets.end())
            live_area_type_selected = GATE;
        gui.is_nav_button = true;
        return;
    }

    const auto live_area_current_open_apps_list_size = static_cast<int32_t>(gui.live_area_current_open_apps_list.size() - 1);

    const auto cancel = [&]() {
        close_live_area_app(gui, emuenv, APP_PATH);
    };
    const auto confirm = [&]() {
        switch (live_area_type_selected) {
        case GATE:
            if (emuenv.io.app_path.empty() || (app_launch_path(APP_PATH) == emuenv.io.app_path))
                gui.gate_animation.start(GateAnimationState::EnterApp);
            else
                pre_run_app(gui, emuenv, APP_PATH, session);
            break;
        case UPDATE:
            process_live_area_update(gui, emuenv, APP_PATH);
            break;
        case SEARCH:
            if (const auto app = get_app_index(gui, emuenv, APP_PATH))
                open_search(app->title);
            break;
        case MANUAL:
            open_manual(gui, emuenv, APP_PATH);
            break;
        case REFRESH:
            refresh_and_check_patch(gui, emuenv, APP_PATH);
            break;
        case CLOUD:
            if (const auto app = get_app_index(gui, emuenv, APP_PATH))
                open_online_storage(gui, emuenv, app->title_id);
            break;
        default:
            break;
        }
    };

    switch (button) {
    case SCE_CTRL_UP: {
        if (manual_found)
            live_area_type_selected = MANUAL;
        else if (live_area_type_selected == GATE && app_index && !is_emu_app(APP_PATH))
            live_area_type_selected = nav_widgets[nav_widgets.size() / 2];
        break;
    }
    case SCE_CTRL_DOWN:
        live_area_type_selected = GATE;
        break;
    case SCE_CTRL_LEFT:
    case SCE_CTRL_L1:
        if (button & SCE_CTRL_LEFT) {
            const auto current = std::find(nav_widgets.begin(), nav_widgets.end(), live_area_type_selected);
            if ((button & SCE_CTRL_LEFT) && current != nav_widgets.end() && current != nav_widgets.begin())
                live_area_type_selected = *std::prev(current);
            else {
                gui.live_area_app_current_open = std::max(gui.live_area_app_current_open - 1, -1);
                gui.vita_area.home_screen = true;
                live_area_type_selected = GATE;
            }
        } else {
            gui.live_area_app_current_open = std::max(gui.live_area_app_current_open - 1, -1);
            gui.vita_area.home_screen = true;
            live_area_type_selected = GATE;
        }
        break;
    case SCE_CTRL_RIGHT:
    case SCE_CTRL_R1: {
        const auto current = std::find(nav_widgets.begin(), nav_widgets.end(), live_area_type_selected);
        if ((button & SCE_CTRL_RIGHT) && current != nav_widgets.end() && std::next(current) != nav_widgets.end())
            live_area_type_selected = *std::next(current);
        else if (gui.live_area_app_current_open < live_area_current_open_apps_list_size) {
            ++gui.live_area_app_current_open;
            live_area_type_selected = GATE;
        }
    } break;
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

void draw_live_area_screen(GuiState &gui, EmuEnvState &emuenv, app::AppSessionController *session, const int32_t app_index) {
    const int32_t displayed_app_index = app_index;
    if (displayed_app_index < 0 || displayed_app_index >= static_cast<int32_t>(gui.live_area_current_open_apps_list.size()))
        return;

    const auto &viewport = sdl::get_viewport();
    const ImVec2 VIEWPORT_SIZE(viewport.logical_width, viewport.logical_height);
    const ImVec2 VIEWPORT_POS(viewport.logical_x, viewport.logical_y);
    const auto RES_SCALE = ImVec2(viewport.scale_x, viewport.scale_y);
    const auto SCALE = ImVec2(RES_SCALE.x * viewport.manual_dpi_scale, RES_SCALE.y * viewport.manual_dpi_scale);

    const auto &app_path = gui.live_area_current_open_apps_list[displayed_app_index];

    const auto app = get_app_index(gui, emuenv, app_path);
    const auto id = app ? app->title_id : fs::path(app_path).filename().string();
    const auto install = gui.updates_install.find(id);
    if (install != gui.updates_install.end() && install->second.state == UpdateState::SUCCESS)
        refresh_live_area(gui, emuenv, app_path);

    const auto INFO_BAR_HEIGHT = 32.f * SCALE.y;

    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INFO_BAR_HEIGHT);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
    if (gui.is_nav_button || !gui.live_area_app_closing.empty() || gui.gate_animation.state != GateAnimationState::None || displayed_app_index != gui.live_area_app_current_open)
        flags |= ImGuiWindowFlags_NoMouseInputs;
    ImGui::BeginChild(fmt::format("##live_area_{}", displayed_app_index).c_str(), WINDOW_SIZE, ImGuiChildFlags_None, flags);
    const auto SCREEN_POS = ImGui::GetCursorScreenPos();

    const auto is_current_app = (app_launch_path(app_path) == emuenv.io.app_path);
    const bool is_current_app_visible = (gui.live_area_app_current_open == displayed_app_index);

    const auto &app_type = type[app_path];

    const auto gate_pos = items_styles[app_type].gate_pos;

    const auto GATE_SIZE = ImVec2(280.0f * SCALE.x, 158.0f * SCALE.y);
    const ImVec2 GATE_FRAME_RECT_SIZE(290.f * SCALE.x, 168.f * SCALE.y);
    const ImVec2 GATE_FRAME_VISIBLE_MARGIN((GATE_FRAME_RECT_SIZE.x - GATE_SIZE.x) / 2.f, (GATE_FRAME_RECT_SIZE.y - GATE_SIZE.y) / 2.f);
    const auto GATE_POS = ImVec2(WINDOW_SIZE.x - (gate_pos.x * SCALE.x), WINDOW_SIZE.y - (gate_pos.y * SCALE.y));
    const ImVec2 GATE_POS_MIN(SCREEN_POS.x + GATE_POS.x, SCREEN_POS.y + GATE_POS.y);
    const ImVec2 GATE_POS_MAX(GATE_POS_MIN.x + GATE_SIZE.x, GATE_POS_MIN.y + GATE_SIZE.y);

    // Set gate frame positions
    ImVec2 gate_screen_pos_min = GATE_POS_MIN;
    ImVec2 gate_screen_pos_max = GATE_POS_MAX;

    float frame_alpha = 1.f;
    const auto COLOR_PULSE_BORDER = get_selectable_color_pulse();

    const auto &FG_DRAW_LIST = ImGui::GetForegroundDrawList();

    const ImVec2 VIEWPORT_MIN(VIEWPORT_POS);
    const ImVec2 VIEWPORT_MAX(VIEWPORT_MIN.x + VIEWPORT_SIZE.x, VIEWPORT_MIN.y + VIEWPORT_SIZE.y);

    const auto is_current_app_visible_animated = is_current_app_visible && (gui.gate_animation.state != GateAnimationState::None);
    if (is_current_app_visible_animated) {
        gui.gate_animation.update();

        switch (gui.gate_animation.state) {
        case GateAnimationState::EnterApp:
        case GateAnimationState::PreRunApp:
            gate_screen_pos_min = ImLerp(GATE_POS_MIN, VIEWPORT_MIN, gui.gate_animation.progress);
            gate_screen_pos_max = ImLerp(GATE_POS_MAX, VIEWPORT_MAX, gui.gate_animation.progress);
            if (gui.gate_animation.progress >= 0.8f)
                frame_alpha -= ((gui.gate_animation.progress - 0.8f) / 0.2f);
            break;
        case GateAnimationState::ReturnApp:
        case GateAnimationState::None:
            gate_screen_pos_min = ImLerp(VIEWPORT_MIN, GATE_POS_MIN, gui.gate_animation.progress);
            gate_screen_pos_max = ImLerp(VIEWPORT_MAX, GATE_POS_MAX, gui.gate_animation.progress);
            frame_alpha = gui.gate_animation.progress;
            break;
        default:
            LOG_ERROR("Unhandled gate animation state: {}", (int)gui.gate_animation.state);
            break;
        }

        frame_alpha = std::clamp(frame_alpha, 0.f, 1.f);
    }

    const auto is_zoom_animation = is_current_app_visible && ((gui.gate_animation.state == GateAnimationState::EnterApp) || (gui.gate_animation.state == GateAnimationState::PreRunApp));

    const ImVec2 gate_size = ImVec2(gate_screen_pos_max.x - gate_screen_pos_min.x, gate_screen_pos_max.y - gate_screen_pos_min.y);
    const ImVec2 ZOOM_RATIO = is_zoom_animation ? ImVec2(gate_size.x / GATE_SIZE.x, gate_size.y / GATE_SIZE.y) : ImVec2(1.f, 1.f);
    const ImVec2 GATE_ORIGIN = is_zoom_animation ? gate_screen_pos_min : GATE_POS_MIN;
    const ImVec2 frame_margin(GATE_FRAME_VISIBLE_MARGIN.x * ZOOM_RATIO.x, GATE_FRAME_VISIBLE_MARGIN.y * ZOOM_RATIO.y);
    const ImVec2 frame_rect_min(gate_screen_pos_min.x - frame_margin.x, gate_screen_pos_min.y - frame_margin.y);
    const ImVec2 frame_rect_size(gate_size.x + frame_margin.x * 2.f, gate_size.y + frame_margin.y * 2.f);
    const ImVec2 alpha_margin(6.f * SCALE.x * ZOOM_RATIO.x, 10.f * SCALE.y * ZOOM_RATIO.y);
    const ImVec2 frame_pos(frame_rect_min.x - alpha_margin.x, frame_rect_min.y - alpha_margin.y);
    const ImVec2 frame_size(frame_rect_size.x + alpha_margin.x * 2.f, frame_rect_size.y + alpha_margin.y * 2.f);

    const auto set_screen_pos = [&](const ImVec2 &offset) -> ImVec2 {
        ImVec2 local(SCREEN_POS.x + offset.x - GATE_POS_MIN.x, SCREEN_POS.y + offset.y - GATE_POS_MIN.y);
        return ImVec2(GATE_ORIGIN.x + (local.x * ZOOM_RATIO.x), GATE_ORIGIN.y + (local.y * ZOOM_RATIO.y));
    };

    const auto apply_zoom_size = [&](const ImVec2 &size) -> ImVec2 {
        return ImVec2(size.x * ZOOM_RATIO.x, size.y * ZOOM_RATIO.y);
    };

    const auto &WINDOW_DRAW_LIST = ImGui::GetWindowDrawList();

    // Always use a single rendering method, with the appropriate draw_list
    const auto &DRAW_LIST = is_zoom_animation ? FG_DRAW_LIST : WINDOW_DRAW_LIST;

    const ImVec2 background_pos = ImVec2(900.f * SCALE.x, 500.f * SCALE.y);
    const ImVec2 background_size = ImVec2(840.f * SCALE.x, 500.f * SCALE.y);

    const ImVec2 BG_POS_MIN(set_screen_pos(ImVec2(WINDOW_SIZE.x - background_pos.x, WINDOW_SIZE.y - background_pos.y)));
    const auto BG_SIZE = apply_zoom_size(background_size);
    const ImVec2 BG_POS_MAX(BG_POS_MIN.x + BG_SIZE.x, BG_POS_MIN.y + BG_SIZE.y);

    if (is_zoom_animation)
        FG_DRAW_LIST->PushClipRect(VIEWPORT_MIN, VIEWPORT_MAX);

    if (gui.live_area_contents[app_path].contains("livearea-background"))
        DRAW_LIST->AddImage(gui.live_area_contents[app_path]["livearea-background"], BG_POS_MIN, BG_POS_MAX);
    else
        DRAW_LIST->AddRectFilled(BG_POS_MIN, BG_POS_MAX, IM_COL32(148, 164, 173, 255), 0.f, ImDrawFlags_RoundCornersAll);

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
        auto bg_scal_size = ImVec2(bg_size.x * SCALE.x, bg_size.y * SCALE.y);
        auto img_scal_size = ImVec2(img_size.x * SCALE.x, img_size.y * SCALE.y);

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
            const auto background_pos_min = set_screen_pos(bg_pos);
            bg_scal_size = apply_zoom_size(bg_scal_size);
            const ImVec2 background_pos_max(background_pos_min.x + bg_scal_size.x, background_pos_min.y + bg_scal_size.y);
            DRAW_LIST->AddImage(gui.live_items[app_path][frame.id]["background"][current_item[app_path][frame.id]], background_pos_min, background_pos_max);
        }
        if (gui.live_items[app_path][frame.id].contains("image")) {
            const auto image_pos_min = set_screen_pos(img_pos);
            img_scal_size = apply_zoom_size(img_scal_size);
            const ImVec2 image_pos_max(image_pos_min.x + img_scal_size.x, image_pos_min.y + img_scal_size.y);
            DRAW_LIST->AddImage(gui.live_items[app_path][frame.id]["image"][current_item[app_path][frame.id]], image_pos_min, image_pos_max);
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
                ImGui::SetWindowFontScale(size_text_scale * RES_SCALE.x);

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
                ImGui::SetNextWindowPos(set_screen_pos(text_pos));
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
                ImGui::SetWindowFontScale(RES_SCALE.x);
            }
        }
    }

    ImGui::PushID(app_path.c_str());

    const auto &GATE_DRAW_LIST = is_current_app_visible_animated ? FG_DRAW_LIST : WINDOW_DRAW_LIST;

    const auto BUTTON_SIZE = ImVec2(72.f * SCALE.x, 30.f * SCALE.y);

    // Draw the gate content
    if (is_current_app && gui.live_area_last_app_frame)
        GATE_DRAW_LIST->AddImage(gui.live_area_last_app_frame, gate_screen_pos_min, gate_screen_pos_max);
    else if (gui.live_area_contents[app_path].contains("gate"))
        GATE_DRAW_LIST->AddImage(gui.live_area_contents[app_path]["gate"], gate_screen_pos_min, gate_screen_pos_max);
    else {
        const ImVec2 gate_scale(gate_size.x / GATE_SIZE.x, gate_size.y / GATE_SIZE.y);
        GATE_DRAW_LIST->AddRectFilled(gate_screen_pos_min, gate_screen_pos_max, IM_COL32(47, 51, 50, 255), 10.f * SCALE.x * gate_scale.x, ImDrawFlags_RoundCornersAll);

        const ImVec2 icon_size(94.f * SCALE.x * gate_scale.x, 94.f * SCALE.x * gate_scale.y);
        const ImVec2 ICON_POS_MIN(gate_screen_pos_min.x + (gate_size.x - icon_size.x) / 2.f, gate_screen_pos_min.y + 15.5f * SCALE.y * gate_scale.y);
        const ImVec2 ICON_POS_MAX(ICON_POS_MIN.x + icon_size.x, ICON_POS_MIN.y + icon_size.y);
        const ImVec2 ICON_CENTER_POS(ICON_POS_MIN.x + icon_size.x / 2.f, ICON_POS_MIN.y + icon_size.y / 2.f);

        // check if app icon exist
        auto &APP_ICON_TYPE = is_emu_app(app_path) ? gui.app_selector.sys_apps_icon : gui.app_selector.user_apps_icon;
        if (APP_ICON_TYPE.contains(app_path))
            GATE_DRAW_LIST->AddImageRounded(APP_ICON_TYPE[app_path], ICON_POS_MIN, ICON_POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 75.f * SCALE.x * std::min(gate_scale.x, gate_scale.y), ImDrawFlags_RoundCornersAll);
        else
            GATE_DRAW_LIST->AddEllipseFilled(ICON_CENTER_POS, ImVec2(icon_size.x / 2.f, icon_size.y / 2.f), IM_COL32_WHITE);
    }

    const float FRAME_THICKNESS = (10.f * SCALE.x) * ZOOM_RATIO.x;

    // Draw gate frame
    if (gui.gate_animation.state != GateAnimationState::ReturnApp) {
        if (const auto gate_icon = gui.vita_icons.find("gate"); gate_icon != gui.vita_icons.end() && gate_icon->second) {
            draw_stretchable_9slice_image(DRAW_LIST, gate_icon->second, frame_pos, frame_size, 74.f, 82.f,
                22.f, 22.f, 26.f, 26.f, IM_COL32(255, 255, 255, static_cast<int>(255 * frame_alpha)),
                0.f, 0.f, 74.f, 82.f, 22.f * SCALE.x * ZOOM_RATIO.x, 22.f * SCALE.x * ZOOM_RATIO.x,
                26.f * SCALE.y * ZOOM_RATIO.y, 26.f * SCALE.y * ZOOM_RATIO.y);
        } else {
            const ImVec2 frame_rect_max(frame_rect_min.x + frame_rect_size.x, frame_rect_min.y + frame_rect_size.y);
            DRAW_LIST->AddRect(frame_rect_min, frame_rect_max, IM_COL32(192, 192, 192, static_cast<int>(255 * frame_alpha)), (10.f * SCALE.x) * ZOOM_RATIO.x, ImDrawFlags_RoundCornersAll, FRAME_THICKNESS);
        }
    }

    ImGui::SetWindowFontScale(RES_SCALE.x);
    const std::string BUTTON_STR = is_current_app ? gui.lang.live_area.main["continue"] : gui.lang.live_area.main["start"];
    const auto default_font_scale = (25.f * sdl::get_viewport().manual_dpi_scale)
        * (ImGui::GetFontSize() / (19.2f * sdl::get_viewport().manual_dpi_scale));
    const auto font_size_scale = default_font_scale / ImGui::GetFontSize();

    // Interaction (button)
    const auto BUTTON_STR_SIZE = ImGui::CalcTextSize(BUTTON_STR.c_str());
    const auto START_SIZE = ImVec2(BUTTON_STR_SIZE.x * font_size_scale, BUTTON_STR_SIZE.y * font_size_scale);
    const auto START_BUTTON_SIZE = ImVec2(START_SIZE.x + (26.0f * SCALE.x), START_SIZE.y + (5.0f * SCALE.y));
    const auto POS_BUTTON = ImVec2((GATE_POS.x + (GATE_SIZE.x - START_BUTTON_SIZE.x) / 2.0f), (GATE_POS.y + (GATE_SIZE.y - START_BUTTON_SIZE.y) / 1.08f));
    const auto POS_START = set_screen_pos(ImVec2(POS_BUTTON.x + (START_BUTTON_SIZE.x - START_SIZE.x) / 2.f, POS_BUTTON.y + (START_BUTTON_SIZE.y - START_SIZE.y) / 2.f));
    const ImVec2 BUTTON_POS_MIN = set_screen_pos(POS_BUTTON);
    DRAW_LIST->AddRectFilled(BUTTON_POS_MIN, ImVec2(BUTTON_POS_MIN.x + (START_BUTTON_SIZE.x * ZOOM_RATIO.x), BUTTON_POS_MIN.y + (START_BUTTON_SIZE.y * ZOOM_RATIO.y)), IM_COL32(20, 168, 222, 255), (10.0f * SCALE.x) * ZOOM_RATIO.x, ImDrawFlags_RoundCornersAll);
    DRAW_LIST->AddText(gui.vita_font[gui.current_font_level], default_font_scale * ZOOM_RATIO.y, POS_START, IM_COL32(255, 255, 255, 255), BUTTON_STR.c_str());

    // Invisible button for gate interaction (only when not animating)
    if (!is_current_app_visible_animated) {
        ImGui::SetCursorPos(ImVec2(GATE_POS.x - GATE_FRAME_VISIBLE_MARGIN.x, GATE_POS.y - GATE_FRAME_VISIBLE_MARGIN.y));
        if (ImGui::InvisibleButton("##gate", GATE_FRAME_RECT_SIZE)) {
            if (emuenv.io.app_path.empty() || is_current_app)
                gui.gate_animation.start(GateAnimationState::EnterApp);
            else
                pre_run_app(gui, emuenv, app_path, session);
        }

        if (ImGui::IsItemHovered())
            live_area_type_selected = GATE;
    }

    // Launch app after animation completes
    if (gui.gate_animation.state == GateAnimationState::PreRunApp) {
        pre_run_app(gui, emuenv, app_path, session);
        gui.gate_animation.state = GateAnimationState::None;
    }
    ImVec2 widget_selection_min, widget_selection_max;
    bool has_widget_selection = false;
    const auto APP_INDEX = get_app_index(gui, emuenv, app_path);
    if (!is_emu_app(app_path) && APP_INDEX) {
        const auto widget_scal_size = apply_zoom_size(ImVec2(70.0f * SCALE.x, 70.f * SCALE.y));
        const auto widget_font_size = (20.f * SCALE.x) * ZOOM_RATIO.x;
        const auto manual_exist = vfs::app_dir_has_entries(emuenv.vita_fs_path, app_path, "sce_sys/manual", APP_INDEX->title_id);
        const bool has_update = has_live_area_update(gui, app_path, APP_INDEX->title_id);

        std::vector<ButtonEntry> buttons;
        if (has_update)
            buttons.emplace_back(gui.lang.app_context.live_area["update"], UPDATE, [&]() { process_live_area_update(gui, emuenv, app_path); });
        buttons.emplace_back("Search", SEARCH, [&]() { open_search(APP_INDEX->title); });
        if (manual_exist)
            buttons.emplace_back("Manual", MANUAL, [&]() { open_manual(gui, emuenv, app_path); });
        if (APP_INDEX->title_id.starts_with("PCS")) {
            buttons.emplace_back(gui.lang.home_screen["refresh"], REFRESH, [&]() {
                refresh_and_check_patch(gui, emuenv, app_path);
            });
            buttons.emplace_back("Cloud", CLOUD, [&]() { open_online_storage(gui, emuenv, APP_INDEX->title_id); });
        }

        const auto BUTTONS_COUNT = static_cast<int>(buttons.size());
        const float spacing = 42.0f * SCALE.x;
        const ImVec2 BUTTONS_POS = ImVec2(
            (WINDOW_SIZE.x - (BUTTONS_COUNT * widget_scal_size.x) - (spacing * (BUTTONS_COUNT - 1))) / 2.f, WINDOW_SIZE.y - (505.0f * SCALE.y));
        const ImVec2 widget_image_size(80.f * SCALE.x * ZOOM_RATIO.x, 80.f * SCALE.y * ZOOM_RATIO.y);
        const ImVec2 refresh_icon_size(64.f * SCALE.x * ZOOM_RATIO.x, 64.f * SCALE.y * ZOOM_RATIO.y);
        const float widget_image_offset_y = 6.f * SCALE.y * ZOOM_RATIO.y;

        for (int i = 0; i < BUTTONS_COUNT; i++) {
            ImVec2 button_pos_scal = ImVec2(BUTTONS_POS.x + i * (widget_scal_size.x + spacing), BUTTONS_POS.y);

            const auto TITLE = buttons[i].title;
            const auto TYPE = buttons[i].type;
            const ImVec2 WIDGET_POS_MIN = set_screen_pos(button_pos_scal);
            auto WIDGET_COLOR = IM_COL32(10, 169, 246, 255);
            if (TYPE == MANUAL)
                WIDGET_COLOR = IM_COL32(202, 0, 106, 255);
            else if (TYPE == CLOUD)
                WIDGET_COLOR = IM_COL32(255, 255, 70, 255);

            const auto draw_widget_fallback = [&]() {
                const auto title_size = ImGui::CalcTextSize(TITLE.c_str());
                const auto title_pos = ImVec2(WIDGET_POS_MIN.x + ((widget_scal_size.x - title_size.x) / 2.f), WIDGET_POS_MIN.y + ((widget_scal_size.y - title_size.y) / 2.f));
                DRAW_LIST->AddRectFilled(WIDGET_POS_MIN, ImVec2(WIDGET_POS_MIN.x + widget_scal_size.x, WIDGET_POS_MIN.y + widget_scal_size.y), WIDGET_COLOR, 12.0f * SCALE.x, ImDrawFlags_RoundCornersAll);
                DRAW_LIST->AddText(gui.vita_font[gui.current_font_level], widget_font_size, title_pos, IM_COL32(255, 255, 255, 255), TITLE.c_str());
            };
            const auto draw_rotated_refresh_icon = [&](ImTextureID texture) {
                const ImVec2 widget_image_pos(WIDGET_POS_MIN.x + ((widget_scal_size.x - widget_image_size.x) / 2.f), WIDGET_POS_MIN.y + ((widget_scal_size.y - widget_image_size.y) / 2.f) + (widget_image_offset_y / 2.f));
                const auto center = ImVec2(widget_image_pos.x + (widget_image_size.x / 2.f), widget_image_pos.y + (widget_image_size.y / 2.f));
                const auto half_size = ImVec2(refresh_icon_size.x / 2.f, refresh_icon_size.y / 2.f);

                float angle = 0.f;
                {
                    std::lock_guard<std::mutex> lock(refresh_animation_mutex);
                    if (auto refresh_it = refresh_animation_states.find(app_path); refresh_it != refresh_animation_states.end()) {
                        const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - refresh_it->second.start_time).count();
                        const float clamped_elapsed = refresh_it->second.stop_requested ? std::min(elapsed, refresh_it->second.stop_after_turns) : elapsed;
                        angle = clamped_elapsed * (2.f * std::numbers::pi_v<float>);

                        if (refresh_it->second.stop_requested && (elapsed >= refresh_it->second.stop_after_turns)) {
                            angle = 0.f;
                            refresh_animation_states.erase(refresh_it);
                        }
                    }
                }

                const auto rotate = [&](float x, float y) {
                    const float cos_angle = std::cos(angle);
                    const float sin_angle = std::sin(angle);
                    return ImVec2(center.x + (x * cos_angle) - (y * sin_angle), center.y + (x * sin_angle) + (y * cos_angle));
                };

                DRAW_LIST->AddImageQuad(texture,
                    rotate(-half_size.x, -half_size.y),
                    rotate(half_size.x, -half_size.y),
                    rotate(half_size.x, half_size.y),
                    rotate(-half_size.x, half_size.y));
            };
            const auto draw_widget_icon = [&](const char *name) {
                if (!gui.vita_icons.contains(name)) {
                    draw_widget_fallback();
                    return;
                }
                const ImVec2 image_pos(WIDGET_POS_MIN.x + ((widget_scal_size.x - widget_image_size.x) / 2.f), WIDGET_POS_MIN.y + ((widget_scal_size.y - widget_image_size.y) / 2.f) + (6.f * SCALE.y * ZOOM_RATIO.y));
                DRAW_LIST->AddImage(gui.vita_icons.at(name), image_pos, ImVec2(image_pos.x + widget_image_size.x, image_pos.y + widget_image_size.y));
            };

            if (TYPE == UPDATE)
                draw_widget_icon("update");
            else if (TYPE == REFRESH && gui.vita_icons.contains("refresh_bg") && gui.vita_icons.contains("refresh")) {
                const ImVec2 image_pos(WIDGET_POS_MIN.x + ((widget_scal_size.x - widget_image_size.x) / 2.f), WIDGET_POS_MIN.y + ((widget_scal_size.y - widget_image_size.y) / 2.f) + (6.f * SCALE.y * ZOOM_RATIO.y));
                DRAW_LIST->AddImage(gui.vita_icons.at("refresh_bg"), image_pos, ImVec2(image_pos.x + widget_image_size.x, image_pos.y + widget_image_size.y));
                draw_rotated_refresh_icon(gui.vita_icons.at("refresh"));
            } else if (TYPE == SEARCH)
                draw_widget_icon("search");
            else if (TYPE == MANUAL)
                draw_widget_icon("manual");
            else if (TYPE == CLOUD)
                draw_widget_icon("cloud");
            else
                draw_widget_fallback();

            ImGui::SetCursorPos(button_pos_scal);
            if (!is_current_app_visible_animated) {
                if (ImGui::InvisibleButton(("##" + TITLE).c_str(), widget_scal_size))
                    buttons[i].callback();

                if (ImGui::IsItemHovered())
                    live_area_type_selected = buttons[i].type;
            }
        }

        if (is_current_app_visible) {
            const auto selected = std::find_if(buttons.begin(), buttons.end(), [](const ButtonEntry &button) {
                return button.type == live_area_type_selected;
            });
            if (selected != buttons.end()) {
                const auto index = static_cast<float>(std::distance(buttons.begin(), selected));
                const ImVec2 target_pos(BUTTONS_POS.x + index * (widget_scal_size.x + spacing), BUTTONS_POS.y);
                const ImVec2 widget_pos = set_screen_pos(target_pos);
                // Widget images have extra transparent padding at the bottom.
                widget_selection_min = ImVec2(widget_pos.x + ((widget_scal_size.x - widget_image_size.x) / 2.f),
                    widget_pos.y + (widget_scal_size.y - widget_image_size.y) + widget_image_offset_y);
                widget_selection_max = ImVec2(widget_selection_min.x + widget_image_size.x, widget_selection_min.y + widget_image_size.y);
                has_widget_selection = true;
            }
        }
    }

    if (is_current_app_visible) {
        static std::string animated_app_path;
        static ImVec2 animated_min, animated_max;
        static bool animated_initialized = false;
        static int last_frame = -1;

        const int frame = ImGui::GetFrameCount();
        if (animated_app_path != app_path || last_frame < frame - 1)
            animated_initialized = false;
        animated_app_path = app_path;
        last_frame = frame;

        const bool gate_selected = live_area_type_selected == GATE;
        if (gate_selected || has_widget_selection) {
            const ImVec2 target_min = gate_selected ? gate_screen_pos_min : widget_selection_min;
            const ImVec2 target_max = gate_selected ? gate_screen_pos_max : widget_selection_max;
            const ImVec2 local_min(target_min.x - SCREEN_POS.x, target_min.y - SCREEN_POS.y);
            const ImVec2 local_max(target_max.x - SCREEN_POS.x, target_max.y - SCREEN_POS.y);
            if (!animated_initialized || is_current_app_visible_animated) {
                animated_min = local_min;
                animated_max = local_max;
                animated_initialized = true;
            } else {
                const float speed = std::clamp(20.f * ImGui::GetIO().DeltaTime, 0.f, 1.f);
                animated_min = ImLerp(animated_min, local_min, speed);
                animated_max = ImLerp(animated_max, local_max, speed);
            }

            const auto SELECT_THICKNESS = FRAME_THICKNESS / 2.f;
            const auto HALF_SELECT_THICKNESS = SELECT_THICKNESS / 2.f;
            const ImVec2 SELECT_POS_MIN(SCREEN_POS.x + animated_min.x + HALF_SELECT_THICKNESS, SCREEN_POS.y + animated_min.y + HALF_SELECT_THICKNESS);
            const ImVec2 SELECT_POS_MAX(SCREEN_POS.x + animated_max.x - HALF_SELECT_THICKNESS, SCREEN_POS.y + animated_max.y - HALF_SELECT_THICKNESS);
            DRAW_LIST->AddRect(SELECT_POS_MIN, SELECT_POS_MAX, COLOR_PULSE_BORDER, (10.f * SCALE.x) * ZOOM_RATIO.x, ImDrawFlags_RoundCornersAll, SELECT_THICKNESS);
        } else
            animated_initialized = false;
    }

    ImGui::PopID();

    if (is_zoom_animation)
        FG_DRAW_LIST->PopClipRect();

    auto &lang = gui.lang.live_area.help;

    if (!gui.vita_area.content_manager && !gui.vita_area.manual) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f * SCALE.x);
        const auto NAV_BUTTON_SIZE = ImVec2(62.f * SCALE.x, 62.f * SCALE.y);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f * SCALE.x);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.8f, 0.8f, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.96f, 0.96f, 0.96f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - (60.0f * SCALE.x) - NAV_BUTTON_SIZE.x, 12.0f * SCALE.y));
        if (ImGui::Button("X", NAV_BUTTON_SIZE))
            close_live_area_app(gui, emuenv, app_path);
        ImGui::SetCursorPos(ImVec2(60.f * SCALE.x, 12.0f * SCALE.y));
        if (ImGui::Button("?", NAV_BUTTON_SIZE))
            ImGui::OpenPopup("Live Area Help");
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
        ImGui::SetNextWindowPos(ImVec2(WINDOW_SIZE.x / 2.f, WINDOW_SIZE.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Live Area Help", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
            TextColoredCentered(GUI_COLOR_TEXT_TITLE, gui.lang.main_menubar.help["title"].c_str());
            ImGui::Spacing();
            auto TextColoredCenteredEx = [](GuiState &gui, const ImVec4 &col, const char *text_id) {
                TextColoredCentered(col, gui.lang.live_area.help[text_id].c_str());
                ImGui::Spacing();
            };
            TextColoredCenteredEx(gui, GUI_COLOR_TEXT, "control_setting");
            if (!has_available_modules())
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
            if (ImGui::Button(::lang::get(::lang::str::ok).c_str(), BUTTON_SIZE) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_cross))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
    }

    const auto SELECTABLE_SIZE = ImVec2(50.f * SCALE.x, 60.f * SCALE.y);

    const auto ARROW_HEIGHT_POS = WINDOW_SIZE.y - (250.f * SCALE.y);
    const auto ARROW_HEIGHT_DRAW_POS = SCREEN_POS.y + ARROW_HEIGHT_POS;
    const auto ARROW_WIDTH_POS = (30.f * SCALE.x);
    const auto ARROW_SELECT_HEIGHT_POS = ARROW_HEIGHT_POS - (SELECTABLE_SIZE.y / 2.f);

    // Draw left arrow
    const auto ARROW_LEFT_CENTER = ImVec2(SCREEN_POS.x + ARROW_WIDTH_POS, ARROW_HEIGHT_DRAW_POS);
    WINDOW_DRAW_LIST->AddTriangleFilled(
        ImVec2(ARROW_LEFT_CENTER.x + (16.f * SCALE.x), ARROW_LEFT_CENTER.y - (20.f * SCALE.y)),
        ImVec2(ARROW_LEFT_CENTER.x - (16.f * SCALE.x), ARROW_LEFT_CENTER.y),
        ImVec2(ARROW_LEFT_CENTER.x + (16.f * SCALE.x), ARROW_LEFT_CENTER.y + (20.f * SCALE.y)), ARROW_COLOR);
    ImGui::SetCursorPos(ImVec2(ARROW_WIDTH_POS - (SELECTABLE_SIZE.x / 2.f), ARROW_SELECT_HEIGHT_POS));
    if (ImGui::InvisibleButton("##left", SELECTABLE_SIZE)) {
        gui.live_area_app_current_open = displayed_app_index - 1;
        gui.vita_area.home_screen = true;
    }

    // Draw right arrow
    if (displayed_app_index < static_cast<int32_t>(gui.live_area_current_open_apps_list.size()) - 1) {
        const auto ARROW_RIGHT_CENTER = ImVec2(SCREEN_POS.x + WINDOW_SIZE.x - ARROW_WIDTH_POS, ARROW_HEIGHT_DRAW_POS);
        WINDOW_DRAW_LIST->AddTriangleFilled(
            ImVec2(ARROW_RIGHT_CENTER.x - (16.f * SCALE.x), ARROW_RIGHT_CENTER.y - (20.f * SCALE.y)),
            ImVec2(ARROW_RIGHT_CENTER.x + (16.f * SCALE.x), ARROW_RIGHT_CENTER.y),
            ImVec2(ARROW_RIGHT_CENTER.x - (16.f * SCALE.x), ARROW_RIGHT_CENTER.y + (20.f * SCALE.y)), ARROW_COLOR);
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - ARROW_WIDTH_POS - (SELECTABLE_SIZE.x / 2.f), ARROW_SELECT_HEIGHT_POS));
        if (ImGui::InvisibleButton("##right", SELECTABLE_SIZE)) {
            gui.live_area_app_current_open = displayed_app_index + 1;
            gui.vita_area.home_screen = true;
        }
    }
    ImGui::SetWindowFontScale(1.f);
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
}

} // namespace gui
