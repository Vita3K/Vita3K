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

#include <gui-imgui/functions.h>

#include <config/state.h>
#include <io/state.h>
#include <packages/sfo.h>
#include <util/safe_time.h>
#include <util/string_utils.h>

#include <io/VitaIoDevice.h>
#include <util/log.h>

#include <pugixml.hpp>
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
        const auto notice_info_it = std::ranges::find_if(notice_info, [&](const NoticeInfo &n) {
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

    if (!vfs::read_file(VitaIoDevice::ux0, buffer, emuenv.vita_fs_path, content_path)) {
        if (info.type == "trophy") {
            LOG_WARN("Icon no found for trophy id: {} on NpComId: {}", info.content_id, info.id);
            return false;
        } else {
            if (!vfs::read_app_file(buffer, emuenv.vita_fs_path, info.id, "sce_sys/icon0.png")) {
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
    gui.notice_info_icon[info.time] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
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
        if (vfs::read_file(VitaIoDevice::ux0, params, emuenv.vita_fs_path, content_path / "sce_sys/param.sfo")) {
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

        const auto trophy_conf_id_path{ emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "trophy/conf" / info.id };
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
        msg = ::lang::get(::lang::str::trophy_earned);

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
    const auto notice_path{ emuenv.vita_fs_path / "ux0/user/notice.xml" };

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

    const auto notice_path{ emuenv.vita_fs_path / "ux0/user/notice.xml" };
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

void draw_indicator(GuiState &gui, EmuEnvState &emuenv) {
    static bool notice_info_state;
    const auto &viewport = sdl::get_viewport();
    const ImVec2 VIEWPORT_POS(viewport.logical_x, viewport.logical_y);
    const ImVec2 VIEWPORT_SIZE(viewport.logical_width, viewport.logical_height);
    const ImVec2 RES_SCALE(viewport.scale_x, viewport.scale_y);
    const ImVec2 SCALE(RES_SCALE.x * viewport.manual_dpi_scale, RES_SCALE.y * viewport.manual_dpi_scale);

    const auto VIEWPORT_WIDTH_POS_MAX(VIEWPORT_POS.x + VIEWPORT_SIZE.x);

    const ImVec2 NOTICE_SIZE = notice_info_count_new ? ImVec2(108.f * SCALE.x, 99.0f * SCALE.y) : ImVec2(88.f * SCALE.x, 81.f * SCALE.y);
    const ImVec2 NOTICE_ICON_POS(VIEWPORT_WIDTH_POS_MAX - NOTICE_SIZE.x, VIEWPORT_POS.y);
    const ImVec2 NOTICE_ICON_POS_MAX(NOTICE_ICON_POS.x + NOTICE_SIZE.x, NOTICE_ICON_POS.y + NOTICE_SIZE.y);

    const auto NOTICE_COLOR = gui.information_bar_color.notice_font;

    const ImVec2 WINDOW_SIZE = notice_info_state ? VIEWPORT_SIZE : (notice_info_count_new ? ImVec2(86.f * SCALE.x, 78.f * SCALE.y) : ImVec2(70.f * SCALE.x, 64.f * SCALE.y));
    const ImVec2 WINDOW_POS = ImVec2(VIEWPORT_POS.x + (notice_info_state ? 0.f : VIEWPORT_SIZE.x - WINDOW_SIZE.x), VIEWPORT_POS.y);

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("##notice_info", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    const auto draw_list = ImGui::GetWindowDrawList();

    const float tex_w = NOTICE_SIZE.x;
    const float tex_h = NOTICE_SIZE.y;
    const ImVec2 uv0(0.5f / tex_w, 0.5f / tex_h);
    const ImVec2 uv1((tex_w - 0.5f) / tex_w, (tex_h - 0.5f) / tex_h);

    const auto FG_DRAW_LIST = ImGui::GetForegroundDrawList();
    FG_DRAW_LIST->PushClipRect(VIEWPORT_POS, ImVec2(VIEWPORT_WIDTH_POS_MAX, VIEWPORT_POS.y + VIEWPORT_SIZE.y), true);
    if (notice_info_count_new) {
        if (gui.theme_information_bar_notice.contains(NoticeIcon::NEW))
            FG_DRAW_LIST->AddImage(gui.theme_information_bar_notice[NoticeIcon::NEW], NOTICE_ICON_POS, NOTICE_ICON_POS_MAX, uv0, uv1);
        else
            FG_DRAW_LIST->AddCircleFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX - (23.f * SCALE.x), VIEWPORT_POS.y + (14.f * SCALE.y)), 61.f * SCALE.x, IM_COL32(11.f, 90.f, 252.f, 255.f));
        const auto FONT_SCALE = 40.f * SCALE.x;
        const auto NOTICE_COUNT_FONT_SCALE = FONT_SCALE / 40.f;
        const auto NOTICE_COUNT_SIZE = ImGui::CalcTextSize(std::to_string(notice_info_count_new).c_str()).x * NOTICE_COUNT_FONT_SCALE;
        FG_DRAW_LIST->AddText(gui.vita_font[gui.current_font_level], FONT_SCALE, ImVec2(VIEWPORT_WIDTH_POS_MAX - (NOTICE_SIZE.x / 2.f) - (NOTICE_COUNT_SIZE / 2.f) + (12.f * SCALE.x), VIEWPORT_POS.y + (15.f * SCALE.y)), NOTICE_COLOR, std::to_string(notice_info_count_new).c_str());
    } else {
        if (gui.theme_information_bar_notice.contains(NoticeIcon::NO))
            FG_DRAW_LIST->AddImage(gui.theme_information_bar_notice[NoticeIcon::NO], NOTICE_ICON_POS, NOTICE_ICON_POS_MAX, uv0, uv1);
        else
            FG_DRAW_LIST->AddCircleFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX - (22.5f * SCALE.x), VIEWPORT_POS.y + (15.f * SCALE.y)), 45.f * SCALE.x, IM_COL32(62.f, 98.f, 160.f, 255.f));
    }
    FG_DRAW_LIST->PopClipRect();

    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (notice_info_state && (notice_info_count_new > 0)) {
            clean_notice_info_new(emuenv.io.user_id);
            save_notice_list(emuenv);
        }
        notice_info_state = !notice_info_state;
    }

    if (notice_info_state) {
        const auto SELECT_SIZE = ImVec2(776.f * SCALE.x, 90.f * SCALE.y);
        const auto BORDER_SIZE = notice_info.empty() ? 0.f : 8.f * SCALE.x;
        const auto HALF_BORDER_SIZE = BORDER_SIZE / 2.f;
        const auto VISIBLE_COUNT = std::min(notice_info.size(), size_t(5));
        const auto INDICATOR_SIZE = notice_info.empty() ? ImVec2(412.f * SCALE.x, 86.f * SCALE.y) : ImVec2(SELECT_SIZE.x, (VISIBLE_COUNT * SELECT_SIZE.y) + BORDER_SIZE + ((VISIBLE_COUNT - 1) * 1.f));
        const auto INDICATOR_POS = ImVec2(VIEWPORT_POS.x + (notice_info.empty() ? VIEWPORT_SIZE.x - (502.f * SCALE.x) : (VIEWPORT_SIZE.x / 2.f) - (INDICATOR_SIZE.x / 2.f)), VIEWPORT_POS.y + (57.f * SCALE.y) + HALF_BORDER_SIZE);
        const ImVec4 INDICATOR_BORDER_COLOR(0.93f, 0.93f, 0.945f, 1.f);
        const auto INDICATOR_BG_COLOR = notice_info.empty() ? INDICATOR_BORDER_COLOR : GUI_SMOOTH_GRAY;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, INDICATOR_BG_COLOR);
        ImGui::PushStyleColor(ImGuiCol_Border, INDICATOR_BORDER_COLOR);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, BORDER_SIZE);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(HALF_BORDER_SIZE, HALF_BORDER_SIZE));
        ImGui::SetNextWindowPos(INDICATOR_POS, ImGuiCond_Always);
        ImGui::BeginChild("##notice_info_child", INDICATOR_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        auto &lang = gui.lang.indicator;

        const ImVec2 tip_top(INDICATOR_POS.x + INDICATOR_SIZE.x, INDICATOR_POS.y + (22.f * SCALE.y));
        const ImVec2 tip_right(tip_top.x + (30.f * SCALE.x), INDICATOR_POS.y + (3.f * SCALE.y));
        const ImVec2 tip_bottom(tip_top.x, INDICATOR_POS.y + (46.f * SCALE.y));
        draw_list->AddTriangleFilled(tip_top, tip_right, tip_bottom, ImGui::ColorConvertFloat4ToU32(INDICATOR_BORDER_COLOR));

        if (notice_info.empty()) {
            ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
            const auto no_notif = lang["no_notif"].c_str();
            const auto calc_text = ImGui::CalcTextSize(no_notif);
            ImGui::SetCursorPos(ImVec2((INDICATOR_SIZE.x / 2.f) - (calc_text.x / 2.f), (INDICATOR_SIZE.y / 2.f) - (calc_text.y / 2.f)));
            ImGui::TextColored(ImVec4(0.f, 0.f, 0.f, 1.f), "%s", no_notif);
        } else {
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 82.f * SCALE.x);
            const auto ICON_SIZE = ImVec2(64.f * SCALE.x, 64.f * SCALE.y);

            for (const auto &notice : notice_info) {
                if (notice.time != notice_info.front().time)
                    ImGui::Separator();
                const auto BEGIN_POS = ImGui::GetCursorPos();
                if (gui.notice_info_icon.contains(notice.time)) {
                    ImGui::SetCursorPos(ImVec2(BEGIN_POS.x + (ImGui::GetColumnWidth() / 2.f) - (ICON_SIZE.x / 2.f), BEGIN_POS.y + (SELECT_SIZE.y / 2.f) - (ICON_SIZE.y / 2.f)));
                    ImGui::Image(gui.notice_info_icon[notice.time], ICON_SIZE);
                }
                ImGui::SetCursorPosY(BEGIN_POS.y);
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
                            select_app(gui, emuenv, notice.id);
                    } else {
                        pre_load_app(gui, emuenv, false, "NPXS10008");
                        open_trophy_unlocked(gui, emuenv, notice.id, notice.content_id);
                    }
                    notice_info_state = false;
                }
                ImGui::PopStyleColor(3);
                ImGui::PopID();
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.34f * RES_SCALE.y);
                ImGui::SetCursorPosY(BEGIN_POS.y + (20.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice.name.c_str());
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (14.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice.msg.c_str());
                ImGui::SetWindowFontScale(0.94f * RES_SCALE.x);
                const auto notice_time = get_notice_time(gui, emuenv, notice.time);
                const auto notice_time_size = ImGui::CalcTextSize(notice_time.c_str());
                ImGui::SetCursorPos(ImVec2(INDICATOR_SIZE.x - (34.f * SCALE.x) - notice_time_size.x, BEGIN_POS.y + (66.f * SCALE.y)));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice_time.c_str());
                ImGui::ScrollWhenDragging();
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(3);

        if (!notice_info.empty()) {
            const auto DELETE_POPUP_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);
            const auto BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);

            const auto MENU_BUTTON_SIZE = ImVec2(246.f * SCALE.x, 60.f * SCALE.y);
            const auto button_size = ImVec2(64.f * SCALE.x, 40.f * SCALE.y);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, button_size.x);
            ImGui::SetCursorPos(ImVec2(VIEWPORT_SIZE.x - (70.f * SCALE.x), VIEWPORT_SIZE.y - (52.f * SCALE.y)));
            const auto button_pos = ImGui::GetCursorScreenPos();
            ImGui::SetWindowFontScale(1.4f * RES_SCALE.y);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.502f, 0.502f, 0.502f, 1.f));
            draw_list->AddCircleFilled(ImVec2(button_pos.x + (button_size.x / 2.f), button_pos.y + (button_size.y / 2.f)), button_size.x / 2.f, IM_COL32(234, 234, 234, 255));
            if (ImGui::Button("...##ellipsis_menu", button_size) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_triangle))
                ImGui::OpenPopup("ellipsis_menu");
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.f * SCALE.x);
            ImGui::SetNextWindowSize(MENU_BUTTON_SIZE, ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2(button_pos.x - MENU_BUTTON_SIZE.x - (45.f * SCALE.x), button_pos.y - MENU_BUTTON_SIZE.y - (6.f * SCALE.y)), ImGuiCond_Always);
            if (ImGui::BeginPopupModal("ellipsis_menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.68f, 1.f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.23f, 0.68f, 1.f, 0.8f));
                if (ImGui::Button(lang["delete_all"].c_str(), MENU_BUTTON_SIZE))
                    ImGui::OpenPopup("Delete All");
                ImGui::SetNextWindowSize(DELETE_POPUP_SIZE, ImGuiCond_Always);
                ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + (VIEWPORT_SIZE.x / 2.f) - (DELETE_POPUP_SIZE.x / 2.f), VIEWPORT_POS.y + (VIEWPORT_SIZE.y / 2.f) - (DELETE_POPUP_SIZE.y / 2.f)));
                if (ImGui::BeginPopupModal("Delete All", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
                    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                    ImGui::SetCursorPosY((DELETE_POPUP_SIZE.y / 2.f) - (46.f * SCALE.y));
                    TextColoredCentered(GUI_COLOR_TEXT, lang["notif_deleted"].c_str());
                    ImGui::SetCursorPos(ImVec2((DELETE_POPUP_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), DELETE_POPUP_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
                    if (ImGui::Button(lang::get(lang::str::cancel).c_str(), BUTTON_SIZE) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_circle)) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine(0, 20.f * SCALE.x);
                    if (ImGui::Button(::lang::get(::lang::str::ok).c_str(), BUTTON_SIZE) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_cross)) {
                        notice_info.clear();
                        for (auto &notice : gui.notice_info_icon)
                            notice.second = {};
                        gui.notice_info_icon.clear();
                        notice_list["global"].clear();
                        notice_list[emuenv.io.user_id].clear();
                        clean_notice_info_new(emuenv.io.user_id);
                        save_notice_list(emuenv);
                        notice_info_state = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                } else {
                    const auto border_color = IM_COL32(255, 255, 255, 80);
                    const ImVec2 p0(ImGui::GetWindowPos().x - HALF_BORDER_SIZE, ImGui::GetWindowPos().y - HALF_BORDER_SIZE);
                    const ImVec2 p1(p0.x + ImGui::GetWindowSize().x + BORDER_SIZE, p0.y + ImGui::GetWindowSize().y + BORDER_SIZE);
                    draw_list->AddRect(p0, p1, border_color, 16.f * SCALE.x, 0, BORDER_SIZE);

                    const ImVec2 tip_top(p1.x + HALF_BORDER_SIZE, p1.y - (28.f * SCALE.y) - HALF_BORDER_SIZE);
                    const ImVec2 tip_right(tip_top.x + (20.f * SCALE.x), p1.y - (20.f * SCALE.y) - HALF_BORDER_SIZE);
                    const ImVec2 tip_bottom(tip_top.x, p1.y - (44.f * SCALE.y) - HALF_BORDER_SIZE);
                    draw_list->AddTriangleFilled(tip_top, tip_right, tip_bottom, border_color);
                }
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar(3);

                if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    ImGui::CloseCurrentPopup();

                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.f);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);

    const auto display_size = ImGui::GetIO().DisplaySize;
    draw_list->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(display_size.x, VIEWPORT_POS.y), IM_COL32(0.f, 0.f, 0.f, 255.f));
    draw_list->AddRectFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX, VIEWPORT_POS.y), ImVec2(display_size.x, VIEWPORT_POS.y + display_size.y), IM_COL32(0.f, 0.f, 0.f, 255.f));
}

} // namespace gui
