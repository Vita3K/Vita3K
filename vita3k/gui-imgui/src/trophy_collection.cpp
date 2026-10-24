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

#include <util/log.h>
#include <util/safe_time.h>
#include <util/string_utils.h>

#include <config/state.h>
#include <dialog/state.h>
#include <io/device.h>
#include <io/functions.h>
#include <v3kn/account.h>
#include <v3kn/storage.h>

#include <pugixml.hpp>
#include <stb_image.h>

#include <atomic>
#include <chrono>
#include <mutex>

namespace gui {
using namespace np::trophy;

struct NPComId {
    std::map<std::string, std::string> name;
    std::map<std::string, std::string> detail;
    std::vector<std::string> group_id;
    std::map<std::string, std::vector<std::string>> trophy_id_by_group;
    tm updated;

    TrophyProgressInfo trophy_progress_info;
    Context context;
};

struct NPComIdSort {
    std::string id;
    std::string name;
    uint32_t progress;
    time_t updated;
};

static std::map<std::string, NPComId> np_com_id_info;
static std::vector<NPComIdSort> np_com_id_list;

enum class TrophyDialogState {
    NONE,
    SYNC_TROPHY,
    DELETE_ALL,
};

static TrophyDialogState trophy_dialog_state = TrophyDialogState::NONE;
static std::atomic<float> sync_progress{ 0.f };
static std::atomic<bool> sync_cancel{ false };
static std::vector<std::string> sync_downloaded;
static std::mutex sync_error_mutex;
static std::string sync_error;
static bool sync_refresh_pending = false;
static const auto get_timestamp_ms = []() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
};
static std::chrono::milliseconds::rep sync_start_time = 0;

enum class NpComIdSortType {
    UPDATED,
    NAME,
    PROGRESS
};
static NpComIdSortType np_com_id_sort;
static bool show_hidden_trophy = false;

static bool load_trophy_conf_data(GuiState &gui, EmuEnvState &emuenv, const std::string &np_com_id, const fs::path &trophy_conf_np_com_id_path) {
    if (!fs::exists(trophy_conf_np_com_id_path) || fs::is_empty(trophy_conf_np_com_id_path))
        return false;

    const std::string sfm_name = fs::exists(trophy_conf_np_com_id_path / fmt::format("TROP_{:0>2d}.SFM", emuenv.cfg.sys_lang)) ? fmt::format("TROP_{:0>2d}.SFM", emuenv.cfg.sys_lang) : "TROP.SFM";

    std::map<std::string, std::string> np_com_list_name_icons;
    np_com_list_name_icons["000"] = fs::exists(trophy_conf_np_com_id_path / fmt::format("ICON0_{:0>2d}.PNG", emuenv.cfg.sys_lang)) ? fmt::format("ICON0_{:0>2d}.PNG", emuenv.cfg.sys_lang) : "ICON0.PNG";

    pugi::xml_document doc;
    if (doc.load_file((trophy_conf_np_com_id_path / sfm_name).c_str())) {
        const auto trophy_conf = doc.child("trophyconf");
        if (np_com_id_info[np_com_id].context.trophy_count_by_group[0])
            np_com_id_info[np_com_id].group_id.emplace_back("000");
        np_com_id_info[np_com_id].name["000"] = trophy_conf.child("title-name").text().as_string();
        np_com_id_info[np_com_id].detail["000"] = trophy_conf.child("title-detail").text().as_string();
        for (const auto &group : trophy_conf.children("group")) {
            const std::string group_id = group.attribute("id").as_string();
            np_com_id_info[np_com_id].group_id.push_back(group_id);
            np_com_id_info[np_com_id].name[group_id] = group.child("name").text().as_string();
            np_com_id_info[np_com_id].detail[group_id] = group.child("detail").text().as_string();
            np_com_list_name_icons[group_id] = fs::exists(trophy_conf_np_com_id_path / fmt::format("GR{}_{:0>2d}.PNG", group_id, emuenv.cfg.sys_lang)) ? fmt::format("GR{}_{:0>2d}.PNG", group_id, emuenv.cfg.sys_lang) : fmt::format("GR{}.PNG", group_id);
        }
        for (const auto &trophy : trophy_conf.children("trophy")) {
            const std::string gid = trophy.attribute("gid").empty() ? "000" : trophy.attribute("gid").as_string();
            const std::string trophy_id = trophy.attribute("id").as_string();
            np_com_id_info[np_com_id].trophy_id_by_group[gid].push_back(trophy_id);
        }
    }

    for (const auto &group : np_com_list_name_icons) {
        int32_t width = 0;
        int32_t height = 0;
        vfs::FileBuffer buffer;

        vfs::read_file(VitaIoDevice::ux0, buffer, emuenv.vita_fs_path, "user/" + emuenv.io.user_id + "/trophy/conf/" + np_com_id + "/" + group.second);

        if (buffer.empty()) {
            LOG_WARN("Icon: '{}', Not found for NPComId: {}.", group.second, np_com_id);
            continue;
        }

        stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data) {
            LOG_ERROR("Invalid icon: '{}' for NPComId: {}.", group.second, np_com_id);
            continue;
        }

        gui.trophy_np_com_id_list_icons[np_com_id][group.first] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);
    }

    return true;
}

static void sync_trophy(EmuEnvState &emuenv) {
    std::map<std::string, TrophySync> trophy_progress;
    for (const auto &[np_com_id, info] : np_com_id_info) {
        const auto &progress_info = info.trophy_progress_info;
        const auto global_progress = progress_info.progress.find("global");
        if (global_progress == progress_info.progress.end() || global_progress->second == 0)
            continue;
        const auto unlocked_ids = progress_info.unlocked_ids.find("global");
        if (unlocked_ids == progress_info.unlocked_ids.end() || unlocked_ids->second.empty())
            continue;

        TrophySync sync_info{};
        sync_info.last_updated = progress_info.last_updated;
        sync_info.unlocked_ids = unlocked_ids->second;
        sync_info.progress = global_progress->second;
        const auto counts_it = progress_info.unlocked_type_count.find("global");
        if (counts_it == progress_info.unlocked_type_count.end())
            continue;
        const auto &counts = counts_it->second;
        const auto get_count = [&counts](np::trophy::SceNpTrophyGrade grade) {
            const auto count = counts.find(grade);
            return count != counts.end() ? count->second : 0;
        };
        sync_info.platinum = get_count(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM);
        sync_info.gold = get_count(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD);
        sync_info.silver = get_count(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER);
        sync_info.bronze = get_count(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE);
        trophy_progress.emplace(np_com_id, std::move(sync_info));
    }

    {
        std::lock_guard lock(sync_error_mutex);
        sync_error.clear();
    }
    sync_refresh_pending = false;
    sync_start_time = get_timestamp_ms();
    v3kn::start_trophy_sync(emuenv, std::move(trophy_progress), sync_progress, sync_cancel, sync_downloaded,
        [](const v3kn::Error &error) {
            std::lock_guard lock(sync_error_mutex);
            sync_error = error.message;
            if (sync_error.empty() && error.response)
                sync_error = error.response->body;
            if (sync_error.empty())
                sync_error = "Trophy synchronization failed.";
        });
}

void init_trophy_collection(GuiState &gui, EmuEnvState &emuenv) {
    const auto TROPHY_PATH{ emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "trophy" };
    const auto TROPHY_DATA_PATH = TROPHY_PATH / "data";
    const auto TROPHY_CONF_PATH = TROPHY_PATH / "conf";

    gui.trophy_np_com_id_list_icons.clear();
    np_com_id_info.clear();
    np_com_id_list.clear();

    if (fs::exists(TROPHY_DATA_PATH) && !fs::is_empty(TROPHY_DATA_PATH)) {
        for (const auto &trophy : fs::directory_iterator(TROPHY_DATA_PATH)) {
            if (!trophy.path().empty() && fs::is_directory(trophy.path())) {
                const std::string np_com_id = trophy.path().stem().generic_string();

                const auto trophy_data_np_com_id_path{ TROPHY_DATA_PATH / np_com_id };
                const auto trophy_conf_np_com_id_path{ TROPHY_CONF_PATH / np_com_id };

                auto &info = np_com_id_info[np_com_id];
                if (!load_and_compute_trophy_stats(emuenv, info.context, np_com_id, info.trophy_progress_info)) {
                    LOG_ERROR("Failed to load and compute trophy stats for NPComId: {}, removing trophy data and config.", np_com_id);
                    fs::remove_all(trophy_data_np_com_id_path);
                    fs::remove_all(trophy_conf_np_com_id_path);
                    continue;
                }

                const auto updated = info.trophy_progress_info.last_updated;
                SAFE_LOCALTIME(&updated, &info.updated);
                const auto progress = info.trophy_progress_info.progress["global"];

                if (!load_trophy_conf_data(gui, emuenv, np_com_id, trophy_conf_np_com_id_path))
                    continue;

                np_com_id_list.push_back({ np_com_id, info.name["000"], progress, updated });
            }
        }
    }

    np_com_id_sort = NpComIdSortType::UPDATED;
    std::sort(np_com_id_list.begin(), np_com_id_list.end(), [](const auto &ta, const auto &tb) {
        return ta.updated > tb.updated;
    });
}

struct Trophy {
    std::string name;
    std::string detail;
    std::map<std::string, std::string> type;
    std::string hidden;
    tm unlocked_time;
    bool earned;

    Context context;
};

struct TrophySort {
    std::string id;
    time_t earned;
    SceNpTrophyGrade grade;
    std::string name;
};

static std::map<std::string, Trophy> trophy_info;
static std::vector<TrophySort> trophy_list;

enum class TrophySortType {
    ORIGINAL,
    EARNED,
    GRADE,
    NAME,
};
static TrophySortType trophy_sort;

static void get_trophy_list(GuiState &gui, EmuEnvState &emuenv, const std::string &np_com_id, const std::string &group_id) {
    const auto trophy_conf_id_path{ emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "trophy/conf" / np_com_id };
    if (fs::is_empty(trophy_conf_id_path)) {
        LOG_WARN("Trophy conf path is empty");
        return;
    }

    gui.trophy_list.clear();
    trophy_info.clear();
    trophy_list.clear();

    const std::string sfm_name = fs::exists(trophy_conf_id_path / fmt::format("TROP_{:0>2d}.SFM", emuenv.cfg.sys_lang)) ? fmt::format("TROP_{:0>2d}.SFM", emuenv.cfg.sys_lang) : "TROP.SFM";

    pugi::xml_document doc;
    if (doc.load_file((trophy_conf_id_path / sfm_name).c_str())) {
        for (const auto &conf : doc.child("trophyconf")) {
            if (conf.name() == std::string("trophy")) {
                const std::string gid = conf.attribute("gid").empty() ? "000" : conf.attribute("gid").as_string();
                const std::string trophy_id = conf.attribute("id").as_string();
                if (group_id == gid) {
                    trophy_info[trophy_id].name = conf.child("name").text().as_string();
                    trophy_info[trophy_id].detail = conf.child("detail").text().as_string();
                    trophy_info[trophy_id].type["general"] = conf.attribute("ttype").as_string();
                    trophy_info[trophy_id].hidden = conf.attribute("hidden").as_string();
                }
            }
        }
    }

    for (const auto &[trophy_id, _] : trophy_info) {
        int32_t width = 0;
        int32_t height = 0;
        vfs::FileBuffer buffer;
        const std::string icon_name = fmt::format("TROP{}.PNG", trophy_id);

        vfs::read_file(VitaIoDevice::ux0, buffer, emuenv.vita_fs_path, fmt::format("user/{}/trophy/conf/{}/{}", emuenv.io.user_id, np_com_id, icon_name));
        if (buffer.empty()) {
            LOG_WARN("Trophy icon, Name: '{}', Not found for trophy id: {}.", icon_name, trophy_id);
            continue;
        }
        stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data) {
            LOG_ERROR("Invalid trophy icon for trophy id {} [{}].", icon_name, trophy_id);
            continue;
        }

        gui.trophy_list[trophy_id] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);

        auto &common = gui.lang.common.main;
        const auto trophy_type = np_com_id_info[np_com_id].context.trophy_kinds[string_utils::stoi_def(trophy_id, 0, "trophy id")];
        switch (trophy_type) {
        case SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM:
            trophy_info[trophy_id].type["detail"] = common["platinum"];
            break;
        case SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD:
            trophy_info[trophy_id].type["detail"] = common["gold"];
            break;
        case SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER:
            trophy_info[trophy_id].type["detail"] = common["silver"];
            break;
        case SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE:
            trophy_info[trophy_id].type["detail"] = common["bronze"];
            break;
        default:
            LOG_ERROR("Trophy id {} unknown type: {}", trophy_id, (SceInt32)trophy_type);
            break;
        }

        const auto unlocked = (time_t)np_com_id_info[np_com_id].context.unlock_timestamps[string_utils::stoi_def(trophy_id, 0, "trophy id")];
        // Check earned trophy and set time
        if (np_com_id_info[np_com_id].context.is_trophy_unlocked(string_utils::stoi_def(trophy_id, 0, "trophy id"))) {
            trophy_info[trophy_id].earned = true;
            SAFE_LOCALTIME(&unlocked, &trophy_info[trophy_id].unlocked_time);
        }

        const auto grade = np_com_id_info[np_com_id].context.trophy_kinds[string_utils::stoi_def(trophy_id, 0, "trophy id")];

        trophy_list.push_back({ trophy_id, unlocked, grade, trophy_info[trophy_id].name });

        trophy_sort = TrophySortType::ORIGINAL;
    }
}

enum class ScrollType {
    Undefined,
    NPCom,
    Trophy,
    Group
};
static ScrollType scroll_type = ScrollType::Undefined;
static std::string np_com_id_selected, group_id_selected, trophy_id_selected, delete_np_com_id;
static bool detail_np_com_id, set_scroll_pos;
static ImGuiTextFilter search_bar;
static std::map<ScrollType, float> scroll_pos;

static void refresh_synced_trophies(GuiState &gui, EmuEnvState &emuenv) {
    for (const auto &np_com_id : sync_downloaded) {
        auto &info = np_com_id_info[np_com_id];
        if (!load_and_compute_trophy_stats(emuenv, info.context, np_com_id, info.trophy_progress_info)) {
            LOG_WARN("Failed to load trophy stats for synced trophy: {}", np_com_id);
            continue;
        }

        const auto updated = info.trophy_progress_info.last_updated;
        SAFE_LOCALTIME(&updated, &info.updated);
        const auto progress = info.trophy_progress_info.progress["global"];
        const auto it = std::find_if(np_com_id_list.begin(), np_com_id_list.end(), [&np_com_id](const auto &np_com) {
            return np_com.id == np_com_id;
        });
        if (it != np_com_id_list.end()) {
            it->progress = progress;
            it->updated = updated;
        } else {
            const auto trophy_conf_np_com_id_path{ emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "trophy/conf" / np_com_id };
            if (load_trophy_conf_data(gui, emuenv, np_com_id, trophy_conf_np_com_id_path))
                np_com_id_list.push_back({ np_com_id, info.name["000"], progress, updated });
        }

        if (np_com_id == np_com_id_selected && !group_id_selected.empty())
            get_trophy_list(gui, emuenv, np_com_id, group_id_selected);
    }

    std::sort(np_com_id_list.begin(), np_com_id_list.end(), [](const auto &ta, const auto &tb) {
        return ta.updated > tb.updated;
    });
    sync_downloaded.clear();
}

void open_trophy_unlocked(GuiState &gui, EmuEnvState &emuenv, const std::string &np_com_id, const std::string &trophy_id) {
    np_com_id_selected = np_com_id;

    // Get group id corresponding trophy_id
    for (const auto &group : np_com_id_info[np_com_id].trophy_id_by_group) {
        if (std::any_of(std::begin(group.second), std::end(group.second), [&trophy_id](const auto &trophy) { return trophy == trophy_id; })) {
            group_id_selected = group.first;
            break;
        }
    }

    trophy_id_selected = trophy_id;
    get_trophy_list(gui, emuenv, np_com_id_selected, group_id_selected);
}

void draw_trophy_collection(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_POS(gui.logical_viewport_pos.x, gui.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(gui.logical_viewport_size.x, gui.logical_viewport_size.y);
    const ImVec2 RES_SCALE(gui.gui_scale.x, gui.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * gui.manual_dpi_scale, RES_SCALE.y * gui.manual_dpi_scale);

    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;

    const auto SIZE_ICON_LIST = ImVec2(160.f * SCALE.x, 88.f * SCALE.y);
    const auto SIZE_TROPHY_LIST = ImVec2(80.f * SCALE.x, 80.f * SCALE.y);

    const auto BUTTON_SIZE = ImVec2(310.f * SCALE.x, 46.f * SCALE.y);

    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INFORMATION_BAR_HEIGHT);
    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INFORMATION_BAR_HEIGHT);

    const auto SIZE_LIST = ImVec2((!np_com_id_selected.empty() && group_id_selected.empty() ? 810.f : 790.f) * SCALE.x, 442.f * SCALE.y);
    const auto SIZE_INFO = ImVec2(820.f * SCALE.x, 484.f * SCALE.y);
    const auto POPUP_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);

    const auto TROPHY_PATH{ emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "trophy" };

    const auto is_background = gui.apps_background.contains("NPXS10008");
    const auto is_12_hour_format = emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("##trophy_collection", &gui.vita_area.trophy_collection, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();

    const auto draw_list = ImGui::GetBackgroundDrawList();
    const ImVec2 BG_POS_MAX(VIEWPORT_POS.x + VIEWPORT_SIZE.x, VIEWPORT_POS.y + VIEWPORT_SIZE.y);
    if (is_background)
        draw_list->AddImage(gui.apps_background["NPXS10008"], VIEWPORT_POS, BG_POS_MAX);
    else
        draw_list->AddRectFilled(VIEWPORT_POS, BG_POS_MAX, IM_COL32(31.f, 12.f, 0.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    auto &lang = gui.lang.trophy_collection;
    auto &common = gui.lang.common.main;
    const auto hidden_trophy_str = gui.lang.common.main["hidden_trophy"].c_str();
    const auto get_unlocked_group_count_str = [&](const std::string &np_com_id, const std::string &group_id, bool detail) {
        const auto &counts_by_group = np_com_id_info[np_com_id].trophy_progress_info.unlocked_type_count;
        const auto counts_it = counts_by_group.find(group_id);
        const auto get_count = [&](SceNpTrophyGrade grade) {
            if (counts_it == counts_by_group.end())
                return 0u;
            const auto count = counts_it->second.find(grade);
            return count != counts_it->second.end() ? count->second : 0u;
        };
        const auto platinum = get_count(SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM);
        const auto gold = get_count(SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD);
        const auto silver = get_count(SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER);
        const auto bronze = get_count(SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE);
        if (detail)
            return fmt::format("P:{}  G:{}  S:{}  B:{}", platinum, gold, silver, bronze);
        return fmt::format("{}   {}   {}   {}", platinum, gold, silver, bronze);
    };

    if (group_id_selected.empty()) {
        ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
        if (!np_com_id_list.empty() && np_com_id_selected.empty()) {
            ImGui::SetCursorPos(ImVec2(VIEWPORT_POS.x + (10.f * SCALE.x), VIEWPORT_POS.y + (27.f * SCALE.y) - (ImGui::CalcTextSize(common["search"].c_str()).y / 2.f)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", common["search"].c_str());
            ImGui::SameLine();
            search_bar.Draw("##search_bar", 180 * SCALE.x);
        }
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - (285.f * SCALE.x), (15.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, "P   G   S   B");
        ImGui::SetCursorPosY(54.f * SCALE.y);
        ImGui::Separator();
        ImGui::SetWindowFontScale(1.f);
    }
    ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + (90.f * SCALE.x), WINDOW_POS.y + (!trophy_id_selected.empty() || detail_np_com_id ? 16.0f : 96.f) * SCALE.y), ImGuiCond_Always);
    ImGui::BeginChild("##trophy_collection_child", !trophy_id_selected.empty() || detail_np_com_id ? SIZE_INFO : SIZE_LIST, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    // Trophy Collection
    if (np_com_id_list.empty()) {
        ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
        ImGui::SetCursorPosY(140.f * SCALE.y);
        ImGui::TextWrapped("%s", lang["no_trophies"].c_str());
    } else {
        // Set Scroll Pos
        if (set_scroll_pos) {
            ImGui::SetScrollY(scroll_pos[scroll_type]);
            set_scroll_pos = false;
        }

        if (np_com_id_selected.empty()) {
            // Ask Delete Trophy Popup
            if (!delete_np_com_id.empty()) {
                ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
                ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
                ImGui::Begin("##trophy_delete", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                ImGui::SetNextWindowBgAlpha(0.999f);
                ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + (WINDOW_SIZE.x / 2.f) - (POPUP_SIZE.x / 2.f), WINDOW_POS.y + (WINDOW_SIZE.y / 2.f) - (POPUP_SIZE.y / 2.f)), ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
                ImGui::BeginChild("##trophy_delete_child", POPUP_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
                const ImVec2 ICON_POS(48.f * SCALE.x, 28.f * SCALE.y);
                if (gui.trophy_np_com_id_list_icons[delete_np_com_id].contains("000")) {
                    ImGui::SetCursorPos(ICON_POS);
                    ImGui::Image(gui.trophy_np_com_id_list_icons[delete_np_com_id]["000"], SIZE_ICON_LIST);
                }
                const auto PADDING = (20.f * SCALE.x);
                ImGui::SetWindowFontScale(1.5f * RES_SCALE.x);
                const auto CALC_TITLE = ImGui::CalcTextSize(np_com_id_info[delete_np_com_id].name["000"].c_str(), nullptr, false, POPUP_SIZE.x - SIZE_ICON_LIST.x - ICON_POS.x - PADDING).y / 2.f;
                ImGui::SetCursorPos(ImVec2(ICON_POS.x + SIZE_ICON_LIST.x + PADDING, ICON_POS.y + (SIZE_ICON_LIST.y / 2.f) - CALC_TITLE));
                ImGui::PushTextWrapPos(POPUP_SIZE.x - PADDING);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", np_com_id_info[delete_np_com_id].name["000"].c_str());
                ImGui::PopTextWrapPos();
                ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                const auto delete_str = lang["trophy_deleted"].c_str();
                ImGui::SetCursorPos(ImVec2(POPUP_SIZE.x / 2 - ImGui::CalcTextSize(delete_str, 0, false, POPUP_SIZE.x - (108.f * SCALE.x)).x / 2, (POPUP_SIZE.y / 2) + 10));
                ImGui::PushTextWrapPos(POPUP_SIZE.x - (54.f * SCALE.x));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", delete_str);
                ImGui::PopTextWrapPos();
                ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), POPUP_SIZE.y - BUTTON_SIZE.y - (22.0f * SCALE.y)));
                if (ImGui::Button(::lang::get(::lang::str::cancel).c_str(), BUTTON_SIZE) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_circle))
                    delete_np_com_id.clear();
                ImGui::SameLine(0, 20.f * SCALE.x);
                if (ImGui::Button(::lang::get(::lang::str::ok).c_str(), BUTTON_SIZE) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_cross)) {
                    fs::remove_all(TROPHY_PATH / "conf" / delete_np_com_id);
                    fs::remove_all(TROPHY_PATH / "data" / delete_np_com_id);
                    const auto np_com_id_index = std::ranges::find_if(np_com_id_list, [&](const NPComIdSort &np_com) {
                        return np_com.id == delete_np_com_id;
                    });
                    np_com_id_list.erase(np_com_id_index);
                    np_com_id_info.erase(delete_np_com_id);
                    gui.trophy_np_com_id_list_icons[delete_np_com_id]["000"] = {};
                    gui.trophy_np_com_id_list_icons.erase(delete_np_com_id);
                    delete_np_com_id.clear();
                }
                ImGui::PopStyleVar();
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::End();
            }

            // Select Np Com ID
            ImGui::Columns(3, nullptr, false);
            ImGui::SetColumnWidth(0, SIZE_ICON_LIST.x + (10.f * SCALE.x));
            ImGui::SetColumnWidth(1, 405.f * SCALE.x);
            for (const auto &np_com : np_com_id_list) {
                if (!search_bar.PassFilter(np_com.name.c_str()))
                    continue;
                ImGui::PushID(np_com.id.c_str());
                if (gui.trophy_np_com_id_list_icons[np_com.id].contains("000"))
                    ImGui::Image(gui.trophy_np_com_id_list_icons[np_com.id]["000"], SIZE_ICON_LIST);
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.3f * RES_SCALE.x);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.2f));
                const auto Title_POS = ImGui::GetCursorPosY();
                if (ImGui::Selectable(np_com.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y))) {
                    np_com_id_selected = np_com.id;
                    if (!np_com_id_info[np_com_id_selected].context.group_count) {
                        group_id_selected = "000";
                        get_trophy_list(gui, emuenv, np_com.id, "000");
                    }
                    scroll_pos[ScrollType::NPCom] = ImGui::GetScrollY();
                    ImGui::SetScrollY(0.f);
                }
                ImGui::PopStyleVar();

                // Trophy Context Menu
                if (ImGui::BeginPopupContextItem("##trophy_context_menu")) {
                    if (ImGui::MenuItem(lang["delete_trophy"].c_str()))
                        delete_np_com_id = np_com.id;
                    ImGui::EndPopup();
                }

                ImGui::SetCursorPosY(Title_POS + (50.f * SCALE.y));
                ImGui::SetWindowFontScale(1.f * RES_SCALE.x);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                ImGui::ProgressBar(np_com.progress / 100.f, ImVec2(200 * SCALE.x, 15.f * SCALE.y), "");
                ImGui::PopStyleColor();
                ImGui::SameLine(0.f, (10.f * SCALE.x));
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (6.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", std::to_string(np_com.progress).append("%").c_str());
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                const auto unlocked_group_count_str = get_unlocked_group_count_str(np_com.id, "global", false);
                ImGui::Selectable(unlocked_group_count_str.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y));
                ImGui::PopID();
                ImGui::PopStyleVar();
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        } else {
            // Select Group ID
            if (group_id_selected.empty()) {
                if (gui.trophy_np_com_id_list_icons[np_com_id_selected].contains("000"))
                    ImGui::Image(gui.trophy_np_com_id_list_icons[np_com_id_selected]["000"], SIZE_ICON_LIST);
                ImGui::SameLine();
                ImGui::SetWindowFontScale(1.3f * RES_SCALE.x);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                if (ImGui::Selectable(np_com_id_info[np_com_id_selected].name["000"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y))) {
                    group_id_selected = "global";
                    detail_np_com_id = true;
                }
                ImGui::PopStyleVar();
                ImGui::Columns(4, nullptr, false);
                ImGui::SetColumnWidth(0, 30.f * SCALE.x);
                ImGui::SetColumnWidth(1, SIZE_ICON_LIST.x + (10.f * SCALE.x));
                ImGui::SetColumnWidth(2, 375.f * SCALE.x);
                for (const auto &group_id : np_com_id_info[np_com_id_selected].group_id) {
                    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (10.f * SCALE.x) - (ImGui::CalcTextSize("+").x / 2.f), ImGui::GetCursorPosY() + (SIZE_ICON_LIST.y / 2.f) - (ImGui::CalcTextSize("+").y / 2.f)));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", group_id != "000" ? "+" : "");
                    ImGui::NextColumn();
                    if (gui.trophy_np_com_id_list_icons[np_com_id_selected].contains(group_id))
                        ImGui::Image(gui.trophy_np_com_id_list_icons[np_com_id_selected][group_id], SIZE_ICON_LIST);
                    ImGui::NextColumn();
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.2f));
                    const auto Title_POS = ImGui::GetCursorPosY();
                    ImGui::PushID(group_id.c_str());
                    if (ImGui::Selectable(np_com_id_info[np_com_id_selected].name[group_id].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y))) {
                        group_id_selected = group_id;
                        get_trophy_list(gui, emuenv, np_com_id_selected, group_id);
                        scroll_pos[ScrollType::Group] = ImGui::GetScrollY();
                        ImGui::SetScrollY(0.f);
                    }
                    ImGui::PopID();
                    ImGui::PopStyleVar();
                    ImGui::SetCursorPosY(Title_POS + (50.f * SCALE.y));
                    ImGui::SetWindowFontScale(1.f * RES_SCALE.x);
                    auto &trophy_progress_info = np_com_id_info[np_com_id_selected].trophy_progress_info;
                    const auto progress = trophy_progress_info.progress[group_id];
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                    ImGui::ProgressBar(progress / 100.f, ImVec2(200 * SCALE.x, 15.f * SCALE.y), "");
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0.f, (10.f * SCALE.x));
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (6.f * SCALE.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", std::to_string(progress).append("%").c_str());
                    ImGui::NextColumn();
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                    const auto unlocked_group_count_str = get_unlocked_group_count_str(np_com_id_selected, group_id, false);
                    ImGui::Selectable(unlocked_group_count_str.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y));
                    ImGui::SameLine(0.f, 25.f * SCALE.x);
                    ImGui::Selectable(">", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y));
                    ImGui::PopStyleVar();
                    ImGui::NextColumn();
                }
                // Detail of Np Com ID
            } else if (detail_np_com_id) {
                ImGui::SetWindowFontScale(1.5f * RES_SCALE.x);
                if (gui.trophy_np_com_id_list_icons[np_com_id_selected].contains(group_id_selected == "global" ? "000" : group_id_selected))
                    ImGui::Image(gui.trophy_np_com_id_list_icons[np_com_id_selected][group_id_selected == "global" ? "000" : group_id_selected], SIZE_ICON_LIST);
                const auto CALC_NAME = ImGui::CalcTextSize(np_com_id_info[np_com_id_selected].name[group_id_selected == "global" ? "000" : group_id_selected].c_str(), nullptr, false, SIZE_INFO.x - SIZE_ICON_LIST.x - 48.f).y / 2.f;
                ImGui::SetCursorPos(ImVec2(SIZE_ICON_LIST.x + 20.f, (SIZE_ICON_LIST.y / 2.f) - CALC_NAME));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_INFO.x - SIZE_ICON_LIST.x - 48.f);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", np_com_id_info[np_com_id_selected].name[group_id_selected == "global" ? "000" : group_id_selected].c_str());
                ImGui::PopTextWrapPos();
                ImGui::SetCursorPosY(SIZE_ICON_LIST.y + (20.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["progress"].c_str());
                ImGui::SameLine(260.f * SCALE.x);
                auto &trophy_progress_info = np_com_id_info[np_com_id_selected].trophy_progress_info;
                const auto progress = trophy_progress_info.progress[group_id_selected];
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", std::to_string(progress).append("%").c_str());
                ImGui::SameLine(360 * SCALE.x);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (6.f * SCALE.y));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                ImGui::ProgressBar(progress / 100.f, ImVec2(200 * SCALE.x, 15.f * SCALE.y), "");
                ImGui::PopStyleColor();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (30.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["trophies"].c_str());
                ImGui::SameLine(260.f * SCALE.x);
                const auto trophy_count = group_id_selected == "global" ? np_com_id_info[np_com_id_selected].context.trophy_count : np_com_id_info[np_com_id_selected].context.trophy_count_by_group[std::stoll(group_id_selected)];
                const auto unlocked_ids = trophy_progress_info.unlocked_ids.find(group_id_selected);
                const auto unlocked_count = unlocked_ids != trophy_progress_info.unlocked_ids.end() ? unlocked_ids->second.size() : 0;
                const auto unlocked_group_count_str = get_unlocked_group_count_str(np_com_id_selected, group_id_selected, true);
                ImGui::TextColored(GUI_COLOR_TEXT, "%d/%d\n%s", static_cast<int>(unlocked_count), static_cast<int>(trophy_count), unlocked_group_count_str.c_str());
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (45.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["updated"].c_str());
                ImGui::SameLine(260.f * SCALE.x);
                auto DATE_TIME = get_date_time(gui, emuenv, np_com_id_info[np_com_id_selected].updated);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", DATE_TIME[DateTime::DATE_MINI].c_str(), DATE_TIME[DateTime::CLOCK].c_str());
                if (is_12_hour_format) {
                    ImGui::SameLine();
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", DATE_TIME[DateTime::DAY_MOMENT].c_str());
                }
                ImGui::PushTextWrapPos(SIZE_INFO.x - 40.f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["details"].c_str());
                ImGui::SameLine(260.f * SCALE.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", np_com_id_info[np_com_id_selected].detail[group_id_selected == "global" ? "000" : group_id_selected].c_str());
                ImGui::PopTextWrapPos();
                // Select Trophy
            } else if (trophy_id_selected.empty()) {
                if (gui.trophy_np_com_id_list_icons[np_com_id_selected].contains(group_id_selected))
                    ImGui::Image(gui.trophy_np_com_id_list_icons[np_com_id_selected][group_id_selected], SIZE_ICON_LIST);
                ImGui::SameLine();
                ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                if (ImGui::Selectable(np_com_id_info[np_com_id_selected].name[group_id_selected].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y)))
                    detail_np_com_id = true;
                ImGui::PopStyleVar();
                ImGui::Columns(4, nullptr, false);
                ImGui::SetColumnWidth(0, 30.f * SCALE.x);
                ImGui::SetColumnWidth(1, SIZE_TROPHY_LIST.x + (15.f * SCALE.x));
                ImGui::SetColumnWidth(2, 40.f * SCALE.x);
                for (const auto &trophy : trophy_list) {
                    ImGui::NextColumn();
                    ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
                    if (trophy_info[trophy.id].earned)
                        ImGui::Image(gui.trophy_list[trophy.id], SIZE_TROPHY_LIST);
                    else {
                        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                        ImGui::PushID(trophy.id.c_str());
                        if (ImGui::Selectable(lang["locked"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_TROPHY_LIST.y))) {
                            trophy_id_selected = trophy.id;
                            scroll_pos[ScrollType::Trophy] = ImGui::GetScrollY();
                        }
                        ImGui::PopID();
                        ImGui::PopStyleVar();
                    }
                    ImGui::NextColumn();
                    const auto hidden_trophy = (!trophy_info[trophy.id].earned && (trophy_info[trophy.id].hidden == "yes"));
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (14.f * SCALE.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", hidden_trophy && !show_hidden_trophy ? "?" : trophy_info[trophy.id].type["general"].c_str());
                    ImGui::NextColumn();
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.2f));
                    const auto name_pos = ImGui::GetCursorPosY();
                    if (ImGui::Selectable(hidden_trophy && !show_hidden_trophy ? hidden_trophy_str : trophy.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_TROPHY_LIST.y)))
                        trophy_id_selected = trophy.id;
                    ImGui::PopStyleVar();
                    if (!hidden_trophy || show_hidden_trophy) {
                        ImGui::SetCursorPosY(name_pos + (40.f * SCALE.y));
                        ImGui::SetWindowFontScale(1.0f * RES_SCALE.x);
                        ImGui::Text("%s", trophy_info[trophy.id].detail.c_str());
                    }
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            } else {
                // Detail Trophy
                ImGui::SetWindowFontScale(1.5f * RES_SCALE.x);
                if (trophy_info[trophy_id_selected].earned)
                    ImGui::Image(gui.trophy_list[trophy_id_selected], SIZE_TROPHY_LIST);
                else {
                    ImGui::SetCursorPosY((SIZE_TROPHY_LIST.y / 2.f) - (15.f * SCALE.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["locked"].c_str());
                }
                const auto hidden_trophy = (!trophy_info[trophy_id_selected].earned && (trophy_info[trophy_id_selected].hidden == "yes"));
                const auto selected_trophy_str = hidden_trophy && !show_hidden_trophy ? hidden_trophy_str : trophy_info[trophy_id_selected].name.c_str();
                const auto CALC_NAME = ImGui::CalcTextSize(selected_trophy_str, nullptr, false, SIZE_INFO.x - SIZE_TROPHY_LIST.x - 48.f).y / 2.f;
                ImGui::SetCursorPos(ImVec2(SIZE_TROPHY_LIST.x + 20.f, (SIZE_TROPHY_LIST.y / 2.f) - CALC_NAME));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_INFO.x - SIZE_TROPHY_LIST.x - (48.f * SCALE.x));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", selected_trophy_str);
                ImGui::PopTextWrapPos();
                ImGui::SetCursorPosY(SIZE_TROPHY_LIST.y + (25.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["grade"].c_str());
                ImGui::SameLine(250.f * SCALE.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", hidden_trophy && !show_hidden_trophy ? "?" : trophy_info[trophy_id_selected].type["detail"].c_str());
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (25.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["earned"].c_str());
                ImGui::SameLine(250.f * SCALE.x);
                if (trophy_info[trophy_id_selected].earned) {
                    auto DATE_TIME = get_date_time(gui, emuenv, trophy_info[trophy_id_selected].unlocked_time);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", DATE_TIME[DateTime::DATE_MINI].c_str(), DATE_TIME[DateTime::CLOCK].c_str());
                    if (is_12_hour_format) {
                        ImGui::SameLine();
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", DATE_TIME[DateTime::DAY_MOMENT].c_str());
                    }
                } else
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["not_earned"].c_str());
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (25.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["details"].c_str());
                ImGui::SameLine(250.f * SCALE.x);
                ImGui::PushTextWrapPos(SIZE_INFO.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", hidden_trophy && !show_hidden_trophy ? "-" : trophy_info[trophy_id_selected].detail.c_str());
                ImGui::PopTextWrapPos();
            }
        }
    }
    ImGui::ScrollWhenDragging();
    ImGui::EndChild();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
    ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);

    // Back
    ImGui::SetCursorPos(ImVec2(8.f * SCALE.x, WINDOW_SIZE.y - (52.f * SCALE.y)));
    if (ImGui::Button("<<", ImVec2(64.f * SCALE.x, 40.f * SCALE.y)) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_circle)) {
        if (!np_com_id_selected.empty()) {
            if (detail_np_com_id) {
                detail_np_com_id = false;
                if (group_id_selected == "global")
                    group_id_selected.clear();
            } else if (!trophy_id_selected.empty()) {
                trophy_id_selected.clear();
                scroll_type = ScrollType::Trophy;
                set_scroll_pos = true;
            } else if (!group_id_selected.empty()) {
                group_id_selected.clear();
                if (!np_com_id_info[np_com_id_selected].context.group_count) {
                    np_com_id_selected.clear();
                    scroll_type = ScrollType::NPCom;
                } else
                    scroll_type = ScrollType::Group;
                set_scroll_pos = true;
            } else {
                np_com_id_selected.clear();
                scroll_type = ScrollType::NPCom;
                set_scroll_pos = true;
            }
        } else
            close_system_app(gui, emuenv);
    }

    // Sort
    if (trophy_id_selected.empty() && !detail_np_com_id && (np_com_id_selected.empty() || !group_id_selected.empty())) {
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - (70.f * SCALE.x), WINDOW_SIZE.y - (52.f * SCALE.y)));
        if (ImGui::Button("...", ImVec2(64.f * SCALE.x, 40.f * SCALE.y)) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_triangle))
            ImGui::OpenPopup("...");
        if (ImGui::BeginPopup("...", ImGuiWindowFlags_NoMove)) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["sort"].c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (np_com_id_selected.empty()) {
                if (ImGui::MenuItem(lang["updated"].c_str(), nullptr, np_com_id_sort == NpComIdSortType::UPDATED)) {
                    std::sort(np_com_id_list.begin(), np_com_id_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.updated > tb.updated;
                    });
                    np_com_id_sort = NpComIdSortType::UPDATED;
                }
                if (ImGui::MenuItem(lang["name"].c_str(), nullptr, np_com_id_sort == NpComIdSortType::NAME)) {
                    std::sort(np_com_id_list.begin(), np_com_id_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.name < tb.name;
                    });
                    np_com_id_sort = NpComIdSortType::NAME;
                }
                if (ImGui::MenuItem(lang["progress"].c_str(), nullptr, np_com_id_sort == NpComIdSortType::PROGRESS)) {
                    std::sort(np_com_id_list.begin(), np_com_id_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.progress > tb.progress;
                    });
                    np_com_id_sort = NpComIdSortType::PROGRESS;
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                if (ImGui::MenuItem(lang["sync_with_server"].c_str())) {
                    sync_trophy(emuenv);
                    trophy_dialog_state = TrophyDialogState::SYNC_TROPHY;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                if (ImGui::Button(gui.lang.indicator["delete_all"].c_str())) {
                    trophy_dialog_state = TrophyDialogState::DELETE_ALL;
                    ImGui::CloseCurrentPopup();
                }
            } else {
                if (ImGui::MenuItem(lang["original"].c_str(), nullptr, trophy_sort == TrophySortType::ORIGINAL)) {
                    std::sort(trophy_list.begin(), trophy_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.id < tb.id;
                    });
                    trophy_sort = TrophySortType::ORIGINAL;
                }
                if (ImGui::MenuItem(lang["earned"].c_str(), nullptr, trophy_sort == TrophySortType::EARNED)) {
                    std::sort(trophy_list.begin(), trophy_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.earned > tb.earned;
                    });
                    trophy_sort = TrophySortType::EARNED;
                }
                if (ImGui::MenuItem(lang["grade"].c_str(), nullptr, trophy_sort == TrophySortType::GRADE)) {
                    std::sort(trophy_list.begin(), trophy_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.grade < tb.grade;
                    });
                    trophy_sort = TrophySortType::GRADE;
                }
                if (ImGui::MenuItem(lang["name"].c_str(), nullptr, trophy_sort == TrophySortType::NAME)) {
                    std::sort(trophy_list.begin(), trophy_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.name < tb.name;
                    });
                    trophy_sort = TrophySortType::NAME;
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["advance"].c_str());
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                if (ImGui::MenuItem(lang["show_hidden"].c_str(), nullptr, show_hidden_trophy))
                    show_hidden_trophy = !show_hidden_trophy;
            }
            ImGui::EndPopup();
        }
    }
    ImGui::SetWindowFontScale(1.f);
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar(2);

    if (trophy_dialog_state != TrophyDialogState::NONE) {
        const auto sync_popup_size = ImVec2(760.f * SCALE.x, 440.f * SCALE.y);
        const auto sync_button_size = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);
        const auto popup_pos = ImVec2(VIEWPORT_POS.x + ((VIEWPORT_SIZE.x - sync_popup_size.x) / 2.f), VIEWPORT_POS.y + ((VIEWPORT_SIZE.y - sync_popup_size.y) / 2.f));
        ImGui::SetNextWindowPos(VIEWPORT_POS, ImGuiCond_Always);
        ImGui::SetNextWindowSize(VIEWPORT_SIZE, ImGuiCond_Always);
        ImGui::Begin("##trophy_dialog_overlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
        if (trophy_dialog_state == TrophyDialogState::SYNC_TROPHY) {
            ImGui::SetNextWindowPos(popup_pos, ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.f * SCALE.x);
            ImGui::BeginChild("##sync_trophy_popup", sync_popup_size, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
            const auto progress = std::clamp(sync_progress.load(), 0.f, 1.f);
            std::string error_text;
            {
                std::lock_guard lock(sync_error_mutex);
                error_text = sync_error;
            }
            const auto has_error = !error_text.empty();
            const auto progress_label = has_error ? error_text.c_str() : lang["syncing_trophy"].c_str();
            ImGui::SetCursorPos(ImVec2(52.f * SCALE.x, 104.f * SCALE.y));
            ImGui::PushTextWrapPos(sync_popup_size.x - (52.f * SCALE.x));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", progress_label);
            ImGui::PopTextWrapPos();
            const ImVec2 progress_bar_size(560.f * SCALE.x, 12.f * SCALE.y);
            ImGui::SetCursorPos(ImVec2((sync_popup_size.x - progress_bar_size.x) / 2.f, (sync_popup_size.y / 2.f) - progress_bar_size.y));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            ImGui::ProgressBar(progress, progress_bar_size, "");
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            const auto progress_text = std::to_string(static_cast<uint32_t>(progress * 100.f)) + "%";
            ImGui::SetCursorPos(ImVec2(sync_popup_size.x / 2.f - ImGui::CalcTextSize(progress_text.c_str()).x / 2.f, (sync_popup_size.y / 2.f) + (18.f * SCALE.y)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", progress_text.c_str());

            if (progress >= 1.f) {
                if (!has_error) {
                    if (!sync_refresh_pending) {
                        sync_refresh_pending = true;
                        if (!sync_downloaded.empty())
                            refresh_synced_trophies(gui, emuenv);
                    }
                    if (get_timestamp_ms() - sync_start_time >= 900) {
                        trophy_dialog_state = TrophyDialogState::NONE;
                    }
                }
                if (has_error) {
                    ImGui::SetCursorPos(ImVec2((sync_popup_size.x - sync_button_size.x) / 2.f, sync_popup_size.y - sync_button_size.y - (24.f * SCALE.y)));
                    if (ImGui::Button(::lang::get(::lang::str::ok).c_str(), sync_button_size))
                        trophy_dialog_state = TrophyDialogState::NONE;
                }
            } else {
                ImGui::SetCursorPos(ImVec2((sync_popup_size.x - sync_button_size.x) / 2.f, sync_popup_size.y - sync_button_size.y - (24.f * SCALE.y)));
                if (ImGui::Button(::lang::get(::lang::str::cancel).c_str(), sync_button_size)) {
                    sync_cancel.store(true);
                    trophy_dialog_state = TrophyDialogState::NONE;
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
        } else {
            const auto delete_popup_pos = ImVec2(WINDOW_POS.x + (WINDOW_SIZE.x - POPUP_SIZE.x) / 2.f, WINDOW_POS.y + (WINDOW_SIZE.y - POPUP_SIZE.y) / 2.f);
            ImGui::SetNextWindowPos(delete_popup_pos, ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.f * SCALE.x);
            ImGui::BeginChild("##delete_all_popup", POPUP_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
            const auto delete_all_str = lang["all_trophy_deleted"].c_str();
            ImGui::SetCursorPos(ImVec2(POPUP_SIZE.x / 2.f - ImGui::CalcTextSize(delete_all_str, nullptr, false, POPUP_SIZE.x - (108.f * SCALE.x)).x / 2.f, (POPUP_SIZE.y / 2.f) - (46.f * SCALE.y)));
            ImGui::PushTextWrapPos(POPUP_SIZE.x - (54.f * SCALE.x));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", delete_all_str);
            ImGui::PopTextWrapPos();
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - (BUTTON_SIZE.x + (20.f * SCALE.x)), POPUP_SIZE.y - BUTTON_SIZE.y - (24.f * SCALE.y)));
            if (ImGui::Button(::lang::get(::lang::str::cancel).c_str(), BUTTON_SIZE) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_circle)) {
                trophy_dialog_state = TrophyDialogState::NONE;
            }
            ImGui::SameLine(0, 20.f * SCALE.x);
            if (ImGui::Button(::lang::get(::lang::str::ok).c_str(), BUTTON_SIZE) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_cross)) {
                if (fs::remove_all(TROPHY_PATH))
                    LOG_INFO("All trophies successfully deleted.");
                init_trophy_collection(gui, emuenv);
                trophy_dialog_state = TrophyDialogState::NONE;
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
        ImGui::End();
    }
}

} // namespace gui
