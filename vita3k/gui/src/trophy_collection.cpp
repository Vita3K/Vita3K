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

#include <util/log.h>
#include <util/safe_time.h>
#include <util/string_utils.h>

#include <config/state.h>
#include <dialog/state.h>
#include <io/device.h>
#include <io/functions.h>

#include <v3kn/storage.h>

#include <pugixml.hpp>
#include <stb_image.h>

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

        vfs::read_file(VitaIoDevice::ux0, buffer, emuenv.pref_path, "user/" + emuenv.io.user_id + "/trophy/conf/" + np_com_id + "/" + group.second);

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

enum class NpComIdSortType {
    UPDATED,
    NAME,
    PROGRESS
};

static NpComIdSortType np_com_id_sort;
static bool show_hidden_trophy = false;

void init_trophy_collection(GuiState &gui, EmuEnvState &emuenv) {
    const auto TROPHY_PATH{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "trophy" };
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

                if (!load_and_compute_trophy_stats(emuenv, np_com_id_info[np_com_id].context, np_com_id, np_com_id_info[np_com_id].trophy_progress_info)) {
                    LOG_ERROR("Failed to load and compute trophy stats for NPComId: {}, removing trophy data and config.", np_com_id);
                    const auto trophy_conf_np_com_id_path_local{ TROPHY_CONF_PATH / np_com_id };
                    fs::remove_all(trophy_data_np_com_id_path);
                    fs::remove_all(trophy_conf_np_com_id_path_local);
                    continue;
                }

                const auto updated = fs::last_write_time(trophy_data_np_com_id_path / "TROPUSR.DAT");
                SAFE_LOCALTIME(&updated, &np_com_id_info[np_com_id].updated);

                const auto progress = np_com_id_info[np_com_id].trophy_progress_info.progress["global"];

                if (!load_trophy_conf_data(gui, emuenv, np_com_id, trophy_conf_np_com_id_path))
                    continue;

                np_com_id_list.push_back({ np_com_id, np_com_id_info[np_com_id].name["000"], progress, updated });
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
    const auto trophy_conf_id_path{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "trophy/conf" / np_com_id };
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
        for (const auto &conf : doc.child("trophyconf").children("trophy")) {
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

    for (const auto &[trophy_id, _] : trophy_info) {
        int32_t width = 0;
        int32_t height = 0;
        vfs::FileBuffer buffer;
        const std::string icon_name = fmt::format("TROP{}.PNG", trophy_id);

        vfs::read_file(VitaIoDevice::ux0, buffer, emuenv.pref_path, fmt::format("user/{}/trophy/conf/{}/{}", emuenv.io.user_id, np_com_id, icon_name));
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

enum class TrophyDialogState {
    NONE,
    SYNC_TROPHY,
    DELETE_ALL,
};

static TrophyDialogState trophy_dialog_state = TrophyDialogState::NONE;
static std::atomic<float> sync_progress{ 0.f };
static std::atomic<bool> sync_cancel{ false };
static std::vector<std::string> sync_downloaded;
static std::chrono::steady_clock::time_point sync_complete_time;
static void sync_trophy(GuiState &gui, EmuEnvState &emuenv) {
    sync_cancel = false;
    sync_progress = 0.f;

    std::map<std::string, TrophySync> trophy_progress;
    for (auto &[np_com_id, info] : np_com_id_info) {
        auto &trophy_progress_info = info.trophy_progress_info;
        const auto global_progress = trophy_progress_info.progress["global"];
        if (global_progress > 0) {
            const auto plat_count = trophy_progress_info.unlocked_type_count["global"][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM];
            const auto gold_count = trophy_progress_info.unlocked_type_count["global"][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD];
            const auto silver_count = trophy_progress_info.unlocked_type_count["global"][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER];
            const auto bronze_count = trophy_progress_info.unlocked_type_count["global"][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE];
            trophy_progress[np_com_id] = {
                .last_updated = trophy_progress_info.last_updated,
                .unlocked_ids = trophy_progress_info.unlocked_ids["global"],
                .platinum = plat_count,
                .gold = gold_count,
                .silver = silver_count,
                .bronze = bronze_count,
                .progress = global_progress,
            };
        }
    }

    v3kn::start_trophy_sync(gui, emuenv, trophy_progress, sync_progress, sync_cancel, sync_downloaded);
}

static void refresh_synced_trophies(GuiState &gui, EmuEnvState &emuenv) {
    std::thread([&]() {
        for (const auto &np_com_id : sync_downloaded) {
            if (!load_and_compute_trophy_stats(emuenv, np_com_id_info[np_com_id].context, np_com_id, np_com_id_info[np_com_id].trophy_progress_info)) {
                LOG_WARN("Failed to load trophy stats for synced trophy: {}", np_com_id);
                continue;
            }

            const auto TROPHY_PATH{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "trophy" };
            const auto trophy_data_np_com_id_path{ TROPHY_PATH / "data" / np_com_id };

            auto &trophy_progress_info = np_com_id_info[np_com_id].trophy_progress_info;
            const auto updated = trophy_progress_info.last_updated;
            const auto progress = trophy_progress_info.progress["global"];

            const auto it = std::find_if(np_com_id_list.begin(), np_com_id_list.end(), [&np_com_id](const auto &np_com) {
                return np_com.id == np_com_id;
            });
            if (it != np_com_id_list.end()) {
                it->progress = progress;
                it->updated = updated;
            } else {
                const auto TROPHY_CONF_PATH{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "trophy" / "conf" / np_com_id };
                if (load_trophy_conf_data(gui, emuenv, np_com_id, TROPHY_CONF_PATH))
                    np_com_id_list.push_back({ np_com_id, np_com_id_info[np_com_id].name["000"], progress, updated });
            }
        }

        std::sort(np_com_id_list.begin(), np_com_id_list.end(), [](const auto &ta, const auto &tb) {
            return ta.updated > tb.updated;
        });
        sync_downloaded.clear();
    }).detach();
}

void draw_trophy_collection(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;

    const auto SIZE_ICON_LIST = ImVec2(160.f * SCALE.x, 88.f * SCALE.y);
    const auto SIZE_TROPHY_LIST = ImVec2(80.f * SCALE.x, 80.f * SCALE.y);

    const auto BUTTON_SIZE = ImVec2(310.f * SCALE.x, 46.f * SCALE.y);

    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INFORMATION_BAR_HEIGHT);
    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INFORMATION_BAR_HEIGHT);

    const auto SIZE_LIST = ImVec2((!np_com_id_selected.empty() && group_id_selected.empty() ? 810.f : 800.f) * SCALE.x, 416.f * SCALE.y);
    const auto SIZE_INFO = ImVec2(820.f * SCALE.x, 484.f * SCALE.y);
    const auto POPUP_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);

    const auto TROPHY_PATH{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "trophy" };

    const auto BG_PATH = "NPXS10008";
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
    if (gui.apps_background.contains(BG_PATH))
        draw_list->AddImage(gui.apps_background[BG_PATH], VIEWPORT_POS, BG_POS_MAX);
    else
        draw_list->AddRectFilled(VIEWPORT_POS, BG_POS_MAX, IM_COL32(31.f, 12.f, 0.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    auto &lang = gui.lang.trophy_collection;
    auto &common = emuenv.common_dialog.lang.common;
    const auto hidden_trophy_str = gui.lang.common.main["hidden_trophy"].c_str();

    if (group_id_selected.empty()) {
        ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
        if (!np_com_id_list.empty() && np_com_id_selected.empty()) {
            ImGui::SetCursorPos(ImVec2(10.f * SCALE.x, (27.f * SCALE.y) - (ImGui::CalcTextSize(common["search"].c_str()).y / 2.f)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", common["search"].c_str());
            ImGui::SameLine();
            search_bar.Draw("##search_bar", 180 * SCALE.x);
        }
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - (292.f * SCALE.x), (24.f * SCALE.y)));
        if (gui.vita_icons.contains("trophy_all")) {
            const ImVec2 original_icon_size(364.f, 132.f);
            const auto ratio = original_icon_size.x / original_icon_size.y;
            const auto icon_height = (66.f * SCALE.y);
            ImGui::Image(gui.vita_icons["trophy_all"], ImVec2(icon_height * ratio, icon_height));
        } else
            ImGui::TextColored(GUI_COLOR_TEXT, "P   G   S   B");
        ImGui::SetCursorPosY(96.f * SCALE.y);
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

        const auto get_unlocked_group_count_str = [&](const auto &np_com_id, const auto &group_id, const std::string &level) {
            auto &trophy_progress_info = np_com_id_info[np_com_id].trophy_progress_info;
            const auto plat_count = trophy_progress_info.unlocked_type_count[group_id][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM];
            const auto gold_count = trophy_progress_info.unlocked_type_count[group_id][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD];
            const auto silver_count = trophy_progress_info.unlocked_type_count[group_id][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER];
            const auto bronze_count = trophy_progress_info.unlocked_type_count[group_id][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE];
            if (level == "detail")
                return fmt::format("P:{}  G:{}  S:{}  B:{}", plat_count, gold_count, silver_count, bronze_count);

            return fmt::format("{}   {}   {}   {}", plat_count, gold_count, silver_count, bronze_count);
        };
        const auto draw_grade_count = [&](const char *icon_name, const uint32_t count, const float spacing_after) {
            const auto row_pos = ImGui::GetCursorPos();
            const ImVec2 icon_size(32.f * SCALE.x, 32.f * SCALE.y);
            const auto text_pos_y = row_pos.y + (2.f * SCALE.y);

            if (gui.vita_icons.contains(icon_name)) {
                ImGui::SetCursorPos(ImVec2(row_pos.x, row_pos.y));
                ImGui::Image(gui.vita_icons[icon_name], icon_size);
                ImGui::SameLine(0.f, 0.f * SCALE.x);
                ImGui::SetCursorPosY(text_pos_y);
            }

            ImGui::TextColored(GUI_COLOR_TEXT, "%u", count);
            if (spacing_after > 0.f)
                ImGui::SameLine(0.f, spacing_after);
        };
        const auto draw_unlocked_group_count_icons = [&](const auto &np_com_id, const auto &group_id) {
            auto &trophy_progress_info = np_com_id_info[np_com_id].trophy_progress_info;

            draw_grade_count("trophy_platinum", trophy_progress_info.unlocked_type_count[group_id][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM], 10.f * SCALE.x);
            draw_grade_count("trophy_gold", trophy_progress_info.unlocked_type_count[group_id][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD], 10.f * SCALE.x);
            draw_grade_count("trophy_silver", trophy_progress_info.unlocked_type_count[group_id][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER], 10.f * SCALE.x);
            draw_grade_count("trophy_bronze", trophy_progress_info.unlocked_type_count[group_id][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE], 0.f);
        };

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
                if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle)))
                    delete_np_com_id.clear();
                ImGui::SameLine(0, 20.f * SCALE.x);
                if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
                    fs::remove_all(TROPHY_PATH / "conf" / delete_np_com_id);
                    fs::remove_all(TROPHY_PATH / "data" / delete_np_com_id);
                    const auto np_com_id_index = std::find_if(np_com_id_list.begin(), np_com_id_list.end(), [&](const NPComIdSort &np_com) {
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
            ImGui::SetColumnWidth(1, 350.f * SCALE.x);
            for (const auto &np_com : np_com_id_list) {
                ImGui::PushID(np_com.id.c_str());
                if (!search_bar.PassFilter(np_com.name.c_str()))
                    continue;
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
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                ImGui::ProgressBar(np_com.progress / 100.f, ImVec2(200 * SCALE.x, 15.f * SCALE.y), "");
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
                ImGui::SameLine(0.f, (10.f * SCALE.x));
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (6.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", std::to_string(np_com.progress).append("%").c_str());
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                const auto get_cursor_screen_pos = ImGui::GetCursorScreenPos();
                const ImVec2 COUNT_BOX_SIZE(220 * SCALE.x, 70 * SCALE.y);
                const ImVec2 COUNT_BOX_POS_MIN(get_cursor_screen_pos.x + (30.f * SCALE.x),
                    get_cursor_screen_pos.y + ((SIZE_ICON_LIST.y - COUNT_BOX_SIZE.y) / 2.f));
                const ImVec2 COUNT_BOX_POS_MAX(COUNT_BOX_POS_MIN.x + COUNT_BOX_SIZE.x, COUNT_BOX_POS_MIN.y + COUNT_BOX_SIZE.y);
                ImGui::GetWindowDrawList()->AddRect(COUNT_BOX_POS_MIN, COUNT_BOX_POS_MAX, IM_COL32(255, 255, 255, 255), 10.f * SCALE.x);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.6f, 0.5f));
                ImGui::Selectable(get_unlocked_group_count_str(np_com.id, "global", "general").c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y));
                ImGui::PopID();
                ImGui::PopStyleVar();
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        } else {
            const auto get_grade_icon_str = [&](const std::string &trophy_id) {
                const auto hidden_trophy = (!trophy_info[trophy_id].earned && (trophy_info[trophy_id].hidden == "yes"));
                std::string grade_icon_str = "trophy_";
                const auto grade = np_com_id_info[np_com_id_selected].context.trophy_kinds[string_utils::stoi_def(trophy_id, 0, "trophy id")];
                if (!hidden_trophy || show_hidden_trophy) {
                    switch (grade) {
                    case SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE:
                        grade_icon_str += "bronze";
                        break;
                    case SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER:
                        grade_icon_str += "silver";
                        break;
                    case SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD:
                        grade_icon_str += "gold";
                        break;
                    case SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM:
                        grade_icon_str += "platinum";
                        break;
                    default:
                        grade_icon_str += "bronze";
                        break;
                    }
                } else
                    grade_icon_str += "hidden";

                return grade_icon_str;
            };

            // Select Group ID
            if (group_id_selected.empty()) {
                if (gui.trophy_np_com_id_list_icons[np_com_id_selected].contains("000"))
                    ImGui::Image(gui.trophy_np_com_id_list_icons[np_com_id_selected]["000"], SIZE_ICON_LIST);
                ImGui::SameLine();
                ImGui::SetWindowFontScale(1.3f * RES_SCALE.x);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                if (ImGui::Selectable(np_com_id_info[np_com_id_selected].name["000"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_ICON_LIST.y))) {
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
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                    ImGui::ProgressBar(trophy_progress_info.progress[group_id] / 100.f, ImVec2(200 * SCALE.x, 15.f * SCALE.y), "");
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0.f, (10.f * SCALE.x));
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (6.f * SCALE.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", std::to_string(trophy_progress_info.progress[group_id]).append("%").c_str());
                    ImGui::NextColumn();
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                    ImGui::Selectable(get_unlocked_group_count_str(np_com_id_selected, group_id, "general").c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y));
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
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", std::to_string(trophy_progress_info.progress[group_id_selected]).append("%").c_str());
                ImGui::SameLine(360 * SCALE.x);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (6.f * SCALE.y));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                ImGui::ProgressBar(trophy_progress_info.progress[group_id_selected] / 100.f, ImVec2(200 * SCALE.x, 15.f * SCALE.y), "");
                ImGui::PopStyleColor();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (30.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["trophies"].c_str());
                ImGui::SameLine(260.f * SCALE.x);
                const auto trophy_count = group_id_selected == "global" ? np_com_id_info[np_com_id_selected].context.trophy_count : np_com_id_info[np_com_id_selected].context.trophy_count_by_group[std::stoll(group_id_selected)];
                ImGui::TextColored(GUI_COLOR_TEXT, "%d/%d", trophy_progress_info.unlocked_ids[group_id_selected].size(), trophy_count);
                ImGui::SetCursorPos(ImVec2(260.f * SCALE.x, ImGui::GetCursorPosY() + (4.f * SCALE.y)));
                draw_unlocked_group_count_icons(np_com_id_selected, group_id_selected);
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
                if (ImGui::Selectable(np_com_id_info[np_com_id_selected].name[group_id_selected].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_ICON_LIST.y)))
                    detail_np_com_id = true;
                ImGui::PopStyleVar();
                ImGui::Columns(4, nullptr, false);
                ImGui::SetColumnWidth(0, 30.f * SCALE.x);
                ImGui::SetColumnWidth(1, SIZE_TROPHY_LIST.x + (15.f * SCALE.x));
                ImGui::SetColumnWidth(2, 38.f * SCALE.x);
                for (const auto &trophy : trophy_list) {
                    ImGui::NextColumn();
                    ImGui::Separator();
                    ImGui::SetWindowFontScale(1.15f * RES_SCALE.x);
                    ImGui::PushID(trophy.id.c_str());
                    if (trophy_info[trophy.id].earned)
                        ImGui::Image(gui.trophy_list[trophy.id], SIZE_TROPHY_LIST);
                    else {
                        const auto icon_pos = ImGui::GetCursorPos();
                        if (gui.vita_icons.contains("trophy_locked")) {
                            const ImVec2 LOCKED_SIZE(64.f * SCALE.x, 64.f * SCALE.y);
                            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (SIZE_TROPHY_LIST.x - LOCKED_SIZE.x) / 2.f, ImGui::GetCursorPosY() + (SIZE_TROPHY_LIST.y - LOCKED_SIZE.y) / 2.f));
                            ImGui::Image(gui.vita_icons["trophy_locked"], LOCKED_SIZE);
                        } else {
                            const auto LOCKED_SIZE = ImGui::CalcTextSize(lang["locked"].c_str());
                            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (SIZE_TROPHY_LIST.x - LOCKED_SIZE.x) / 2.f, ImGui::GetCursorPosY() + (SIZE_TROPHY_LIST.y - LOCKED_SIZE.y) / 2.f));
                            ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["locked"].c_str());
                        }
                        ImGui::SetCursorPosY(icon_pos.y + SIZE_TROPHY_LIST.y + ImGui::GetStyle().ItemSpacing.y);
                    }
                    ImGui::NextColumn();
                    ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
                    const auto hidden_trophy = (!trophy_info[trophy.id].earned && (trophy_info[trophy.id].hidden == "yes"));
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (10.f * SCALE.y));
                    const auto trophy_grade_name = get_grade_icon_str(trophy.id);
                    if (gui.vita_icons.contains(trophy_grade_name)) {
                        const ImVec2 ICON_SIZE = !hidden_trophy || show_hidden_trophy ? ImVec2(32.f * SCALE.x, 32.f * SCALE.y) : ImVec2(27.f * SCALE.x, 27.f * SCALE.y);
                        ImGui::Image(gui.vita_icons[trophy_grade_name], ICON_SIZE);
                    } else
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", hidden_trophy && !show_hidden_trophy ? "?" : trophy_info[trophy.id].type["general"].c_str());
                    ImGui::NextColumn();
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.2f));
                    const auto name_pos = ImGui::GetCursorPosY();
                    if (ImGui::Selectable(hidden_trophy && !show_hidden_trophy ? hidden_trophy_str : trophy.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_TROPHY_LIST.y))) {
                        trophy_id_selected = trophy.id;
                        scroll_pos[ScrollType::Trophy] = ImGui::GetScrollY();
                    }
                    ImGui::PopStyleVar();
                    if (!hidden_trophy || show_hidden_trophy) {
                        ImGui::SetCursorPosY(name_pos + (40.f * SCALE.y));
                        ImGui::SetWindowFontScale(1.f * RES_SCALE.y);
                        ImGui::Text("%s", trophy_info[trophy.id].detail.c_str());
                    }
                    ImGui::PopID();
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
                const auto TROPHY_GRADE_STR_POS_Y = SIZE_TROPHY_LIST.y + (25.f * SCALE.y);
                ImGui::SetCursorPosY(TROPHY_GRADE_STR_POS_Y);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["grade"].c_str());
                const ImVec2 TROHY_GRADE_POS(250.f * SCALE.x, TROPHY_GRADE_STR_POS_Y);
                ImGui::SetCursorPos(TROHY_GRADE_POS);
                const auto trophy_grade_name = get_grade_icon_str(trophy_id_selected);
                if (gui.vita_icons.contains(trophy_grade_name)) {
                    const ImVec2 ICON_SIZE = !hidden_trophy || show_hidden_trophy ? ImVec2(32.f * SCALE.x, 32.f * SCALE.y) : ImVec2(27.f * SCALE.x, 27.f * SCALE.y);
                    const ImVec2 ICON_POS(TROHY_GRADE_POS.x - (ICON_SIZE.x / 8.f), TROHY_GRADE_POS.y - (ICON_SIZE.y / 8.f));
                    ImGui::SetCursorPos(ICON_POS);
                    ImGui::Image(gui.vita_icons[trophy_grade_name], ICON_SIZE);
                    ImGui::SetCursorPos(ImVec2(TROHY_GRADE_POS.x + (30.f * SCALE.x), TROPHY_GRADE_STR_POS_Y));
                }
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

    const ImVec2 SMALL_BUTTON_SIZE(60.f * SCALE.x, 40.f * SCALE.y);
    const ImVec2 SMALL_BUTTON_POS(8.f * SCALE.x, WINDOW_SIZE.y - (52.f * SCALE.y));

    // Back
    ImGui::SetCursorPos(SMALL_BUTTON_POS);
    if (ImGui::Button("<<", SMALL_BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle))) {
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

    // Sort and Sync
    if (trophy_id_selected.empty() && !detail_np_com_id && (np_com_id_selected.empty() || !group_id_selected.empty())) {
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - (SMALL_BUTTON_SIZE.x + SMALL_BUTTON_POS.x), SMALL_BUTTON_POS.y));
        if (ImGui::Button("...", SMALL_BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_triangle)))
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
                if (ImGui::Button(lang["sync_with_server"].c_str())) {
                    sync_trophy(gui, emuenv);
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

        static const auto get_timestamp_ms = []() {
            return duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        };

        if (trophy_dialog_state != TrophyDialogState::NONE) {
            ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
            ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
            ImGui::Begin("##trophy_dialog_overlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

            switch (trophy_dialog_state) {
            case TrophyDialogState::SYNC_TROPHY: {
                const ImVec2 SYNC_POPUP_SIZE(760.f * SCALE.x, 440.f * SCALE.y);
                const ImVec2 SYNC_BUTTON_SIZE(320.f * SCALE.x, 46.f * SCALE.y);
                ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + ((VIEWPORT_SIZE.x - SYNC_POPUP_SIZE.x) / 2.f), VIEWPORT_POS.y + ((VIEWPORT_SIZE.y - SYNC_POPUP_SIZE.y) / 2.f)), ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.f * SCALE.x);
                ImGui::BeginChild("##sync_trophy_popup", SYNC_POPUP_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                const auto sync_str = lang["syncing_trophy"].c_str();
                ImGui::SetCursorPos(ImVec2(SYNC_POPUP_SIZE.x / 2 - ImGui::CalcTextSize(sync_str, 0, false, SYNC_POPUP_SIZE.x - (104.f * SCALE.x)).x / 2, 116.f * SCALE.y));
                ImGui::PushTextWrapPos(SYNC_POPUP_SIZE.x - (52.f * SCALE.x));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", sync_str);
                ImGui::PopTextWrapPos();
                const ImVec2 progress_bar_size(560.f * SCALE.x, 12.f * SCALE.y);
                ImGui::SetCursorPos(ImVec2((SYNC_POPUP_SIZE.x - progress_bar_size.x) / 2, (SYNC_POPUP_SIZE.y / 2.f) - progress_bar_size.y));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                ImGui::ProgressBar(sync_progress.load(), progress_bar_size, "");
                ImGui::PopStyleColor();
                const auto progress_str = std::to_string(static_cast<uint32_t>(sync_progress.load() * 100)).append("%");
                ImGui::SetCursorPos(ImVec2(SYNC_POPUP_SIZE.x / 2 - ImGui::CalcTextSize(progress_str.c_str()).x / 2, (SYNC_POPUP_SIZE.y / 2.f) + (18.f * SCALE.y)));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", progress_str.c_str());
                ImGui::SetCursorPos(ImVec2((SYNC_POPUP_SIZE.x - SYNC_BUTTON_SIZE.x) / 2.f, SYNC_POPUP_SIZE.y - SYNC_BUTTON_SIZE.y - (24.0f * SCALE.y)));
                ImGui::BeginDisabled((sync_progress.load() >= 1.f));
                if (ImGui::Button(common["cancel"].c_str(), SYNC_BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
                    sync_cancel.store(true);
                    trophy_dialog_state = TrophyDialogState::NONE;
                }
                ImGui::EndDisabled();
                if ((sync_progress.load() >= 1.f) || sync_cancel.load()) {
                    if (sync_progress.load() >= 1.f) {
                        static uint64_t sync_complete_time;
                        if (sync_complete_time == 0) {
                            sync_complete_time = get_timestamp_ms();
                            if (!sync_downloaded.empty())
                                refresh_synced_trophies(gui, emuenv);
                        }

                        auto elapsed = get_timestamp_ms() - sync_complete_time;

                        if (elapsed >= 900) {
                            trophy_dialog_state = TrophyDialogState::NONE;
                            sync_complete_time = 0;
                        }
                    } else {
                        trophy_dialog_state = TrophyDialogState::NONE;
                    }
                }
                ImGui::PopStyleVar(2);
                ImGui::EndChild();
                break;
            }
            case TrophyDialogState::DELETE_ALL: {
                ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + (WINDOW_SIZE.x / 2.f) - (POPUP_SIZE.x / 2.f), WINDOW_POS.y + (WINDOW_SIZE.y / 2.f) - (POPUP_SIZE.y / 2.f)), ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.f * SCALE.x);
                ImGui::BeginChild("##delete_all_popup", POPUP_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                const auto delete_all_str = lang["all_trophy_deleted"].c_str();
                ImGui::SetCursorPos(ImVec2(POPUP_SIZE.x / 2 - ImGui::CalcTextSize(delete_all_str, 0, false, POPUP_SIZE.x - (108.f * SCALE.x)).x / 2, (POPUP_SIZE.y / 2) - (46.f * SCALE.y)));
                ImGui::PushTextWrapPos(POPUP_SIZE.x - (54.f * SCALE.x));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", delete_all_str);
                ImGui::PopTextWrapPos();
                ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), POPUP_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
                if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle))) {
                    trophy_dialog_state = TrophyDialogState::NONE;
                }
                ImGui::SameLine(0, 20.f * SCALE.x);
                if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
                    if (fs::remove_all(TROPHY_PATH))
                        LOG_INFO("All trophies successfully deleted.");
                    trophy_dialog_state = TrophyDialogState::NONE;
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
                break;
            }
            default:
                break;
            }

            ImGui::End();
        }
    }
    ImGui::SetWindowFontScale(1.f);
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace gui
