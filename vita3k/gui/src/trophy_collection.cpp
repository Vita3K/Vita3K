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

#include <util/log.h>
#include <util/safe_time.h>

#include <io/device.h>
#include <io/functions.h>

#include <nfd.h>
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
        LOG_ERROR("Fail read stuff magic");
        return false;
    }

    // Read trophy progress
    std::fill(np_com_id_info[np_com_id].context.trophy_progress, np_com_id_info[np_com_id].context.trophy_progress + (MAX_TROPHIES >> 5), 0);
    if (read_trophy_progress_file(np_com_id_info[np_com_id].context.trophy_progress, sizeof(np_com_id_info[np_com_id].context.trophy_progress)) != sizeof(np_com_id_info[np_com_id].context.trophy_progress)) {
        LOG_ERROR("Fail read trophy progress");
        return false;
    }

    // Read trophy availability
    if (read_trophy_progress_file(np_com_id_info[np_com_id].context.trophy_availability, sizeof(np_com_id_info[np_com_id].context.trophy_availability)) != sizeof(np_com_id_info[np_com_id].context.trophy_availability)) {
        LOG_ERROR("Fail read trophy availability");
        return false;
    }

    // Read group count
    if (read_trophy_progress_file(&np_com_id_info[np_com_id].context.group_count, 4) != 4) {
        LOG_ERROR("Fail read count group id");
        return false;
    }

    // Read trophy count
    if (read_trophy_progress_file(&np_com_id_info[np_com_id].context.trophy_count, 4) != 4) {
        LOG_ERROR("Fail read trophy count");
        return false;
    }

    // Read platinum trophy ID
    if (read_trophy_progress_file(&np_com_id_info[np_com_id].context.platinum_trophy_id, 4) != 4) {
        LOG_ERROR("Fail read platinum trophy ID");
        return false;
    }

    // Read trophy count by group
    if (read_trophy_progress_file(np_com_id_info[np_com_id].context.trophy_count_by_group.data(), (uint32_t)np_com_id_info[np_com_id].context.trophy_count_by_group.size() * 4) != (int)np_com_id_info[np_com_id].context.trophy_count_by_group.size() * 4) {
        LOG_ERROR("Fail read trophy count by group");
        return false;
    }

    // Read trophy timestamps
    if (read_trophy_progress_file(np_com_id_info[np_com_id].context.unlock_timestamps.data(), (uint32_t)np_com_id_info[np_com_id].context.unlock_timestamps.size() * 8) != (int)np_com_id_info[np_com_id].context.unlock_timestamps.size() * 8) {
        LOG_ERROR("Fail read unlock_timestamp");
        return false;
    }

    // Read trophy type
    if (read_trophy_progress_file(np_com_id_info[np_com_id].context.trophy_kinds.data(), (uint32_t)np_com_id_info[np_com_id].context.trophy_kinds.size() * 4) != (int)np_com_id_info[np_com_id].context.trophy_kinds.size() * 4) {
        LOG_ERROR("Fail read trophy type");
        return false;
    }

    return true;
}

static std::string np_com_id_sort;

void init_trophy_collection(GuiState &gui, HostState &host) {
    const auto TROPHY_PATH{ fs::path(host.pref_path) / "ux0/user" / host.io.user_id / "trophy" };
    const auto TROPHY_CONF_PATH = TROPHY_PATH / "conf";

    gui.trophy_np_com_id_list_icons.clear(), np_com_id_info.clear(), np_com_id_list.clear();

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
                const auto trophy_progress_save_file = device::construct_normalized_path(VitaIoDevice::ux0, "user/" + host.io.user_id + "/trophy/data/" + np_com_id + "/TROPUSR.DAT");
                const auto progress_input_file = open_file(host.io, trophy_progress_save_file.c_str(), SCE_O_RDONLY, host.pref_path, "load_trophy_progress_file");

                if (!load_trophy_progress(host.io, progress_input_file, np_com_id)) {
                    LOG_ERROR("Fail load trophy progress for Np Com ID: {}, clean trophy file.", np_com_id);
                    close_file(host.io, progress_input_file, "load_trophy_progress_file");
                    fs::remove_all(trophy_data_np_com_id_path);
                    fs::remove_all(trophy_conf_np_com_id_path);
                    continue;
                }

                close_file(host.io, progress_input_file, "load_trophy_progress_file");

                const std::string sfm_name = fs::exists(trophy_conf_np_com_id_path / fmt::format("TROP_{:0>2d}.SFM", host.cfg.sys_lang)) ? fmt::format("TROP_{:0>2d}.SFM", host.cfg.sys_lang) : "TROP.SFM";

                std::map<std::string, std::string> np_com_list_name_icons;
                np_com_list_name_icons["000"] = fs::exists(trophy_conf_np_com_id_path / fmt::format("ICON0_{:0>2d}.PNG", host.cfg.sys_lang)) ? fmt::format("ICON0_{:0>2d}.PNG", host.cfg.sys_lang) : "ICON0.PNG";

                pugi::xml_document doc;
                if (doc.load_file((trophy_conf_np_com_id_path / sfm_name).c_str())) {
                    const auto trophy_conf = doc.child("trophyconf");
                    if (np_com_id_info[np_com_id].context.trophy_count_by_group[0])
                        np_com_id_info[np_com_id].group_id.push_back("000");
                    np_com_id_info[np_com_id].name["000"] = trophy_conf.child("title-name").text().as_string();
                    np_com_id_info[np_com_id].detail["000"] = trophy_conf.child("title-detail").text().as_string();
                    for (const auto &conf : trophy_conf) {
                        if (conf.name() == std::string("group")) {
                            const std::string group_id = conf.attribute("id").as_string();
                            np_com_id_info[np_com_id].group_id.push_back(group_id);
                            np_com_id_info[np_com_id].name[group_id] = conf.child("name").text().as_string();
                            np_com_id_info[np_com_id].detail[group_id] = conf.child("detail").text().as_string();
                            np_com_list_name_icons[group_id] = fs::exists(trophy_conf_np_com_id_path / fmt::format("GR{}_{:0>2d}.PNG", group_id, host.cfg.sys_lang)) ? fmt::format("GR{}_{:0>2d}.PNG", group_id, host.cfg.sys_lang) : fmt::format("GR{}.PNG", group_id);
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
                                    ++unlocked_type_count[group_id][(SceNpTrophyGrade)np_com_id_info[np_com_id].context.trophy_kinds[trophy_index]];
                                    ++unlocked_type_count["global"][(SceNpTrophyGrade)np_com_id_info[np_com_id].context.trophy_kinds[trophy_index]];
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

                    vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, "user/" + host.io.user_id + "/trophy/conf/" + np_com_id + "/" + group.second);

                    if (buffer.empty()) {
                        LOG_WARN("Icon: '{}', Not found for NPComId: {}.", group.second, np_com_id);
                        continue;
                    }

                    stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                    if (!data) {
                        LOG_ERROR("Invalid icon: '{}' for NPComId: {}.", group.second, np_com_id);
                        continue;
                    }

                    gui.trophy_np_com_id_list_icons[np_com_id][group.first].init(gui.imgui_state.get(), data, width, height);
                    stbi_image_free(data);
                }
            }
        }
    }

    np_com_id_sort = "updated";
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
    uint32_t grade;
    std::string name;
};

static std::map<std::string, Trophy> trophy_info;
static std::vector<TrophySort> trophy_list;
static std::string trophy_sort;

static void get_trophy_list(GuiState &gui, HostState &host, const std::string &np_com_id, const std::string &group_id) {
    const auto trophy_conf_id_path{ fs::path(host.pref_path) / "ux0/user" / host.io.user_id / "trophy/conf" / np_com_id };
    if (fs::is_empty(trophy_conf_id_path)) {
        LOG_WARN("Trophy conf path is empty");
        return;
    }

    gui.trophy_list.clear(), trophy_info.clear(), trophy_list.clear();

    const std::string sfm_name = fs::exists(trophy_conf_id_path / fmt::format("TROP_{:0>2d}.SFM", host.cfg.sys_lang)) ? fmt::format("TROP_{:0>2d}.SFM", host.cfg.sys_lang) : "TROP.SFM";

    pugi::xml_document doc;
    if (doc.load_file((trophy_conf_id_path / sfm_name).c_str())) {
        std::string type;
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

    for (const auto &trophy : trophy_info) {
        int32_t width = 0;
        int32_t height = 0;
        vfs::FileBuffer buffer;
        const std::string trophy_id = trophy.first;
        const std::string icon_name = fmt::format("TROP{}.PNG", trophy_id);

        vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, "user/" + host.io.user_id + "/trophy/conf/" + np_com_id + "/" + icon_name);
        if (buffer.empty()) {
            LOG_WARN("Trophy icon, Name: '{}', Not found for trophy id: {}.", icon_name, trophy.first);
            continue;
        }
        stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data) {
            LOG_ERROR("Invalid trophy icon for trophy id {} [{}].", icon_name, trophy_id);
            continue;
        }

        gui.trophy_list[trophy_id].init(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);

        auto common = gui.lang.common.main;
        const auto trophy_type = np_com_id_info[np_com_id].context.trophy_kinds[std::stoi(trophy_id)];
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

        const auto unlocked = (time_t)np_com_id_info[np_com_id].context.unlock_timestamps[std::stoi(trophy_id)];
        // Check earned trophy and set time
        if (np_com_id_info[np_com_id].context.is_trophy_unlocked(std::stoi(trophy.first))) {
            trophy_info[trophy_id].earned = true;
            SAFE_LOCALTIME(&unlocked, &trophy_info[trophy_id].unlocked_time);
        }

        const auto grade = uint32_t(np_com_id_info[np_com_id].context.trophy_kinds[std::stoi(trophy.first)]);

        trophy_list.push_back({ trophy_id, unlocked, grade, trophy_info[trophy_id].name });

        trophy_sort = "original";
    }
}

static std::string np_com_id_selected, group_id_selected, trophy_id_selected, delete_np_com_id, scroll_type;
static bool detail_np_com_id, set_scroll_pos;
static ImGuiTextFilter search_bar;
static std::map<std::string, float> scroll_pos;

void open_trophy_unlocked(GuiState &gui, HostState &host, const std::string &np_com_id, const std::string &trophy_id) {
    np_com_id_selected = np_com_id;

    // Get group id corresponding trophy_id
    for (const auto &group : np_com_id_info[np_com_id].trophy_id_by_group) {
        if (std::any_of(std::begin(group.second), std::end(group.second), [&trophy_id](const auto &trophy) { return trophy == trophy_id; })) {
            group_id_selected = group.first;
            break;
        }
    }

    trophy_id_selected = trophy_id;
    get_trophy_list(gui, host, np_com_id_selected, group_id_selected);
}

void draw_trophy_collection(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);
    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;

    const auto SIZE_ICON_LIST = ImVec2(160.f * SCALE.x, 88.f * SCALE.y);
    const auto SIZE_TROPHY_LIST = ImVec2(80.f * SCALE.x, 80.f * SCALE.y);

    const auto BUTTON_SIZE = ImVec2(310.f * SCALE.x, 46.f * SCALE.y);

    const auto WINDOW_SIZE = ImVec2(display_size.x, display_size.y - INFORMATION_BAR_HEIGHT);
    const auto SIZE_LIST = ImVec2((!np_com_id_selected.empty() && group_id_selected.empty() ? 810.f : 790.f) * SCALE.x, 442.f * SCALE.y);
    const auto SIZE_INFO = ImVec2(820.f * SCALE.x, 484.f * SCALE.y);
    const auto POPUP_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);

    const auto TROPHY_PATH{ fs::path(host.pref_path) / "ux0/user" / host.io.user_id / "trophy" };

    const auto is_background = gui.apps_background.find("NPXS10008") != gui.apps_background.end();

    ImGui::SetNextWindowPos(ImVec2(0, INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##trophy_collection", &gui.live_area.trophy_collection, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    if (is_background)
        ImGui::GetBackgroundDrawList()->AddImage(gui.apps_background["NPXS10008"], ImVec2(0.f, 0.f), display_size);
    else
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, 0.f), display_size, IM_COL32(31.f, 12.f, 0.f, 255.f), 0.f, ImDrawCornerFlags_All);

    if (group_id_selected.empty()) {
        ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
        if (!np_com_id_list.empty() && np_com_id_selected.empty()) {
            ImGui::TextColored(GUI_COLOR_TEXT, "Search");
            ImGui::SameLine();
            search_bar.Draw("##search_bar", 200 * SCALE.x);
        }
        ImGui::SetCursorPos(ImVec2(display_size.x - (285.f * SCALE.x), (15.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, "P   G   S   B");
        ImGui::SetCursorPosY(54.f * SCALE.y);
        ImGui::Separator();
        ImGui::SetWindowFontScale(1.f);
    }
    ImGui::SetNextWindowPos(ImVec2(90.f * SCALE.x, (!trophy_id_selected.empty() || detail_np_com_id ? 48.0f : 90.f) * SCALE.y), ImGuiCond_Always, ImVec2(0.f, 0.f));
    ImGui::BeginChild("##trophy_collection_child", !trophy_id_selected.empty() || detail_np_com_id ? SIZE_INFO : SIZE_LIST, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    auto lang = gui.lang.trophy_collection;

    // Trophy Collection
    if (np_com_id_list.empty()) {
        ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
        ImGui::SetCursorPosY(140.f * SCALE.y);
        const auto no_trophy_string = lang["no_trophies"].c_str();
        ImGui::TextWrapped("%s", no_trophy_string);
    } else {
        // Set Scroll Pos
        if (set_scroll_pos) {
            ImGui::SetScrollY(scroll_pos[scroll_type]);
            set_scroll_pos = false;
        }

        auto common = gui.lang.common.main;
        const auto hidden_trophy_str = common["hidden_trophy"].c_str();

        if (np_com_id_selected.empty()) {
            // Ask Delete Trophy Popup
            if (!delete_np_com_id.empty()) {
                ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
                ImGui::Begin("##trophy_delete", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                ImGui::SetNextWindowBgAlpha(0.999f);
                ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
                ImGui::BeginChild("##trophy_delete_child", POPUP_SIZE, true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
                ImGui::SetCursorPos(ImVec2(48.f * SCALE.x, 28.f * SCALE.y));
                if (gui.trophy_np_com_id_list_icons[delete_np_com_id].find("000") != gui.trophy_np_com_id_list_icons[delete_np_com_id].end())
                    ImGui::Image(gui.trophy_np_com_id_list_icons[delete_np_com_id]["000"], SIZE_ICON_LIST);
                ImGui::SameLine();
                ImGui::SetWindowFontScale(1.5f * RES_SCALE.x);
                const auto CALC_TITLE = ImGui::CalcTextSize(np_com_id_info[delete_np_com_id].name["000"].c_str(), nullptr, false, POPUP_SIZE.x - SIZE_ICON_LIST.x - 48.f).y / 2.f;
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (SIZE_ICON_LIST.y / 2.f) - CALC_TITLE);
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + POPUP_SIZE.x - SIZE_ICON_LIST.x - (55.f * SCALE.x));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", np_com_id_info[delete_np_com_id].name["000"].c_str());
                ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
                ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - (ImGui::CalcTextSize("This trophy information saved on this user will be deleted.").x / 2.f), POPUP_SIZE.y / 2.f));
                ImGui::TextColored(GUI_COLOR_TEXT, "This trophy information saved on this user will be deleted.");
                ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), POPUP_SIZE.y - BUTTON_SIZE.y - (22.0f * SCALE.y)));
                const auto cancel_str = !host.common_dialog.lang.common["cancel"].empty() ? host.common_dialog.lang.common["cancel"].c_str() : "Cancel";
                if (ImGui::Button(cancel_str, BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle))
                    delete_np_com_id.clear();
                ImGui::SameLine(0, 20.f * SCALE.x);
                if (ImGui::Button("OK", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
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
                if (gui.trophy_np_com_id_list_icons[np_com.id].find("000") != gui.trophy_np_com_id_list_icons[np_com.id].end())
                    ImGui::Image(gui.trophy_np_com_id_list_icons[np_com.id]["000"], SIZE_ICON_LIST);
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.3f * RES_SCALE.x);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.2f));
                const auto Title_POS = ImGui::GetCursorPosY();
                if (ImGui::Selectable(np_com.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y))) {
                    np_com_id_selected = np_com.id;
                    if (!np_com_id_info[np_com_id_selected].context.group_count) {
                        group_id_selected = "000";
                        get_trophy_list(gui, host, np_com.id, "000");
                    }
                    scroll_pos["np_com"] = ImGui::GetScrollY();
                    ImGui::SetScrollY(0.f);
                }
                ImGui::PopStyleVar();

                // Trophy Context Menu
                if (ImGui::BeginPopupContextItem("##trophy_context_menu")) {
                    if (ImGui::MenuItem("Delete Trophy"))
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
                if (gui.trophy_np_com_id_list_icons[np_com_id_selected].find("000") != gui.trophy_np_com_id_list_icons[np_com_id_selected].end())
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
                    if (gui.trophy_np_com_id_list_icons[np_com_id_selected].find(group_id) != gui.trophy_np_com_id_list_icons[np_com_id_selected].end())
                        ImGui::Image(gui.trophy_np_com_id_list_icons[np_com_id_selected][group_id], SIZE_ICON_LIST);
                    ImGui::NextColumn();
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.2f));
                    const auto Title_POS = ImGui::GetCursorPosY();
                    ImGui::PushID(group_id.c_str());
                    if (ImGui::Selectable(np_com_id_info[np_com_id_selected].name[group_id].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y))) {
                        group_id_selected = group_id;
                        get_trophy_list(gui, host, np_com_id_selected, group_id);
                        scroll_pos["group"] = ImGui::GetScrollY();
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
                if (gui.trophy_np_com_id_list_icons[np_com_id_selected].find(group_id_selected == "global" ? "000" : group_id_selected) != gui.trophy_np_com_id_list_icons[np_com_id_selected].end())
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
                auto DATE_TIME = get_date_time(gui, host, np_com_id_info[np_com_id_selected].updated);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", DATE_TIME[DateTime::DATE_MINI].c_str(), DATE_TIME[DateTime::CLOCK].c_str());
                if (gui.users[host.io.user_id].clock_12_hour) {
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
                if (gui.trophy_np_com_id_list_icons[np_com_id_selected].find(group_id_selected) != gui.trophy_np_com_id_list_icons[np_com_id_selected].end())
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
                        if (ImGui::Selectable("Locked", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_TROPHY_LIST.y))) {
                            trophy_id_selected = trophy.id;
                            scroll_pos["trophy"] = ImGui::GetScrollY();
                        }
                        ImGui::PopID();
                        ImGui::PopStyleVar();
                    }
                    ImGui::NextColumn();
                    const auto hidden_trophy = (!trophy_info[trophy.id].earned && (trophy_info[trophy.id].hidden == "yes"));
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (14.f * SCALE.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", hidden_trophy ? "?" : trophy_info[trophy.id].type["general"].c_str());
                    ImGui::NextColumn();
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.2f));
                    const auto name_pos = ImGui::GetCursorPosY();
                    if (ImGui::Selectable(hidden_trophy ? hidden_trophy_str : trophy.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_TROPHY_LIST.y)))
                        trophy_id_selected = trophy.id;
                    ImGui::PopStyleVar();
                    if (!hidden_trophy) {
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
                    ImGui::TextColored(GUI_COLOR_TEXT, "Lock");
                }
                const auto hidden_trophy = (!trophy_info[trophy_id_selected].earned && (trophy_info[trophy_id_selected].hidden == "yes"));
                const auto CALC_NAME = ImGui::CalcTextSize(hidden_trophy ? hidden_trophy_str : trophy_info[trophy_id_selected].name.c_str(), nullptr, false, SIZE_INFO.x - SIZE_TROPHY_LIST.x - 48.f).y / 2.f;
                ImGui::SetCursorPos(ImVec2(SIZE_TROPHY_LIST.x + 20.f, (SIZE_TROPHY_LIST.y / 2.f) - CALC_NAME));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_INFO.x - SIZE_TROPHY_LIST.x - (48.f * SCALE.x));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", hidden_trophy ? hidden_trophy_str : trophy_info[trophy_id_selected].name.c_str());
                ImGui::PopTextWrapPos();
                ImGui::SetCursorPosY(SIZE_TROPHY_LIST.y + (25.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["grade"].c_str());
                ImGui::SameLine(250.f * SCALE.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", hidden_trophy ? "?" : trophy_info[trophy_id_selected].type["detail"].c_str());
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (25.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["earned"].c_str());
                ImGui::SameLine(250.f * SCALE.x);
                if (trophy_info[trophy_id_selected].earned) {
                    auto DATE_TIME = get_date_time(gui, host, trophy_info[trophy_id_selected].unlocked_time);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", DATE_TIME[DateTime::DATE_MINI].c_str(), DATE_TIME[DateTime::CLOCK].c_str());
                    if (gui.users[host.io.user_id].clock_12_hour) {
                        ImGui::SameLine();
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", DATE_TIME[DateTime::DAY_MOMENT].c_str());
                    }
                } else
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["not_earned"].c_str());
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (25.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["details"].c_str());
                ImGui::SameLine(250.f * SCALE.x);
                ImGui::PushTextWrapPos(SIZE_INFO.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", hidden_trophy ? "-" : trophy_info[trophy_id_selected].detail.c_str());
                ImGui::PopTextWrapPos();
            }
        }
    }
    ImGui::EndChild();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
    ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
    ImGui::SetCursorPos(ImVec2(8.f * SCALE.x, display_size.y - (84.f * SCALE.y)));
    if (ImGui::Button("Back", ImVec2(64.f * SCALE.x, 40.f * SCALE.y))) {
        if (!np_com_id_selected.empty()) {
            if (detail_np_com_id) {
                detail_np_com_id = false;
                if (group_id_selected == "global")
                    group_id_selected.clear();
            } else if (!trophy_id_selected.empty()) {
                trophy_id_selected.clear();
                scroll_type = "trophy";
                set_scroll_pos = true;
            } else if (!group_id_selected.empty()) {
                group_id_selected.clear();
                if (!np_com_id_info[np_com_id_selected].context.group_count) {
                    np_com_id_selected.clear();
                    scroll_type = "np_com";
                } else
                    scroll_type = "group";
                set_scroll_pos = true;
            } else {
                np_com_id_selected.clear();
                scroll_type = "np_com";
                set_scroll_pos = true;
            }
        } else {
            if (!gui.apps_list_opened.empty() && (gui.current_app_selected >= 0))
                gui.live_area.live_area_screen = true;
            else
                gui.live_area.app_selector = true;
            gui.live_area.trophy_collection = false;
        }
    }
    if (trophy_id_selected.empty() && !detail_np_com_id && (np_com_id_selected.empty() || !group_id_selected.empty())) {
        ImGui::SetCursorPos(ImVec2(display_size.x - (70.f * SCALE.x), display_size.y - (84.f * SCALE.y)));
        if (ImGui::Button("...", ImVec2(64.f * SCALE.x, 40.f * SCALE.y)) || ImGui::IsKeyPressed(host.cfg.keyboard_button_triangle))
            ImGui::OpenPopup("...");
        if (ImGui::BeginPopup("...", ImGuiWindowFlags_NoMove)) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["sort"].c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            const auto NAME = lang["name"].c_str();
            if (np_com_id_selected.empty()) {
                if (ImGui::MenuItem(lang["updated"].c_str(), nullptr, np_com_id_sort == "updated")) {
                    std::sort(np_com_id_list.begin(), np_com_id_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.updated > tb.updated;
                    });
                    np_com_id_sort = "updated";
                }
                if (ImGui::MenuItem(NAME, nullptr, np_com_id_sort == "name")) {
                    std::sort(np_com_id_list.begin(), np_com_id_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.name < tb.name;
                    });
                    np_com_id_sort = "name";
                }
                if (ImGui::MenuItem(lang["progress"].c_str(), nullptr, np_com_id_sort == "progress")) {
                    std::sort(np_com_id_list.begin(), np_com_id_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.progress > tb.progress;
                    });
                    np_com_id_sort = "progress";
                }
            } else {
                if (ImGui::MenuItem(lang["original"].c_str(), nullptr, trophy_sort == "original")) {
                    std::sort(trophy_list.begin(), trophy_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.id < tb.id;
                    });
                    trophy_sort = "original";
                }
                if (ImGui::MenuItem(lang["earned"].c_str(), nullptr, trophy_sort == "earned")) {
                    std::sort(trophy_list.begin(), trophy_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.earned > tb.earned;
                    });
                    trophy_sort = "earned";
                }
                if (ImGui::MenuItem(lang["grade"].c_str(), nullptr, trophy_sort == "grade")) {
                    std::sort(trophy_list.begin(), trophy_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.grade < tb.grade;
                    });
                    trophy_sort = "grade";
                }
                if (ImGui::MenuItem(NAME, nullptr, trophy_sort == "name")) {
                    std::sort(trophy_list.begin(), trophy_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.name < tb.name;
                    });
                    trophy_sort = "name";
                }
            }
            ImGui::EndPopup();
        }
    }
    ImGui::SetWindowFontScale(1.f);
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
