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

#include <util/log.h>
#include <util/safe_time.h>
#include <util/string_utils.h>

#include <config/state.h>
#include <dialog/state.h>
#include <io/device.h>
#include <io/functions.h>

#include <pugixml.hpp>
#include <stb_image.h>

namespace gui {
using namespace np::trophy;

struct NPComId {
    std::map<std::string, std::string> name;
    std::map<std::string, std::string> detail;
    std::vector<std::string> group_id;
    std::map<std::string, uint32_t> trophy_count_by_group;
    std::map<std::string, std::vector<std::string>> trophy_id_by_group;
    tm updated;
    std::map<std::string, uint32_t> unlocked_count;
    std::map<std::string, uint32_t> progress;
    std::map<std::string, std::map<std::string, std::string>> unlocked_type_count;

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

static constexpr uint32_t TROPHY_USR_MAGIC = 0x12D5819A;
static bool load_trophy_progress(IOState &io, const SceUID &progress_input_file, const std::string &np_com_id) {
    auto read_trophy_progress_file = [&](void *data, uint32_t amount) -> int {
        return read_file(data, io, progress_input_file, amount, "load_trophy_progress_file");
    };

    // Check magic
    uint32_t magic;
    if (read_trophy_progress_file(&magic, 4) != 4 || magic != TROPHY_USR_MAGIC) {
        LOG_ERROR("Failed to read stuff magic");
        return false;
    }

    // Read trophy progress
    std::fill_n(np_com_id_info[np_com_id].context.trophy_progress, (MAX_TROPHIES >> 5), 0);
    if (read_trophy_progress_file(np_com_id_info[np_com_id].context.trophy_progress, sizeof(np_com_id_info[np_com_id].context.trophy_progress)) != sizeof(np_com_id_info[np_com_id].context.trophy_progress)) {
        LOG_ERROR("Failed to read trophy progress");
        return false;
    }

    // Read trophy availability
    if (read_trophy_progress_file(np_com_id_info[np_com_id].context.trophy_availability, sizeof(np_com_id_info[np_com_id].context.trophy_availability)) != sizeof(np_com_id_info[np_com_id].context.trophy_availability)) {
        LOG_ERROR("Failed to read trophy availability");
        return false;
    }

    // Read group count
    if (read_trophy_progress_file(&np_com_id_info[np_com_id].context.group_count, 4) != 4) {
        LOG_ERROR("Failed to read count group id");
        return false;
    }

    // Read trophy count
    if (read_trophy_progress_file(&np_com_id_info[np_com_id].context.trophy_count, 4) != 4) {
        LOG_ERROR("Failed to read trophy count");
        return false;
    }

    // Read platinum trophy ID
    if (read_trophy_progress_file(&np_com_id_info[np_com_id].context.platinum_trophy_id, 4) != 4) {
        LOG_ERROR("Failed to read platinum trophy ID");
        return false;
    }

    // Read trophy count by group
    if (read_trophy_progress_file(np_com_id_info[np_com_id].context.trophy_count_by_group.data(), (uint32_t)np_com_id_info[np_com_id].context.trophy_count_by_group.size() * 4) != (int)np_com_id_info[np_com_id].context.trophy_count_by_group.size() * 4) {
        LOG_ERROR("Failed to read trophy count by group");
        return false;
    }

    // Read trophy timestamps
    if (read_trophy_progress_file(np_com_id_info[np_com_id].context.unlock_timestamps.data(), (uint32_t)np_com_id_info[np_com_id].context.unlock_timestamps.size() * 8) != (int)np_com_id_info[np_com_id].context.unlock_timestamps.size() * 8) {
        LOG_ERROR("Failed to read unlock_timestamp");
        return false;
    }

    // Read trophy type
    if (read_trophy_progress_file(np_com_id_info[np_com_id].context.trophy_kinds.data(), (uint32_t)np_com_id_info[np_com_id].context.trophy_kinds.size() * 4) != (int)np_com_id_info[np_com_id].context.trophy_kinds.size() * 4) {
        LOG_ERROR("Failed to read trophy type");
        return false;
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
    const auto TROPHY_CONF_PATH = TROPHY_PATH / "conf";

    gui.trophy_np_com_id_list_icons.clear();
    np_com_id_info.clear();
    np_com_id_list.clear();

    if (fs::exists(TROPHY_CONF_PATH) && !fs::is_empty(TROPHY_CONF_PATH)) {
        for (const auto &trophy : fs::directory_iterator(TROPHY_CONF_PATH)) {
            if (!trophy.path().empty() && fs::is_directory(trophy.path())) {
                const std::string np_com_id = trophy.path().stem().generic_string();

                const auto trophy_conf_np_com_id_path{ TROPHY_CONF_PATH / np_com_id };
                const auto trophy_data_np_com_id_path{ fs::path(TROPHY_PATH) / "data" / np_com_id };

                if (!fs::exists(trophy_data_np_com_id_path / "TROPUSR.DAT")) {
                    LOG_ERROR("Trophy Data progress not found for Np Com ID: {}, clean trophy file.", np_com_id);
                    fs::remove_all(trophy_data_np_com_id_path);
                    fs::remove_all(trophy_conf_np_com_id_path);
                    continue;
                }

                // Read Updated time
                const auto updated = fs::last_write_time(trophy_data_np_com_id_path / "TROPUSR.DAT");
                SAFE_LOCALTIME(&updated, &np_com_id_info[np_com_id].updated);

                // Open trophy progress file
                const auto trophy_progress_save_file = device::construct_normalized_path(VitaIoDevice::ux0, "user/" + emuenv.io.user_id + "/trophy/data/" + np_com_id + "/TROPUSR.DAT");
                const auto progress_input_file = open_file(emuenv.io, trophy_progress_save_file.c_str(), SCE_O_RDONLY, emuenv.pref_path, "load_trophy_progress_file");

                if (!load_trophy_progress(emuenv.io, progress_input_file, np_com_id)) {
                    LOG_ERROR("Failed to load trophy progress for Np Com ID: {}, cleaning the trophy file.", np_com_id);
                    close_file(emuenv.io, progress_input_file, "load_trophy_progress_file");
                    fs::remove_all(trophy_data_np_com_id_path);
                    fs::remove_all(trophy_conf_np_com_id_path);
                    continue;
                }

                close_file(emuenv.io, progress_input_file, "load_trophy_progress_file");

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
                    for (const auto &conf : trophy_conf) {
                        if (conf.name() == std::string("group")) {
                            const std::string group_id = conf.attribute("id").as_string();
                            np_com_id_info[np_com_id].group_id.push_back(group_id);
                            np_com_id_info[np_com_id].name[group_id] = conf.child("name").text().as_string();
                            np_com_id_info[np_com_id].detail[group_id] = conf.child("detail").text().as_string();
                            np_com_list_name_icons[group_id] = fs::exists(trophy_conf_np_com_id_path / fmt::format("GR{}_{:0>2d}.PNG", group_id, emuenv.cfg.sys_lang)) ? fmt::format("GR{}_{:0>2d}.PNG", group_id, emuenv.cfg.sys_lang) : fmt::format("GR{}.PNG", group_id);
                        }
                        if (conf.name() == std::string("trophy")) {
                            const std::string gid = conf.attribute("gid").empty() ? "000" : conf.attribute("gid").as_string();
                            const std::string trophy_id = conf.attribute("id").as_string();
                            np_com_id_info[np_com_id].trophy_id_by_group[gid].push_back(trophy_id);
                        }
                    }
                }

                np_com_id_info[np_com_id].trophy_count_by_group["global"] = np_com_id_info[np_com_id].context.trophy_count;
                np_com_id_info[np_com_id].unlocked_count["global"] = np_com_id_info[np_com_id].context.total_trophy_unlocked();

                std::map<std::string, std::map<SceNpTrophyGrade, uint32_t>> unlocked_type_count;
                auto trophy_index = 0;
                for (uint32_t gid = 0; gid < np_com_id_info[np_com_id].context.group_count + 1; gid++) {
                    if (np_com_id_info[np_com_id].context.trophy_count_by_group[gid]) {
                        const std::string group_id = fmt::format("{:0>3d}", gid);
                        np_com_id_info[np_com_id].trophy_count_by_group[group_id] = np_com_id_info[np_com_id].context.trophy_count_by_group[gid];
                        if (np_com_id_info[np_com_id].unlocked_count["global"]) {
                            for (uint32_t i = 0; i < np_com_id_info[np_com_id].trophy_count_by_group[group_id]; i++, trophy_index++) {
                                if (np_com_id_info[np_com_id].context.is_trophy_unlocked(trophy_index)) {
                                    ++unlocked_type_count[group_id][np_com_id_info[np_com_id].context.trophy_kinds[trophy_index]];
                                    ++unlocked_type_count["global"][np_com_id_info[np_com_id].context.trophy_kinds[trophy_index]];
                                    ++np_com_id_info[np_com_id].unlocked_count[group_id];
                                }
                            }
                        }
                        const auto plat_count = unlocked_type_count[group_id][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM];
                        const auto gold_count = unlocked_type_count[group_id][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD];
                        const auto silver_count = unlocked_type_count[group_id][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER];
                        const auto bronze_count = unlocked_type_count[group_id][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE];
                        np_com_id_info[np_com_id].unlocked_type_count[group_id]["detail"] = fmt::format("P:{}  G:{}  S:{}  B:{}", plat_count, gold_count, silver_count, bronze_count);
                        np_com_id_info[np_com_id].unlocked_type_count[group_id]["general"] = fmt::format("{}   {}   {}   {}", plat_count, gold_count, silver_count, bronze_count);
                        np_com_id_info[np_com_id].progress[group_id] = (np_com_id_info[np_com_id].unlocked_count[group_id] * 100) / np_com_id_info[np_com_id].trophy_count_by_group[group_id];
                    }
                }

                const auto plat_count = unlocked_type_count["global"][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM];
                const auto gold_count = unlocked_type_count["global"][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD];
                const auto silver_count = unlocked_type_count["global"][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER];
                const auto bronze_count = unlocked_type_count["global"][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE];
                np_com_id_info[np_com_id].unlocked_type_count["global"]["detail"] = fmt::format("P:{}  G:{}  S:{}  B:{}", plat_count, gold_count, silver_count, bronze_count);
                np_com_id_info[np_com_id].unlocked_type_count["global"]["general"] = fmt::format("{}   {}   {}   {}", plat_count, gold_count, silver_count, bronze_count);

                const auto progress = np_com_id_info[np_com_id].unlocked_count["global"] * 100 / np_com_id_info[np_com_id].context.trophy_count;
                np_com_id_info[np_com_id].progress["global"] = progress;

                np_com_id_list.push_back({ np_com_id, np_com_id_info[np_com_id].name["000"], progress, updated });

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

    const auto SIZE_LIST = ImVec2((!np_com_id_selected.empty() && group_id_selected.empty() ? 810.f : 790.f) * SCALE.x, 442.f * SCALE.y);
    const auto SIZE_INFO = ImVec2(820.f * SCALE.x, 484.f * SCALE.y);
    const auto POPUP_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);

    const auto TROPHY_PATH{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "trophy" };

    const auto BG_PATH = "vs0:app/NPXS10008";
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
    ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + (90.f * SCALE.x), WINDOW_POS.y + (!trophy_id_selected.empty() || detail_np_com_id ? 16.0f : 58.f) * SCALE.y), ImGuiCond_Always);
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
                ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
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
            ImGui::SetColumnWidth(1, 405.f * SCALE.x);
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
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                ImGui::ProgressBar(np_com.progress / 100.f, ImVec2(200 * SCALE.x, 15.f * SCALE.y), "");
                ImGui::PopStyleColor();
                ImGui::SameLine(0.f, (10.f * SCALE.x));
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (6.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", std::to_string(np_com.progress).append("%").c_str());
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                ImGui::Selectable(np_com_id_info[np_com.id].unlocked_type_count["global"]["general"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y));
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
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                    ImGui::ProgressBar(np_com_id_info[np_com_id_selected].progress[group_id] / 100.f, ImVec2(200 * SCALE.x, 15.f * SCALE.y), "");
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0.f, (10.f * SCALE.x));
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (6.f * SCALE.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", std::to_string(np_com_id_info[np_com_id_selected].progress[group_id]).append("%").c_str());
                    ImGui::NextColumn();
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                    ImGui::Selectable(np_com_id_info[np_com_id_selected].unlocked_type_count[group_id]["general"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y));
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
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", std::to_string(np_com_id_info[np_com_id_selected].progress[group_id_selected]).append("%").c_str());
                ImGui::SameLine(360 * SCALE.x);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (6.f * SCALE.y));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                ImGui::ProgressBar(np_com_id_info[np_com_id_selected].progress[group_id_selected] / 100.f, ImVec2(200 * SCALE.x, 15.f * SCALE.y), "");
                ImGui::PopStyleColor();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (30.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["trophies"].c_str());
                ImGui::SameLine(260.f * SCALE.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%d/%d\n%s", np_com_id_info[np_com_id_selected].unlocked_count[group_id_selected], np_com_id_info[np_com_id_selected].trophy_count_by_group[group_id_selected], np_com_id_info[np_com_id_selected].unlocked_type_count[group_id_selected]["detail"].c_str());
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
    if (ImGui::Button("<<", ImVec2(64.f * SCALE.x, 40.f * SCALE.y)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle))) {
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
        if (ImGui::Button("...", ImVec2(64.f * SCALE.x, 40.f * SCALE.y)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_triangle)))
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
                if (ImGui::MenuItem(gui.lang.indicator["delete_all"].c_str())) {
                    if (fs::remove_all(TROPHY_PATH))
                        LOG_INFO("All trophies successfully deleted.");
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
}

} // namespace gui
