// Vita3K emulator project
// Copyright (C) 2020 Vita3K team
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
    std::string updated;
    std::map<std::string, uint32_t> unlocked_count;
    std::map<std::string, uint32_t> progress;
    std::map<std::string, std::map<std::string, std::string>> unlocked_type_count;

    Context context;
};

struct NPComIdSort {
    std::string np_com_id;
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

void get_trophy_np_com_id_list(GuiState &gui, HostState &host) {
    host.io.user_id = fmt::format("{:0>2d}", host.cfg.user_id);

    const auto TROPHY_PATH{ fs::path(host.pref_path) / "ux0/user" / host.io.user_id / "trophy" };
    const auto TROPHY_CONF_PATH = TROPHY_PATH / "conf";

    gui.trophy_np_com_id_list.clear(), np_com_id_info.clear(), np_com_id_list.clear();

    std::map<std::string, std::map<std::string, std::string>> np_com_id_list_name;
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
                const auto updated_date = std::localtime(&updated);
                np_com_id_info[np_com_id].updated = fmt::format("{}/{}/{}  {}:{:0>2d}", updated_date->tm_mday, updated_date->tm_mon + 1, updated_date->tm_year + 1900, updated_date->tm_hour, updated_date->tm_min).c_str();

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

                np_com_id_list_name[np_com_id]["000"] = fs::exists(trophy_conf_np_com_id_path / fmt::format("ICON0_{:0>2d}.PNG", host.cfg.sys_lang)) ? fmt::format("ICON0_{:0>2d}.PNG", host.cfg.sys_lang) : "ICON0.PNG";

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
                            np_com_id_list_name[np_com_id][group_id] = fmt::format("GR{}.PNG", group_id);
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
                        np_com_id_info[np_com_id].unlocked_type_count[group_id]["general"] = fmt::format("{}    {}    {}    {}", plat_count, gold_count, silver_count, bronze_count);
                        np_com_id_info[np_com_id].progress[group_id] = (np_com_id_info[np_com_id].unlocked_count[group_id] * 100) / np_com_id_info[np_com_id].trophy_count_by_group[group_id];
                    }
                }

                const auto plat_count = unlocked_type_count["global"][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM];
                const auto gold_count = unlocked_type_count["global"][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD];
                const auto silver_count = unlocked_type_count["global"][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER];
                const auto bronze_count = unlocked_type_count["global"][SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE];
                np_com_id_info[np_com_id].unlocked_type_count["global"]["detail"] = fmt::format("P:{}  G:{}  S:{}  B:{}", plat_count, gold_count, silver_count, bronze_count);
                np_com_id_info[np_com_id].unlocked_type_count["global"]["general"] = fmt::format("{}    {}    {}    {}", plat_count, gold_count, silver_count, bronze_count);

                const auto progress = np_com_id_info[np_com_id].unlocked_count["global"] * 100 / np_com_id_info[np_com_id].context.trophy_count;
                np_com_id_info[np_com_id].progress["global"] = progress;

                np_com_id_list.push_back({ np_com_id, np_com_id_info[np_com_id].name["000"], progress, updated });
            }
        }
    }

    const auto trophy_app_bg_path{ fs::path(host.pref_path) / "vs0/app/NPXS10008/sce_sys/pic0.png" };
    if (fs::exists(trophy_app_bg_path))
        np_com_id_list_name["bg"]["000"] = "pic0.png";

    for (const auto &np_com_id : np_com_id_list_name) {
        for (const auto &group : np_com_id.second) {
            int32_t width = 0;
            int32_t height = 0;
            vfs::FileBuffer buffer;

            if (np_com_id.first == "bg")
                vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, "app/NPXS10008/sce_sys/" + group.second);
            else
                vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, "user/" + host.io.user_id + "/trophy/conf/" + np_com_id.first + "/" + group.second);

            if (buffer.empty()) {
                LOG_WARN("Icon, Name: '{}', Not found for NPComId: {}.", group.second, np_com_id.first);
                continue;
            }
            stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
            if (!data) {
                LOG_ERROR("Invalid icon for title: {} [{}].", group.second, np_com_id.first);
                continue;
            }

            gui.trophy_np_com_id_list[np_com_id.first][group.first].init(gui.imgui_state.get(), data, width, height);
            stbi_image_free(data);
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
    std::string unlocked_time;
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

        vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, "user/00/trophy/conf/" + np_com_id + "/" + icon_name);
        if (buffer.empty()) {
            LOG_WARN("Trophy icon, Name: '{}', Not found for trophy id: {}.", icon_name, trophy.first);
            continue;
        }
        stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data) {
            LOG_ERROR("Invalid trophy icon for trophy id {} [{}].", icon_name, trophy.first);
            continue;
        }

        gui.trophy_list[trophy.first].init(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);

        const auto trophy_type = np_com_id_info[np_com_id].context.trophy_kinds[std::stoi(trophy_id)];
        switch (trophy_type) {
        case SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM:
            trophy_info[trophy_id].type["detail"] = "Platinum";
            break;
        case SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD:
            trophy_info[trophy_id].type["detail"] = "Gold";
            break;
        case SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER:
            trophy_info[trophy_id].type["detail"] = "Silver";
            break;
        case SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE:
            trophy_info[trophy_id].type["detail"] = "Bronze";
            break;
        default:
            LOG_ERROR("Trophy id {} unknown type: {}", trophy_id, (SceInt32)trophy_type);
            break;
        }

        const auto unlocked = (time_t)np_com_id_info[np_com_id].context.unlock_timestamps[std::stoi(trophy_id)];
        // Check earned trophy and set time
        if (np_com_id_info[np_com_id].context.is_trophy_unlocked(std::stoi(trophy.first))) {
            trophy_info[trophy_id].earned = true;
            const auto unlocked_date = std::localtime(&unlocked);
            trophy_info[trophy_id].unlocked_time = fmt::format("{}/{}/{}  {}:{:0>2d}", unlocked_date->tm_mday, unlocked_date->tm_mon + 1, unlocked_date->tm_year + 1900, unlocked_date->tm_hour, unlocked_date->tm_min);
        }

        const auto grade = uint32_t(np_com_id_info[np_com_id].context.trophy_kinds[std::stoi(trophy.first)]);

        trophy_list.push_back({ trophy_id, unlocked, grade, trophy_info[trophy_id].name });

        trophy_sort = "original";
    }
}

static std::string np_com_id_selected, np_com_id_hovered, group_id_selected, trophy_id_selected, delete_np_com_id, scroll_type;
static bool detail_np_com_id, delete_trophy, set_scroll_pos;
static ImGuiTextFilter search_bar;
static std::map<std::string, float> scroll_pos;

void draw_trophy_collection(GuiState &gui, HostState &host) {
    draw_information_bar(gui);

    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto SCAL = ImVec2(display_size.x / 960.0f, display_size.y / 544.0f);
    const auto MENUBAR_HEIGHT = 32.f * SCAL.y;

    const auto SIZE_ICON_LIST = ImVec2(160.f * SCAL.x, 88.f * SCAL.y);
    const auto SIZE_TROPHY_LIST = ImVec2(80.f * SCAL.x, 80.f * SCAL.y);

    const auto BUTTON_SIZE = ImVec2(310.f * SCAL.x, 46.f * SCAL.y);

    const auto WINDOW_SIZE = ImVec2(display_size.x, display_size.y - MENUBAR_HEIGHT);
    const auto SIZE_LIST = ImVec2(780 * SCAL.x, 442.f * SCAL.y);
    const auto SIZE_INFO = ImVec2(780 * SCAL.x, 484.f * SCAL.y);
    const auto POPUP_SIZE = ImVec2(756.0f * SCAL.x, 436.0f * SCAL.y);

    const auto TROPHY_PATH{ fs::path(host.pref_path) / "ux0/user" / host.io.user_id / "trophy" };
    const char progress_dummy[32] = "";

    ImGui::SetNextWindowPos(ImVec2(0, MENUBAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    if (gui.trophy_np_com_id_list["bg"].find("000") != gui.trophy_np_com_id_list["bg"].end())
        ImGui::SetNextWindowBgAlpha(0.999f);
    ImGui::Begin("##trophy_collection", &gui.theme.theme_background, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    if (gui.trophy_np_com_id_list.find("bg") != gui.trophy_np_com_id_list.end())
        ImGui::GetWindowDrawList()->AddImage(gui.trophy_np_com_id_list["bg"]["000"], ImVec2(0.f, MENUBAR_HEIGHT), display_size);
    if (group_id_selected.empty()) {
        ImGui::SetWindowFontScale(1.4f * SCAL.x);
        if (np_com_id_selected.empty()) {
            ImGui::TextColored(GUI_COLOR_TEXT, "Search");
            ImGui::SameLine();
            search_bar.Draw("##search_bar", 200 * SCAL.x);
        }
        ImGui::SetCursorPos(ImVec2(display_size.x - (305.f * SCAL.x), (15.f * SCAL.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, "P    G    S    B");
        ImGui::SetCursorPosY(54.f * SCAL.y);
        ImGui::Separator();
        ImGui::SetWindowFontScale(1.f);
    }
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, (!trophy_id_selected.empty() || detail_np_com_id ? 48.0f : 90.f) * SCAL.y), ImGuiCond_Always, ImVec2(0.5f, 0.f));
    ImGui::BeginChild("##trophy_collection_child", !trophy_id_selected.empty() || detail_np_com_id ? SIZE_INFO : SIZE_LIST, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    // Trophy Collection
    if (np_com_id_list.empty()) {
        ImGui::SetWindowFontScale(1.6f * SCAL.x);
        ImGui::SetCursorPosY(120.f * SCAL.y);
        ImGui::PushTextWrapPos(SIZE_LIST.x);
        ImGui::TextColored(GUI_COLOR_TEXT, "There are no trophies for this user.\nYou can earn trophies by using an application that supports the trophy feature.");
        ImGui::PopTextWrapPos();
    } else {
        // Set Scroll Pos
        if (set_scroll_pos) {
            ImGui::SetScrollY(scroll_pos[scroll_type]);
            set_scroll_pos = false;
        }

        // Delete Trophy
        if (!delete_np_com_id.empty()) {
            const auto np_com_id_index = std::find_if(np_com_id_list.begin(), np_com_id_list.end(), [&](const NPComIdSort &np) {
                return np.np_com_id == delete_np_com_id;
            });
            np_com_id_list.erase(np_com_id_index);
            np_com_id_info.erase(delete_np_com_id);
            gui.trophy_np_com_id_list.erase(delete_np_com_id);
            np_com_id_hovered.clear();
            delete_np_com_id.clear();
        }

        // Select Np Com ID
        if (np_com_id_selected.empty()) {
            ImGui::Columns(3, nullptr, false);
            ImGui::SetColumnWidth(0, SIZE_ICON_LIST.x + (10.f * SCAL.x));
            ImGui::SetColumnWidth(1, 385.f * SCAL.x);
            for (const auto &np_com : np_com_id_list) {
                if (!search_bar.PassFilter(np_com.name.c_str()))
                    continue;
                if (gui.trophy_np_com_id_list.find(np_com.np_com_id) != gui.trophy_np_com_id_list.end())
                    ImGui::Image(gui.trophy_np_com_id_list[np_com.np_com_id]["000"], SIZE_ICON_LIST);
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.3f * SCAL.x);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.2f));
                const auto Title_POS = ImGui::GetCursorPosY();
                if (ImGui::Selectable(np_com.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y))) {
                    np_com_id_selected = np_com.np_com_id;
                    if (!np_com_id_info[np_com_id_selected].context.group_count) {
                        group_id_selected = "000";
                        get_trophy_list(gui, host, np_com.np_com_id, "000");
                    }
                    scroll_pos["np_com"] = ImGui::GetScrollY();
                    ImGui::SetScrollY(0.f);
                }
                ImGui::PopStyleVar();
                if (ImGui::IsItemHovered())
                    np_com_id_hovered = np_com.np_com_id;
                // Trophy Context Menu
                if (np_com_id_hovered == np_com.np_com_id) {
                    if (ImGui::BeginPopupContextItem("##trophy_context_menu")) {
                        ImGui::MenuItem("Detete Trophy", nullptr, &delete_trophy);
                        ImGui::EndPopup();
                    }
                }
                ImGui::SetCursorPosY(Title_POS + (50.f * SCAL.y));
                ImGui::SetWindowFontScale(1.f * SCAL.x);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                ImGui::ProgressBar(np_com.progress / 100.f, ImVec2(200 * SCAL.x, 15.f * SCAL.y), progress_dummy);
                ImGui::PopStyleColor();
                ImGui::SameLine(0.f, (10.f * SCAL.x));
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (6.f * SCAL.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", std::to_string(np_com.progress).append("%").c_str());
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.4f * SCAL.x);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                ImGui::Selectable(np_com_id_info[np_com.np_com_id].unlocked_type_count["global"]["general"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y));
                ImGui::PopStyleVar();
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
            // Ask Delete Trophy Popup
            if (delete_trophy) {
                ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
                ImGui::Begin("##trophy_delete", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                ImGui::SetNextWindowBgAlpha(0.999f);
                ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f);
                ImGui::BeginChild("##trophy_delete_child", POPUP_SIZE, true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
                ImGui::SetCursorPos(ImVec2(48.f * SCAL.x, 28.f * SCAL.y));
                ImGui::Image(gui.trophy_np_com_id_list[np_com_id_hovered]["000"], SIZE_ICON_LIST);
                ImGui::SameLine();
                ImGui::SetWindowFontScale(1.5f * SCAL.x);
                const auto CALC_TITLE = ImGui::CalcTextSize(np_com_id_info[np_com_id_hovered].name["000"].c_str(), nullptr, false, POPUP_SIZE.x - SIZE_ICON_LIST.x - 48.f).y / 2.f;
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (SIZE_ICON_LIST.y / 2.f) - CALC_TITLE);
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + POPUP_SIZE.x - SIZE_ICON_LIST.x - (55.f * SCAL.x));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", np_com_id_info[np_com_id_hovered].name["000"].c_str());
                ImGui::SetWindowFontScale(1.2f * SCAL.x);
                ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - (ImGui::CalcTextSize("This trophy information saved on this user will be deleted.").x / 2.f), POPUP_SIZE.y / 2.f));
                ImGui::TextColored(GUI_COLOR_TEXT, "This trophy information saved on this user will be deleted.");
                ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCAL.x)), POPUP_SIZE.y - BUTTON_SIZE.y - (22.0f * SCAL.y)));
                if (ImGui::Button("Cancel", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle))
                    delete_trophy = false;
                ImGui::SameLine(0, 20.f * SCAL.x);
                if (ImGui::Button("Ok", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
                    fs::remove_all(TROPHY_PATH / "conf" / np_com_id_hovered);
                    fs::remove_all(TROPHY_PATH / "data" / np_com_id_hovered);
                    delete_np_com_id = np_com_id_hovered;
                    delete_trophy = false;
                }
                ImGui::PopStyleVar(2);
                ImGui::EndChild();
                ImGui::End();
            }
        } else {
            // Select Group ID
            if (group_id_selected.empty()) {
                ImGui::Image(gui.trophy_np_com_id_list[np_com_id_selected]["000"], SIZE_ICON_LIST);
                ImGui::SameLine();
                ImGui::SetWindowFontScale(1.3f * SCAL.x);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                if (ImGui::Selectable(np_com_id_info[np_com_id_selected].name["000"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_ICON_LIST.y))) {
                    group_id_selected = "global";
                    detail_np_com_id = true;
                }
                ImGui::PopStyleVar();
                ImGui::Columns(4, nullptr, false);
                ImGui::SetColumnWidth(0, (40.f * SCAL.x));
                ImGui::SetColumnWidth(1, SIZE_ICON_LIST.x + (10.f * SCAL.x));
                ImGui::SetColumnWidth(2, 345.f * SCAL.x);
                for (const auto &group_id : np_com_id_info[np_com_id_selected].group_id) {
                    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (15.f * SCAL.x) - (ImGui::CalcTextSize("+").x / 2.f), ImGui::GetCursorPosY() + (SIZE_ICON_LIST.y / 2.f) - (ImGui::CalcTextSize("+").y / 2.f)));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", group_id != "000" ? "+" : "");
                    ImGui::NextColumn();
                    ImGui::Image(gui.trophy_np_com_id_list[np_com_id_selected][group_id], SIZE_ICON_LIST);
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
                    ImGui::SetCursorPosY(Title_POS + (50.f * SCAL.y));
                    ImGui::SetWindowFontScale(1.f * SCAL.x);
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                    ImGui::ProgressBar(np_com_id_info[np_com_id_selected].progress[group_id] / 100.f, ImVec2(200 * SCAL.x, 15.f * SCAL.y), progress_dummy);
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0.f, (10.f * SCAL.x));
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (6.f * SCAL.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", std::to_string(np_com_id_info[np_com_id_selected].progress[group_id]).append("%").c_str());
                    ImGui::NextColumn();
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                    ImGui::SetWindowFontScale(1.4f * SCAL.x);
                    ImGui::Selectable(np_com_id_info[np_com_id_selected].unlocked_type_count[group_id]["general"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y));
                    ImGui::SameLine(0.f, 15.f * SCAL.x);
                    ImGui::Selectable(">", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, SIZE_ICON_LIST.y));
                    ImGui::PopStyleVar();
                    ImGui::NextColumn();
                }
                // Detail for Np Com ID
            } else if (detail_np_com_id) {
                ImGui::SetWindowFontScale(1.5f * SCAL.x);
                ImGui::Image(gui.trophy_np_com_id_list[np_com_id_selected][group_id_selected == "global" ? "000" : group_id_selected], SIZE_ICON_LIST);
                const auto CALC_NAME = ImGui::CalcTextSize(np_com_id_info[np_com_id_selected].name[group_id_selected == "global" ? "000" : group_id_selected].c_str(), nullptr, false, SIZE_INFO.x - SIZE_ICON_LIST.x - 48.f).y / 2.f;
                ImGui::SetCursorPos(ImVec2(SIZE_ICON_LIST.x + 20.f, (SIZE_ICON_LIST.y / 2.f) - CALC_NAME));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_INFO.x - SIZE_ICON_LIST.x - 48.f);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", np_com_id_info[np_com_id_selected].name[group_id_selected == "global" ? "000" : group_id_selected].c_str());
                ImGui::PopTextWrapPos();
                ImGui::SetCursorPosY(SIZE_ICON_LIST.y + (20.f * SCAL.y));
                ImGui::Text("Progress");
                ImGui::SameLine(260.f * SCAL.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", std::to_string(np_com_id_info[np_com_id_selected].progress[group_id_selected]).append("%").c_str());
                ImGui::SameLine(360 * SCAL.x);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (6.f * SCAL.y));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                ImGui::ProgressBar(np_com_id_info[np_com_id_selected].progress[group_id_selected] / 100.f, ImVec2(200 * SCAL.x, 15.f * SCAL.y), progress_dummy);
                ImGui::PopStyleColor();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (30.f * SCAL.y));
                ImGui::Text("Trophies");
                ImGui::SameLine(260.f * SCAL.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%d/%d\n%s", np_com_id_info[np_com_id_selected].unlocked_count[group_id_selected], np_com_id_info[np_com_id_selected].trophy_count_by_group[group_id_selected], np_com_id_info[np_com_id_selected].unlocked_type_count[group_id_selected]["detail"].c_str());
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (45.f * SCAL.y));
                ImGui::Text("Updated");
                ImGui::SameLine(260.f * SCAL.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", np_com_id_info[np_com_id_selected].updated.c_str());
                ImGui::PushTextWrapPos(SIZE_INFO.x - 40.f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCAL.y));
                ImGui::Text("Details");
                ImGui::SameLine(260.f * SCAL.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", np_com_id_info[np_com_id_selected].detail[group_id_selected == "global" ? "000" : group_id_selected].c_str());
                ImGui::PopTextWrapPos();
                // Select Trophy
            } else if (trophy_id_selected.empty()) {
                ImGui::Image(gui.trophy_np_com_id_list[np_com_id_selected][group_id_selected], SIZE_ICON_LIST);
                ImGui::SameLine();
                ImGui::SetWindowFontScale(1.6f * SCAL.x);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                if (ImGui::Selectable(np_com_id_info[np_com_id_selected].name[group_id_selected].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_ICON_LIST.y)))
                    detail_np_com_id = true;
                ImGui::PopStyleVar();
                ImGui::Columns(3, nullptr, false);
                ImGui::SetColumnWidth(0, SIZE_TROPHY_LIST.x + (15.f * SCAL.x));
                ImGui::SetColumnWidth(1, (40.f * SCAL.x));
                for (const auto &trophy : trophy_list) {
                    ImGui::SetWindowFontScale(1.2f * SCAL.x);
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
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (14.f * SCAL.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", hidden_trophy ? "?" : trophy_info[trophy.id].type["general"].c_str());
                    ImGui::NextColumn();
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.2f));
                    const auto name_pos = ImGui::GetCursorPosY();
                    if (ImGui::Selectable(hidden_trophy ? "Hidden Trophy" : trophy.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_TROPHY_LIST.y)))
                        trophy_id_selected = trophy.id;
                    ImGui::PopStyleVar();
                    if (!hidden_trophy) {
                        ImGui::SetCursorPosY(name_pos + (40.f * SCAL.y));
                        ImGui::SetWindowFontScale(1.0f * SCAL.x);
                        ImGui::Text("%s", trophy_info[trophy.id].detail.c_str());
                    }
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            } else {
                // Detail Trophy
                ImGui::SetWindowFontScale(1.5f * SCAL.x);
                if (trophy_info[trophy_id_selected].earned)
                    ImGui::Image(gui.trophy_list[trophy_id_selected], SIZE_TROPHY_LIST);
                else {
                    ImGui::SetCursorPosY((SIZE_TROPHY_LIST.y / 2.f) - (15.f * SCAL.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "Lock");
                }
                const auto hidden_trophy = (!trophy_info[trophy_id_selected].earned && (trophy_info[trophy_id_selected].hidden == "yes"));
                const auto CALC_NAME = ImGui::CalcTextSize(hidden_trophy ? "Hidden Trophy" : trophy_info[trophy_id_selected].name.c_str(), nullptr, false, SIZE_INFO.x - SIZE_TROPHY_LIST.x - 48.f).y / 2.f;
                ImGui::SetCursorPos(ImVec2(SIZE_TROPHY_LIST.x + 20.f, (SIZE_TROPHY_LIST.y / 2.f) - CALC_NAME));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_INFO.x - SIZE_TROPHY_LIST.x - (48.f * SCAL.x));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", hidden_trophy ? "Hidden Trophy" : trophy_info[trophy_id_selected].name.c_str());
                ImGui::PopTextWrapPos();
                ImGui::SetCursorPosY(SIZE_TROPHY_LIST.y + (25.f * SCAL.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "Grade");
                ImGui::SameLine(250.f * SCAL.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", hidden_trophy ? "?" : trophy_info[trophy_id_selected].type["detail"].c_str());
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (25.f * SCAL.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "Earned");
                ImGui::SameLine(250.f * SCAL.x);
                ImGui::TextColored(GUI_COLOR_TEXT, trophy_info[trophy_id_selected].earned ? trophy_info[trophy_id_selected].unlocked_time.c_str() : "Not Earned");
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (25.f * SCAL.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "Details");
                ImGui::SameLine(250.f * SCAL.x);
                ImGui::PushTextWrapPos(SIZE_INFO.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", hidden_trophy ? "-" : trophy_info[trophy_id_selected].detail.c_str());
                ImGui::PopTextWrapPos();
            }
        }
    }
    ImGui::EndChild();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    ImGui::SetCursorPos(ImVec2(6.f, display_size.y - (84.f * SCAL.y)));
    if (ImGui::Button("Back", ImVec2(64.f * SCAL.x, 40.f * SCAL.y))) {
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
        } else
            gui.trophy.trophy_collection = false;
    }

    if (trophy_id_selected.empty() && !detail_np_com_id && (np_com_id_selected.empty() || !group_id_selected.empty())) {
        ImGui::SetCursorPos(ImVec2(display_size.x - (70.f * SCAL.x), display_size.y - (84.f * SCAL.y)));
        if (ImGui::Button("...", ImVec2(64.f * SCAL.x, 40.f * SCAL.y)) || ImGui::IsKeyPressed(host.cfg.keyboard_button_triangle))
            ImGui::OpenPopup("...");
        if (ImGui::BeginPopup("...", ImGuiWindowFlags_NoMove)) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Sort");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (np_com_id_selected.empty()) {
                if (ImGui::MenuItem("Updated", nullptr, np_com_id_sort == "updated")) {
                    std::sort(np_com_id_list.begin(), np_com_id_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.updated > tb.updated;
                    });
                    np_com_id_sort = "updated";
                }
                if (ImGui::MenuItem("Name", nullptr, np_com_id_sort == "name")) {
                    std::sort(np_com_id_list.begin(), np_com_id_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.name < tb.name;
                    });
                    np_com_id_sort = "name";
                }
                if (ImGui::MenuItem("Progress", nullptr, np_com_id_sort == "progress")) {
                    std::sort(np_com_id_list.begin(), np_com_id_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.progress > tb.progress;
                    });
                    np_com_id_sort = "progress";
                }
            } else {
                if (ImGui::MenuItem("Orignal", nullptr, trophy_sort == "original")) {
                    std::sort(trophy_list.begin(), trophy_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.id < tb.id;
                    });
                    trophy_sort = "original";
                }
                if (ImGui::MenuItem("Earned", nullptr, trophy_sort == "earned")) {
                    std::sort(trophy_list.begin(), trophy_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.earned > tb.earned;
                    });
                    trophy_sort = "earned";
                }
                if (ImGui::MenuItem("Grade", nullptr, trophy_sort == "grade")) {
                    std::sort(trophy_list.begin(), trophy_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.grade < tb.grade;
                    });
                    trophy_sort = "grade";
                }
                if (ImGui::MenuItem("Name", nullptr, trophy_sort == "name")) {
                    std::sort(trophy_list.begin(), trophy_list.end(), [](const auto &ta, const auto &tb) {
                        return ta.name < tb.name;
                    });
                    trophy_sort = "name";
                }
            }
            ImGui::EndPopup();
        }
    }
    ImGui::PopStyleVar();
    ImGui::End();
}

} // namespace gui
