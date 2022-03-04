﻿// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <host/functions.h>
#include <util/safe_time.h>

#include <io/VitaIoDevice.h>
#include <util/log.h>

#include <pugixml.hpp>

#include <stb_image.h>

namespace gui {

struct NoticeInfo {
    std::string id;
    std::string content_id;
    std::string group;
    std::string type;
    time_t time;
    std::string name;
    std::string msg;
};

struct NoticeList {
    std::string id;
    std::string content_id;
    std::string group;
    std::string type;
    time_t time;
};

static std::map<std::string, std::vector<NoticeList>> notice_list;
static std::map<std::string, int> notice_list_count_new;
static std::map<std::string, std::map<time_t, bool>> notice_list_new;
static std::map<time_t, bool> notice_info_new;
static int notice_info_count_new = 0;
static std::vector<NoticeInfo> notice_info;

static bool init_notice_icon(GuiState &gui, HostState &host, const fs::path &content_path, const NoticeList &info) {
    gui.notice_info_icon[info.time] = {};
    int32_t width = 0;
    int32_t height = 0;
    vfs::FileBuffer buffer;

    if (!vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, content_path)) {
        if (info.type == "trophy") {
            LOG_WARN("Icon no found for trophy id: {} on NpComId: {}", info.content_id, info.id);
            return false;
        } else {
            if (!vfs::read_app_file(buffer, host.pref_path, info.id, "sce_sys/icon0.png")) {
                buffer = init_default_icon(gui, host);
                if (buffer.empty()) {
                    LOG_WARN("Not found defaut icon for this notice content: {}", info.content_id);
                    return false;
                }
            }
        }
    }
    stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
    if (!data) {
        LOG_ERROR("Invalid icon for notice id: {}, [{}] in path {}.", info.id, content_path.string());
        return false;
    }
    gui.notice_info_icon[info.time].init(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);

    return gui.notice_info_icon.find(info.time) != gui.notice_info_icon.end();
}

static bool set_notice_info(GuiState &gui, HostState &host, const NoticeList &info) {
    std::string msg, name;
    fs::path content_path;

    auto lang = gui.lang.indicator;
    if (info.type == "content") {
        if (info.group.find("gd") != std::string::npos) {
            content_path = fs::path("app") / info.id;
            msg = lang["app_added_home"].c_str();
        } else {
            if (info.group == "ac")
                content_path = fs::path("addcont") / info.id / info.content_id;
            else if (info.group.find("gp") != std::string::npos)
                content_path = fs::path("app") / info.id;
            else if (info.group == "theme")
                content_path = fs::path("theme") / info.content_id;
            msg = lang["install_complete"].c_str();
        }
        vfs::FileBuffer params;
        if (vfs::read_file(VitaIoDevice::ux0, params, host.pref_path, content_path / "sce_sys/param.sfo")) {
            SfoFile sfo_handle;
            sfo::load(sfo_handle, params);
            if (!sfo::get_data_by_key(name, sfo_handle, fmt::format("TITLE_{:0>2d}", host.cfg.sys_lang)))
                sfo::get_data_by_key(name, sfo_handle, "TITLE");
        } else {
            LOG_WARN("Content not found for id: {}, in path: {}", info.content_id, content_path.string());
            return false;
        }
        init_notice_icon(gui, host, content_path / "sce_sys/icon0.png", info);
    } else {
        auto common = gui.lang.common.main;
        switch (static_cast<np::trophy::SceNpTrophyGrade>(std::stoi(info.group))) {
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM:
            name = fmt::format("({}) ", common["platinium"]);
            break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD:
            name = fmt::format("({}) ", common["gold"]);
            break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER:
            name = fmt::format("({}) ", common["silver"]);
            break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE:
            name = fmt::format("({}) ", common["bronze"]);
            break;
        default: break;
        }

        const auto trophy_conf_id_path{ fs::path(host.pref_path) / "ux0/user" / host.io.user_id / "trophy/conf" / info.id };
        const std::string sfm_name = fs::exists(trophy_conf_id_path / fmt::format("TROP_{:0>2d}.SFM", host.cfg.sys_lang)) ? fmt::format("TROP_{:0>2d}.SFM", host.cfg.sys_lang) : "TROP.SFM";

        pugi::xml_document doc;
        if (doc.load_file((trophy_conf_id_path / sfm_name).c_str())) {
            for (const auto &conf : doc.child("trophyconf")) {
                if (conf.name() == std::string("trophy")) {
                    if (info.content_id == conf.attribute("id").as_string())
                        name += conf.child("name").text().as_string();
                }
            }
        } else {
            LOG_WARN("Trophy sfm in conf no found for NpComId: {}", info.id);
            return false;
        }
        msg = lang["trophy_earned"];

        content_path = fs::path("user") / host.io.user_id / "trophy/conf" / info.id / fmt::format("TROP{}.PNG", info.content_id);
        if (!init_notice_icon(gui, host, content_path, info))
            return false;
    }

    notice_info.push_back({ info.id, info.content_id, info.group, info.type, info.time, name, msg });

    return true;
}

void init_notice_info(GuiState &gui, HostState &host) {
    if (!notice_info.empty()) {
        notice_info.clear();
        notice_info_count_new = 0;
        for (auto &notice : gui.notice_info_icon)
            notice.second = {};
        gui.notice_info_icon.clear();
        notice_info_new.clear();
    }

    if (!notice_list.empty()) {
        for (const auto user : notice_list) {
            if ((user.first == "global") || (user.first == host.io.user_id)) {
                for (const auto &notice : user.second) {
                    if (!set_notice_info(gui, host, notice)) {
                        const auto notice_index = std::find_if(notice_list[user.first].begin(), notice_list[user.first].end(), [&](const NoticeList &n) {
                            return n.time == notice.time;
                        });
                        notice_list[user.first].erase(notice_index);
                        save_notice_list(host);
                    } else
                        notice_info_new[notice.time] = notice_list_new[user.first][notice.time];
                }
            }
        }

        notice_info_count_new = notice_list_count_new["global"] + notice_list_count_new[host.io.user_id];

        // Sort in date order
        std::sort(notice_info.begin(), notice_info.end(), [&](const NoticeInfo &na, const NoticeInfo &nb) {
            return na.time > nb.time;
        });
    }
}

void get_notice_list(HostState &host) {
    notice_list.clear();
    notice_list_count_new.clear();
    notice_list_new.clear();
    const auto notice_path{ fs::path(host.pref_path) / "ux0/user/notice.xml" };

    if (fs::exists(notice_path)) {
        pugi::xml_document notice_xml;
        if (notice_xml.load_file(notice_path.c_str())) {
            const auto notice_child = notice_xml.child("notice");
            if (!notice_child.child("user").empty()) {
                // Load Notice List
                for (const auto &user : notice_child) {
                    auto user_id = user.attribute("id").as_string();
                    notice_list_count_new[user_id] = user.attribute("count_new").as_int();
                    for (const auto &notice : user) {
                        NoticeList noticeList;
                        noticeList.id = notice.attribute("id").as_string();
                        noticeList.content_id = notice.attribute("content_id").as_string();
                        noticeList.group = notice.attribute("group").as_string();
                        noticeList.type = notice.attribute("type").as_string();
                        noticeList.time = !notice.attribute("time").empty() ? notice.attribute("time").as_llong() : (notice.attribute("date").as_llong() * 1000); // Backward Compat
                        notice_list_new[user_id][noticeList.time] = notice.attribute("new").as_bool();
                        notice_list[user_id].push_back(noticeList);
                    }
                }
            }
        } else {
            LOG_ERROR("Notice XML found is corrupted on path: {}", notice_path.string());
            fs::remove(notice_path);
        }
    }
}

void save_notice_list(HostState &host) {
    pugi::xml_document notice_xml;
    auto declarationUser = notice_xml.append_child(pugi::node_declaration);
    declarationUser.append_attribute("version") = "1.0";
    declarationUser.append_attribute("encoding") = "utf-8";

    // Save notif
    auto notice_child = notice_xml.append_child("notice");

    for (const auto &user : notice_list) {
        auto user_child = notice_child.append_child("user");
        user_child.append_attribute("id") = user.first.c_str();
        user_child.append_attribute("count_new") = notice_list_count_new[user.first];

        std::sort(notice_list[user.first].begin(), notice_list[user.first].end(), [&](const NoticeList &na, const NoticeList &nb) {
            return na.time > nb.time;
        });

        for (const auto &notice : user.second) {
            auto info_child = user_child.append_child("info");
            info_child.append_attribute("id") = notice.id.c_str();
            info_child.append_attribute("content_id") = notice.content_id.c_str();
            info_child.append_attribute("group") = notice.group.c_str();
            info_child.append_attribute("type") = notice.type.c_str();
            info_child.append_attribute("time") = notice.time;
            info_child.append_attribute("new") = notice_list_new[user.first][notice.time];
        }
    }

    const auto notice_path{ fs::path(host.pref_path) / "ux0/user/notice.xml" };
    const auto save_xml = notice_xml.save_file(notice_path.c_str());
    if (!save_xml)
        LOG_ERROR("Fail save xml");
}

void update_notice_info(GuiState &gui, HostState &host, const std::string &type) {
    NoticeList info;
    const auto user_id = type == "content" ? "global" : host.io.user_id;
    if (type == "content") {
        info.id = host.app_title_id;
        info.content_id = host.app_content_id;
        info.group = host.app_category;
    } else {
        const auto trophy_data = gui.trophy_unlock_display_requests.back();
        info.id = trophy_data.np_com_id;
        info.content_id = trophy_data.trophy_id;
        info.group = std::to_string(int(trophy_data.trophy_kind));
    }
    info.type = type;
    info.time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    notice_info_new[info.time] = true;
    notice_list_new[user_id][info.time] = true;
    notice_list[user_id].push_back(info);
    if (set_notice_info(gui, host, info)) {
        ++notice_info_count_new;
        ++notice_list_count_new[user_id];
        std::sort(notice_info.begin(), notice_info.end(), [&](const NoticeInfo &na, const NoticeInfo &nb) {
            return na.time > nb.time;
        });

        save_notice_list(host);
    }
}

static void clean_notice_info_new(const std::string &user_id) {
    notice_info_count_new = 0;
    notice_info_new.clear();
    notice_list_count_new["global"] = 0;
    notice_list_count_new[user_id] = 0;
    notice_list_new["global"].clear();
    notice_list_new[user_id].clear();
}

static std::string get_notice_time(GuiState &gui, HostState &host, const time_t &time) {
    std::string date;
    const auto time_in_second = time / 1000;
    const auto diff_time = difftime(std::time(nullptr), time_in_second);
    static const auto minute = 60;
    static const auto hour = minute * 60;
    static const auto day = hour * 24;
    if (diff_time >= day) {
        tm date_tm = {};
        SAFE_LOCALTIME(&time_in_second, &date_tm);
        auto DATE_TIME = get_date_time(gui, host, date_tm);
        date = fmt::format("{} {}", DATE_TIME[DateTime::DATE_MINI], DATE_TIME[DateTime::CLOCK]);
        if (host.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR)
            date += fmt::format(" {}", DATE_TIME[DateTime::DAY_MOMENT]);
    } else {
        auto lang = gui.lang.common.main;
        if (diff_time >= (hour * 2))
            date = fmt::format(lang["hours_ago"], uint32_t(diff_time / hour));
        else if (diff_time >= hour)
            date = lang["one_hour_ago"];
        else if (diff_time >= (minute * 2))
            date = fmt::format(lang["minutes_ago"], uint32_t(diff_time / 60));
        else
            date = lang["one_minute_ago"];
    }

    return date;
}
static bool notice_info_state;

static void draw_notice_info(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);

    const auto NOTICE_SIZE = notice_info_count_new ? ImVec2(104.0f * SCALE.x, 95.0f * SCALE.y) : ImVec2(90.0f * SCALE.x, 82.0f * SCALE.y);
    const auto NOTICE_POS = ImVec2(display_size.x - NOTICE_SIZE.x, 0.f);
    const auto NOTICE_COLOR = gui.information_bar_color.notice_font;
    const auto WINDOW_SIZE = notice_info_state ? display_size : (notice_info_count_new ? ImVec2(84.f * SCALE.x, 76.f * SCALE.y) : ImVec2(72.f * SCALE.x, 62.f * SCALE.y));
    const auto WINDOW_POS = ImVec2(notice_info_state ? 0.f : display_size.x - WINDOW_SIZE.x, 0.f);

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 50.f * SCALE.x);
    ImGui::Begin("##notice_info", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    if (notice_info_count_new) {
        if (gui.theme_information_bar_notice.find(NoticeIcon::NEW) != gui.theme_information_bar_notice.end())
            ImGui::GetForegroundDrawList()->AddImage(gui.theme_information_bar_notice[NoticeIcon::NEW], NOTICE_POS, ImVec2(NOTICE_POS.x + NOTICE_SIZE.x, NOTICE_SIZE.y));
        else
            ImGui::GetForegroundDrawList()->AddCircleFilled(ImVec2(display_size.x - (24.f * SCALE.x), (16.f * SCALE.y)), 60.f * SCALE.x, IM_COL32(11.f, 90.f, 252.f, 255.f));
        const auto FONT_SCALE = 40.f * SCALE.x;
        const auto NOTICE_COUNT_FONT_SCALE = FONT_SCALE / 40.f;
        const auto NOTICE_COUNT_SIZE = ImGui::CalcTextSize(std::to_string(notice_info_count_new).c_str()).x * NOTICE_COUNT_FONT_SCALE;
        ImGui::GetForegroundDrawList()->AddText(gui.vita_font, FONT_SCALE, ImVec2(display_size.x - (NOTICE_SIZE.x / 2.f) - (NOTICE_COUNT_SIZE / 2.f) + (12.f * SCALE.x), (15.f * SCALE.y)), NOTICE_COLOR, std::to_string(notice_info_count_new).c_str());
    } else {
        if (gui.theme_information_bar_notice.find(NoticeIcon::NO) != gui.theme_information_bar_notice.end())
            ImGui::GetForegroundDrawList()->AddImage(gui.theme_information_bar_notice[NoticeIcon::NO], NOTICE_POS, ImVec2(NOTICE_POS.x + NOTICE_SIZE.x, NOTICE_SIZE.y));
        else
            ImGui::GetForegroundDrawList()->AddCircleFilled(ImVec2(display_size.x - (28.f * SCALE.x), (18.f * SCALE.y)), 44.f * SCALE.x, IM_COL32(62.f, 98.f, 160.f, 255.f));
    }

    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (notice_info_state) {
            clean_notice_info_new(host.io.user_id);
            save_notice_list(host);
        }
        notice_info_state = !notice_info_state;
    }

    if (notice_info_state) {
        const auto POPUP_SIZE = notice_info.empty() ? ImVec2(412.f * SCALE.x, 86.f * SCALE.y) : ImVec2(782.f * SCALE.x, notice_info.size() < 5 ? 22.f * host.dpi_scale + ((80.f * SCALE.y) * notice_info.size() + (10.f * (notice_info.size() - 1) * host.dpi_scale)) : 464.f * SCALE.y);
        const auto POPUP_POS = ImVec2(notice_info.empty() ? display_size.x - (502.f * SCALE.y) : (display_size.x / 2.f) - (POPUP_SIZE.x / 2.f), 56.f * SCALE.y);
        const auto POPUP_BG_COLOR = notice_info.empty() ? GUI_COLOR_TEXT : GUI_SMOOTH_GRAY;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, POPUP_BG_COLOR);
        ImGui::PushStyleColor(ImGuiCol_Border, GUI_COLOR_TEXT);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, notice_info.empty() ? 0.f : 8.0f * SCALE.x);
        ImGui::SetNextWindowPos(POPUP_POS, ImGuiCond_Always);
        ImGui::BeginChild("##notice_info_child", POPUP_SIZE, true, ImGuiWindowFlags_NoSavedSettings);
        auto lang = gui.lang.indicator;
        if (notice_info.empty()) {
            ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
            const auto no_notif = lang["no_notif"].c_str();
            const auto calc_text = ImGui::CalcTextSize(no_notif);
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - (calc_text.x / 2.f), (POPUP_SIZE.y / 2.f) - (calc_text.y / 2.f)));
            ImGui::TextColored(ImVec4(0.f, 0.f, 0.f, 1.f), "%s", no_notif);
        } else {
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 85.f * SCALE.x);
            const auto ICON_SIZE = ImVec2(64.f * SCALE.x, 64.f * SCALE.y);
            const auto SELECT_SIZE = ImVec2(POPUP_SIZE.x, 80.f * SCALE.y);

            for (const auto &notice : notice_info) {
                if (notice.time != notice_info.front().time)
                    ImGui::Separator();
                const auto ICON_POS = ImGui::GetCursorPos();
                if (gui.notice_info_icon.find(notice.time) != gui.notice_info_icon.end()) {
                    ImGui::SetCursorPos(ImVec2(ICON_POS.x + (ImGui::GetColumnWidth() / 2.f) - (ICON_SIZE.x / 2.f), ICON_POS.y + (SELECT_SIZE.y / 2.f) - (ICON_SIZE.y / 2.f)));
                    ImGui::Image(gui.notice_info_icon[notice.time], ICON_SIZE);
                }
                ImGui::SetCursorPosY(ICON_POS.y);
                ImGui::PushID(std::to_string(notice.time).c_str());
                const auto SELECT_COLOR = ImVec4(0.23f, 0.68f, 0.95f, 0.60f);
                const auto SELECT_COLOR_HOVERED = ImVec4(0.23f, 0.68f, 0.99f, 0.80f);
                const auto SELECT_COLOR_ACTIVE = ImVec4(0.23f, 0.68f, 1.f, 1.f);
                ImGui::PushStyleColor(ImGuiCol_Header, SELECT_COLOR);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, SELECT_COLOR_HOVERED);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, SELECT_COLOR_ACTIVE);
                if (ImGui::Selectable("##icon", notice_info_new[notice.time], ImGuiSelectableFlags_SpanAllColumns, SELECT_SIZE)) {
                    clean_notice_info_new(host.io.user_id);
                    save_notice_list(host);
                    if (notice.type == "content") {
                        if (notice.group == "theme")
                            pre_load_app(gui, host, false, "NPXS10015");
                        else
                            pre_load_app(gui, host, host.cfg.show_live_area_screen, notice.id);
                    } else {
                        pre_load_app(gui, host, false, "NPXS10008");
                        open_trophy_unlocked(gui, host, notice.id, notice.content_id);
                    }
                    notice_info_state = false;
                }
                ImGui::PopStyleColor(3);
                ImGui::PopID();
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.3f * RES_SCALE.x);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (14.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice.name.c_str());
                ImGui::Spacing();
                ImGui::SetWindowFontScale(0.9f * RES_SCALE.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice.msg.c_str());
                const auto notice_time = get_notice_time(gui, host, notice.time);
                const auto notice_time_size = ImGui::CalcTextSize(notice_time.c_str());
                ImGui::SetCursorPos(ImVec2(POPUP_SIZE.x - (34.f * SCALE.x) - notice_time_size.x, ImGui::GetCursorPosY() - (8.f * SCALE.y)));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice_time.c_str());
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        if (!notice_info.empty()) {
            const auto DELETE_POPUP_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);
            const auto BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);

            ImGui::SetWindowFontScale(1.f * RES_SCALE.x);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
            ImGui::SetCursorPos(ImVec2(display_size.x - (70.f * SCALE.x), display_size.y - (52.f * SCALE.y)));
            if (ImGui::Button("...", ImVec2(64.f * SCALE.x, 40.f * SCALE.y)) || ImGui::IsKeyPressed(host.cfg.keyboard_button_triangle))
                ImGui::OpenPopup("...");
            if (ImGui::BeginPopup("...", ImGuiWindowFlags_NoMove)) {
                if (ImGui::Button(lang["delete_all"].c_str()))
                    ImGui::OpenPopup("Delete All");
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
                ImGui::SetNextWindowSize(DELETE_POPUP_SIZE, ImGuiCond_Always);
                ImGui::SetNextWindowPos(ImVec2((display_size.x / 2.f) - (DELETE_POPUP_SIZE.x / 2.f), (display_size.y / 2.f) - (DELETE_POPUP_SIZE.y / 2.f)));
                if (ImGui::BeginPopupModal("Delete All", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
                    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                    const auto notif_deleted = lang["notif_deleted"].c_str();
                    auto common = host.common_dialog.lang.common;
                    ImGui::SetCursorPos(ImVec2((DELETE_POPUP_SIZE.x / 2.f) - (ImGui::CalcTextSize(notif_deleted).x / 2.f), (DELETE_POPUP_SIZE.y / 2.f) - (46.f * SCALE.y)));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", notif_deleted);
                    ImGui::SetCursorPos(ImVec2((DELETE_POPUP_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), DELETE_POPUP_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
                    if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle)) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine(0.f, 20.f);
                    if (ImGui::Button("OK", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
                        notice_info.clear();
                        for (auto &notice : gui.notice_info_icon)
                            notice.second = {};
                        gui.notice_info_icon.clear();
                        notice_list["global"].clear();
                        notice_list[host.io.user_id].clear();
                        clean_notice_info_new(host.io.user_id);
                        save_notice_list(host);
                        notice_info_state = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar();
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
            ImGui::SetWindowFontScale(1.f);
        }
    }
    ImGui::End();
}

void draw_information_bar(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);
    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;
    const ImU32 DEFAULT_BAR_COLOR = 0xFF000000; // Black
    const ImU32 DEFAULT_INDICATOR_COLOR = 0xFFFFFFFF; // White
    const auto is_12_hour_format = host.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;
    const auto is_notif_pos = !gui.live_area.start_screen && (gui.live_area.live_area_screen || gui.live_area.app_selector) ? 78.f * SCALE.x : 0.f;
    const auto is_theme_color = gui.live_area.app_selector || gui.live_area.live_area_screen || gui.live_area.start_screen;
    const auto indicator_color = gui.information_bar_color.indicator;
    const auto bar_color = gui.information_bar_color.bar;

    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_size.x, INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::Begin("##information_bar", &gui.live_area.information_bar, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(display_size.x, INFORMATION_BAR_HEIGHT), is_theme_color ? bar_color : DEFAULT_BAR_COLOR, 0.f, ImDrawFlags_RoundCornersAll);

    if (gui.live_area.app_selector || gui.live_area.live_area_screen) {
        const auto HOME_ICON_POS_CENTER = (display_size.x / 2.f) - (32.f * ((float(gui.apps_list_opened.size())) / 2.f)) * SCALE.x;
        const auto APP_IS_OPEN = gui.current_app_selected >= 0;

        // Draw Home Icon
        const std::vector<ImVec2> HOME_UP_POS = { ImVec2(HOME_ICON_POS_CENTER - (13.f * SCALE.x), 16.f * SCALE.y), ImVec2(HOME_ICON_POS_CENTER, 6.f * SCALE.y), ImVec2(HOME_ICON_POS_CENTER + (13.f * SCALE.x), 16.f * SCALE.y) };
        const auto HOME_DOWN_POS_MINI = ImVec2(HOME_ICON_POS_CENTER - (8.f * SCALE.x), 16.f * SCALE.y);
        const auto HOME_DOWN_POS_MAX = ImVec2(HOME_ICON_POS_CENTER + (8.f * SCALE.x), 26.f * SCALE.y);

        ImGui::GetForegroundDrawList()->AddTriangleFilled(HOME_UP_POS[0], HOME_UP_POS[1], HOME_UP_POS[2], indicator_color);
        ImGui::GetForegroundDrawList()->AddRectFilled(HOME_DOWN_POS_MINI, HOME_DOWN_POS_MAX, indicator_color);
        if (APP_IS_OPEN) {
            ImGui::GetForegroundDrawList()->AddTriangleFilled(HOME_UP_POS[0], HOME_UP_POS[1], HOME_UP_POS[2], IM_COL32(0.f, 0.f, 0.f, 100.f));
            ImGui::GetForegroundDrawList()->AddRectFilled(HOME_DOWN_POS_MINI, HOME_DOWN_POS_MAX, IM_COL32(0.f, 0.f, 0.f, 100.f));
        }
        ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(HOME_ICON_POS_CENTER - (3.f * SCALE.x), 18.5f * SCALE.y), ImVec2(HOME_ICON_POS_CENTER + (3.f * SCALE.x), 26.f * SCALE.y), bar_color);

        // Draw App Icon
        const float decal_app_icon_pos = 34.f * ((float(gui.apps_list_opened.size()) - 2) / 2.f);
        const auto ICON_SIZE_SCALE = 28.f * SCALE.x;

        for (auto a = 0; a < gui.apps_list_opened.size(); a++) {
            const auto ICON_POS_MINI_SCALE = ImVec2((display_size.x / 2.f) - (14.f * SCALE.x) - (decal_app_icon_pos * SCALE.x) + (a * (34 * SCALE.x)), 2.f * SCALE.y);
            const auto ICON_POS_MAX_SCALE = ImVec2(ICON_POS_MINI_SCALE.x + ICON_SIZE_SCALE, ICON_POS_MINI_SCALE.y + ICON_SIZE_SCALE);
            const auto ICON_CENTER_POS = ImVec2(ICON_POS_MINI_SCALE.x + (ICON_SIZE_SCALE / 2.f), ICON_POS_MINI_SCALE.y + (ICON_SIZE_SCALE / 2.f));
            const auto APPS_OPENED = gui.apps_list_opened[a];
            auto &APP_ICON_TYPE = APPS_OPENED.find("NPXS") != std::string::npos ? gui.app_selector.sys_apps_icon : gui.app_selector.user_apps_icon;

            // Check if icon exist
            if (APP_ICON_TYPE.find(APPS_OPENED) != APP_ICON_TYPE.end())
                ImGui::GetForegroundDrawList()->AddImageRounded(APP_ICON_TYPE[APPS_OPENED], ICON_POS_MINI_SCALE, ICON_POS_MAX_SCALE, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 15.f * SCALE.x, ImDrawFlags_RoundCornersAll);
            else
                ImGui::GetForegroundDrawList()->AddCircleFilled(ICON_CENTER_POS, ICON_SIZE_SCALE / 2.f, IM_COL32_WHITE);

            // hide Icon no opened
            if (!APP_IS_OPEN || (gui.apps_list_opened[gui.current_app_selected] != APPS_OPENED))
                ImGui::GetForegroundDrawList()->AddCircleFilled(ICON_CENTER_POS, ICON_SIZE_SCALE / 2.f, IM_COL32(0.f, 0.f, 0.f, 140.f));
        }
    }

    const auto PIX_FONT_SCALE = 19.2f / 24.f;
    const auto DEFAULT_FONT_SCALE = ImGui::GetFontSize() / (19.2f * host.dpi_scale);
    const auto CLOCK_DEFAULT_FONT_SCALE = (24.f * host.dpi_scale) * DEFAULT_FONT_SCALE;
    const auto DAY_MOMENT_DEFAULT_FONT_SCALE = (18.f * host.dpi_scale) * DEFAULT_FONT_SCALE;
    const auto CLOCK_FONT_SIZE_SCALE = CLOCK_DEFAULT_FONT_SCALE / ImGui::GetFontSize();
    const auto DAY_MOMENT_FONT_SIZE_SCALE = DAY_MOMENT_DEFAULT_FONT_SCALE / ImGui::GetFontSize();

    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);

    tm local = {};
    SAFE_LOCALTIME(&tt, &local);

    auto DATE_TIME = get_date_time(gui, host, local);
    const auto CALC_CLOCK_SIZE = ImGui::CalcTextSize(DATE_TIME[DateTime::CLOCK].c_str());
    const auto CLOCK_SIZE_SCALE = ImVec2((CALC_CLOCK_SIZE.x * CLOCK_FONT_SIZE_SCALE) * RES_SCALE.x, (CALC_CLOCK_SIZE.y * CLOCK_FONT_SIZE_SCALE * PIX_FONT_SCALE) * RES_SCALE.y);
    const auto CALC_DAY_MOMENT_SIZE = ImGui::CalcTextSize(DATE_TIME[DateTime::DAY_MOMENT].c_str());
    const auto DAY_MOMENT_SIZE_SCALE = host.io.user_id.empty() || is_12_hour_format ? ImVec2((CALC_DAY_MOMENT_SIZE.x * DAY_MOMENT_FONT_SIZE_SCALE) * RES_SCALE.y, (CALC_DAY_MOMENT_SIZE.y * DAY_MOMENT_FONT_SIZE_SCALE * PIX_FONT_SCALE) * RES_SCALE.y) : ImVec2(0.f, 0.f);

    const auto CLOCK_POS = ImVec2(display_size.x - (64.f * SCALE.x) - CLOCK_SIZE_SCALE.x - DAY_MOMENT_SIZE_SCALE.x - is_notif_pos, (INFORMATION_BAR_HEIGHT / 2.f) - (CLOCK_SIZE_SCALE.y / 2.f));
    const auto DAY_MOMENT_POS = ImVec2(CLOCK_POS.x + CLOCK_SIZE_SCALE.x + (6.f * SCALE.x), CLOCK_POS.y + (CLOCK_SIZE_SCALE.y - DAY_MOMENT_SIZE_SCALE.y));

    ImGui::GetForegroundDrawList()->AddText(gui.vita_font, CLOCK_DEFAULT_FONT_SCALE * RES_SCALE.x, CLOCK_POS, is_theme_color ? indicator_color : DEFAULT_INDICATOR_COLOR, DATE_TIME[DateTime::CLOCK].c_str());
    if (host.io.user_id.empty() || is_12_hour_format)
        ImGui::GetForegroundDrawList()->AddText(gui.vita_font, DAY_MOMENT_DEFAULT_FONT_SCALE * RES_SCALE.x, DAY_MOMENT_POS, is_theme_color ? indicator_color : DEFAULT_INDICATOR_COLOR, DATE_TIME[DateTime::DAY_MOMENT].c_str());
    ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(display_size.x - (54.f * SCALE.x) - is_notif_pos, 12.f * SCALE.y), ImVec2(display_size.x - (50.f * SCALE.x) - is_notif_pos, 20 * SCALE.y), IM_COL32(81.f, 169.f, 32.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);
    ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(display_size.x - (50.f * SCALE.x) - is_notif_pos, 5.f * SCALE.y), ImVec2(display_size.x - (12.f * SCALE.x) - is_notif_pos, 27 * SCALE.y), IM_COL32(81.f, 169.f, 32.f, 255.f), 2.f * SCALE.x, ImDrawFlags_RoundCornersAll);

    if (host.display.imgui_render && !gui.live_area.start_screen && !gui.live_area.live_area_screen && get_sys_apps_state(gui) && (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) || ImGui::IsItemClicked(0)))
        gui.live_area.information_bar = false;

    if (is_notif_pos)
        draw_notice_info(gui, host);

    ImGui::End();
}

} // namespace gui
