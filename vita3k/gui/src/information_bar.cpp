// Vita3K emulator project
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
    time_t date;
    std::string name;
    std::string msg;
};

struct NoticeList {
    std::string id;
    std::string content_id;
    std::string group;
    std::string type;
    time_t date;
};

static std::map<std::string, std::vector<NoticeList>> notice_list;
static std::map<std::string, int> notice_list_count_new;
static std::map<std::string, std::map<time_t, bool>> notice_list_new;
static std::map<time_t, bool> notice_info_new;
static int notice_info_count_new = 0;
static std::vector<NoticeInfo> notice_info;

static bool init_notice_icon(GuiState &gui, HostState &host, const fs::path &content_path, const NoticeList &info) {
    gui.notice_info_icon[info.date] = {};
    int32_t width = 0;
    int32_t height = 0;
    vfs::FileBuffer buffer;

    if (!vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, content_path)) {
        if (info.type == "trophy") {
            LOG_WARN("Icon no found for trophy id: {} on NpComId: {}", info.content_id, info.id);
            return false;
        } else {
            const auto default_fw_icon{ fs::path(host.pref_path) / "vs0/data/internal/livearea/default/sce_sys/icon0.png" };
            const auto default_icon{ fs::path(host.base_path) / "data/image/icon.png" };
            if (fs::exists(default_fw_icon) || fs::exists(default_icon)) {
                std::ifstream image_stream(fs::exists(default_fw_icon) ? default_fw_icon.string() : default_icon.string(), std::ios::binary | std::ios::ate);
                const std::size_t fsize = image_stream.tellg();
                buffer.resize(fsize);
                image_stream.seekg(0, std::ios::beg);
                image_stream.read(reinterpret_cast<char *>(&buffer[0]), fsize);
            } else {
                LOG_WARN("Not found defaut icon for this notice content: {}", info.content_id);
                return false;
            }
        }
    }
    stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
    if (!data) {
        LOG_ERROR("Invalid icon for notice id: {}, [{}] in path {}.", info.id, content_path.string());
        return false;
    }
    gui.notice_info_icon[info.date].init(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);

    return gui.notice_info_icon.find(info.date) != gui.notice_info_icon.end();
}

static bool set_notice_info(GuiState &gui, HostState &host, const NoticeList &info) {
    std::string msg, name;
    fs::path content_path;

    auto indicator = gui.lang.indicator;
    if (info.type == "content") {
        if (info.group.find("gd") != std::string::npos) {
            content_path = fs::path("app") / info.id;
            msg = !indicator["app_added_home"].empty() ? indicator["app_added_home"] : "The application has been added to the home screen.";
        } else {
            if (info.group == "ac")
                content_path = fs::path("addcont") / info.id / info.content_id;
            else if (info.group.find("gp") != std::string::npos)
                content_path = fs::path("patch") / info.id;
            else if (info.group == "theme")
                content_path = fs::path("theme") / info.content_id;
            msg = !indicator["install_complete"].empty() ? indicator["install_complete"] : "Installation complete.";
        }
        vfs::FileBuffer params;
        if (vfs::read_file(VitaIoDevice::ux0, params, host.pref_path, content_path.string() + "/sce_sys/param.sfo")) {
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
        switch (static_cast<np::trophy::SceNpTrophyGrade>(std::stoi(info.group))) {
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM:
            name = "(Platinum) ";
            break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD:
            name = ("Gold");
            break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER:
            name = "(Silver) ";
            break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE:
            name = "(Bronze) ";
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
        msg = !indicator["trophy_earned"].empty() ? indicator["trophy_earned"] : "You have earned a trophy!";

        content_path = fs::path("user") / host.io.user_id / "trophy/conf" / info.id / fmt::format("TROP{}.PNG", info.content_id);
        if (!init_notice_icon(gui, host, content_path, info))
            return false;
    }

    notice_info.push_back({ info.id, info.content_id, info.group, info.type, info.date, name, msg });

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
                            return n.date == notice.date;
                        });
                        notice_list[user.first].erase(notice_index);
                        save_notice_list(host);
                    }
                }
            }
        }

        notice_info_count_new = notice_list_count_new["global"] + notice_list_count_new[host.io.user_id];

        // Sort in date order
        std::sort(notice_info.begin(), notice_info.end(), [&](const NoticeInfo &na, const NoticeInfo &nb) {
            return na.date > nb.date;
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
                        noticeList.date = time_t(notice.attribute("date").as_int());
                        notice_list_new[user_id][noticeList.date] = notice.attribute("new").as_bool();
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

        for (const auto &notice : user.second) {
            auto info_child = user_child.append_child("info");
            info_child.append_attribute("id") = notice.id.c_str();
            info_child.append_attribute("content_id") = notice.content_id.c_str();
            info_child.append_attribute("group") = notice.group.c_str();
            info_child.append_attribute("type") = notice.type.c_str();
            info_child.append_attribute("date") = notice.date;
            info_child.append_attribute("new") = notice_list_new[user.first][notice.date];
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
    info.date = std::time(nullptr);
    notice_info_new[info.date] = true;
    notice_list_new[user_id][info.date] = true;
    notice_list[user_id].push_back(info);
    if (set_notice_info(gui, host, info)) {
        ++notice_info_count_new;
        ++notice_list_count_new[user_id];
        std::sort(notice_info.begin(), notice_info.end(), [&](const NoticeInfo &na, const NoticeInfo &nb) {
            return na.date > nb.date;
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
    auto diff_time = difftime(std::time(nullptr), time);
    static const auto minute = 60;
    static const auto hour = minute * 60;
    static const auto day = hour * 24;
    if (diff_time >= day) {
        tm date_tm = {};
        SAFE_LOCALTIME(&time, &date_tm);
        auto DATE_TIME = get_date_time(gui, host, date_tm);
        date = fmt::format("{} {}", DATE_TIME["date"], DATE_TIME["clock"]);
        if (gui.users[host.io.user_id].clock_12_hour)
            date += fmt::format(" {}", DATE_TIME["day-moment"]);
    } else {
        auto lang = gui.lang.common.common;
        if (diff_time >= (hour * 2))
            date = fmt::format(!lang["hours_ago"].empty() ? lang["hours_ago"] : "{} Hours Ago", uint32_t(diff_time / hour));
        else if (diff_time >= hour)
            date = !lang["one_hour_ago"].empty() ? lang["one_hour_ago"] : "1 Hour Ago";
        else if (diff_time >= (minute * 2))
            date = fmt::format(!lang["minutes_ago"].empty() ? lang["minutes_ago"] : "{} Minutes Ago", uint32_t(diff_time / 60));
        else
            date = !lang["one_minute_ago"].empty() ? lang["one_minute_ago"] : "1 Minute Ago";
    }

    return date;
}
static bool notice_info_state;

static void draw_notice_info(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto SCAL = ImVec2(display_size.x / 960.0f, display_size.y / 544.0f);
    const auto NOTICE_SIZE = notice_info_count_new ? ImVec2(104.0f * SCAL.x, 95.0f * SCAL.y) : ImVec2(90.0f * SCAL.x, 82.0f * SCAL.y);
    const auto NOTICE_POS = ImVec2(display_size.x - NOTICE_SIZE.x, 0.f);
    const ImU32 NOTICE_COLOR = 4294967295; // White
    const auto WINDOW_SIZE = notice_info_state ? display_size : (notice_info_count_new ? ImVec2(84.f * SCAL.x, 76.f * SCAL.y) : ImVec2(72.f * SCAL.x, 62.f * SCAL.y));
    const auto WINDOW_POS = ImVec2(notice_info_state ? 0.f : display_size.x - WINDOW_SIZE.x, 0.f);

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 50.f * SCAL.x);
    ImGui::Begin("##notice_info", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    if (notice_info_count_new) {
        if (gui.theme_information_bar_notice.find("new") != gui.theme_information_bar_notice.end())
            ImGui::GetForegroundDrawList()->AddImage(gui.theme_information_bar_notice["new"], NOTICE_POS, ImVec2(NOTICE_POS.x + NOTICE_SIZE.x, NOTICE_SIZE.y));
        else
            ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(display_size.x - (84.f * SCAL.x), (-40.f * SCAL.y)), ImVec2(display_size.x + (32.f * SCAL.y), 76.f * SCAL.y), IM_COL32(11.f, 90.f, 252.f, 255.f), 75.f * SCAL.x, ImDrawCornerFlags_All);
        ImGui::GetForegroundDrawList()->AddText(gui.vita_font, 40.f * SCAL.x, ImVec2(display_size.x - (NOTICE_SIZE.x / 2.f) + (10.f * SCAL.x), (10.f * SCAL.y)), NOTICE_COLOR, std::to_string(notice_info_count_new).c_str());
    } else {
        if (gui.theme_information_bar_notice.find("no") != gui.theme_information_bar_notice.end())
            ImGui::GetForegroundDrawList()->AddImage(gui.theme_information_bar_notice["no"], NOTICE_POS, ImVec2(NOTICE_POS.x + NOTICE_SIZE.x, NOTICE_SIZE.y));
        else
            ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(display_size.x - (70.f * SCAL.x), (-24.f * SCAL.y)), ImVec2(display_size.x + (14.f * SCAL.y), 60.f * SCAL.y), IM_COL32(62.f, 98.f, 160.f, 255.f), 75.f * SCAL.x, ImDrawCornerFlags_All);
    }

    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (notice_info_state) {
            clean_notice_info_new(host.io.user_id);
            save_notice_list(host);
        }
        notice_info_state = !notice_info_state;
    }

    if (notice_info_state) {
        const auto POPUP_SIZE = notice_info.empty() ? ImVec2(412.f * SCAL.x, 86.f * SCAL.y) : ImVec2(782.f * SCAL.x, notice_info.size() < 5 ? 22.f + ((80.f * SCAL.y) * notice_info.size() + (10.f * (notice_info.size() - 1))) : 464.f * SCAL.y);
        const auto POPUP_POS = ImVec2(notice_info.empty() ? display_size.x - (502.f * SCAL.y) : (display_size.x / 2.f) - (POPUP_SIZE.x / 2.f), 56.f * SCAL.y);
        const auto POPUP_BG_COLOR = notice_info.empty() ? GUI_COLOR_TEXT : GUI_SMOOTH_GRAY;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, POPUP_BG_COLOR);
        ImGui::PushStyleColor(ImGuiCol_Border, GUI_COLOR_TEXT);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, notice_info.empty() ? 0.f : 8.0f);
        ImGui::SetNextWindowPos(POPUP_POS, ImGuiCond_Always);
        ImGui::BeginChild("##notice_info_child", POPUP_SIZE, true, ImGuiWindowFlags_NoSavedSettings);
        auto indicator = gui.lang.indicator;
        if (notice_info.empty()) {
            ImGui::SetWindowFontScale(1.2f * SCAL.x);
            const auto no_notif = !indicator["no_notif"].empty() ? indicator["no_notif"].c_str() : "There are no notifications";
            const auto calc_text = ImGui::CalcTextSize(no_notif);
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - (calc_text.x / 2.f), (POPUP_SIZE.y / 2.f) - (calc_text.y / 2.f)));
            ImGui::TextColored(ImVec4(0.f, 0.f, 0.f, 1.f), "%s", no_notif);
        } else {
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 85.f * SCAL.x);
            const auto ICON_SIZE = ImVec2(64.f * SCAL.x, 64.f * SCAL.y);
            const auto SELECT_SIZE = ImVec2(POPUP_SIZE.x, 80.f * SCAL.y);

            for (const auto &notice : notice_info) {
                if (notice.date != notice_info[0].date)
                    ImGui::Separator();
                const auto ICON_POS = ImGui::GetCursorPos();
                if (gui.notice_info_icon.find(notice.date) != gui.notice_info_icon.end()) {
                    ImGui::SetCursorPos(ImVec2(ICON_POS.x + (ImGui::GetColumnWidth() / 2.f) - (ICON_SIZE.x / 2.f), ICON_POS.y + (SELECT_SIZE.y / 2.f) - (ICON_SIZE.y / 2.f)));
                    ImGui::Image(gui.notice_info_icon[notice.date], ICON_SIZE);
                }
                ImGui::SetCursorPosY(ICON_POS.y);
                ImGui::PushID(std::to_string(notice.date).c_str());
                const auto SELECT_COLOR = ImVec4(0.23f, 0.68f, 0.95f, 0.60f);
                const auto SELECT_COLOR_HOVERED = ImVec4(0.23f, 0.68f, 0.99f, 0.80f);
                const auto SELECT_COLOR_ACTIVE = ImVec4(0.23f, 0.68f, 1.f, 1.f);
                ImGui::PushStyleColor(ImGuiCol_Header, SELECT_COLOR);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, SELECT_COLOR_HOVERED);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, SELECT_COLOR_ACTIVE);
                if (ImGui::Selectable("##icon", notice_info_new[notice.date], host.io.app_path.empty() || ((notice.type == "content") && (notice.group == "theme")) || (notice.type == "trophy") ? ImGuiSelectableFlags_SpanAllColumns : ImGuiSelectableFlags_Disabled, SELECT_SIZE)) {
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
                ImGui::SetWindowFontScale(1.3f * SCAL.x);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (14.f * SCAL.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice.name.c_str());
                ImGui::Spacing();
                ImGui::SetWindowFontScale(0.9f * SCAL.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice.msg.c_str());
                const auto notice_time = get_notice_time(gui, host, notice.date);
                const auto notice_time_size = ImGui::CalcTextSize(notice_time.c_str());
                ImGui::SetCursorPos(ImVec2(POPUP_SIZE.x - (34.f * SCAL.x) - notice_time_size.x, ImGui::GetCursorPosY() - (8.f * SCAL.y)));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice_time.c_str());
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        if (!notice_info.empty()) {
            const auto DELETE_POPUP_SIZE = ImVec2(756.0f * SCAL.x, 436.0f * SCAL.y);
            const auto BUTTON_SIZE = ImVec2(320.f * SCAL.x, 46.f * SCAL.y);

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
            ImGui::SetCursorPos(ImVec2(display_size.x - (70.f * SCAL.x), display_size.y - (52.f * SCAL.y)));
            if (ImGui::Button("...", ImVec2(64.f * SCAL.x, 40.f * SCAL.y)) || ImGui::IsKeyPressed(host.cfg.keyboard_button_triangle))
                ImGui::OpenPopup("...");
            if (ImGui::BeginPopup("...", ImGuiWindowFlags_NoMove)) {
                if (ImGui::Button(!indicator["delete_all"].empty() ? indicator["delete_all"].c_str() : "Delete All"))
                    ImGui::OpenPopup("Delete All");
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
                ImGui::SetNextWindowSize(DELETE_POPUP_SIZE, ImGuiCond_Always);
                ImGui::SetNextWindowPos(ImVec2((display_size.x / 2.f) - (DELETE_POPUP_SIZE.x / 2.f), (display_size.y / 2.f) - (DELETE_POPUP_SIZE.y / 2.f)));
                if (ImGui::BeginPopupModal("Delete All", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
                    ImGui::SetWindowFontScale(1.4f * SCAL.x);
                    const auto notif_deleted = !indicator["notif_deleted"].empty() ? indicator["notif_deleted"].c_str() : "The notifications will be deleted.";
                    ImGui::SetCursorPos(ImVec2((DELETE_POPUP_SIZE.x / 2.f) - (ImGui::CalcTextSize(notif_deleted).x / 2.f), (DELETE_POPUP_SIZE.y / 2.f) - (46.f * SCAL.y)));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", notif_deleted);
                    ImGui::SetCursorPos(ImVec2((DELETE_POPUP_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCAL.x)), DELETE_POPUP_SIZE.y - BUTTON_SIZE.y - (24.0f * SCAL.y)));
                    if (ImGui::Button("Cancel", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle)) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine(0.f, 20.f);
                    if (ImGui::Button("Ok", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
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
        }
    }
    ImGui::End();
}

void draw_information_bar(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto SCAL = ImVec2(display_size.x / 960.0f, display_size.y / 544.0f);
    const auto MENUBAR_HEIGHT = 32.f * SCAL.y;
    ImU32 DEFAULT_BAR_COLOR = 4278190080; // Black
    ImU32 DEFAULT_INDICATOR_COLOR = 4294967295; // White
    const auto is_notif_pos = !gui.live_area.start_screen && (gui.live_area.live_area_screen || gui.live_area.app_selector) ? 78.f * SCAL.x : 0.f;

    const auto is_theme_color = gui.live_area.app_selector || gui.live_area.live_area_screen || gui.live_area.start_screen;

    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_size.x, MENUBAR_HEIGHT), ImGuiCond_Always);
    ImGui::Begin("##information_bar", &gui.live_area.information_bar, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(display_size.x, MENUBAR_HEIGHT), is_theme_color ? gui.information_bar_color["bar"] : DEFAULT_BAR_COLOR, 0.f, ImDrawCornerFlags_All);

    if (gui.live_area.app_selector || gui.live_area.live_area_screen) {
        const float decal_icon_pos = 38.f * ((float(gui.apps_list_opened.size()) - 1.f) / 2.f);
        for (auto a = 0; a < gui.apps_list_opened.size(); a++) {
            const auto icon_scal_pos = ImVec2((display_size.x / 2.f) - (16.f * SCAL.x) - (decal_icon_pos * SCAL.x) + (a * (38.f * SCAL.x)), display_size.y - (544.f * SCAL.y));
            const auto icon_scal_size = ImVec2(icon_scal_pos.x + (32.0f * SCAL.x), icon_scal_pos.y + (32.f * SCAL.y));
            if (get_app_icon(gui, gui.apps_list_opened[a])->first == gui.apps_list_opened[a])
                ImGui::GetForegroundDrawList()->AddImageRounded(get_app_icon(gui, gui.apps_list_opened[a])->second, icon_scal_pos, icon_scal_size, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 15.f, ImDrawCornerFlags_All);
            else
                ImGui::GetForegroundDrawList()->AddRectFilled(icon_scal_pos, icon_scal_size, IM_COL32_WHITE, 0.f, ImDrawCornerFlags_All);
            if ((gui.current_app_selected < 0) || (gui.apps_list_opened[gui.current_app_selected] != gui.apps_list_opened[a]))
                ImGui::GetForegroundDrawList()->AddRectFilled(icon_scal_pos, icon_scal_size, IM_COL32(0.f, 0.f, 0.f, 140.f), 15.f, ImDrawCornerFlags_All);
        }
    }

    const auto SCAL_PIX_FONT = 19.2f / 24.f;
    const auto scal_default_font = ImGui::GetFontSize() / 19.2f;
    const auto scal_clock_default_font = 24.f * scal_default_font;
    const auto scal_format_default_font = 18.f * scal_default_font;
    const auto scal_clock_font_size = scal_clock_default_font / ImGui::GetFontSize();
    const auto scal_format_font_size = scal_format_default_font / ImGui::GetFontSize();

    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);

    tm local = {};
    SAFE_LOCALTIME(&tt, &local);

    auto DATE_TIME = get_date_time(gui, host, local);
    const auto CALC_CLOCK_SIZE = ImGui::CalcTextSize(DATE_TIME["clock"].c_str());
    const auto SCAL_CLOCK_SIZE = ImVec2(CALC_CLOCK_SIZE.x * scal_clock_font_size, CALC_CLOCK_SIZE.y * scal_clock_font_size * SCAL_PIX_FONT);
    const auto CALC_FORMAT_SIZE = ImGui::CalcTextSize(DATE_TIME["day-moment"].c_str());
    const auto SCAL_FORMAT_SIZE = host.io.user_id.empty() || gui.users[host.io.user_id].clock_12_hour ? ImVec2((CALC_FORMAT_SIZE.x * scal_format_font_size), CALC_FORMAT_SIZE.y * scal_format_font_size * SCAL_PIX_FONT) : ImVec2(0.f, 0.f);

    const auto CLOCK_POS = ImVec2(display_size.x - (62.f * SCAL.x) - (SCAL_CLOCK_SIZE.x * SCAL.x) - (SCAL_FORMAT_SIZE.x * SCAL.x) - is_notif_pos, (MENUBAR_HEIGHT / 2.f) - ((SCAL_CLOCK_SIZE.y * SCAL.y) / 2.f));
    const auto FORMAT_POS = ImVec2(CLOCK_POS.x + (SCAL_CLOCK_SIZE.x * SCAL.x) + (4.f * SCAL.x), CLOCK_POS.y + (SCAL_CLOCK_SIZE.y - SCAL_FORMAT_SIZE.y));

    ImGui::GetForegroundDrawList()->AddText(gui.vita_font, scal_clock_default_font * SCAL.x, CLOCK_POS, is_theme_color ? gui.information_bar_color["indicator"] : DEFAULT_INDICATOR_COLOR, DATE_TIME["clock"].c_str());
    if (host.io.user_id.empty() || gui.users[host.io.user_id].clock_12_hour)
        ImGui::GetForegroundDrawList()->AddText(gui.vita_font, scal_format_default_font * SCAL.x, FORMAT_POS, is_theme_color ? gui.information_bar_color["indicator"] : DEFAULT_INDICATOR_COLOR, DATE_TIME["day-moment"].c_str());
    ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(display_size.x - (54.f * SCAL.x) - is_notif_pos, 12.f * SCAL.y), ImVec2(display_size.x - (50.f * SCAL.x) - is_notif_pos, 20 * SCAL.y), IM_COL32(81.f, 169.f, 32.f, 255.f), 0.f, ImDrawCornerFlags_All);
    ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(display_size.x - (50.f * SCAL.x) - is_notif_pos, 5.f * SCAL.y), ImVec2(display_size.x - (12.f * SCAL.x) - is_notif_pos, 27 * SCAL.y), IM_COL32(81.f, 169.f, 32.f, 255.f), 2.f, ImDrawCornerFlags_All);

    if (host.display.imgui_render && !gui.live_area.start_screen && !gui.live_area.live_area_screen && get_live_area_sys_app_state(gui) && (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) || ImGui::IsItemClicked(0)))
        gui.live_area.information_bar = false;

    if (is_notif_pos)
        draw_notice_info(gui, host);

    ImGui::End();
}

} // namespace gui
