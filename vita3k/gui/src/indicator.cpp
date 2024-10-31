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

#include <gui/functions.h>

#include <indicator/state.h>

#include <patch_check/functions.h>

#include <config/state.h>
#include <dialog/state.h>
#include <io/state.h>

#include <packages/sfo.h>

#include <util/safe_time.h>

#include <util/lock_and_find.h>
#include <util/net_utils.h>
#include <util/string_utils.h>

#include <io/VitaIoDevice.h>
#include <io/vfs.h>

#include <pugixml.hpp>

#include <stb_image.h>

namespace gui {

static patch_check::RemoteUpdateInfo *ensure_remote_update_info(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const auto APP_INDEX = get_app_index(gui, app_path);
    if (!APP_INDEX) {
        LOG_ERROR("App index not found for path: {}", app_path);
        return nullptr;
    }

    return patch_check::ensure_cached_remote_update_info(emuenv.pref_path, app_path, APP_INDEX->title_id, APP_INDEX->app_ver, emuenv.cfg.sys_lang);
}

void erase_app_notice(GuiState &gui, const std::string &title_id) {
    for (const auto time : indicator::get_state().erase_app_notice(title_id))
        gui.notice_info_icon.erase(time);
}

static bool init_notice_icon(GuiState &gui, EmuEnvState &emuenv, const fs::path &content_path, const indicator::NoticeList &info) {
    int32_t width = 0;
    int32_t height = 0;
    vfs::FileBuffer buffer;

    if (!vfs::read_file(VitaIoDevice::ux0, buffer, emuenv.pref_path, content_path)) {
        if (info.type == "trophy") {
            LOG_WARN("Icon no found for trophy id: {} on NpComId: {}", info.trophy.id, info.id);
            return false;
        } else {
            if (!vfs::read_app_file(buffer, emuenv.pref_path, info.id, "sce_sys/icon0.png")) {
                buffer = init_default_icon(gui, emuenv);
                if (buffer.empty()) {
                    LOG_WARN("Not found default icon for this notice content: {}", info.content.id);
                    return false;
                }
            }
        }
    }
    stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
    if (!data) {
        LOG_ERROR("Invalid icon for notice id: {} in path {}.", info.id, content_path);
        return false;
    }
    gui.notice_info_icon[info.time] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);

    return gui.notice_info_icon.contains(info.time);
}

void draw_notifications(GuiState &gui, EmuEnvState &emuenv) {
    auto &indicator_state = indicator::get_state();
    const auto NOTICE_INFO_STATE = indicator_state.notice_info_state;

    constexpr float FADE_IN_SPEED = 0.15f;
    constexpr float FADE_OUT_SPEED = 0.20f;

    static bool fading_out = false;
    static bool fading_in = true;

    static float alpha = 0.0f;

    double now = ImGui::GetTime();

    bool activated_now = false;
    Notification n;
    {
        std::lock_guard<std::mutex> lock(gui.notifications_mutex);
        auto &org = gui.notifications.back();
        if (!org.active) {
            activated_now = true;
            org.active = true;
            org.start_time = now;
            alpha = 0.0f;
            fading_in = true;
            fading_out = false;
        }

        n = org;
    }

    if (NOTICE_INFO_STATE) {
        std::lock_guard<std::mutex> lock(gui.notifications_mutex);
        if (gui.notifications.size() > 1)
            gui.notifications.erase(gui.notifications.begin(), gui.notifications.end() - 1);

        if (activated_now) {
            alpha = 1.0f;
            fading_in = false;
        }

        fading_out = true;
    }

    auto fade_towards = [&](float &value, float target, float speed, float threshold = 0.01f) {
        value = std::lerp(value, target, speed);
        if (fabs(value - target) < threshold)
            value = target;

        return value == target;
    };

    constexpr float DISPLAY_TIME = 4.f;
    float elapsed = float(now - n.start_time);
    float remaining = DISPLAY_TIME - elapsed;

    if (fading_in) {
        if (fade_towards(alpha, 1.0f, FADE_IN_SPEED)) {
            fading_in = false;
        }
    } else if (remaining <= 0.5f && !fading_out) {
        fading_out = true;
    }

    if (fading_out) {
        if (fade_towards(alpha, 0.0f, FADE_OUT_SPEED)) {
            std::lock_guard<std::mutex> lock(gui.notifications_mutex);
            gui.notifications.pop_back();
            return;
        }
    }

    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    const ImVec2 WINDOW_SIZE(410.f * SCALE.x, 58.f * SCALE.y);
    const ImVec2 ICON_SIZE(36.f * SCALE.x, 36.f * SCALE.y);
    const ImVec2 WINDOW_POS(VIEWPORT_POS.x + VIEWPORT_SIZE.x - WINDOW_SIZE.x - (16.f * SCALE.x), VIEWPORT_POS.y + ((!gui.vita_area.home_screen ? 35.f : 68.f) * SCALE.y));

    const float PAD_X = 10.f * SCALE.x;

    ImGui::SetNextWindowPos(WINDOW_POS);
    ImGui::SetNextWindowSize(WINDOW_SIZE);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.f * SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.867f, 0.902f, 0.918f, 0.9f * alpha));
    ImGui::Begin("##download_toast", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    const auto &app_icon = n.app_id.starts_with("NPXS") && (n.app_id != "NPXS10007") ? gui.app_selector.sys_apps_icon[n.app_id] : gui.app_selector.user_apps_icon[n.app_id];
    if (app_icon) {
        ImGui::SetCursorPos(ImVec2(PAD_X, (WINDOW_SIZE.y - ICON_SIZE.y) / 2.f));
        const auto SCREEN_ICON_POS = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddImageRounded(
            app_icon,
            SCREEN_ICON_POS,
            ImVec2(SCREEN_ICON_POS.x + ICON_SIZE.x, SCREEN_ICON_POS.y + ICON_SIZE.y),
            ImVec2(0, 0),
            ImVec2(1, 1),
            IM_COL32(255, 255, 255, int(255 * alpha)),
            ICON_SIZE.x);
    }

    const ImVec4 COLOR_TEXT_BLACK_ALPHA(0.f, 0.f, 0.f, alpha);
    ImGui::SetWindowFontScale(RES_SCALE.y);
    const ImVec2 BEGIN_TEXT((PAD_X * 2) + ICON_SIZE.x, 10.f * SCALE.y);
    if (!n.title.empty()) {
        ImGui::SetCursorPos(BEGIN_TEXT);
        ImGui::TextColored(COLOR_TEXT_BLACK_ALPHA, "%s", n.title.c_str());
        ImGui::SetCursorPos(ImVec2(BEGIN_TEXT.x, BEGIN_TEXT.y + (24.f * SCALE.y)));
    } else {
        const auto MSG_SIZE = ImGui::CalcTextSize(n.message.c_str());
        ImGui::SetCursorPos(ImVec2(BEGIN_TEXT.x, (WINDOW_SIZE.y - MSG_SIZE.y) / 2.f));
    }
    ImGui::TextColored(COLOR_TEXT_BLACK_ALPHA, "%s", n.message.c_str());
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

static bool set_notice_icon(GuiState &gui, EmuEnvState &emuenv, const indicator::NoticeList &info) {
    fs::path content_path;

    if (info.type == "trophy") {
        content_path = fs::path("user") / emuenv.io.user_id / "trophy/conf" / info.id / fmt::format("TROP{}.PNG", info.trophy.id);
    } else {
        if (info.type == "content") {
            if (info.group.find("gd") != std::string::npos) {
                content_path = fs::path("app") / info.id;
            } else {
                if (info.group == "ac")
                    content_path = fs::path("addcont") / info.id / info.content.id;
                else if (info.group.find("gp") != std::string::npos)
                    content_path = fs::path("app") / info.id;
                else if (info.group == "theme")
                    content_path = fs::path("theme") / info.content.id;
            }
        } else
            content_path = fs::path("app") / info.id;

        content_path /= "sce_sys/icon0.png";
    }

    return init_notice_icon(gui, emuenv, content_path, info);
}

static const char *get_trophy_grade_icon_name(np::trophy::SceNpTrophyGrade trophy_grade) {
    switch (trophy_grade) {
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM:
        return "trophy_platinum";
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD:
        return "trophy_gold";
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER:
        return "trophy_silver";
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE:
        return "trophy_bronze";
    default:
        return nullptr;
    }
}

static std::string get_trophy_grade_label(GuiState &gui, np::trophy::SceNpTrophyGrade trophy_grade) {
    auto &common = gui.lang.common.main;

    switch (trophy_grade) {
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM:
        return common["platinum"];
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD:
        return common["gold"];
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER:
        return common["silver"];
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE:
        return common["bronze"];
    default:
        return "?";
    }
}

static std::string get_user_id_for_notice(const std::string &type, const EmuEnvState &emuenv) {
    if (type == "trophy")
        return emuenv.io.user_id;

    return "global";
}

static bool set_notice_info(GuiState &gui, EmuEnvState &emuenv, const indicator::NoticeList &info) {
    auto &indicator_state = indicator::get_state();
    auto &notice_info = indicator_state.notice_info;
    std::string msg, name;

    const std::string user_id = get_user_id_for_notice(info.type, emuenv);
    auto &lang = gui.lang.indicator;
    if (info.type == "content") {
        fs::path content_path;
        if (info.type == "content") {
            if (info.group.find("gd") != std::string::npos) {
                content_path = fs::path("app") / info.id;
                msg = lang["app_added_home"];
            } else {
                if (info.group == "ac")
                    content_path = fs::path("addcont") / info.id / info.content.id;
                else if (info.group.find("gp") != std::string::npos)
                    content_path = fs::path("app") / info.id;
                else if (info.group == "theme")
                    content_path = fs::path("theme") / info.content.id;
                msg = lang["install_complete"];
            }
            vfs::FileBuffer params;
            if (vfs::read_file(VitaIoDevice::ux0, params, emuenv.pref_path, content_path / "sce_sys/param.sfo")) {
                SfoFile sfo_handle;
                sfo::load(sfo_handle, params);
                if (!sfo::get_data_by_key(name, sfo_handle, fmt::format("TITLE_{:0>2d}", emuenv.cfg.sys_lang)))
                    sfo::get_data_by_key(name, sfo_handle, "TITLE");
            } else {
                LOG_WARN("Content not found for id: {}, in path: {}", info.content.id, content_path);
                return false;
            }
        }
    } else if (info.type == "download_failed") {
        const auto UPDATE_INFOS = ensure_remote_update_info(gui, emuenv, info.id);
        if (!UPDATE_INFOS)
            return false;
        name = UPDATE_INFOS->title;
        msg = emuenv.common_dialog.lang.common["download_failed"];
    } else if (info.type.find("downloading") != std::string::npos) {
        const auto UPDATE_INFOS = ensure_remote_update_info(gui, emuenv, info.id);
        if (!UPDATE_INFOS)
            return false;
        name = UPDATE_INFOS->title;

        patch_check::get_state().begin_update_download(info.id);

        const auto IS_PAUSED = (info.type == "downloading_paused");
        indicator::get_state().queue_download_task(info.id, info.group, emuenv.pref_path / "ux0/bgdl/t" / info.group / fs::path(UPDATE_INFOS->url).filename(), UPDATE_INFOS->url, static_cast<uint64_t>(UPDATE_INFOS->size), IS_PAUSED);
    } else if (info.type == "wait_install") {
        const auto UPDATE_INFOS = ensure_remote_update_info(gui, emuenv, info.id);
        if (!UPDATE_INFOS)
            return false;
        name = UPDATE_INFOS->title;
        msg = lang["waiting_install"];

        patch_check::get_state().queue_update_install(info.id, info.content.id, emuenv.pref_path / "ux0/bgdl/t" / info.group / fs::path(UPDATE_INFOS->url).filename());
    } else if (info.type == "failed_install") {
        const auto UPDATE_INFOS = ensure_remote_update_info(gui, emuenv, info.id);
        if (!UPDATE_INFOS)
            return false;
        name = UPDATE_INFOS->title;
        msg = lang["install_failed"];
    } else {
        const auto TROPHY_CONF_ID_PATH{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "trophy/conf" / info.id };
        const std::string SFM_NAME = fs::exists(TROPHY_CONF_ID_PATH / fmt::format("TROP_{:0>2d}.SFM", emuenv.cfg.sys_lang)) ? fmt::format("TROP_{:0>2d}.SFM", emuenv.cfg.sys_lang) : "TROP.SFM";

        pugi::xml_document doc;
        if (doc.load_file((TROPHY_CONF_ID_PATH / SFM_NAME).c_str())) {
            for (const auto &conf : doc.child("trophyconf").children("trophy")) {
                if (info.trophy.id == conf.attribute("id").as_string())
                    name += conf.child("name").text().as_string();
            }
        } else {
            LOG_WARN("Trophy sfm in conf no found for NpComId: {}", info.id);
            return false;
        }
        msg = lang["trophy_earned"];
    }

    // remove newline characters from name with regex
    name = std::regex_replace(name, std::regex(R"(\s+)"), " ");
    notice_info.push_back({ info, name, msg });

    std::sort(notice_info.begin(), notice_info.end(), [&](const indicator::NoticeInfo &na, const indicator::NoticeInfo &nb) {
        return na.time > nb.time;
    });

    indicator::get_state().push_notice_task(indicator::NoticeAction::ADD, info.time, user_id);

    return true;
}

void init_notice_info(GuiState &gui, EmuEnvState &emuenv) {
    auto &indicator_state = indicator::get_state();
    auto &notice_list = indicator_state.notice_list;
    auto &notice_info = indicator_state.notice_info;
    gui.notice_info_icon.clear();
    indicator::get_state().init_notice_info(emuenv);

    if (!notice_list.empty()) {
        for (auto &[user, lists] : notice_list) {
            if ((user == "global") || (user == emuenv.io.user_id)) {
                auto notice_it = lists.begin();
                while (notice_it != lists.end()) {
                    if (!set_notice_info(gui, emuenv, *notice_it)) {
                        notice_it = lists.erase(notice_it);
                        indicator::get_state().save_notice_list(emuenv);
                    } else {
                        ++notice_it;
                    }
                }
            }
        }

        std::sort(notice_info.begin(), notice_info.end(), [](const indicator::NoticeInfo &a, const indicator::NoticeInfo &b) {
            return a.time > b.time;
        });
    }
}

void update_notice_info(GuiState &gui, EmuEnvState &emuenv, const std::string &type, const std::string &id) {
    auto &indicator_state = indicator::get_state();
    auto &notice_list = indicator_state.notice_list;
    auto &notice_info = indicator_state.notice_info;

    indicator::NoticeList info;
    if (type == "content") {
        info.id = emuenv.app_info.app_title_id;
        info.content.id = emuenv.app_info.app_content_id;
        info.group = emuenv.app_info.app_category;
        if (emuenv.app_info.app_category == "gp") {
            for (const auto time : indicator::get_state().remove_notice_by_type_and_content_and_save("wait_install", info.content.id, "global", emuenv))
                gui.notice_info_icon.erase(time);
        }
    } else if (type == "downloading") {
        const auto UPDATE_INFOS = ensure_remote_update_info(gui, emuenv, id);
        if (!UPDATE_INFOS)
            return;
        info.id = UPDATE_INFOS->titleid;
        info.content.id = UPDATE_INFOS->content_id;
        const auto BGDL_PATH = emuenv.pref_path / "ux0/bgdl/t";
        for (int i = 0; i <= 99; ++i) {
            const auto &group = fmt::format("{:02}", i);
            const fs::path t = BGDL_PATH / group;
            if (!fs::exists(t) || fs::is_empty(t)) {
                info.group = group;
                break;
            }
        }
    } else if ((type == "wait_install") || (type == "download_failed")) {
        const auto UPDATE_INFOS = ensure_remote_update_info(gui, emuenv, id);
        if (!UPDATE_INFOS)
            return;

        for (const auto &notice : notice_info) {
            if (notice.type.find("downloading") == std::string::npos)
                continue;

            const auto &PKG_PATH = emuenv.pref_path / "ux0/bgdl/t" / notice.group / fs::path(UPDATE_INFOS->url).filename();
            if (fs::exists(PKG_PATH)) {
                info.group = notice.group;
                info.time = notice.time;
                break;
            }
        }

        info.id = UPDATE_INFOS->titleid;
        info.content.id = UPDATE_INFOS->content_id;
        for (const auto time : indicator::get_state().remove_downloading_notice_by_group(info.group))
            gui.notice_info_icon.erase(time);
    } else if (type == "failed_install") {
        const auto UPDATE_INFOS = ensure_remote_update_info(gui, emuenv, id);
        if (!UPDATE_INFOS)
            return;
        info.id = id;
        info.content.id = UPDATE_INFOS->content_id;
        info.group = "gp";

        for (const auto time : indicator::get_state().remove_notice_by_type_and_id_and_save("wait_install", info.id, "global", emuenv))
            gui.notice_info_icon.erase(time);
    } else {
        const auto &trophy_data = gui.trophy_unlock_display_requests.back();
        info.id = trophy_data.np_com_id;
        info.trophy.id = trophy_data.trophy_id;
        info.group = std::to_string(static_cast<int>(trophy_data.trophy_kind));
    }
    info.type = type;
    indicator::get_state().add_notice(info, emuenv);

    const auto USER_ID = get_user_id_for_notice(type, emuenv);
    set_notice_info(gui, emuenv, notice_list[USER_ID].back());
}

static void process_download_notice_events(GuiState &gui, EmuEnvState &emuenv) {
    auto &indicator_state = indicator::get_state();
    const auto NOTICE_INFO_STATE = indicator_state.notice_info_state;
    for (const auto &event : indicator_state.take_download_notice_events()) {
        update_notice_info(gui, emuenv, event.completed ? "wait_install" : "download_failed", event.id);

        if (!NOTICE_INFO_STATE) {
            const auto APP_INDEX = get_app_index(gui, event.id);
            const auto TITLE = APP_INDEX ? APP_INDEX->title : event.id;
            std::lock_guard<std::mutex> lock(gui.notifications_mutex);
            gui.notifications.insert(gui.notifications.begin(), {
                                                                    event.id,
                                                                    TITLE,
                                                                    event.completed ? gui.lang.indicator["downloading_complete"] : emuenv.common_dialog.lang.common["download_failed"],
                                                                });
        }
    }
}

static void process_notice_tasks(EmuEnvState &emuenv, GuiState &gui) {
    auto &indicator_state = indicator::get_state();
    auto &notice_list = indicator_state.notice_list;
    for (const auto &task : indicator_state.take_notice_tasks()) {
        if (!notice_list.contains(task.user_id)) {
            LOG_ERROR("Notice list not found for user id: {}", task.user_id);
            continue;
        }
        switch (task.action) {
        case indicator::NoticeAction::REMOVE_FULL: {
            for (const auto time : indicator_state.remove_notice_and_save(task.time, task.user_id, emuenv.pref_path, emuenv))
                gui.notice_info_icon.erase(time);
            break;
        }
        case indicator::NoticeAction::REMOVE_ICON: {
            gui.notice_info_icon.erase(task.time);
            break;
        }
        case indicator::NoticeAction::ADD: {
            auto it = std::find_if(notice_list[task.user_id].begin(), notice_list[task.user_id].end(), [&](const auto &n) {
                return n.time == task.time;
            });
            if (it == notice_list[task.user_id].end()) {
                LOG_ERROR("Notice with time {} not found.", task.time);
                continue;
            }
            if (!set_notice_icon(gui, emuenv, *it)) {
                LOG_ERROR("Failed to set notice icon for time: {}, user id: {}", task.time, task.user_id);
                // If setting notice info fails, remove it from the list
                notice_list[task.user_id].erase(it);
                indicator_state.save_notice_list(emuenv);
                continue;
            }

            break;
        }
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

static std::string get_remaining_str(LangState &lang, const uint64_t remaining) {
    if (remaining > 120)
        return fmt::format(fmt::runtime(lang.indicator["minutes_left"]), remaining / 60);
    else
        return fmt::format(fmt::runtime(lang.indicator["seconds_left"]), remaining);
}

void draw_indicator(GuiState &gui, EmuEnvState &emuenv) {
    auto &indicator_state = indicator::get_state();
    auto &notice_tasks = indicator_state.notice_tasks;
    auto &notice_info = indicator_state.notice_info;
    auto &notice_info_count_new = indicator_state.notice_info_count_new;
    auto &notice_list = indicator_state.notice_list;
    auto &download_mutex = indicator_state.download_mutex;
    auto &download_tasks = indicator_state.download_tasks;
    auto &notice_info_state = indicator_state.notice_info_state;

    process_download_notice_events(gui, emuenv);

    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("##notice_info", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    if (!notice_tasks.empty())
        process_notice_tasks(emuenv, gui);

    const auto DRAW_LIST = ImGui::GetWindowDrawList();

    const float tex_w = NOTICE_SIZE.x;
    const float tex_h = NOTICE_SIZE.y;
    const ImVec2 uv0(0.5f / tex_w, 0.5f / tex_h);
    const ImVec2 uv1((tex_w - 0.5f) / tex_w, (tex_h - 0.5f) / tex_h);

    const auto FG_DRAW_LIST = ImGui::GetForegroundDrawList();
    if (notice_info_count_new) {
        if (gui.theme_information_bar_notice.contains(NoticeIcon::NEW))
            FG_DRAW_LIST->AddImage(gui.theme_information_bar_notice[NoticeIcon::NEW], NOTICE_ICON_POS, NOTICE_ICON_POS_MAX, uv0, uv1);
        else
            FG_DRAW_LIST->AddCircleFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX - (23.f * SCALE.x), VIEWPORT_POS.y + (14.f * SCALE.y)), 61.f * SCALE.x, IM_COL32(11.f, 90.f, 252.f, 255.f));
        const auto FONT_SCALE = 40.f * SCALE.x;
        const auto NOTICE_COUNT_FONT_SCALE = FONT_SCALE / 40.f;
        const auto NOTICE_COUNT_SIZE = ImGui::CalcTextSize(std::to_string(notice_info_count_new).c_str()).x * NOTICE_COUNT_FONT_SCALE;
        FG_DRAW_LIST->AddText(gui.vita_font[emuenv.current_font_level], FONT_SCALE, ImVec2(VIEWPORT_WIDTH_POS_MAX - (NOTICE_SIZE.x / 2.f) - (NOTICE_COUNT_SIZE / 2.f) + (12.f * SCALE.x), VIEWPORT_POS.y + (15.f * SCALE.y)), NOTICE_COLOR, std::to_string(notice_info_count_new).c_str());
    } else {
        if (gui.theme_information_bar_notice.contains(NoticeIcon::NO))
            FG_DRAW_LIST->AddImage(gui.theme_information_bar_notice[NoticeIcon::NO], NOTICE_ICON_POS, NOTICE_ICON_POS_MAX);
        else
            FG_DRAW_LIST->AddCircleFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX - (22.5f * SCALE.x), VIEWPORT_POS.y + (15.f * SCALE.y)), 45.f * SCALE.x, IM_COL32(62.f, 98.f, 160.f, 255.f));
    }

    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (notice_info_state && (notice_info_count_new > 0)) {
            indicator_state.clean_notice_info_new_and_save(emuenv, emuenv.io.user_id);
        }
        notice_info_state = !notice_info_state;
    }

    if (notice_info_state) {
        const auto SELECT_SIZE = ImVec2(776.f * SCALE.x, 90.f * SCALE.y);
        const auto BORDER_SIZE = notice_info.empty() ? 0.f : 8.f * SCALE.x;
        const auto HALF_BORDER_SIZE = BORDER_SIZE / 2.f;
        const auto INDICATOR_SIZE = notice_info.empty() ? ImVec2(412.f * SCALE.x, 86.f * SCALE.y) : ImVec2(SELECT_SIZE.x, notice_info.size() < 5 ? (notice_info.size() * SELECT_SIZE.y) + BORDER_SIZE + ((notice_info.size() - 1) * 1.f) : (5.f * SELECT_SIZE.y) + BORDER_SIZE + (4.f * 1.f));
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
        auto &common = emuenv.common_dialog.lang.common;

        const auto BORDER_TRANSLUCIDE = IM_COL32(255, 255, 255, 80);
        const ImVec4 ELIPSIS_TEXT_COLOR = ImVec4(0.502f, 0.502f, 0.502f, 1.f);

        ImVec2 TIP_TOP = ImVec2(INDICATOR_POS.x + INDICATOR_SIZE.x, INDICATOR_POS.y + 22.f * SCALE.y);
        ImVec2 TIP_RIGHT = ImVec2(TIP_TOP.x + 30.f * SCALE.x, INDICATOR_POS.y + 3.f * SCALE.y);
        ImVec2 TIP_BOTTOM = ImVec2(INDICATOR_POS.x + INDICATOR_SIZE.x, INDICATOR_POS.y + 46.f * SCALE.y);
        DRAW_LIST->AddTriangleFilled(TIP_TOP, TIP_RIGHT, TIP_BOTTOM, IM_COL32(237, 238, 241, 255));

        if (notice_info.empty()) {
            ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
            const auto NO_NOTIF = lang["no_notif"].c_str();
            const auto CALC_TEXT = ImGui::CalcTextSize(NO_NOTIF);
            ImGui::SetCursorPos(ImVec2((INDICATOR_SIZE.x / 2.f) - (CALC_TEXT.x / 2.f), (INDICATOR_SIZE.y / 2.f) - (CALC_TEXT.y / 2.f)));
            ImGui::TextColored(ImVec4(0.f, 0.f, 0.f, 1.f), "%s", NO_NOTIF);
        } else {
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 82.f * SCALE.x);
            const ImVec2 ICON_SIZE(64.f * SCALE.x, 64.f * SCALE.y);
            for (const auto &notice : notice_info) {
                if (notice.time != notice_info.front().time)
                    ImGui::Separator();
                const auto BEGIN_POS = ImGui::GetCursorPos();
                const auto BEGIN_SCREEN_POS = ImGui::GetCursorScreenPos();
                const ImVec2 ICON_OFFSET((ImGui::GetColumnWidth() / 2.f) - (ICON_SIZE.x / 2.f), (SELECT_SIZE.y / 2.f) - (ICON_SIZE.y / 2.f));
                const ImVec2 ICON_POS(BEGIN_POS.x + ICON_OFFSET.x, BEGIN_POS.y + ICON_OFFSET.y);
                if (gui.notice_info_icon.contains(notice.time)) {
                    ImGui::SetCursorPos(ICON_POS);
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
                if (ImGui::Selectable("##icon", notice.is_new, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, SELECT_SIZE)) {
                    indicator_state.clean_notice_info_new_and_save(emuenv, emuenv.io.user_id);
                    if (notice.type == "content") {
                        if (notice.group == "theme")
                            pre_run_app(gui, emuenv, "NPXS10015");
                        else
                            select_app(gui, notice.id);
                    } else if (notice.type == "wait_install") {
                        const auto CURRENT_HOME_SCREEN_STATE = gui.vita_area.home_screen;
                        if (CURRENT_HOME_SCREEN_STATE) {
                            const auto APP_PATH = notice.id;
                            const auto LIVE_AREA_APP_INDEX = gui::get_live_area_current_open_apps_list_index(gui, emuenv.io.app_path);
                            if (LIVE_AREA_APP_INDEX == gui.live_area_current_open_apps_list.end())
                                gui::open_live_area(gui, emuenv, APP_PATH);
                            else
                                gui.live_area_app_current_open = std::distance(gui.live_area_current_open_apps_list.begin(), LIVE_AREA_APP_INDEX);
                            gui.gate_animation.start(GateAnimationState::EnterApp);
                        }
                    } else if (notice.type == "trophy") {
                        pre_load_app(gui, emuenv, false, "NPXS10008");
                        open_trophy_unlocked(gui, emuenv, notice.id, notice.trophy.id);
                    }
                    if (notice.type != "failed_install")
                        notice_info_state = false;
                }
                ImGui::PopStyleColor(3);
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.34f * RES_SCALE.y);
                const auto BEGIN_TITLE_SPACE = ((notice.type.find("downloading") != std::string::npos) ? 16.f : 20.f) * SCALE.y;
                if (!notice.name.empty()) {
                    ImGui::SetCursorPosY(BEGIN_POS.y + BEGIN_TITLE_SPACE);
                    if (notice.type == "trophy") {
                        const auto TROPHY_GRADE = static_cast<np::trophy::SceNpTrophyGrade>(string_utils::stoi_def(notice.group, 0, "trophy group"));
                        const auto TROPHY_KIND_ICON_NAME = get_trophy_grade_icon_name(TROPHY_GRADE);
                        const auto TROPHY_KIND_S = get_trophy_grade_label(gui, TROPHY_GRADE);
                        ImGui::TextColored(GUI_COLOR_TEXT, "(%s) %s", TROPHY_KIND_S.c_str(), notice.name.c_str());
                    } else {
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice.name.c_str());
                    }
                }
                ImGui::SetWindowFontScale(0.94f * RES_SCALE.y);
                if (notice.type.find("downloading") != std::string::npos) {
                    auto dt = lock_and_find(notice.group, download_tasks, download_mutex);
                    if (dt) {
                        const auto &dp = dt->download_progress;
                        ImGui::SetCursorPosY(BEGIN_POS.y + (45.f * SCALE.y));
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                        ImGui::ProgressBar(dp->progress.load(), ImVec2(602.f * SCALE.x, 12.f * SCALE.y), "");
                        ImGui::PopStyleColor();
                        ImGui::SetCursorPosY(BEGIN_POS.y + (60.f * SCALE.y));
                        if (dp->is_paused.load()) {
                            ImGui::Text("%s", lang["paused"].c_str());
                        } else if (dp->is_waiting.load()) {
                            ImGui::Text("%s", lang["waiting_download"].c_str());
                        } else {
                            const auto REMAINING_STR = get_remaining_str(gui.lang, dp->remaining.load());
                            ImGui::Text("%s  %s", REMAINING_STR.c_str(), fmt::format("({} / {})", get_unit_size(dp->downloaded.load()), get_unit_size(dt->total_size)).c_str());
                        }
                    }
                } else {
                    if (!notice.name.empty())
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (14.f * SCALE.y));
                    else // If there is no title, set the message in center of the notice
                        ImGui::SetCursorPosY(BEGIN_POS.y + (SELECT_SIZE.y / 2.f) - (ImGui::CalcTextSize(notice.msg.c_str()).y / 2.f));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice.msg.c_str());
                }
                if ((notice.type.find("download") != std::string::npos) || (notice.type == "wait_install")) {
                    ImGui::SetWindowFontScale(0.94f * RES_SCALE.y);
                    const ImVec2 BUTTON_SIZE(40.f * SCALE.x, 40.f * SCALE.y);
                    ImGui::SetCursorPos(ImVec2(BEGIN_POS.x + SELECT_SIZE.x - BUTTON_SIZE.x - (35.f * SCALE.x), BEGIN_POS.y + (14.f * SCALE.y)));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, BUTTON_SIZE.x);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.902f, 0.902f, 0.902f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ELIPSIS_TEXT_COLOR);
                    ImVec2 BTN_POS = ImGui::GetCursorScreenPos();
                    if (ImGui::Button("...##menu_btn", BUTTON_SIZE))
                        ImGui::OpenPopup(fmt::format("download_menu_{}", notice.group).c_str());
                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar();
                    const ImVec2 POPUP_BUTTON_SIZE(200.f * SCALE.x, 60.f * SCALE.y);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.f * SCALE.x);
                    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.f, 0.f, 0.f, 0.f));
                    const auto BUTTON_COUNT = (notice.type.find("download") != std::string::npos) ? 2 : 1;
                    const auto POPUP_HEIGHT = POPUP_BUTTON_SIZE.y * BUTTON_COUNT + ((BUTTON_COUNT - 1) * 1.f);
                    ImGui::SetNextWindowSize(ImVec2(POPUP_BUTTON_SIZE.x, POPUP_HEIGHT), ImGuiCond_Always);
                    ImGui::SetNextWindowPos(ImVec2(BTN_POS.x - POPUP_BUTTON_SIZE.x - (45.f * SCALE.x), BTN_POS.y + (POPUP_BUTTON_SIZE.y / 2.f) + (6.f * SCALE.y)), ImGuiCond_Always);
                    if (ImGui::BeginPopupModal(fmt::format("download_menu_{}", notice.group).c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.f * SCALE.x);
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.68f, 1.f, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.23f, 0.68f, 1.f, 0.8f));
                        if (notice.type == "download_failed") {
                            if (ImGui::Button(lang["resume"].c_str(), POPUP_BUTTON_SIZE)) {
                                for (const auto time : indicator_state.remove_notice_and_save(notice.time, "global", emuenv.pref_path, emuenv))
                                    gui.notice_info_icon.erase(time);
                                indicator::NoticeList new_notice = indicator_state.resume_failed_download_notice_and_save(notice.time, emuenv);
                                set_notice_info(gui, emuenv, new_notice);
                                ImGui::CloseCurrentPopup();
                            }
                        } else if (notice.type.find("downloading") != std::string::npos) {
                            auto dt = lock_and_find(notice.group, download_tasks, download_mutex);
                            if (dt) {
                                const auto &dp = dt->download_progress;
                                const auto IS_PAUSED = dp->is_paused.load();
                                const auto &BUTTON_STR = IS_PAUSED ? lang["resume"] : lang["pause"];
                                if (ImGui::Button(BUTTON_STR.c_str(), POPUP_BUTTON_SIZE)) {
                                    indicator_state.set_download_paused_and_save(notice.group, notice.time, "global", !IS_PAUSED, emuenv);
                                    ImGui::CloseCurrentPopup();
                                }
                            }
                        }

                        if (BUTTON_COUNT == 2)
                            ImGui::Separator();

                        if (ImGui::Button(common["cancel"].c_str(), POPUP_BUTTON_SIZE)) {
                            indicator_state.cancel_download(notice.group, true);
                            ImGui::OpenPopup("cancel_download_popup");
                        }
                        ImGui::PopStyleColor(3);
                        ImGui::PopStyleVar(4);

                        // Draw Cancel popup
                        const ImVec2 POPUP_SIZE(760.0f * SCALE.x, 436.0f * SCALE.y);
                        ImGui::SetNextWindowSize(POPUP_SIZE, ImGuiCond_Always);
                        ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + (VIEWPORT_SIZE.x / 2.f) - (POPUP_SIZE.x / 2.f), WINDOW_POS.y + (VIEWPORT_SIZE.y / 2.f) - (POPUP_SIZE.y / 2.f)), ImGuiCond_Always);
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f * SCALE.x);
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.f * SCALE.x);
                        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 8.f * SCALE.x);
                        if (ImGui::BeginPopupModal("cancel_download_popup", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration)) {
                            ImGui::SetWindowFontScale(1.34f * RES_SCALE.y);
                            const ImVec2 LARGE_BUTTON_SIZE(310.f * SCALE.x, 46.f * SCALE.y);
                            const std::string &msg = notice.type.find("downloading") != std::string::npos ? lang["cancel_download"] : lang["cancel_install"];
                            const auto STR_SIZE = ImGui::CalcTextSize(msg.c_str(), 0, false, POPUP_SIZE.x - (120.f * SCALE.y));
                            ImGui::PushTextWrapPos(POPUP_SIZE.x - (120.f * SCALE.x));
                            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - (STR_SIZE.x / 2.f), (POPUP_SIZE.y / 2.f) - (STR_SIZE.y / 2.f) - (LARGE_BUTTON_SIZE.y / 2.f) - (22.0f * SCALE.y)));
                            ImGui::Text("%s", msg.c_str());
                            ImGui::PopTextWrapPos();
                            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - LARGE_BUTTON_SIZE.x - (20.f * SCALE.x), POPUP_SIZE.y - LARGE_BUTTON_SIZE.y - (22.0f * SCALE.y)));
                            ImGui::SetWindowFontScale(1.54f * RES_SCALE.y);
                            if (ImGui::Button(common["no"].c_str(), LARGE_BUTTON_SIZE)) {
                                indicator_state.resume_download_cancel(notice.group);
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::SameLine(0, 40.f * SCALE.x);
                            if (ImGui::Button(common["yes"].c_str(), LARGE_BUTTON_SIZE)) {
                                indicator_state.cancel_download(notice.group, false);
                                for (const auto time : indicator_state.remove_notice_and_save(notice.time, "global", emuenv.pref_path, emuenv))
                                    gui.notice_info_icon.erase(time);
                                patch_check::get_state().erase_update_install(notice.id);

                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        } else {
                            ImVec2 P0 = ImVec2(ImGui::GetWindowPos().x - HALF_BORDER_SIZE, ImGui::GetWindowPos().y - HALF_BORDER_SIZE);
                            ImVec2 P1 = ImVec2(P0.x + ImGui::GetWindowSize().x + BORDER_SIZE, P0.y + ImGui::GetWindowSize().y + BORDER_SIZE);
                            FG_DRAW_LIST->AddRect(P0, P1, BORDER_TRANSLUCIDE, 16.f * SCALE.x, 0, BORDER_SIZE);

                            const ImVec2 TIP_TOP(P1.x + HALF_BORDER_SIZE, P0.y + (28.f * SCALE.y) - HALF_BORDER_SIZE);
                            const ImVec2 TIP_RIGHT(P1.x + HALF_BORDER_SIZE + (20.f * SCALE.x), P0.y + (20.f * SCALE.y) - HALF_BORDER_SIZE);
                            const ImVec2 TIP_BOTTOM(P1.x + HALF_BORDER_SIZE, P0.y + (44.f * SCALE.y) - HALF_BORDER_SIZE);
                            FG_DRAW_LIST->AddTriangleFilled(TIP_TOP, TIP_RIGHT, TIP_BOTTOM, BORDER_TRANSLUCIDE);
                        }

                        ImGui::PopStyleVar(3);

                        if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                            ImGui::CloseCurrentPopup();

                        ImGui::EndPopup();
                    }
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();
                }
                ImGui::SetWindowFontScale(0.94f * RES_SCALE.x);
                const auto NOTICE_TIME = get_notice_time(gui, emuenv, notice.time);
                const auto NOTICE_TIME_SIZE = ImGui::CalcTextSize(NOTICE_TIME.c_str());
                ImGui::SetCursorPos(ImVec2(INDICATOR_SIZE.x - (28.f * SCALE.x) - NOTICE_TIME_SIZE.x, BEGIN_POS.y + (66.f * SCALE.y)));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", NOTICE_TIME.c_str());
                ImGui::PopID();
                ImGui::ScrollWhenDragging();
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(3);

        if (!notice_info.empty()) {
            const ImVec2 DELETE_POPUP_SIZE(756.0f * SCALE.x, 436.0f * SCALE.y);
            ImGui::SetWindowFontScale(RES_SCALE.x);

            // Draw white circle background for button
            const ImVec2 MENU_BUTTON_SIZE(246.f * SCALE.x, 60.f * SCALE.y);
            const auto HAS_DOWNLOAD_STATE = [&](const std::string &type) {
                return std::any_of(notice_info.begin(), notice_info.end(), [&](const auto &n) {
                    return n.type == type;
                });
            };
            const auto HAS_DOWNLOADING = HAS_DOWNLOAD_STATE("downloading");
            const auto HAS_DOWNLOAD_PAUSED = HAS_DOWNLOAD_STATE("downloading_paused");
            const auto BUTTON_COUNT = (static_cast<int>(HAS_DOWNLOADING) + static_cast<int>(HAS_DOWNLOAD_PAUSED) + 1);
            const auto POPUP_HEIGHT = MENU_BUTTON_SIZE.y * BUTTON_COUNT + ((BUTTON_COUNT - 1) * 1.f);
            const ImVec2 BUTTON_SIZE(64.f * SCALE.x, 40.f * SCALE.y);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, BUTTON_SIZE.x);
            ImGui::SetCursorPos(ImVec2(VIEWPORT_SIZE.x - (70.f * SCALE.x), VIEWPORT_SIZE.y - (52.f * SCALE.y)));
            const auto BUTTON_POS = ImGui::GetCursorScreenPos();
            ImGui::SetWindowFontScale(1.4f * RES_SCALE.y);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_Text, ELIPSIS_TEXT_COLOR);
            DRAW_LIST->AddCircleFilled(ImVec2(BUTTON_POS.x + (BUTTON_SIZE.x / 2.f), BUTTON_POS.y + (BUTTON_SIZE.y / 2.f)), BUTTON_SIZE.x / 2.f, IM_COL32(234.f, 234.f, 234.f, 255.f));
            if (ImGui::Button("...##ellipsis_menu", BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_triangle)))
                ImGui::OpenPopup("ellipsis_menu");
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.f * SCALE.x);
            ImGui::SetNextWindowSize(ImVec2(MENU_BUTTON_SIZE.x, POPUP_HEIGHT), ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2(BUTTON_POS.x - MENU_BUTTON_SIZE.x - (45.f * SCALE.x), BUTTON_POS.y - POPUP_HEIGHT - (6.f * SCALE.y)), ImGuiCond_Always);
            if (ImGui::BeginPopupModal("ellipsis_menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.f * SCALE.x);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.68f, 1.f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.23f, 0.68f, 1.f, 0.8f));
                auto set_download_state = [&](bool pause) {
                    indicator_state.set_all_downloads_paused_and_save(pause, emuenv);
                    ImGui::CloseCurrentPopup();
                };

                if (HAS_DOWNLOAD_PAUSED) {
                    if (ImGui::Button(lang["resume_all"].c_str(), MENU_BUTTON_SIZE))
                        set_download_state(false);

                    ImGui::Separator();
                }

                if (HAS_DOWNLOADING) {
                    if (ImGui::Button(lang["pause_all"].c_str(), MENU_BUTTON_SIZE))
                        set_download_state(true);

                    ImGui::Separator();
                }
                const auto BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);

                if (ImGui::Button(lang["delete_all"].c_str(), MENU_BUTTON_SIZE))
                    ImGui::OpenPopup("Delete All");
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
                    ImGui::SameLine(0, 20.f * SCALE.x);
                    if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
                        for (const auto &info : notice_info) {
                            if ((info.type.find("downloading") == std::string::npos) && (info.type != "wait_install")) {
                                const auto USER_ID = get_user_id_for_notice(info.type, emuenv);
                                indicator_state.push_notice_task(indicator::NoticeAction::REMOVE_FULL, info.time, USER_ID);
                            }
                        }
                        if (notice_info.empty())
                            notice_info_state = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                } else {
                    ImVec2 P0 = ImVec2(ImGui::GetWindowPos().x - HALF_BORDER_SIZE, ImGui::GetWindowPos().y - HALF_BORDER_SIZE);
                    ImVec2 P1 = ImVec2(P0.x + ImGui::GetWindowSize().x + BORDER_SIZE, P0.y + ImGui::GetWindowSize().y + BORDER_SIZE);
                    FG_DRAW_LIST->AddRect(P0, P1, BORDER_TRANSLUCIDE, 16.f * SCALE.x, 0, BORDER_SIZE);

                    const ImVec2 TIP_TOP(P1.x + HALF_BORDER_SIZE, P1.y - (28.f * SCALE.y) - HALF_BORDER_SIZE);
                    const ImVec2 TIP_RIGHT(P1.x + HALF_BORDER_SIZE + (20.f * SCALE.x), P1.y - (20.f * SCALE.y) - HALF_BORDER_SIZE);
                    const ImVec2 TIP_BOTTOM(P1.x + HALF_BORDER_SIZE, P1.y - (44.f * SCALE.y) - HALF_BORDER_SIZE);
                    FG_DRAW_LIST->AddTriangleFilled(TIP_TOP, TIP_RIGHT, TIP_BOTTOM, BORDER_TRANSLUCIDE);
                }

                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar(4);

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

    const auto DISPLAY_SIZE = ImGui::GetIO().DisplaySize;
    DRAW_LIST->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(DISPLAY_SIZE.x, VIEWPORT_POS.y), IM_COL32(0.f, 0.f, 0.f, 255.f));
    DRAW_LIST->AddRectFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX, VIEWPORT_POS.y), ImVec2(DISPLAY_SIZE.x, VIEWPORT_POS.y + DISPLAY_SIZE.y), IM_COL32(0.f, 0.f, 0.f, 255.f));
}

} // namespace gui
