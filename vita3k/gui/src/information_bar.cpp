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

#include <gui/functions.h>

#include <config/state.h>
#include <dialog/state.h>
#include <display/state.h>
#include <io/state.h>
#include <packages/sfo.h>
#include <util/safe_time.h>
#include <util/string_utils.h>

#include <io/VitaIoDevice.h>
#include <util/log.h>

#include <pugixml.hpp>

#include <SDL3/SDL_power.h>
#include <stb_image.h>

namespace gui {

struct NoticeList {
    std::string id;
    std::string content_id;
    std::string group;
    std::string type;
    time_t time;
    bool is_new;
};

struct NoticeInfo : NoticeList {
    std::string name;
    std::string msg;
};

static std::map<std::string, std::vector<NoticeList>> notice_list;
static std::map<std::string, int> notice_list_count_new;
static int notice_info_count_new = 0;
static std::vector<NoticeInfo> notice_info;

void erase_app_notice(GuiState &gui, const std::string &title_id) {
    auto &notice_global = notice_list["global"];

    // Check if the notice list is empty
    if (notice_global.empty()) {
        LOG_WARN("Notice list is empty.");
        return;
    }

    auto notice_list_it = notice_global.begin();
    while (notice_list_it != notice_global.end()) {
        if (notice_list_it->id != title_id) {
            ++notice_list_it;
            continue;
        }

        // Find and erase the corresponding entry in notice_info
        const auto notice_info_it = std::find_if(notice_info.begin(), notice_info.end(), [&](const NoticeInfo &n) {
            return n.time == notice_list_it->time;
        });
        if (notice_info_it != notice_info.end())
            notice_info.erase(notice_info_it); // Erase the entry from notice_info

        // Erase the entry from notice_info_icon
        gui.notice_info_icon.erase(notice_list_it->time);

        // Erase the item from notice_general and update the iterator
        notice_list_it = notice_global.erase(notice_list_it);

        LOG_INFO("Notice content with title id: {} has been erased.", title_id);
    }
}

static bool init_notice_icon(GuiState &gui, EmuEnvState &emuenv, const fs::path &content_path, const NoticeList &info) {
    gui.notice_info_icon[info.time] = {};
    int32_t width = 0;
    int32_t height = 0;
    vfs::FileBuffer buffer;

    if (!vfs::read_file(VitaIoDevice::ux0, buffer, emuenv.pref_path, content_path)) {
        if (info.type == "trophy") {
            LOG_WARN("Icon no found for trophy id: {} on NpComId: {}", info.content_id, info.id);
            return false;
        } else {
            if (!vfs::read_app_file(buffer, emuenv.pref_path, info.id, "sce_sys/icon0.png")) {
                buffer = init_default_icon(gui, emuenv);
                if (buffer.empty()) {
                    LOG_WARN("Not found default icon for this notice content: {}", info.content_id);
                    return false;
                }
            }
        }
    }
    stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
    if (!data) {
        LOG_ERROR("Invalid icon for notice id: {} in path {}.", info.id, content_path);
        return false;
    }
    gui.notice_info_icon[info.time].init(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);

    return gui.notice_info_icon.contains(info.time);
}

static bool set_notice_info(GuiState &gui, EmuEnvState &emuenv, const NoticeList &info) {
    std::string msg, name;
    fs::path content_path;

    auto &lang = gui.lang.indicator;
    if (info.type == "content") {
        if (info.group.find("gd") != std::string::npos) {
            content_path = fs::path("app") / info.id;
            msg = lang["app_added_home"];
        } else {
            if (info.group == "ac")
                content_path = fs::path("addcont") / info.id / info.content_id;
            else if (info.group.find("gp") != std::string::npos)
                content_path = fs::path("app") / info.id;
            else if (info.group == "theme")
                content_path = fs::path("theme") / info.content_id;
            msg = lang["install_complete"];
        }
        vfs::FileBuffer params;
        if (vfs::read_file(VitaIoDevice::ux0, params, emuenv.pref_path, content_path / "sce_sys/param.sfo")) {
            SfoFile sfo_handle;
            sfo::load(sfo_handle, params);
            if (!sfo::get_data_by_key(name, sfo_handle, fmt::format("TITLE_{:0>2d}", emuenv.cfg.sys_lang)))
                sfo::get_data_by_key(name, sfo_handle, "TITLE");
        } else {
            LOG_WARN("Content not found for id: {}, in path: {}", info.content_id, content_path);
            return false;
        }
        init_notice_icon(gui, emuenv, content_path / "sce_sys/icon0.png", info);
    } else {
        auto &common = gui.lang.common.main;
        switch (static_cast<np::trophy::SceNpTrophyGrade>(string_utils::stoi_def(info.group, 0, "trophy group"))) {
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM:
            name = fmt::format("({}) ", common["platinum"]);
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

        const auto trophy_conf_id_path{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "trophy/conf" / info.id };
        const std::string sfm_name = fs::exists(trophy_conf_id_path / fmt::format("TROP_{:0>2d}.SFM", emuenv.cfg.sys_lang)) ? fmt::format("TROP_{:0>2d}.SFM", emuenv.cfg.sys_lang) : "TROP.SFM";

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

        content_path = fs::path("user") / emuenv.io.user_id / "trophy/conf" / info.id / fmt::format("TROP{}.PNG", info.content_id);
        if (!init_notice_icon(gui, emuenv, content_path, info))
            return false;
    }

    notice_info.push_back({ info, name, msg });

    return true;
}

void init_notice_info(GuiState &gui, EmuEnvState &emuenv) {
    notice_info.clear();
    notice_info_count_new = 0;
    gui.notice_info_icon.clear();

    if (!notice_list.empty()) {
        for (auto &[user, lists] : notice_list) {
            if ((user == "global") || (user == emuenv.io.user_id)) {
                auto notice_it = lists.begin();
                while (notice_it != lists.end()) {
                    if (!set_notice_info(gui, emuenv, *notice_it)) {
                        notice_it = lists.erase(notice_it);
                        save_notice_list(emuenv);
                    } else {
                        ++notice_it;
                    }
                }
            }
        }

        notice_info_count_new = notice_list_count_new["global"] + notice_list_count_new[emuenv.io.user_id];

        // Sort in date order
        std::sort(notice_info.begin(), notice_info.end(), [&](const NoticeInfo &na, const NoticeInfo &nb) {
            return na.time > nb.time;
        });
    }
}

void get_notice_list(EmuEnvState &emuenv) {
    notice_list.clear();
    notice_list_count_new.clear();
    const auto notice_path{ emuenv.pref_path / "ux0/user/notice.xml" };

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
                        noticeList.is_new = notice.attribute("new").as_bool();
                        notice_list[user_id].push_back(noticeList);
                    }
                }
            }
        } else {
            LOG_ERROR("Notice XML found is corrupted on path: {}", notice_path);
            fs::remove(notice_path);
        }
    }
}

void save_notice_list(EmuEnvState &emuenv) {
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
            info_child.append_attribute("new") = notice.is_new;
        }
    }

    const auto notice_path{ emuenv.pref_path / "ux0/user/notice.xml" };
    const auto save_xml = notice_xml.save_file(notice_path.c_str());
    if (!save_xml)
        LOG_ERROR("Fail save xml");
}

void update_notice_info(GuiState &gui, EmuEnvState &emuenv, const std::string &type) {
    NoticeList info;
    const auto user_id = type == "content" ? "global" : emuenv.io.user_id;
    if (type == "content") {
        info.id = emuenv.app_info.app_title_id;
        info.content_id = emuenv.app_info.app_content_id;
        info.group = emuenv.app_info.app_category;
    } else {
        const auto &trophy_data = gui.trophy_unlock_display_requests.back();
        info.id = trophy_data.np_com_id;
        info.content_id = trophy_data.trophy_id;
        info.group = std::to_string(static_cast<int>(trophy_data.trophy_kind));
    }
    info.type = type;
    info.time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    info.is_new = true;
    notice_list[user_id].push_back(info);
    if (set_notice_info(gui, emuenv, info)) {
        ++notice_list_count_new[user_id];
        ++notice_info_count_new;
        std::sort(notice_info.begin(), notice_info.end(), [&](const NoticeInfo &na, const NoticeInfo &nb) {
            return na.time > nb.time;
        });

        save_notice_list(emuenv);
    }
}

static void clean_notice_info_new(const std::string &user_id) {
    notice_info_count_new = 0;
    notice_list_count_new["global"] = 0;
    notice_list_count_new[user_id] = 0;
    for (auto &notice : notice_info)
        notice.is_new = false;
    for (auto &[user, lists] : notice_list) {
        if ((user == "global") || (user == user_id)) {
            for (auto &list : lists)
                list.is_new = false;
        }
    }
}

static std::string get_notice_time(GuiState &gui, EmuEnvState &emuenv, const time_t &time) {
    std::string date;
    const auto time_in_second = time / 1000;
    const auto diff_time = difftime(std::time(nullptr), time_in_second);
    constexpr auto minute = 60;
    constexpr auto hour = minute * 60;
    constexpr auto day = hour * 24;
    if (diff_time >= day) {
        tm date_tm = {};
        SAFE_LOCALTIME(&time_in_second, &date_tm);
        auto DATE_TIME = get_date_time(gui, emuenv, date_tm);
        date = fmt::format("{} {}", DATE_TIME[DateTime::DATE_MINI], DATE_TIME[DateTime::CLOCK]);
        if (emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR)
            date += fmt::format(" {}", DATE_TIME[DateTime::DAY_MOMENT]);
    } else {
        auto &lang = gui.lang.common.main;
        if (diff_time >= (hour * 2))
            date = fmt::format(fmt::runtime(lang["hours_ago"]), static_cast<uint32_t>(diff_time / hour));
        else if (diff_time >= hour)
            date = lang["one_hour_ago"];
        else if (diff_time >= (minute * 2))
            date = fmt::format(fmt::runtime(lang["minutes_ago"]), static_cast<uint32_t>(diff_time / 60));
        else
            date = lang["one_minute_ago"];
    }

    return date;
}

static void draw_notice_info(GuiState &gui, EmuEnvState &emuenv) {
    static bool notice_info_state;
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    const auto VIEWPORT_WIDTH_POS_MAX(VIEWPORT_POS.x + VIEWPORT_SIZE.x);

    const ImVec2 NOTICE_SIZE = notice_info_count_new ? ImVec2(104.0f * SCALE.x, 95.0f * SCALE.y) : ImVec2(90.0f * SCALE.x, 82.0f * SCALE.y);
    const ImVec2 NOTICE_ICON_POS(VIEWPORT_WIDTH_POS_MAX - NOTICE_SIZE.x, VIEWPORT_POS.y);
    const ImVec2 NOTICE_ICON_POS_MAX(NOTICE_ICON_POS.x + NOTICE_SIZE.x, NOTICE_ICON_POS.y + NOTICE_SIZE.y);

    const auto NOTICE_COLOR = gui.information_bar_color.notice_font;

    const ImVec2 WINDOW_SIZE = notice_info_state ? VIEWPORT_SIZE : (notice_info_count_new ? ImVec2(84.f * SCALE.x, 76.f * SCALE.y) : ImVec2(72.f * SCALE.x, 62.f * SCALE.y));
    const ImVec2 WINDOW_POS = ImVec2(VIEWPORT_POS.x + (notice_info_state ? 0.f : VIEWPORT_SIZE.x - WINDOW_SIZE.x), VIEWPORT_POS.y);

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 50.f * SCALE.x);
    ImGui::Begin("##notice_info", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    const auto draw_list = ImGui::GetForegroundDrawList();
    if (notice_info_count_new) {
        if (gui.theme_information_bar_notice.contains(NoticeIcon::NEW))
            draw_list->AddImage(gui.theme_information_bar_notice[NoticeIcon::NEW], NOTICE_ICON_POS, NOTICE_ICON_POS_MAX);
        else
            draw_list->AddCircleFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX - (24.f * SCALE.x), VIEWPORT_POS.y + (16.f * SCALE.y)), 60.f * SCALE.x, IM_COL32(11.f, 90.f, 252.f, 255.f));
        const auto FONT_SCALE = 40.f * SCALE.x;
        const auto NOTICE_COUNT_FONT_SCALE = FONT_SCALE / 40.f;
        const auto NOTICE_COUNT_SIZE = ImGui::CalcTextSize(std::to_string(notice_info_count_new).c_str()).x * NOTICE_COUNT_FONT_SCALE;
        draw_list->AddText(gui.vita_font[emuenv.current_font_level], FONT_SCALE, ImVec2(VIEWPORT_WIDTH_POS_MAX - (NOTICE_SIZE.x / 2.f) - (NOTICE_COUNT_SIZE / 2.f) + (12.f * SCALE.x), VIEWPORT_POS.y + (15.f * SCALE.y)), NOTICE_COLOR, std::to_string(notice_info_count_new).c_str());
    } else {
        if (gui.theme_information_bar_notice.contains(NoticeIcon::NO))
            draw_list->AddImage(gui.theme_information_bar_notice[NoticeIcon::NO], NOTICE_ICON_POS, NOTICE_ICON_POS_MAX);
        else
            draw_list->AddCircleFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX - (28.f * SCALE.x), VIEWPORT_POS.y + (18.f * SCALE.y)), 44.f * SCALE.x, IM_COL32(62.f, 98.f, 160.f, 255.f));
    }

    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (notice_info_state && (notice_info_count_new > 0)) {
            clean_notice_info_new(emuenv.io.user_id);
            save_notice_list(emuenv);
        }
        notice_info_state = !notice_info_state;
    }

    if (notice_info_state) {
        const auto POPUP_SIZE = notice_info.empty() ? ImVec2(412.f * SCALE.x, 86.f * SCALE.y) : ImVec2(782.f * SCALE.x, notice_info.size() < 5 ? 22.f * emuenv.manual_dpi_scale + ((80.f * SCALE.y) * notice_info.size() + (10.f * (notice_info.size() - 1) * emuenv.manual_dpi_scale)) : 464.f * SCALE.y);
        const auto POPUP_POS = ImVec2(VIEWPORT_POS.x + (notice_info.empty() ? VIEWPORT_SIZE.x - (502.f * SCALE.y) : (VIEWPORT_SIZE.x / 2.f) - (POPUP_SIZE.x / 2.f)), VIEWPORT_POS.y + (56.f * SCALE.y));
        const auto POPUP_BG_COLOR = notice_info.empty() ? GUI_COLOR_TEXT : GUI_SMOOTH_GRAY;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, POPUP_BG_COLOR);
        ImGui::PushStyleColor(ImGuiCol_Border, GUI_COLOR_TEXT);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, notice_info.empty() ? 0.f : 8.0f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::SetNextWindowPos(POPUP_POS, ImGuiCond_Always);
        ImGui::BeginChild("##notice_info_child", POPUP_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoSavedSettings);
        auto &lang = gui.lang.indicator;
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
                if (gui.notice_info_icon.contains(notice.time)) {
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
                if (ImGui::Selectable("##icon", notice.is_new, ImGuiSelectableFlags_SpanAllColumns, SELECT_SIZE)) {
                    clean_notice_info_new(emuenv.io.user_id);
                    save_notice_list(emuenv);
                    if (notice.type == "content") {
                        if (notice.group == "theme")
                            pre_load_app(gui, emuenv, false, "NPXS10015");
                        else
                            select_app(gui, notice.id);
                    } else {
                        pre_load_app(gui, emuenv, false, "NPXS10008");
                        open_trophy_unlocked(gui, emuenv, notice.id, notice.content_id);
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
                const auto notice_time = get_notice_time(gui, emuenv, notice.time);
                const auto notice_time_size = ImGui::CalcTextSize(notice_time.c_str());
                ImGui::SetCursorPos(ImVec2(POPUP_SIZE.x - (34.f * SCALE.x) - notice_time_size.x, ImGui::GetCursorPosY() - (8.f * SCALE.y)));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice_time.c_str());
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);

        if (!notice_info.empty()) {
            const auto DELETE_POPUP_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);
            const auto BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);

            ImGui::SetWindowFontScale(1.f * RES_SCALE.x);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
            ImGui::SetCursorPos(ImVec2(VIEWPORT_SIZE.x - (70.f * SCALE.x), VIEWPORT_SIZE.y - (52.f * SCALE.y)));
            if (ImGui::Button("...", ImVec2(64.f * SCALE.x, 40.f * SCALE.y)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_triangle)))
                ImGui::OpenPopup("...");
            if (ImGui::BeginPopup("...", ImGuiWindowFlags_NoMove)) {
                if (ImGui::Button(lang["delete_all"].c_str()))
                    ImGui::OpenPopup("Delete All");
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
                ImGui::SetNextWindowSize(DELETE_POPUP_SIZE, ImGuiCond_Always);
                ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + (VIEWPORT_SIZE.x / 2.f) - (DELETE_POPUP_SIZE.x / 2.f), VIEWPORT_POS.y + (VIEWPORT_SIZE.y / 2.f) - (DELETE_POPUP_SIZE.y / 2.f)));
                if (ImGui::BeginPopupModal("Delete All", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
                    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                    auto &common = emuenv.common_dialog.lang.common;
                    ImGui::SetCursorPosY((DELETE_POPUP_SIZE.y / 2.f) - (46.f * SCALE.y));
                    TextColoredCentered(GUI_COLOR_TEXT, lang["notif_deleted"].c_str());
                    ImGui::SetCursorPos(ImVec2((DELETE_POPUP_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), DELETE_POPUP_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
                    if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine(0.f, 20.f);
                    if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
                        notice_info.clear();
                        for (auto &notice : gui.notice_info_icon)
                            notice.second = {};
                        gui.notice_info_icon.clear();
                        notice_list["global"].clear();
                        notice_list[emuenv.io.user_id].clear();
                        save_notice_list(emuenv);
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
    ImGui::PopStyleVar();

    const auto display_size = ImGui::GetIO().DisplaySize;
    draw_list->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(display_size.x, VIEWPORT_POS.y), IM_COL32(0.f, 0.f, 0.f, 255.f));
    draw_list->AddRectFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX, VIEWPORT_POS.y), ImVec2(display_size.x, VIEWPORT_POS.y + display_size.y), IM_COL32(0.f, 0.f, 0.f, 255.f));
}

void draw_information_bar(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    const ImVec2 INFO_BAR_SIZE(VIEWPORT_SIZE.x, 32.f * SCALE.y);

    constexpr ImU32 DEFAULT_BAR_COLOR = 0xFF000000; // Black
    constexpr ImU32 DEFAULT_INDICATOR_COLOR = 0xFFFFFFFF; // White

    const auto is_12_hour_format = emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;
    const auto is_notif_pos = !gui.vita_area.start_screen && (gui.vita_area.live_area_screen || gui.vita_area.home_screen) ? 78.f * SCALE.x : 0.f;
    const auto is_theme_color = gui.vita_area.home_screen || gui.vita_area.live_area_screen || gui.vita_area.start_screen;
    const auto indicator_color = gui.information_bar_color.indicator;
    const auto bar_color = gui.information_bar_color.bar;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, VIEWPORT_POS.y + INFO_BAR_SIZE.y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("##information_bar", &gui.vita_area.information_bar, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(3);

    const ImVec2 INFO_BAR_POS_MAX(VIEWPORT_POS.x + INFO_BAR_SIZE.x, VIEWPORT_POS.y + INFO_BAR_SIZE.y);
    const auto draw_list = ImGui::GetBackgroundDrawList();
    draw_list->AddRectFilled(VIEWPORT_POS, INFO_BAR_POS_MAX, is_theme_color ? bar_color : DEFAULT_BAR_COLOR, 0.f, ImDrawFlags_RoundCornersAll);

    if (gui.vita_area.home_screen || gui.vita_area.live_area_screen) {
        const auto HOME_ICON_POS_CENTER = VIEWPORT_POS.x + (INFO_BAR_SIZE.x / 2.f) - (32.f * (static_cast<float>(gui.live_area_current_open_apps_list.size()) / 2.f)) * SCALE.x;
        const auto APP_IS_OPEN = gui.live_area_app_current_open >= 0;

        // Draw Home Icon
        const std::vector<ImVec2> HOME_UP_POS = {
            ImVec2(HOME_ICON_POS_CENTER - (13.f * SCALE.x), VIEWPORT_POS.y + (16.f * SCALE.y)),
            ImVec2(HOME_ICON_POS_CENTER, VIEWPORT_POS.y + (6.f * SCALE.y)),
            ImVec2(HOME_ICON_POS_CENTER + (13.f * SCALE.x), VIEWPORT_POS.y + (16.f * SCALE.y))
        };
        const auto HOME_DOWN_POS_MINI = ImVec2(HOME_ICON_POS_CENTER - (8.f * SCALE.x), VIEWPORT_POS.y + (16.f * SCALE.y));
        const auto HOME_DOWN_POS_MAX = ImVec2(HOME_ICON_POS_CENTER + (8.f * SCALE.x), HOME_DOWN_POS_MINI.y + (10.f * SCALE.y));

        draw_list->AddTriangleFilled(HOME_UP_POS[0], HOME_UP_POS[1], HOME_UP_POS[2], indicator_color);
        draw_list->AddRectFilled(HOME_DOWN_POS_MINI, HOME_DOWN_POS_MAX, indicator_color);
        if (APP_IS_OPEN) {
            draw_list->AddTriangleFilled(HOME_UP_POS[0], HOME_UP_POS[1], HOME_UP_POS[2], IM_COL32(0.f, 0.f, 0.f, 100.f));
            draw_list->AddRectFilled(HOME_DOWN_POS_MINI, HOME_DOWN_POS_MAX, IM_COL32(0.f, 0.f, 0.f, 100.f));
        }
        draw_list->AddRectFilled(ImVec2(HOME_ICON_POS_CENTER - (3.f * SCALE.x), VIEWPORT_POS.y + (18.5f * SCALE.y)), ImVec2(HOME_ICON_POS_CENTER + (3.f * SCALE.x), VIEWPORT_POS.y + (26.f * SCALE.y)), bar_color);

        // Draw App Icon
        const float decal_app_icon_pos = 34.f * ((static_cast<float>(gui.live_area_current_open_apps_list.size()) - 2) / 2.f);
        const auto ICON_SIZE_SCALE = 28.f * SCALE.x;

        for (auto a = 0; a < gui.live_area_current_open_apps_list.size(); a++) {
            const ImVec2 ICON_POS_MIN(VIEWPORT_POS.x + (INFO_BAR_SIZE.x / 2.f) - (14.f * SCALE.x) - (decal_app_icon_pos * SCALE.x) + (a * (34 * SCALE.x)), VIEWPORT_POS.y + (2.f * SCALE.y));
            const ImVec2 ICON_POS_MAX(ICON_POS_MIN.x + ICON_SIZE_SCALE, ICON_POS_MIN.y + ICON_SIZE_SCALE);
            const ImVec2 ICON_CENTER_POS(ICON_POS_MIN.x + (ICON_SIZE_SCALE / 2.f), ICON_POS_MIN.y + (ICON_SIZE_SCALE / 2.f));
            const auto &APPS_OPENED = gui.live_area_current_open_apps_list[a];
            auto &APP_ICON_TYPE = APPS_OPENED.starts_with("NPXS") && (APPS_OPENED != "NPXS10007") ? gui.app_selector.sys_apps_icon : gui.app_selector.user_apps_icon;

            // Check if icon exist
            if (APP_ICON_TYPE.contains(APPS_OPENED))
                draw_list->AddImageRounded(APP_ICON_TYPE[APPS_OPENED], ICON_POS_MIN, ICON_POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_SIZE_SCALE, ImDrawFlags_RoundCornersAll);
            else
                draw_list->AddCircleFilled(ICON_CENTER_POS, ICON_SIZE_SCALE / 2.f, IM_COL32_WHITE);

            // hide Icon no opened
            if (!APP_IS_OPEN || (gui.live_area_current_open_apps_list[gui.live_area_app_current_open] != APPS_OPENED))
                draw_list->AddCircleFilled(ICON_CENTER_POS, ICON_SIZE_SCALE / 2.f, IM_COL32(0.f, 0.f, 0.f, 140.f));
        }
    }

    constexpr auto PIX_FONT_SCALE = 19.2f / 24.f;
    const auto DEFAULT_FONT_SCALE = ImGui::GetFontSize() / (19.2f * emuenv.manual_dpi_scale);
    const auto CLOCK_DEFAULT_FONT_SCALE = (24.f * emuenv.manual_dpi_scale) * DEFAULT_FONT_SCALE;
    const auto DAY_MOMENT_DEFAULT_FONT_SCALE = (18.f * emuenv.manual_dpi_scale) * DEFAULT_FONT_SCALE;
    const auto CLOCK_FONT_SIZE_SCALE = CLOCK_DEFAULT_FONT_SCALE / ImGui::GetFontSize();
    const auto DAY_MOMENT_FONT_SIZE_SCALE = DAY_MOMENT_DEFAULT_FONT_SCALE / ImGui::GetFontSize();

    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);

    tm local = {};
    SAFE_LOCALTIME(&tt, &local);

    auto DATE_TIME = get_date_time(gui, emuenv, local);
    const auto CALC_CLOCK_SIZE = ImGui::CalcTextSize(DATE_TIME[DateTime::CLOCK].c_str());
    const auto CLOCK_SIZE_SCALE = ImVec2((CALC_CLOCK_SIZE.x * CLOCK_FONT_SIZE_SCALE) * RES_SCALE.x, (CALC_CLOCK_SIZE.y * CLOCK_FONT_SIZE_SCALE * PIX_FONT_SCALE) * RES_SCALE.y);
    const auto CALC_DAY_MOMENT_SIZE = ImGui::CalcTextSize(DATE_TIME[DateTime::DAY_MOMENT].c_str());
    const auto DAY_MOMENT_SIZE_SCALE = emuenv.io.user_id.empty() || is_12_hour_format ? ImVec2((CALC_DAY_MOMENT_SIZE.x * DAY_MOMENT_FONT_SIZE_SCALE) * RES_SCALE.y, (CALC_DAY_MOMENT_SIZE.y * DAY_MOMENT_FONT_SIZE_SCALE * PIX_FONT_SCALE) * RES_SCALE.y) : ImVec2(0.f, 0.f);

    const ImVec2 CLOCK_POS(VIEWPORT_POS.x + INFO_BAR_SIZE.x - (64.f * SCALE.x) - CLOCK_SIZE_SCALE.x - DAY_MOMENT_SIZE_SCALE.x - is_notif_pos, VIEWPORT_POS.y + (INFO_BAR_SIZE.y / 2.f) - (CLOCK_SIZE_SCALE.y / 2.f));
    const auto DAY_MOMENT_POS = ImVec2(CLOCK_POS.x + CLOCK_SIZE_SCALE.x + (6.f * SCALE.x), CLOCK_POS.y + (CLOCK_SIZE_SCALE.y - DAY_MOMENT_SIZE_SCALE.y));

    // Draw clock
    draw_list->AddText(gui.vita_font[emuenv.current_font_level], CLOCK_DEFAULT_FONT_SCALE * RES_SCALE.x, CLOCK_POS, is_theme_color ? indicator_color : DEFAULT_INDICATOR_COLOR, DATE_TIME[DateTime::CLOCK].c_str());
    if (emuenv.io.user_id.empty() || is_12_hour_format)
        draw_list->AddText(gui.vita_font[emuenv.current_font_level], DAY_MOMENT_DEFAULT_FONT_SCALE * RES_SCALE.x, DAY_MOMENT_POS, is_theme_color ? indicator_color : DEFAULT_INDICATOR_COLOR, DATE_TIME[DateTime::DAY_MOMENT].c_str());

    // Set full size and position of battery
    const auto FULL_BATTERY_SIZE = 38.f * SCALE.x;
    const auto BATTERY_HEIGHT_MIN_POS = VIEWPORT_POS.y + (5.f * SCALE.y);
    const ImVec2 BATTERY_MAX_POS(VIEWPORT_POS.x + INFO_BAR_SIZE.x - (12.f * SCALE.x) - is_notif_pos, BATTERY_HEIGHT_MIN_POS + (22 * SCALE.y));
    const ImVec2 BATTERY_BASE_MIN_POS(BATTERY_MAX_POS.x - FULL_BATTERY_SIZE - (4.f * SCALE.x), VIEWPORT_POS.y + (12.f * SCALE.y));
    const ImVec2 BATTERY_BASE_MAX_POS(BATTERY_BASE_MIN_POS.x + (4.f * SCALE.x), BATTERY_BASE_MIN_POS.y + (8 * SCALE.y));

    // Draw battery background
    constexpr ImU32 BATTERY_BG_COLOR = IM_COL32(128.f, 124.f, 125.f, 255.f);
    draw_list->AddRectFilled(BATTERY_BASE_MIN_POS, BATTERY_BASE_MAX_POS, BATTERY_BG_COLOR);
    draw_list->AddRectFilled(ImVec2(BATTERY_MAX_POS.x - FULL_BATTERY_SIZE, BATTERY_HEIGHT_MIN_POS), BATTERY_MAX_POS, BATTERY_BG_COLOR, 2.f * SCALE.x, ImDrawFlags_RoundCornersAll);

    // Set default battery size
    auto battery_size = FULL_BATTERY_SIZE;

    // Get battery level and adjust size accordingly to the battery level
    int32_t res;
    SDL_GetPowerInfo(NULL, &res);
    if ((res >= 0) && (res <= 75)) {
        if (res <= 25)
            battery_size *= 25.f;
        else if (res <= 50)
            battery_size *= 50.f;
        else
            battery_size *= 75.f;

        battery_size /= 100.f;
    }

    // Set battery color depending on battery level: red for levels less than or equal to 25% and green for levels above this threshold.
    const ImU32 BATTERY_COLOR = (res >= 0) && (res <= 25) ? IM_COL32(225.f, 50.f, 50.f, 255.f) : IM_COL32(90.f, 200.f, 30.f, 255.f);

    // Draw battery level
    if (battery_size == FULL_BATTERY_SIZE)
        draw_list->AddRectFilled(BATTERY_BASE_MIN_POS, BATTERY_BASE_MAX_POS, BATTERY_COLOR);
    draw_list->AddRectFilled(ImVec2(BATTERY_MAX_POS.x - battery_size, BATTERY_HEIGHT_MIN_POS), BATTERY_MAX_POS, BATTERY_COLOR, 2.f * SCALE.x, ImDrawFlags_RoundCornersAll);

    if (emuenv.display.imgui_render && !gui.vita_area.start_screen && !gui.vita_area.live_area_screen && !gui.vita_area.user_management && !gui.help_menu.vita3k_update && get_sys_apps_state(gui) && (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) || ImGui::IsItemClicked(0)))
        gui.vita_area.information_bar = false;

    if (is_notif_pos)
        draw_notice_info(gui, emuenv);

    ImGui::End();
}

} // namespace gui
