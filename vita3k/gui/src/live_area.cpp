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

#include <config/state.h>
#include <ctrl/ctrl.h>
#include <dialog/state.h>
#include <gui/functions.h>

#include <io/device.h>
#include <io/functions.h>
#include <io/state.h>

#include <kernel/state.h>

#include <packages/license.h>
#include <packages/sce_types.h>

#include <renderer/state.h>

#include <io/VitaIoDevice.h>
#include <io/vfs.h>

#include <util/log.h>
#include <util/net_utils.h>

#include <boost/algorithm/string/trim.hpp>

#include <pugixml.hpp>

#include <chrono>
#include <stb_image.h>

#include <iostream>

namespace gui {

bool get_sys_apps_state(GuiState &gui) {
    return !gui.vita_area.content_manager && !gui.vita_area.settings && !gui.vita_area.trophy_collection && !gui.vita_area.manual;
}

struct FRAME {
    std::string id;
    std::string multi;
    SceUInt64 autoflip;
};

struct STR {
    std::string color;
    float size;
    std::string text;
};

static std::map<std::string, std::vector<FRAME>> frames;
static std::map<std::string, std::map<std::string, std::vector<STR>>> str;
static std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::pair<SceInt32, std::string>>>>> liveitem;
static std::map<std::string, std::map<std::string, std::map<std::string, ImVec2>>> items_size;
static std::map<std::string, std::map<std::string, std::string>> target;
static std::map<std::string, std::map<std::string, uint64_t>> current_item, last_time;
static std::map<std::string, std::string> type;

struct Items {
    ImVec2 gate_pos;
    struct Frames {
        ImVec2 pos;
        ImVec2 size;
    };
    std::map<std::string, Frames> frames;
};

// clang-format off
static std::map<std::string, Items> items_styles = {
    // A1
    { "a1", { 
        ImVec2(620.f, 361.f), { 
            { "frame1", { ImVec2(900.f, 414.f), ImVec2(260.f, 260.f) } },
            { "frame2", { ImVec2(320.f, 414.f), ImVec2(260.f, 260.f) } },
            { "frame3", { ImVec2(900.f, 154.f), ImVec2(840.f, 150.f) } }
        }
    } },
    // A2
    { "a2", {
        ImVec2(620.f, 395.f), {
            { "frame1", { ImVec2(900.f, 404.f), ImVec2(260.f, 400.f) } },
            { "frame2", { ImVec2(320.f, 404.f), ImVec2(260.f, 400.f) } },
            { "frame3", { ImVec2(640.f, 204.f), ImVec2(320.f, 200.f) } }
        }
    } },
    // A3
    { "a3", {
        ImVec2(620.f, 395.f), {
            { "frame1", { ImVec2(900.f, 414.f), ImVec2(260.f, 200.f) } },
            { "frame2", { ImVec2(320.f, 414.f), ImVec2(260.f, 200.f) } },
            { "frame3", { ImVec2(900.f, 214.f), ImVec2(260.f, 210.f) } },
            { "frame4", { ImVec2(640.f, 214.f), ImVec2(320.f, 210.f) } },
            { "frame5", { ImVec2(320.f, 214.f), ImVec2(260.f, 210.f) } }
		}
    } },
    // A4
    { "a4", { 
        ImVec2(620.f, 395.f), {
            { "frame1", { ImVec2(900.f, 414.f), ImVec2(260.f, 200.f) } },
            { "frame2", { ImVec2(320.f, 414.f), ImVec2(260.f, 200.f) } },
            { "frame3", { ImVec2(900.f, 214.f), ImVec2(840.f, 70.f) } },
            { "frame4", { ImVec2(900.f, 144.f), ImVec2(840.f, 70.f) } },
            { "frame5", { ImVec2(900.f, 74.f), ImVec2(840.f, 70.f) } }
        }
    } },
    // A5
    { "a5", {
        ImVec2(380.f, 395.f), {
            { "frame1", { ImVec2(900.f, 412.f), ImVec2(480.f, 68.f) } },
            { "frame2", { ImVec2(900.f, 344.f), ImVec2(480.f, 68.f) } },
            { "frame3", { ImVec2(900.f, 276.f), ImVec2(480.f, 68.f) } },
            { "frame4", { ImVec2(900.f, 208.f), ImVec2(480.f, 68.f) } },
            { "frame5", { ImVec2(900.f, 140.f), ImVec2(480.f, 68.f) } },
            { "frame6", { ImVec2(900.f, 72.f), ImVec2(480.f, 68.f) } },
            { "frame7", { ImVec2(420.f, 214.f), ImVec2(360.f, 210.f) } }
        }
    } },
    // Browser
    { "browser", {
        ImVec2(381.f, 395.f), {
            { "frame1", { ImVec2(890.f, 405.f), ImVec2(472.f, 50.f) } },
            { "frame2", { ImVec2(890.f, 355.f), ImVec2(472.f, 50.f) } },
            { "frame3", { ImVec2(890.f, 305.f), ImVec2(472.f, 50.f) } },
            { "frame4", { ImVec2(890.f, 255.f), ImVec2(472.f, 50.f) } },
            { "frame5", { ImVec2(890.f, 205.f), ImVec2(472.f, 50.f) } },
            { "frame6", { ImVec2(890.f, 155.f), ImVec2(472.f, 50.f) } },
            { "frame7", { ImVec2(890.f, 105.f), ImVec2(472.f, 50.f) } },
            { "frame8", { ImVec2(890.f, 55.f), ImVec2(472.f, 50.f) } },
            { "frame9", { ImVec2(381.f, 223.f), ImVec2(282.f, 108.f) } },
            { "frame10", { ImVec2(381.f, 115.f), ImVec2(282.f, 108.f) } }
        }
    } },
    // Content manager
    { "content_manager", { ImVec2(620.f, 361.f), {} } },
    // PSMobile
    { "psmobile", {
        ImVec2(380.f, 345.f), {
            { "frame1", { ImVec2(866.f, 414.f), ImVec2(446.f, 154.f) } }, 
            { "frame2", { ImVec2(866.f, 249.5f), ImVec2(446.f, 109.f) } },
            { "frame3", { ImVec2(866.f, 119.f), ImVec2(196.f, 58.f) } },
            { "frame4", { ImVec2(866.f, 34.f), ImVec2(772.f, 30.f) } }
        }
    } },
};
// clang-format on

enum class HeadingLevel {
    None = 0,
    H1,
    H2,
    H3,
    H4,
    H5,
    H6
};

struct UpdateLine {
    ImVec4 font_color;
    uint32_t font_size;
    HeadingLevel heading_level;
    bool in_list;
    bool is_bullet;
    std::string text;
};

static std::map<std::string, std::vector<UpdateLine>> update_history_infos;

static const std::string bullet = "\u30FB"; // Unicode character for bullet (U+30FB)
static bool is_new_version;
bool get_update_history_infos(const std::string &update_story, const std::string &ver) {
    update_history_infos.clear();

    is_new_version = ver != "0";

    pugi::xml_document doc;
    if (!doc.load_string(update_story.c_str())) {
        LOG_ERROR("Failed to parse update history XML: \n{}", update_story);
        return false;
    }

    const auto split_version = [](const std::string &version) {
        uint32_t major, minor;
        size_t pos = version.find('.');
        if (pos != std::string::npos) {
            major = std::stoi(version.substr(0, pos));
            minor = std::stoi(version.substr(pos + 1));
        }
        return std::make_pair(major, minor);
    };

    const auto current_ver = split_version(ver);

    static const std::map<std::string, std::string> entities = {
        { R"(&lt;)", "<" },
        { R"(&gt;)", ">" },
        { R"(&quot;)", "\"" },
        { R"(&apos;)", "'" },
        { R"(&copy;)", "\xC2\xA9" },
        { R"(&reg;)", "\xC2\xAE" },
        { R"(&trade;)", "\xE2\x84\xA2" },
        { R"(&hellip;)", "\xE2\x80\xA6" },
        { R"(&mdash;)", "\xE2\x80\x94" },
        { R"(&ndash;)", "\xE2\x80\x93" },
        { R"(&nbsp;)", "\xC2\xA0" },
        { R"(&amp;)", "&" },
    };

    auto html_decode_regex = [&](std::string text) {
        for (const auto &[key, value] : entities) {
            text = std::regex_replace(text, std::regex(key), value);
        }
        return text;
    };

    for (const auto &info : doc.child("changeinfo")) {
        std::string app_ver = info.attribute("app_ver").as_string();
        // Split the version string to handle both major and minor versions
        if (app_ver.empty()) {
            LOG_ERROR("App version is empty in update history XML.");
            continue;
        }

        // Remove leading zeros from the version string
        if (app_ver[0] == '0')
            app_ver.erase(app_ver.begin());

        if (is_new_version) {
            auto version = split_version(app_ver);
            // Check if the version is greater than or equal to the current version
            if (version.first < current_ver.first || ((version.first == current_ver.first) && (version.second <= current_ver.second))) {
                // continue; // Skip versions that are less or equal than the current version
            }
        }

        // Extract the text content from the XML node
        std::string text = info.text().as_string();

        // Normalize line endings to '\n'
        text = std::regex_replace(text, std::regex(R"(\r\n?)"), "\n");

        // Remove all to new line
        text = std::regex_replace(text, std::regex(R"(\n\s*)"), " ");

        // Replace specific HTML tags before removing all remaining tags
        text = std::regex_replace(text, std::regex(R"(<li>)"), bullet); // Replace <li> with bullet

        // Replace <br>, </li>, <p>, and </p> with newlines
        text = std::regex_replace(text, std::regex(R"(<br\s*/?>|</li>|</?p>)"), "\n");

        // Replace <hN> with a start token
        std::regex h_start_regex(R"(<h([1-6])>)");
        text = std::regex_replace(text, h_start_regex, "\n__H_START_$1__");

        // Replace </hN> with an end token
        std::regex h_end_regex(R"(</h[1-6]>)");
        text = std::regex_replace(text, h_end_regex, "\n__H_END__");

        // Replace <ul> and </ul> with unordered list tokens
        const std::string ulstart_token = "__UL_START__";
        const std::string ulend_token = "__UL_END__";
        text = std::regex_replace(text, std::regex(R"(<ul>)"), "\n" + ulstart_token);
        text = std::regex_replace(text, std::regex(R"(</ul>)"), ulend_token);

        // Replace <font color="#RRGGBB" size="N"> with color and size tokens
        std::regex font_regex(R"(<font\s*(?:color=["']#([0-9a-fA-F]{6})["'])?\s*(?:size=["']([1-7])["'])?\s*>)");
        std::smatch match;
        while (std::regex_search(text, match, font_regex)) {
            std::string replacement;

            if (match[1].matched)
                replacement += "__COLOR_" + match[1].str() + "__";
            if (match[2].matched)
                replacement += "__SIZE_" + match[2].str() + "__";

            text = std::regex_replace(text, font_regex, replacement, std::regex_constants::format_first_only);
        }

        // Remove all remaining HTML tags
        text = std::regex_replace(text, std::regex(R"(<[^>]+>)"), "");

        // Remove non-breaking spaces (C2 A0)
        text = std::regex_replace(text, std::regex(R"(\xC2\xA0)"), "");

        // Replace Right Single Quotation Mark (U+2019) with a regular apostrophe
        text = std::regex_replace(text, std::regex(R"(\xE2\x80\x99)"), "'");

        text = html_decode_regex(text);

        std::istringstream stream(text);
        std::string line;
        std::vector<UpdateLine> update_lines;

        auto hl = HeadingLevel::None;
        bool is_list = false;

        while (std::getline(stream, line)) {
            bool is_bullet = false;

            bool is_start_list = false;
            bool is_end_list = false;
            ImVec4 font_color = GUI_COLOR_TEXT;
            uint32_t font_size = 3;

            std::smatch match;
            // Check for heading start tokens
            if (std::regex_search(line, match, std::regex(R"(__H_START_(\d)__)", std::regex_constants::icase))) {
                int level = std::stoi(match[1].str());
                if (level >= 1 && level <= 6) {
                    hl = static_cast<HeadingLevel>(level);
                    line = std::regex_replace(line, std::regex(R"(__H_START_\d__)"), "");
                }
            } else if (line.find("__H_END__") != std::string::npos) {
                hl = HeadingLevel::None;
                line = std::regex_replace(line, std::regex(R"(__H_END__)"), "");
            }

            if (std::regex_search(line, match, std::regex(R"(__COLOR_([0-9a-fA-F]{6})__)", std::regex_constants::icase))) {
                std::string color_code = match[1].str();
                uint32_t color = std::strtoul(color_code.c_str(), nullptr, 16);
                font_color = ImVec4((float((color >> 16) & 0xFF)) / 255.f, (float((color >> 8) & 0xFF)) / 255.f, (float((color >> 0) & 0xFF)) / 255.f, 1.f);
                line = std::regex_replace(line, std::regex(R"(__COLOR_[0-9a-fA-F]{6}__)"), "");
            }

            if (std::regex_search(line, match, std::regex(R"(__SIZE_([1-7])__)", std::regex_constants::icase))) {
                font_size = std::stoi(match[1].str());
                line = std::regex_replace(line, std::regex(R"(__SIZE_[1-7]__)"), "");
            }

            if (line.find(ulstart_token) != std::string::npos) {
                is_start_list = true;
                line = std::regex_replace(line, std::regex(ulstart_token), "");
            }

            if (line.find(ulend_token) != std::string::npos) {
                is_end_list = true;
                line = std::regex_replace(line, std::regex(ulend_token), "");
            }

            is_list = is_start_list || (is_list && !is_end_list);

            if (line.find(bullet) != std::string::npos) {
                is_bullet = true;
                line = std::regex_replace(line, std::regex(bullet), ""); // Remove bullet characters at the start
            } else
                is_bullet = false;

            boost::trim(line); // Trim leading whitespace

            update_lines.push_back({ font_color, font_size, hl, is_list, is_bullet, line });
            LOG_DEBUG("Update line: {}, is_list: {}", line, is_list);
        }

        update_history_infos[app_ver] = update_lines;
    }

    return !update_history_infos.empty();
}

static std::map<HeadingLevel, float> heading_sizes = {
    { HeadingLevel::H1, 30.0f },
    { HeadingLevel::H2, 24.0f },
    { HeadingLevel::H3, 20.0f },
    { HeadingLevel::H4, 16.0f },
    { HeadingLevel::H5, 14.0f },
    { HeadingLevel::H6, 12.0f }
};

static std::map<int, float> font_size_px = {
    { 1, 13.3f },
    { 2, 16.7f },
    { 3, 20.0f },
    { 4, 23.3f },
    { 5, 30.0f },
    { 6, 40.0f },
    { 7, 60.0f }
};

void draw_update_history_infos(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);

    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const auto WINDOW_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);
    const auto BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);

    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(VIEWPORT_SIZE, ImGuiCond_Always);
    ImGui::Begin("##update_history_infos", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowBgAlpha(0.999f * SCALE.y);
    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x + (VIEWPORT_SIZE.x / 2.f) - (WINDOW_SIZE.x / 2.f), emuenv.logical_viewport_pos.y + (VIEWPORT_SIZE.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
    ImGui::BeginChild("##update_history_infos_child", WINDOW_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);

    auto &lang = gui.lang.app_context;
    auto &patch_check = gui.lang.patch_check;
    auto &common = emuenv.common_dialog.lang.common;
    const auto &id = is_new_version ? fs::path(gui.live_area_current_open_apps_list[gui.live_area_app_current_open]).stem().string() : "";

    const auto padding_list = ImVec2(24.f * SCALE.x, 56.f * SCALE.y);

    // Update History
    const auto UPDATE_LIST_SIZE = ImVec2(WINDOW_SIZE.x - (padding_list.x * 2.f), WINDOW_SIZE.y / 1.51f);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowPos().x + padding_list.x, ImGui::GetWindowPos().y + padding_list.y));
    ImGui::BeginChild("##info_update_list", UPDATE_LIST_SIZE, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(1.48f * RES_SCALE.y);
    if (is_new_version) {
        ImGui::TextWrapped("%s", patch_check["new_app_version_available"].c_str());
        const auto update_infos = gui.new_update_infos[id];
        auto size_str = get_unit_size(update_infos.size);
        size_str = std::regex_replace(size_str, std::regex("\\s+"), "");
        ImGui::NewLine();
        ImGui::TextColored(GUI_COLOR_TEXT, "%s(%s)", fmt::format(fmt::runtime(patch_check["version"]), update_infos.version).c_str(), size_str.c_str());
        ImGui::NewLine();
    }

    const auto wrap_width = UPDATE_LIST_SIZE.x - ImGui::GetStyle().ScrollbarSize;

    // Reverse iterator to show the latest update first
    for (auto it = update_history_infos.rbegin(); it != update_history_infos.rend(); ++it) {
        ImGui::SetWindowFontScale(1.48f * RES_SCALE.y);
        const auto version_str = is_new_version ? fmt::format("{}", it->first) : fmt::format(fmt::runtime(lang.main["history_version"]), it->first);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", version_str.c_str());

        for (const auto &line : it->second) {
            const float font_scale = (line.heading_level != HeadingLevel::None ? heading_sizes[line.heading_level] : font_size_px[line.font_size]) / ImGui::GetFont()->FontSize;
            ImGui::SetWindowFontScale(font_scale * RES_SCALE.y);

            const auto bullet_size = ImGui::CalcTextSize(bullet.c_str());
            if (line.in_list || line.is_bullet || line.text.find("*") != std::string::npos) {
                const auto text_pos = ImGui::GetCursorPosY();
                if (line.is_bullet)
                    ImGui::TextUnformatted(bullet.c_str());

                ImGui::SetCursorPos(ImVec2(bullet_size.x + ImGui::GetStyle().ItemSpacing.x, text_pos));
            }

            ImGui::PushTextWrapPos(wrap_width);
            ImGui::PushStyleColor(ImGuiCol_Text, line.font_color);
            ImGui::TextWrapped("%s", line.text.c_str());
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
            ImGui::ScrollWhenDragging();
        }

        if (std::next(it) != update_history_infos.rend())
            ImGui::NewLine();
    }
    ImGui::EndChild();
    ImGui::SetWindowFontScale(1.25f * RES_SCALE.y);
    if (is_new_version) {
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - BUTTON_SIZE.x - (10.f * SCALE.x), WINDOW_SIZE.y - BUTTON_SIZE.y - (22.f * SCALE.y)));
        if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle)))
            gui.vita_area.update_history_info = false;
        ImGui::SameLine(0, 20.f * SCALE.x);
        if (ImGui::Button(patch_check["download"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle))) {
            gui.vita_area.update_history_info = false;
            update_notice_info(gui, emuenv, "downloading", id);
        }
    } else {
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (BUTTON_SIZE.x / 2.f), WINDOW_SIZE.y - BUTTON_SIZE.y - (22.f * SCALE.y)));
        if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
            gui.vita_area.update_history_info = false;
        }
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
}

static bool check_has_patch(EmuEnvState &emuenv, const std::string &title_id, const std::string current_version) {
    const auto ver_xml_path = emuenv.pref_path / "ux0/bgdl" / fmt::format("{}-ver.xml", title_id);

    pugi::xml_document doc;
    if (doc.load_file(ver_xml_path.string().c_str())) {
        auto tag = doc.child("titlepatch").child("tag");
        const auto last_package = tag.last_child();
        if (last_package.empty() || (std::string(last_package.name()) != "package")) {
            LOG_ERROR("No package found in the version XML for title ID: {}", title_id);
            return false;
        }

        std::string latest_version = last_package.attribute("version").as_string();
        if (latest_version[0] == '0')
            latest_version.erase(latest_version.begin());
        if (latest_version != current_version) {
            LOG_INFO("New update {} is available", latest_version);
            return true;
        }
        return true;
    }

    return false;
}

void init_live_area(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const auto &live_area_lang = gui.lang.user_lang[LIVE_AREA];

    const auto APP_INDEX = get_app_index(gui, app_path);
    const auto TITLE_ID = APP_INDEX->title_id;

    const auto is_com_app = APP_INDEX->title_id.starts_with("PCS");
    const auto is_emu_app = app_path.starts_with("emu");
    const auto is_fw_app = app_path.starts_with("vs0");
    const auto is_ps_vita_os = app_path == "emu:vsh/shell";

    get_license(emuenv, APP_INDEX->title_id, APP_INDEX->content_id);

    if (!gui.live_area_contents.contains(app_path)) {
        auto default_contents = false;
        const auto fw_path{ emuenv.pref_path / "vs0" };
        const auto default_fw_contents{ fw_path / "data/internal/livearea/default/sce_sys/livearea/contents/template.xml" };
        const auto real_app_path = app_path.starts_with("emu") ? "vs0:app/" + fs::path(app_path).stem().string() : app_path;
        const auto APP_PATH{ emuenv.pref_path / convert_path(real_app_path) };
        const auto live_area_contents_path{ fs::path("sce_sys") / ((emuenv.license.rif[TITLE_ID].sku_flag == 3) && fs::exists(APP_PATH / "sce_sys/retail/livearea") ? "retail/livearea" : "livearea") / "contents" };
        auto template_xml{ APP_PATH / live_area_contents_path / "template.xml" };

        pugi::xml_document doc;

        if (!doc.load_file(template_xml.c_str())) {
            if (!is_ps_vita_os && (is_com_app || is_fw_app || is_emu_app))
                LOG_WARN("Live Area Contents is corrupted or missing for title: {} '{}' in path: {}.", APP_INDEX->title_id, APP_INDEX->title, template_xml);
            if (doc.load_file(default_fw_contents.c_str())) {
                template_xml = default_fw_contents;
                default_contents = true;
                LOG_INFO_IF(!is_ps_vita_os, "Using default firmware contents.");
            } else {
                type[app_path] = "a1";
                LOG_WARN("Default firmware contents is corrupted or missing, install firmware for fix it.");
                return;
            }
        }

        if (doc.load_file(template_xml.c_str())) {
            std::map<std::string, std::string> name;

            type[app_path].clear();
            type[app_path] = doc.child("livearea").attribute("style").as_string();

            if (!doc.child("livearea").child("livearea-background").child("image").child("lang").text().empty()) {
                for (const auto &livearea_background : doc.child("livearea").child("livearea-background")) {
                    if (livearea_background.child("lang").text().as_string() == live_area_lang) {
                        name["livearea-background"] = livearea_background.text().as_string();
                        break;
                    } else if (!livearea_background.child("exclude-lang").empty() && livearea_background.child("exclude-lang").text().as_string() != live_area_lang) {
                        name["livearea-background"] = livearea_background.text().as_string();
                        break;
                    } else if (livearea_background.attribute("default").as_string() == std::string("on")) {
                        name["livearea-background"] = livearea_background.text().as_string();
                        break;
                    }
                }
            } else
                name["livearea-background"] = doc.child("livearea").child("livearea-background").child("image").text().as_string();

            if (name["livearea-background"].empty())
                name["livearea-background"] = doc.child("livearea").child("livearea-background").child("image").text().as_string();

            if (!default_contents) {
                for (const auto &gate : doc.child("livearea").child("gate")) {
                    if (gate.child("lang")) {
                        if (gate.child("lang").text().as_string() == live_area_lang) {
                            name["gate"] = gate.text().as_string();
                            break;
                        } else if (gate.attribute("default").as_string() == std::string("on"))
                            name["gate"] = gate.text().as_string();
                        else
                            for (const auto &lang : gate)
                                if (lang.text().as_string() == live_area_lang) {
                                    name["gate"] = gate.text().as_string();
                                    break;
                                }
                    } else if (gate.child("cntry")) {
                        if (gate.child("cntry").attribute("lang").as_string() == live_area_lang) {
                            name["gate"] = gate.text().as_string();
                            break;
                        } else
                            name["gate"] = gate.text().as_string();
                    } else
                        name["gate"] = gate.text().as_string();
                }
                if (name["gate"].find('\n') != std::string::npos)
                    name["gate"].erase(remove(name["gate"].begin(), name["gate"].end(), '\n'), name["gate"].end());
                name["gate"].erase(remove_if(name["gate"].begin(), name["gate"].end(), isspace), name["gate"].end());
            }

            if (name["livearea-background"].find('\n') != std::string::npos)
                name["livearea-background"].erase(remove(name["livearea-background"].begin(), name["livearea-background"].end(), '\n'), name["livearea-background"].end());
            name["livearea-background"].erase(remove_if(name["livearea-background"].begin(), name["livearea-background"].end(), isspace), name["livearea-background"].end());

            for (const auto &contents : name) {
                int32_t width = 0;
                int32_t height = 0;
                vfs::FileBuffer buffer;

                if (contents.second.empty()) {
                    LOG_WARN("Content '{}' is empty for title {} [{}].", contents.first, app_path, APP_INDEX->title);
                    continue;
                }

                if (default_contents)
                    vfs::read_file(VitaIoDevice::vs0, buffer, emuenv.pref_path, "data/internal/livearea/default/sce_sys/livearea/contents/" + contents.second);
                else
                    vfs::read_app_file(buffer, emuenv.pref_path, real_app_path, live_area_contents_path / contents.second);

                if (buffer.empty()) {
                    if (is_com_app || is_fw_app)
                        LOG_WARN("Contents {} '{}' Not found for title {} [{}].", contents.first, contents.second, app_path, APP_INDEX->title);
                    continue;
                }
                stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                if (!data) {
                    LOG_ERROR("Invalid Live Area Contents '{}' for title {} [{}].", contents.second, app_path, APP_INDEX->title);
                    continue;
                }

                gui.live_area_contents[app_path][contents.first] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
                stbi_image_free(data);
            }

            std::map<std::string, std::map<std::string, std::vector<std::string>>> items_name;

            frames[app_path].clear();
            liveitem[app_path].clear();
            str[app_path].clear();
            target[app_path].clear();

            for (const auto &livearea : doc.child("livearea")) {
                if (!livearea.attribute("id").empty()) {
                    frames[app_path].push_back({ livearea.attribute("id").as_string(), livearea.attribute("multi").as_string(), livearea.attribute("autoflip").as_uint() });

                    std::string frame = frames[app_path].back().id;

                    // Position background
                    if (!livearea.child("liveitem").child("background").empty()) {
                        if (!livearea.child("liveitem").child("background").attribute("x").empty())
                            liveitem[app_path][frame]["background"]["x"].first = livearea.child("liveitem").child("background").attribute("x").as_int();
                        if (!livearea.child("liveitem").child("background").attribute("y").empty())
                            liveitem[app_path][frame]["background"]["y"].first = livearea.child("liveitem").child("background").attribute("y").as_int();
                        if (!livearea.child("liveitem").child("background").attribute("align").empty())
                            liveitem[app_path][frame]["background"]["align"].second = livearea.child("liveitem").child("background").attribute("align").as_string();
                        if (!livearea.child("liveitem").child("background").attribute("valign").empty())
                            liveitem[app_path][frame]["background"]["valign"].second = livearea.child("liveitem").child("background").attribute("valign").as_string();
                    }

                    // Position image
                    if (!livearea.child("liveitem").child("image").empty()) {
                        if (!livearea.child("liveitem").child("image").attribute("x").empty())
                            liveitem[app_path][frame]["image"]["x"].first = livearea.child("liveitem").child("image").attribute("x").as_int();
                        if (!livearea.child("liveitem").child("image").attribute("y").empty())
                            liveitem[app_path][frame]["image"]["y"].first = livearea.child("liveitem").child("image").attribute("y").as_int();
                        if (!livearea.child("liveitem").child("image").attribute("align").empty())
                            liveitem[app_path][frame]["image"]["align"].second = livearea.child("liveitem").child("image").attribute("align").as_string();
                        if (!livearea.child("liveitem").child("image").attribute("valign").empty())
                            liveitem[app_path][frame]["image"]["valign"].second = livearea.child("liveitem").child("image").attribute("valign").as_string();
                        if (!livearea.child("liveitem").child("image").attribute("origin").empty())
                            liveitem[app_path][frame]["image"]["origin"].second = livearea.child("liveitem").child("image").attribute("origin").as_string();
                    }

                    if (!livearea.child("liveitem").child("text").empty()) {
                        // SceInt32
                        if (!livearea.child("liveitem").child("text").attribute("width").empty())
                            liveitem[app_path][frame]["text"]["width"].first = livearea.child("liveitem").child("text").attribute("width").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("height").empty())
                            liveitem[app_path][frame]["text"]["height"].first = livearea.child("liveitem").child("text").attribute("height").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("x").empty())
                            liveitem[app_path][frame]["text"]["x"].first = livearea.child("liveitem").child("text").attribute("x").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("y").empty())
                            liveitem[app_path][frame]["text"]["y"].first = livearea.child("liveitem").child("text").attribute("y").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("margin-top").empty())
                            liveitem[app_path][frame]["text"]["margin-top"].first = livearea.child("liveitem").child("text").attribute("margin-top").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("margin-bottom").empty())
                            liveitem[app_path][frame]["text"]["margin-bottom"].first = livearea.child("liveitem").child("text").attribute("margin-bottom").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("margin-left").empty())
                            liveitem[app_path][frame]["text"]["margin-left"].first = livearea.child("liveitem").child("text").attribute("margin-left").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("margin-right").empty())
                            liveitem[app_path][frame]["text"]["margin-right"].first = livearea.child("liveitem").child("text").attribute("margin-right").as_int();

                        // String
                        if (!livearea.child("liveitem").child("text").attribute("align").empty())
                            liveitem[app_path][frame]["text"]["align"].second = livearea.child("liveitem").child("text").attribute("align").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("valign").empty())
                            liveitem[app_path][frame]["text"]["valign"].second = livearea.child("liveitem").child("text").attribute("valign").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("origin").empty())
                            liveitem[app_path][frame]["text"]["origin"].second = livearea.child("liveitem").child("text").attribute("origin").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("line-align").empty())
                            liveitem[app_path][frame]["text"]["line-align"].second = livearea.child("liveitem").child("text").attribute("line-align").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("text-align").empty())
                            liveitem[app_path][frame]["text"]["text-align"].second = livearea.child("liveitem").child("text").attribute("text-align").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("text-valign").empty())
                            liveitem[app_path][frame]["text"]["text-valign"].second = livearea.child("liveitem").child("text").attribute("text-valign").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("word-wrap").empty())
                            liveitem[app_path][frame]["text"]["word-wrap"].second = livearea.child("liveitem").child("text").attribute("word-wrap").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("word-scroll").empty())
                            liveitem[app_path][frame]["text"]["word-scroll"].second = livearea.child("liveitem").child("text").attribute("word-scroll").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("ellipsis").empty())
                            liveitem[app_path][frame]["text"]["ellipsis"].second = livearea.child("liveitem").child("text").attribute("ellipsis").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("line-space").empty())
                            liveitem[app_path][frame]["text"]["line-space"].second = livearea.child("liveitem").child("text").attribute("line-space").as_string();
                    }

                    for (const auto &frame_id : livearea) {
                        if (frame_id.name() == std::string("liveitem")) {
                            if (!frame_id.child("cntry").empty()) {
                                for (const auto &cntry : frame_id) {
                                    if ((cntry.name() == std::string("cntry")) && (cntry.attribute("lang").as_string() == live_area_lang)) {
                                        if (!frame_id.child("background").text().empty())
                                            items_name[frame]["background"].push_back({ frame_id.child("background").text().as_string() });
                                        if (!frame_id.child("image").text().empty())
                                            items_name[frame]["image"].push_back({ frame_id.child("image").text().as_string() });
                                        if (!frame_id.child("target").text().empty())
                                            target[app_path][frame] = frame_id.child("target").text().as_string();
                                        if (!frame_id.child("text").child("str").text().empty()) {
                                            for (const auto &str_child : frame_id.child("text"))
                                                str[app_path][frame].push_back({ str_child.attribute("color").as_string(), str_child.attribute("size").as_float(), str_child.text().as_string() });
                                        }
                                        break;
                                    }
                                }
                            } else if (!frame_id.child("exclude-lang").empty()) {
                                bool exclude_lang = false;
                                for (const auto &liveitem : frame_id)
                                    if ((liveitem.name() == std::string("exclude-lang")) && (liveitem.text().as_string() == live_area_lang)) {
                                        exclude_lang = true;
                                        break;
                                    }
                                if (!exclude_lang) {
                                    if (!frame_id.child("background").text().empty())
                                        items_name[frame]["background"].push_back({ frame_id.child("background").text().as_string() });
                                    if (!frame_id.child("image").text().empty())
                                        items_name[frame]["image"].push_back({ frame_id.child("image").text().as_string() });
                                    if (!frame_id.child("target").text().empty())
                                        target[app_path][frame] = frame_id.child("target").text().as_string();
                                    if (!frame_id.child("text").child("str").text().empty()) {
                                        for (const auto &str_child : frame_id.child("text"))
                                            str[app_path][frame].push_back({ str_child.attribute("color").as_string(), str_child.attribute("size").as_float(), str_child.text().as_string() });
                                    }
                                    break;
                                }
                            } else if (frame_id.child("lang").text().as_string() == live_area_lang) {
                                if (!frame_id.child("background").text().empty())
                                    items_name[frame]["background"].push_back({ frame_id.child("background").text().as_string() });
                                if (!frame_id.child("image").text().empty())
                                    items_name[frame]["image"].push_back({ frame_id.child("image").text().as_string() });
                                if (!frame_id.child("target").text().empty())
                                    target[app_path][frame] = frame_id.child("target").text().as_string();
                                if (!frame_id.child("text").child("str").text().empty()) {
                                    for (const auto &str_child : frame_id.child("text"))
                                        str[app_path][frame].push_back({ str_child.attribute("color").as_string(), str_child.attribute("size").as_float(), str_child.text().as_string() });
                                }
                                break;
                            }
                        }
                    }

                    // if empty
                    const auto liveitem = livearea.child("liveitem");
                    if (items_name[frame]["background"].empty() && !liveitem.child("background").text().empty())
                        items_name[frame]["background"].push_back({ liveitem.child("background").text().as_string() });
                    if (items_name[frame]["image"].empty() && !liveitem.child("image").text().empty())
                        items_name[frame]["image"].push_back({ liveitem.child("image").text().as_string() });
                    if (target[app_path][frame].empty() && !liveitem.child("target").text().empty())
                        target[app_path][frame] = liveitem.child("target").text().as_string();
                    if (str[app_path][frame].empty() && !liveitem.child("text").child("str").text().empty()) {
                        for (const auto &str_child : liveitem.child("text"))
                            str[app_path][frame].push_back({ str_child.attribute("color").as_string(), str_child.attribute("size").as_float(), str_child.text().as_string() });
                    }

                    // Remove space or return line if exist on target
                    if (!target[app_path][frame].empty()) {
                        if (target[app_path][frame].find('\n') != std::string::npos)
                            target[app_path][frame].erase(remove(target[app_path][frame].begin(), target[app_path][frame].end(), '\n'), target[app_path][frame].end());
                        target[app_path][frame].erase(remove_if(target[app_path][frame].begin(), target[app_path][frame].end(), isspace), target[app_path][frame].end());
                    }
                }
            }

            for (auto &item : items_name) {
                current_item[app_path][item.first] = 0;
                if (!item.second.empty()) {
                    for (auto &bg_name : item.second["background"]) {
                        if (!bg_name.empty()) {
                            if (bg_name.find('\n') != std::string::npos)
                                bg_name.erase(remove(bg_name.begin(), bg_name.end(), '\n'), bg_name.end());
                            bg_name.erase(remove_if(bg_name.begin(), bg_name.end(), isspace), bg_name.end());

                            int32_t width = 0;
                            int32_t height = 0;
                            vfs::FileBuffer buffer;

                            vfs::read_app_file(buffer, emuenv.pref_path, real_app_path, live_area_contents_path / bg_name);

                            if (buffer.empty()) {
                                if (is_com_app || is_fw_app || is_emu_app)
                                    LOG_WARN("background, Id: {}, Name: '{}', Not found for title: {} [{}].", item.first, bg_name, app_path, APP_INDEX->title);
                                continue;
                            }
                            stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                            if (!data) {
                                LOG_ERROR("Frame: {}, Invalid Live Area Contents for title: {} [{}].", item.first, app_path, APP_INDEX->title);
                                continue;
                            }

                            items_size[app_path][item.first]["background"] = ImVec2(static_cast<float>(width), static_cast<float>(height));
                            gui.live_items[app_path][item.first]["background"].emplace_back(gui.imgui_state.get(), data, width, height);
                            stbi_image_free(data);
                        }
                    }

                    for (auto &img_name : item.second["image"]) {
                        if (!img_name.empty()) {
                            if (img_name.find('\n') != std::string::npos)
                                img_name.erase(remove(img_name.begin(), img_name.end(), '\n'), img_name.end());
                            img_name.erase(remove_if(img_name.begin(), img_name.end(), isspace), img_name.end());

                            int32_t width = 0;
                            int32_t height = 0;
                            vfs::FileBuffer buffer;

                            vfs::read_app_file(buffer, emuenv.pref_path, real_app_path, live_area_contents_path / img_name);

                            if (buffer.empty()) {
                                if (is_com_app || is_fw_app || is_emu_app)
                                    LOG_WARN("Image, Id: {} Name: '{}', Not found for title {} [{}].", item.first, img_name, app_path, APP_INDEX->title);
                                continue;
                            }
                            stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                            if (!data) {
                                LOG_ERROR("Frame: {}, Invalid Live Area Contents for title: {} [{}].", item.first, app_path, APP_INDEX->title);
                                continue;
                            }

                            items_size[app_path][item.first]["image"] = ImVec2(static_cast<float>(width), static_cast<float>(height));
                            gui.live_items[app_path][item.first]["image"].emplace_back(gui.imgui_state.get(), data, width, height);
                            stbi_image_free(data);
                        }
                    }
                }
            }
        }
    }

    if (check_has_patch(emuenv, APP_INDEX->title_id, APP_INDEX->app_ver))
        gui.app_has_update[app_path] = true;

    if (type[app_path].empty() || !items_styles.contains(type[app_path])) {
        LOG_WARN("Type of style {} no found for: {}", type[app_path], app_path);
        type[app_path] = "a1";
    }
}

inline static uint64_t current_time() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

void open_search(const std::string &title) {
    auto search_url = "https://www.google.com/search?q=" + title;
    std::replace(search_url.begin(), search_url.end(), ' ', '+');
    open_path(search_url);
}

void update_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    if (gui.live_area_contents.contains(app_path))
        gui.live_area_contents.erase(app_path);
    if (gui.live_items.contains(app_path))
        gui.live_items.erase(app_path);

    init_vita_app(gui, emuenv, app_path);
    save_apps_cache(gui, emuenv);

    if (get_live_area_current_open_apps_list_index(gui, app_path) != gui.live_area_current_open_apps_list.end())
        init_live_area(gui, emuenv, app_path);
}

bool get_remote_update_info(GuiState &gui, EmuEnvState &emuenv, const std::string &id) {
    const auto &APP_INDEX = get_app_index(gui, fmt::format("ux0:app/{}", id));
    if (!APP_INDEX) {
        LOG_ERROR("App index not found for id: {}", id);
        return false;
    }

    const auto TITLE_ID = APP_INDEX->title_id;
    const auto ver_xml_path = emuenv.pref_path / "ux0/bgdl" / fmt::format("{}-ver.xml", TITLE_ID);
    gui.new_update_infos[id] = {};

    pugi::xml_document doc;
    if (doc.load_file(ver_xml_path.string().c_str())) {
        const auto titlepatch = doc.child("titlepatch");
        const std::string titleid = titlepatch.attribute("titleid").as_string();

        const auto tag_child = titlepatch.child("tag");
        std::vector<std::string> package_updates_version;

        for (const auto package : tag_child.children("package")) {
            std::string version = package.attribute("version").as_string();
            if (version.empty()) {
                LOG_ERROR("Package version is empty for title ID: {}", titleid);
                return false;
            }
            if (version[0] == '0') {
                version.erase(version.begin());
            }
            LOG_DEBUG("package update info for title ID: {}, version: {}", titleid, version);
            package_updates_version.emplace_back(version);
        }

        size_t size = 0;
        std::string url;
        std::string content_id;
        const std::string version = package_updates_version.back();

        const auto last_package_child = tag_child.last_child();
        const auto hybrid_package_child = last_package_child.child("hybrid_package");
        if (((package_updates_version.size() >= 2) && (package_updates_version[package_updates_version.size() - 2] == APP_INDEX->app_ver))
            || hybrid_package_child.empty()) {
            size = last_package_child.attribute("size").as_uint();
            url = last_package_child.attribute("url").as_string();
            content_id = last_package_child.attribute("content_id").as_string();
            LOG_INFO("Preview update is current version or hybrid package no exist: {}, new version: {}", APP_INDEX->app_ver, version);
        } else {
            size = hybrid_package_child.attribute("size").as_uint();
            url = hybrid_package_child.attribute("url").as_string();
            content_id = hybrid_package_child.attribute("content_id").as_string();
            LOG_INFO("Found hybrid package update for title ID: {}, version: {}, size: {}, url: {}", TITLE_ID, version, size, url);
        }

        const auto paramsfo_child = last_package_child.child("paramsfo");
        if (paramsfo_child.empty()) {
            LOG_ERROR("Paramsfo child is empty for title ID: {}", TITLE_ID);
            return false;
        }
        const auto title_lang = fmt::format("title_{:02x}", emuenv.cfg.sys_lang);
        const auto title = !paramsfo_child.child(title_lang).empty() ? title_lang : "title";
        const std::string title_str = paramsfo_child.child(title).text().as_string();
        if (title_str.empty()) {
            LOG_ERROR("Title string is empty for title ID: {}", TITLE_ID);
            return false;
        }

        // Final assignment of new update info
        gui.new_update_infos[id] = { titleid, version, size, url, content_id, title_str };
    }

    return gui.new_update_infos.contains(id);
}

static void get_remote_update_history(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const auto &APP_INDEX = get_app_index(gui, app_path);
    const auto TITLE_ID = APP_INDEX->title_id;

    const auto id = fs::path(app_path).stem().string();
    if (!gui.new_update_infos.contains(id))
        get_remote_update_info(gui, emuenv, id);

    const auto ver_xml_path = emuenv.pref_path / "ux0/bgdl" / fmt::format("{}-ver.xml", TITLE_ID);

    pugi::xml_document doc;
    if (doc.load_file(ver_xml_path.string().c_str())) {
        const auto tag_child = doc.child("titlepatch").child("tag");
        const auto last_package_child = tag_child.last_child();
        const auto changeinfo_lang = fmt::format("changeinfo_{:02x}", emuenv.cfg.sys_lang);
        const auto changeinfo = !last_package_child.child(changeinfo_lang).empty() ? changeinfo_lang : "changeinfo";
        const std::string changeinfo_url = last_package_child.child(changeinfo).attribute("url").as_string();
        if (changeinfo_url.empty()) {
            LOG_ERROR("Change info URL is empty for title ID: {}", TITLE_ID);
            return;
        }
        const auto changeinfo_str = net_utils::get_web_response(changeinfo_url);
        if (changeinfo_str.empty()) {
            LOG_ERROR("Failed to fetch change info for title ID: {}", TITLE_ID);
            return;
        }

        if (get_update_history_infos(changeinfo_str, APP_INDEX->app_ver))
            gui.vita_area.update_history_info = true;
        else
            LOG_ERROR("Failed to parse update history for title ID: {}", TITLE_ID);
    }
}

std::mutex has_update_mutex;

static void refresh_and_check_patch(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    gui.app_has_update.erase(app_path);

    std::thread update_thread([&gui, &emuenv, app_path]() {
        const auto &APP_INDEX = get_app_index(gui, app_path);
        const auto TITLE_ID = APP_INDEX->title_id;

        const auto resolved_ver_xml_url = resolve_ver_xml_url(TITLE_ID);
        if (resolved_ver_xml_url.empty()) {
            LOG_ERROR("Failed to resolve version XML URL for title ID: {}", TITLE_ID);
            return;
        }

        const auto bgdl_path = emuenv.pref_path / "ux0/bgdl";
        fs::create_directories(bgdl_path);
        const auto ver_xml_path = bgdl_path / fs::path(resolved_ver_xml_url).filename();
        fs::remove(ver_xml_path);
        if (!net_utils::download_file(resolved_ver_xml_url, ver_xml_path.string())) {
            LOG_WARN("Failed to download or find ver.xml for title ID: {}", TITLE_ID);
            return;
        }

        if (check_has_patch(emuenv, TITLE_ID, APP_INDEX->app_ver)) {
            std::lock_guard<std::mutex> lock(has_update_mutex);
            gui.app_has_update[app_path] = true;
        }
    });
    update_thread.detach();
}

void close_live_area_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    if (app_path == emuenv.io.app_path) {
        update_time_app_used(gui, emuenv, app_path);
        emuenv.kernel.exit_delete_all_threads();
        emuenv.load_exec = true;
        // make sure we are not stuck waiting for a gpu command
        emuenv.renderer->should_display = true;
    } else {
        gui.live_area_current_open_apps_list.erase(get_live_area_current_open_apps_list_index(gui, app_path));
        --gui.live_area_app_current_open;
    }
}

enum LiveAreaType {
    GATE,
    SEARCH,
    MANUAL,
};

static constexpr ImU32 ARROW_COLOR = 0xFFFFFFFF; // White
static LiveAreaType live_area_type_selected = GATE;

void browse_live_area_apps_list(GuiState &gui, EmuEnvState &emuenv, const uint32_t button) {
    const auto &APP_PATH = gui.live_area_current_open_apps_list[gui.live_area_app_current_open];
    const auto manual_path{ emuenv.pref_path / convert_path(APP_PATH) / "sce_sys/manual" };
    const auto manual_found = fs::exists(manual_path) && !fs::is_empty(manual_path);

    if (!gui.is_nav_button) {
        if ((live_area_type_selected == MANUAL) && !manual_found)
            live_area_type_selected = GATE;
        gui.is_nav_button = true;
        return;
    }

    const auto live_area_current_open_apps_list_size = static_cast<int32_t>(gui.live_area_current_open_apps_list.size() - 1);

    const auto cancel = [&]() {
        close_live_area_app(gui, emuenv, APP_PATH);
    };
    const auto confirm = [&]() {
        switch (live_area_type_selected) {
        case GATE:
            if (emuenv.io.app_path.empty() || (APP_PATH == emuenv.io.app_path))
                gui.gate_animation.start(GateAnimationState::EnterApp);
            else
                pre_run_app(gui, emuenv, APP_PATH);
            break;
        case SEARCH:
            open_search(get_app_index(gui, APP_PATH)->title);
            break;
        case MANUAL:
            open_manual(gui, emuenv, APP_PATH);
            break;
        default:
            break;
        }
    };

    switch (button) {
    case SCE_CTRL_UP: {
        if (manual_found)
            live_area_type_selected = MANUAL;
        break;
    }
    case SCE_CTRL_DOWN:
        live_area_type_selected = GATE;
        break;
    case SCE_CTRL_LEFT:
    case SCE_CTRL_L1:
        if ((button & SCE_CTRL_LEFT) && live_area_type_selected == MANUAL)
            live_area_type_selected = SEARCH;
        else {
            gui.live_area_app_current_open = std::max(gui.live_area_app_current_open - 1, -1);
            live_area_type_selected = GATE;
        }
        break;
    case SCE_CTRL_RIGHT:
    case SCE_CTRL_R1:
        if ((button & SCE_CTRL_RIGHT) && live_area_type_selected == SEARCH)
            live_area_type_selected = MANUAL;
        else if (gui.live_area_app_current_open < live_area_current_open_apps_list_size) {
            ++gui.live_area_app_current_open;
            live_area_type_selected = GATE;
        }
        break;
    case SCE_CTRL_CIRCLE:
        if (emuenv.cfg.sys_button == 1)
            cancel();
        else
            confirm();
        break;
    case SCE_CTRL_CROSS:
        if (emuenv.cfg.sys_button == 1)
            confirm();
        else
            cancel();
        break;
    default:
        break;
    }
}

void refresh_live_area(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const auto &id = fs::path(app_path).stem().string();
    if (gui.live_items.contains(app_path)) {
        gui.live_area_contents.erase(app_path);
        gui.live_items.erase(app_path);
        init_live_area(gui, emuenv, app_path);
    }
    gui.updates_install.erase(id);
    gui.app_has_update.erase(app_path);
}

void draw_live_area_screen(GuiState &gui, EmuEnvState &emuenv, const uint32_t app_index) {
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    const auto &app_path = gui.live_area_current_open_apps_list[app_index];
    const VitaIoDevice app_device = device::get_device(app_path);

    const auto INFO_BAR_HEIGHT = 32.f * SCALE.y;

    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INFO_BAR_HEIGHT);

    const auto &id = fs::path(app_path).stem().string();
    if (gui.updates_install.contains(id) && (gui.updates_install[id].state == UpdateState::SUCCESS))
        refresh_live_area(gui, emuenv, app_path);

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
    if (gui.is_nav_button)
        flags |= ImGuiWindowFlags_NoMouseInputs;
    ImGui::BeginChild(fmt::format("##live_area_{}", app_index).c_str(), WINDOW_SIZE, 1, flags);
    ImGui::PopStyleVar(3);
    const auto SCREEN_POS = ImGui::GetCursorScreenPos();

    const auto is_current_app = (app_path == emuenv.io.app_path);
    const bool is_current_app_visible = (gui.live_area_app_current_open == app_index);

    const auto &app_type = type[app_path];
    LOG_WARN_IF(!items_styles.contains(app_type), "Type of style {} no found for: {}", app_type, app_path);

    const auto gate_pos = items_styles[app_type].gate_pos;

    const auto GATE_SIZE = ImVec2(280.0f * SCALE.x, 158.0f * SCALE.y);
    const auto GATE_POS = ImVec2(WINDOW_SIZE.x - (gate_pos.x * SCALE.x), WINDOW_SIZE.y - (gate_pos.y * SCALE.y));
    const ImVec2 GATE_POS_MIN(SCREEN_POS.x + GATE_POS.x, SCREEN_POS.y + GATE_POS.y);
    const ImVec2 GATE_POS_MAX(GATE_POS_MIN.x + GATE_SIZE.x, GATE_POS_MIN.y + GATE_SIZE.y);

    // Set gate frame positions
    ImVec2 gate_screen_pos_min = GATE_POS_MIN;
    ImVec2 gate_screen_pos_max = GATE_POS_MAX;
    ImVec2 gate_local_pos = GATE_POS;

    float frame_alpha = 1.f;
    const auto COLOR_PULSE_BORDER = get_selectable_color_pulse();

    const auto is_current_app_visible_animated = is_current_app_visible && (gui.gate_animation.state != GateAnimationState::None);

    if (is_current_app_visible_animated) {
        gui.gate_animation.update();

        const auto ImLerp = [](const ImVec2 &a, const ImVec2 &b, float t) {
            return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
        };

        switch (gui.gate_animation.state) {
        case GateAnimationState::EnterApp:
        case GateAnimationState::PreRunApp:
            gate_screen_pos_min = ImLerp(GATE_POS_MIN, ImVec2(0.f, 0.f), gui.gate_animation.progress);
            gate_screen_pos_max = ImLerp(GATE_POS_MAX, VIEWPORT_SIZE, gui.gate_animation.progress);
            gate_local_pos = ImLerp(GATE_POS, ImVec2(0.f, 0.f), gui.gate_animation.progress);
            if (gui.gate_animation.progress >= 0.8f)
                frame_alpha -= ((gui.gate_animation.progress - 0.8f) / 0.2f);
            break;
        case GateAnimationState::ReturnApp:
        case GateAnimationState::None:
            gate_screen_pos_min = ImLerp(ImVec2(0.f, 0.f), GATE_POS_MIN, gui.gate_animation.progress);
            gate_screen_pos_max = ImLerp(VIEWPORT_SIZE, GATE_POS_MAX, gui.gate_animation.progress);
            gate_local_pos = ImLerp(ImVec2(0.f, 0.f), GATE_POS, gui.gate_animation.progress);
            frame_alpha = gui.gate_animation.progress;
            break;
        default:
            LOG_ERROR("Unhandled gate animation state: {}", (int)gui.gate_animation.state);
            break;
        }

        frame_alpha = std::clamp(frame_alpha, 0.f, 1.f);
    }

    const auto is_zoom_animation = is_current_app_visible && ((gui.gate_animation.state == GateAnimationState::EnterApp) || (gui.gate_animation.state == GateAnimationState::PreRunApp));

    const ImVec2 gate_size = ImVec2(gate_screen_pos_max.x - gate_screen_pos_min.x, gate_screen_pos_max.y - gate_screen_pos_min.y);
    const ImVec2 ZOOM_RATIO = is_zoom_animation ? ImVec2(gate_size.x / GATE_SIZE.x, gate_size.y / GATE_SIZE.y) : ImVec2(1.f, 1.f);
    const ImVec2 GATE_ORIGIN = is_zoom_animation ? gate_screen_pos_min : GATE_POS_MIN;

    const auto set_screen_pos = [&](const ImVec2 &offset) -> ImVec2 {
        ImVec2 local(SCREEN_POS.x + offset.x - GATE_POS_MIN.x, SCREEN_POS.y + offset.y - GATE_POS_MIN.y);
        return ImVec2(GATE_ORIGIN.x + (local.x * ZOOM_RATIO.x), GATE_ORIGIN.y + (local.y * ZOOM_RATIO.y));
    };

    const auto set_local_pos = [&](const ImVec2 &offset) -> ImVec2 {
        ImVec2 local(offset.x - GATE_POS.x, offset.y - GATE_POS.y);
        return ImVec2(GATE_ORIGIN.x + (local.x * ZOOM_RATIO.x), GATE_ORIGIN.y + (local.y * ZOOM_RATIO.y));
    };

    const auto apply_zoom_size = [&](const ImVec2 &size) -> ImVec2 {
        return ImVec2(size.x * ZOOM_RATIO.x, size.y * ZOOM_RATIO.y);
    };

    const auto &WINDOW_DRAW_LIST = ImGui::GetWindowDrawList();
    const auto &FG_DRAW_LIST = ImGui::GetForegroundDrawList();

    // Always use a single rendering method, with the appropriate draw_list
    const auto &DRAW_LIST = is_zoom_animation ? FG_DRAW_LIST : WINDOW_DRAW_LIST;

    const ImVec2 background_pos = ImVec2(900.f * SCALE.x, 500.f * SCALE.y);
    const ImVec2 background_size = ImVec2(840.f * SCALE.x, 500.f * SCALE.y);

    const ImVec2 BG_POS_MIN(set_screen_pos(ImVec2(WINDOW_SIZE.x - background_pos.x, WINDOW_SIZE.y - background_pos.y)));
    const auto BG_SIZE = apply_zoom_size(background_size);
    const ImVec2 BG_POS_MAX(BG_POS_MIN.x + BG_SIZE.x, BG_POS_MIN.y + BG_SIZE.y);

    if (gui.live_area_contents[app_path].contains("livearea-background"))
        DRAW_LIST->AddImage(gui.live_area_contents[app_path]["livearea-background"], BG_POS_MIN, BG_POS_MAX);
    else
        DRAW_LIST->AddRectFilled(BG_POS_MIN, BG_POS_MAX, IM_COL32(148, 164, 173, 255), 0.f, ImDrawFlags_RoundCornersAll);

    for (const auto &frame : frames[app_path]) {
        if (frame.autoflip != 0) {
            if (last_time[app_path][frame.id] == 0)
                last_time[app_path][frame.id] = current_time();

            while (last_time[app_path][frame.id] + frame.autoflip < current_time()) {
                last_time[app_path][frame.id] += frame.autoflip;

                if (gui.live_items[app_path][frame.id].contains("background")) {
                    if (current_item[app_path][frame.id] != gui.live_items[app_path][frame.id]["background"].size() - 1)
                        ++current_item[app_path][frame.id];
                    else
                        current_item[app_path][frame.id] = 0;
                } else if (gui.live_items[app_path][frame.id].contains("image")) {
                    if (current_item[app_path][frame.id] != gui.live_items[app_path][frame.id]["image"].size() - 1)
                        ++current_item[app_path][frame.id];
                    else
                        current_item[app_path][frame.id] = 0;
                }
            }
        }

        if (!items_styles[app_type].frames.contains(frame.id))
            continue;

        const auto FRAME = items_styles[app_type].frames[frame.id];
        const auto FRAME_SIZE = FRAME.size;

        auto FRAME_POS = ImVec2(FRAME.pos.x * SCALE.x, FRAME.pos.y * SCALE.y);

        auto bg_size = items_size[app_path][frame.id]["background"];

        // Resize items
        const auto bg_resize = ImVec2(bg_size.x / FRAME_SIZE.x, bg_size.y / FRAME_SIZE.y);
        if (bg_size.x > FRAME_SIZE.x)
            bg_size.x /= bg_resize.x;
        if (bg_size.y > FRAME_SIZE.y)
            bg_size.y /= bg_resize.y;

        auto img_size = items_size[app_path][frame.id]["image"];
        const auto img_resize = ImVec2(img_size.x / FRAME_SIZE.x, img_size.y / FRAME_SIZE.y);
        if (img_size.x > FRAME_SIZE.x)
            img_size.x /= img_resize.x;
        if (img_size.y > FRAME_SIZE.y)
            img_size.y /= img_resize.y;

        // Items pos init (Center)
        auto bg_pos_init = ImVec2((FRAME_SIZE.x - bg_size.x) / 2.f, (FRAME_SIZE.y - bg_size.y) / 2.f);
        auto img_pos_init = ImVec2((FRAME_SIZE.x - img_size.x) / 2.f, (FRAME_SIZE.y - img_size.y) / 2.f);

        // Align items
        if ((liveitem[app_path][frame.id]["background"]["align"].second == "left") && (liveitem[app_path][frame.id]["background"]["x"].first >= 0))
            bg_pos_init.x = 0.0f;
        else if ((liveitem[app_path][frame.id]["background"]["align"].second == "right") && (liveitem[app_path][frame.id]["background"]["x"].first <= 0))
            bg_pos_init.x = FRAME_SIZE.x - bg_size.x;
        else
            bg_pos_init.x += liveitem[app_path][frame.id]["background"]["x"].first;

        if ((liveitem[app_path][frame.id]["image"]["align"].second == "left") && (liveitem[app_path][frame.id]["image"]["x"].first >= 0))
            img_pos_init.x = 0.0f;
        else if ((liveitem[app_path][frame.id]["image"]["align"].second == "right") && (liveitem[app_path][frame.id]["image"]["x"].first <= 0))
            img_pos_init.x = FRAME_SIZE.x - img_size.x;
        else
            img_pos_init.x += liveitem[app_path][frame.id]["image"]["x"].first;

        // Valign items
        if ((liveitem[app_path][frame.id]["background"]["valign"].second == "top") && (liveitem[app_path][frame.id]["background"]["y"].first <= 0))
            bg_pos_init.y = 0.0f;
        else if ((liveitem[app_path][frame.id]["background"]["valign"].second == "bottom") && (liveitem[app_path][frame.id]["background"]["y"].first >= 0))
            bg_pos_init.y = FRAME_SIZE.y - bg_size.y;
        else
            bg_pos_init.y -= liveitem[app_path][frame.id]["background"]["y"].first;

        if ((liveitem[app_path][frame.id]["image"]["valign"].second == "top") && (liveitem[app_path][frame.id]["image"]["y"].first <= 0))
            img_pos_init.y = 0.0f;
        else if ((liveitem[app_path][frame.id]["image"]["valign"].second == "bottom") && (liveitem[app_path][frame.id]["image"]["y"].first >= 0))
            img_pos_init.y = FRAME_SIZE.y - img_size.y;
        else
            img_pos_init.y -= liveitem[app_path][frame.id]["image"]["y"].first;

        // Set items pos
        auto bg_pos = ImVec2((WINDOW_SIZE.x - FRAME_POS.x) + (bg_pos_init.x * SCALE.x), (WINDOW_SIZE.y - FRAME_POS.y) + (bg_pos_init.y * SCALE.y));

        auto img_pos = ImVec2((WINDOW_SIZE.x - FRAME_POS.x) + (img_pos_init.x * SCALE.x), (WINDOW_SIZE.y - FRAME_POS.y) + (img_pos_init.y * SCALE.y));

        if (bg_size.x == FRAME_SIZE.x)
            bg_pos.x = WINDOW_SIZE.x - FRAME_POS.x;
        if (bg_size.y == FRAME_SIZE.y)
            bg_pos.y = WINDOW_SIZE.y - FRAME_POS.y;

        if (img_size.x == FRAME_SIZE.x)
            img_pos.x = WINDOW_SIZE.x - FRAME_POS.x;
        if (img_size.y == FRAME_SIZE.y)
            img_pos.y = WINDOW_SIZE.y - FRAME_POS.y;

        // Scale size items
        auto bg_scal_size = ImVec2(bg_size.x * SCALE.x, bg_size.y * SCALE.y);
        auto img_scal_size = ImVec2(img_size.x * SCALE.x, img_size.y * SCALE.y);

        const auto pos_frame = ImVec2(WINDOW_SIZE.x - FRAME_POS.x, WINDOW_SIZE.y - FRAME_POS.y);

        // Scale size frame
        const auto scal_size_frame = ImVec2(FRAME_SIZE.x * SCALE.x, FRAME_SIZE.y * SCALE.y);

        // Reset position if get outside frame
        if ((bg_pos.x + bg_scal_size.x) > (pos_frame.x + scal_size_frame.x))
            bg_pos.x += (pos_frame.x + scal_size_frame.x) - (bg_pos.x + bg_scal_size.x);
        if (bg_pos.x < pos_frame.x)
            bg_pos.x += pos_frame.x - bg_pos.x;

        if ((bg_pos.y + bg_scal_size.y) > (pos_frame.y + scal_size_frame.y))
            bg_pos.y += (pos_frame.y + scal_size_frame.y) - (bg_pos.y + bg_scal_size.y);
        if (bg_pos.y < pos_frame.y)
            bg_pos.y += pos_frame.y - bg_pos.y;

        if ((img_pos.x + img_scal_size.x) > (pos_frame.x + scal_size_frame.x))
            img_size.x += (pos_frame.x + scal_size_frame.x) - (img_pos.x + img_scal_size.x);
        if (img_pos.x < pos_frame.x)
            img_pos.x += pos_frame.x - img_pos.x;

        if ((img_pos.y + img_scal_size.y) > (pos_frame.y + scal_size_frame.y))
            img_pos.y += (pos_frame.y + scal_size_frame.y) - (img_pos.y + img_scal_size.y);
        if (img_pos.y < pos_frame.y)
            img_pos.y += pos_frame.y - img_pos.y;

        // Display items
        if (gui.live_items[app_path][frame.id].contains("background")) {
            const auto background_pos_min = set_screen_pos(bg_pos);
            bg_scal_size = apply_zoom_size(bg_scal_size);
            const ImVec2 background_pos_max(background_pos_min.x + bg_scal_size.x, background_pos_min.y + bg_scal_size.y);
            DRAW_LIST->AddImage(gui.live_items[app_path][frame.id]["background"][current_item[app_path][frame.id]], background_pos_min, background_pos_max);
        }

        if (gui.live_items[app_path][frame.id].contains("image")) {
            const auto image_pos_min = set_screen_pos(img_pos);
            img_scal_size = apply_zoom_size(img_scal_size);
            const ImVec2 image_pos_max(image_pos_min.x + img_scal_size.x, image_pos_min.y + img_scal_size.y);
            DRAW_LIST->AddImage(gui.live_items[app_path][frame.id]["image"][current_item[app_path][frame.id]], image_pos_min, image_pos_max);
        }

        // Target link
        if (!target[app_path][frame.id].empty() && (target[app_path][frame.id].find("psts:") == std::string::npos)) {
            ImGui::SetCursorPos(pos_frame);
            ImGui::PushID(frame.id.c_str());
            if (ImGui::Selectable("##target_link", false, ImGuiSelectableFlags_None, scal_size_frame))
                open_path(target[app_path][frame.id]);
            ImGui::PopID();
        }

        // Text
        for (const auto &str_tag : str[app_path][frame.id]) {
            if (!str_tag.text.empty()) {
                std::vector<ImVec4> str_color;

                if (!str_tag.color.empty()) {
                    uint32_t color = 0xFF'FF'FF'FF;

                    if (frame.autoflip)
                        std::istringstream{ str[app_path][frame.id][current_item[app_path][frame.id]].color }.ignore(1, '#') >> std::hex >> color;
                    else
                        std::istringstream{ str_tag.color }.ignore(1, '#') >> std::hex >> color;

                    str_color.emplace_back(((color >> 16) & 0xFF) / 255.f, ((color >> 8) & 0xFF) / 255.f, ((color >> 0) & 0xFF) / 255.f, 1.f);
                } else
                    str_color.emplace_back(1.f, 1.f, 1.f, 1.f);

                auto str_size = scal_size_frame, text_pos = pos_frame;

                // Origin
                if (liveitem[app_path][frame.id]["text"]["origin"].second.empty() || (liveitem[app_path][frame.id]["text"]["origin"].second == "background")) {
                    if (gui.live_items[app_path][frame.id].contains("background")) {
                        str_size = bg_scal_size;
                        text_pos = bg_pos;
                    } else if (!liveitem[app_path][frame.id]["text"]["origin"].second.empty() && gui.live_items[app_path][frame.id].contains("image")) {
                        str_size = img_scal_size;
                        text_pos = img_pos;
                    }
                } else if (liveitem[app_path][frame.id]["text"]["origin"].second == "image") {
                    if (gui.live_items[app_path][frame.id].contains("image")) {
                        str_size = img_scal_size;
                        text_pos = img_pos;
                    } else if (gui.live_items[app_path][frame.id].contains("background")) {
                        str_size = bg_scal_size;
                        text_pos = bg_pos;
                    }
                }

                auto str_wrap = scal_size_frame.x;
                if (liveitem[app_path][frame.id]["text"]["align"].second == "outside-right")
                    str_wrap = str_size.x;

                if (liveitem[app_path][frame.id]["text"]["width"].first > 0) {
                    if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                        str_wrap = static_cast<float>(liveitem[app_path][frame.id]["text"]["width"].first) * SCALE.x;
                    text_pos.x += (str_size.x - (static_cast<float>(liveitem[app_path][frame.id]["text"]["width"].first) * SCALE.x)) / 2.f;
                    str_size.x = static_cast<float>(liveitem[app_path][frame.id]["text"]["width"].first) * SCALE.x;
                }

                if ((liveitem[app_path][frame.id]["text"]["height"].first > 0)
                    && (liveitem[app_path][frame.id]["text"]["word-scroll"].second == "on" || liveitem[app_path][frame.id]["text"]["height"].first <= FRAME_SIZE.y)) {
                    text_pos.y += (str_size.y - (static_cast<float>(liveitem[app_path][frame.id]["text"]["height"].first) * SCALE.y)) / 2.f;
                    str_size.y = static_cast<float>(liveitem[app_path][frame.id]["text"]["height"].first) * SCALE.y;
                }

                const auto size_text_scale = str_tag.size != 0 ? str_tag.size / 19.2f : 1.f;
                ImGui::SetWindowFontScale(size_text_scale * RES_SCALE.x);

                // Calculate text pixel size
                ImVec2 calc_text_size;
                if (frame.autoflip > 0) {
                    if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                        calc_text_size = ImGui::CalcTextSize(str[app_path][frame.id][current_item[app_path][frame.id]].text.c_str(), 0, false, str_wrap);
                    else
                        calc_text_size = ImGui::CalcTextSize(str[app_path][frame.id][current_item[app_path][frame.id]].text.c_str());
                } else {
                    if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                        calc_text_size = ImGui::CalcTextSize(str_tag.text.c_str(), 0, false, str_wrap);
                    else
                        calc_text_size = ImGui::CalcTextSize(str_tag.text.c_str());
                }

                if (liveitem[app_path][frame.id]["text"]["ellipsis"].second == "on") {
                    LOG_DEBUG("Ellipsis on");
                    // TODO ellipsis
                }

                ImVec2 str_pos_init;

                // Align
                if (liveitem[app_path][frame.id]["text"]["align"].second.empty()) {
                    if (liveitem[app_path][frame.id]["text"]["text-align"].second.empty()) {
                        if (liveitem[app_path][frame.id]["text"]["line-align"].second == "left")
                            str_pos_init.x = 0.f;
                        else if (liveitem[app_path][frame.id]["text"]["line-align"].second == "right")
                            str_pos_init.x = str_size.x - calc_text_size.x;
                        else if (liveitem[app_path][frame.id]["text"]["line-align"].second == "outside-right") {
                            text_pos.x += str_size.x;
                            if (liveitem[app_path][frame.id]["text"]["origin"].second == "image") {
                                if (gui.live_items[app_path][frame.id].contains("background"))
                                    str_size.x = bg_scal_size.x - img_scal_size.x - (img_pos.x - bg_pos.x);
                                else {
                                    str_size = scal_size_frame;
                                    text_pos = pos_frame;
                                }
                            }
                        } else
                            str_pos_init.x = (str_size.x - calc_text_size.x) / 2.0f;
                    } else {
                        if ((liveitem[app_path][frame.id]["text"]["text-align"].second == "center")
                            || ((liveitem[app_path][frame.id]["text"]["text-align"].second == "left") && (liveitem[app_path][frame.id]["text"]["x"].first < 0)))
                            str_pos_init.x = (str_size.x - calc_text_size.x) / 2.0f;
                        else if (liveitem[app_path][frame.id]["text"]["text-align"].second == "left")
                            str_pos_init.x = 0.f;
                        else if (liveitem[app_path][frame.id]["text"]["text-align"].second == "right")
                            str_pos_init.x = str_size.x - calc_text_size.x;
                        else if (liveitem[app_path][frame.id]["text"]["text-align"].second == "outside-right") {
                            text_pos.x += str_size.x;
                            if (liveitem[app_path][frame.id]["text"]["origin"].second == "image") {
                                if (gui.live_items[app_path][frame.id].contains("background"))
                                    str_size.x = bg_scal_size.x - img_scal_size.x - (img_pos.x - bg_pos.x);
                                else {
                                    str_size = scal_size_frame;
                                    text_pos = pos_frame;
                                }
                            }
                        }
                    }
                } else {
                    if (liveitem[app_path][frame.id]["text"]["align"].second == "center")
                        str_pos_init.x = (str_size.x - calc_text_size.x) / 2.0f;
                    else if (liveitem[app_path][frame.id]["text"]["align"].second == "left")
                        str_pos_init.x = 0.f;
                    else if (liveitem[app_path][frame.id]["text"]["align"].second == "right")
                        str_pos_init.x = str_size.x - calc_text_size.x;
                    else if (liveitem[app_path][frame.id]["text"]["align"].second == "outside-right") {
                        text_pos.x += str_size.x;
                        if (liveitem[app_path][frame.id]["text"]["origin"].second == "image") {
                            if (gui.live_items[app_path][frame.id].contains("background"))
                                str_size.x = bg_scal_size.x - img_scal_size.x - (img_pos.x - bg_pos.x);
                            else {
                                str_size = scal_size_frame;
                                text_pos = pos_frame;
                            }
                        }
                    }
                }

                // Valign
                if (liveitem[app_path][frame.id]["text"]["valign"].second.empty()) {
                    if (liveitem[app_path][frame.id]["text"]["text-valign"].second.empty() || (liveitem[app_path][frame.id]["text"]["text-valign"].second == "center"))
                        str_pos_init.y = (str_size.y - calc_text_size.y) / 2.f;
                    else if (liveitem[app_path][frame.id]["text"]["text-valign"].second == "bottom")
                        str_pos_init.y = str_size.y - calc_text_size.y;
                    else if (liveitem[app_path][frame.id]["text"]["text-valign"].second == "top")
                        str_pos_init.y = 0.f;
                } else {
                    if ((liveitem[app_path][frame.id]["text"]["valign"].second == "center")
                        || ((liveitem[app_path][frame.id]["text"]["valign"].second == "bottom") && (liveitem[app_path][frame.id]["text"]["y"].first != 0))
                        || ((liveitem[app_path][frame.id]["text"]["valign"].second == "top") && (liveitem[app_path][frame.id]["text"]["y"].first != 0)))
                        str_pos_init.y = (str_size.y - calc_text_size.y) / 2.f;
                    else if (liveitem[app_path][frame.id]["text"]["valign"].second == "bottom")
                        str_pos_init.y = str_size.y - calc_text_size.y;
                    else if (liveitem[app_path][frame.id]["text"]["valign"].second == "top") {
                        str_pos_init.y = 0.f;
                    } else if (liveitem[app_path][frame.id]["text"]["valign"].second == "outside-top") {
                        str_pos_init.y = 0.f;
                    }
                }

                auto pos_str = ImVec2(str_pos_init.x, str_pos_init.y - (liveitem[app_path][frame.id]["text"]["y"].first * SCALE.y));

                if (liveitem[app_path][frame.id]["text"]["x"].first > 0) {
                    text_pos.x += liveitem[app_path][frame.id]["text"]["x"].first * SCALE.x;
                    str_size.x -= liveitem[app_path][frame.id]["text"]["x"].first * SCALE.x;
                }

                if ((liveitem[app_path][frame.id]["text"]["margin-left"].first > 0) && !liveitem[app_path][frame.id]["text"]["width"].first) {
                    text_pos.x += liveitem[app_path][frame.id]["text"]["margin-left"].first * SCALE.x;
                    str_size.x -= liveitem[app_path][frame.id]["text"]["margin-left"].first * SCALE.x;
                }

                if (liveitem[app_path][frame.id]["text"]["margin-right"].first > 0)
                    str_size.x -= liveitem[app_path][frame.id]["text"]["margin-right"].first * SCALE.x;

                if (liveitem[app_path][frame.id]["text"]["margin-top"].first > 0) {
                    text_pos.y += liveitem[app_path][frame.id]["text"]["margin-top"].first * SCALE.y;
                    str_size.y -= liveitem[app_path][frame.id]["text"]["margin-top"].first * SCALE.y;
                }

                // Text Display
                // TODO Multiple color support on same frame, used by eg: Asphalt: Injection
                // TODO Correct display few line on same frame, used by eg: Asphalt: Injection
                ImGui::SetNextWindowPos(set_screen_pos(text_pos));
                ImGui::BeginChild(frame.id.c_str(), str_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
                if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                    ImGui::PushTextWrapPos(str_wrap);
                if (liveitem[app_path][frame.id]["text"]["word-scroll"].second == "on") {
                    static std::map<std::string, std::map<std::string, std::pair<bool, float>>> scroll;
                    if (liveitem[app_path][frame.id]["text"]["word-wrap"].second == "off") {
                        if (scroll[app_path][frame.id].first) {
                            if (scroll[app_path][frame.id].second >= 0.f)
                                scroll[app_path][frame.id].second -= 0.5f;
                            else
                                scroll[app_path][frame.id].first = false;
                        } else {
                            if (scroll[app_path][frame.id].second <= ImGui::GetScrollMaxX())
                                scroll[app_path][frame.id].second += 0.5f;
                            else
                                scroll[app_path][frame.id].first = true;
                        }
                        ImGui::SetScrollX(scroll[app_path][frame.id].second);
                    } else {
                        if (scroll[app_path][frame.id].first) {
                            if (scroll[app_path][frame.id].second >= 0.f)
                                scroll[app_path][frame.id].second -= 0.5f;
                            else
                                scroll[app_path][frame.id].first = false;
                        } else {
                            if (scroll[app_path][frame.id].second <= ImGui::GetScrollMaxY())
                                scroll[app_path][frame.id].second += 0.5f;
                            else
                                scroll[app_path][frame.id].first = true;
                        }
                        ImGui::SetScrollY(scroll[app_path][frame.id].second);
                    }
                }
                ImGui::SetCursorPos(pos_str);
                if (liveitem[app_path][frame.id]["text"].contains("line-space") && !liveitem[app_path][frame.id]["text"]["line-space"].second.empty()) {
                    std::vector<std::string> lines;
                    std::istringstream iss(str[app_path][frame.id][current_item[app_path][frame.id]].text);
                    std::string line;
                    while (std::getline(iss, line)) {
                        lines.push_back(line);
                    }
                    for (const auto &line : lines) {
                        ImGui::TextColored(str_color[0], "%s", line.c_str());
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (std::stoi(liveitem[app_path][frame.id]["text"]["line-space"].second) * SCALE.y));
                    }
                } else {
                    if (frame.autoflip > 0) {
                        ImGui::TextColored(str_color[0], "%s", str[app_path][frame.id][current_item[app_path][frame.id]].text.c_str());
                        LOG_DEBUG("Autoflip: {}, str: {}", frame.autoflip, str[app_path][frame.id][current_item[app_path][frame.id]].text.c_str());
                    } else
                        ImGui::TextColored(str_color[0], "%s", str_tag.text.c_str());
                }
                if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                    ImGui::PopTextWrapPos();
                ImGui::EndChild();
                ImGui::SetWindowFontScale(RES_SCALE.x);
            }
        }
    }

    const auto BUTTON_SIZE = ImVec2(72.f * SCALE.x, 30.f * SCALE.y);

    ImGui::PushID(app_path.c_str());

    const auto &GATE_DRAW_LIST = is_current_app_visible_animated ? FG_DRAW_LIST : WINDOW_DRAW_LIST;

    // Draw the gate content
    if (is_current_app && gui.live_area_last_app_frame)
        GATE_DRAW_LIST->AddImage(gui.live_area_last_app_frame, gate_screen_pos_min, gate_screen_pos_max);
    else if (gui.live_area_contents[app_path].contains("gate"))
        GATE_DRAW_LIST->AddImage(gui.live_area_contents[app_path]["gate"], gate_screen_pos_min, gate_screen_pos_max);
    else {
        // Default gate background rendering
        GATE_DRAW_LIST->AddRectFilled(gate_screen_pos_min, gate_screen_pos_max, IM_COL32(47, 51, 50, 255), 10.0f * SCALE.x, ImDrawFlags_RoundCornersAll);

        const auto ICON_SIZE_SCALE = (94.f * SCALE.x) * ZOOM_RATIO.x;
        const auto ICON_CENTER_POS = ImVec2(gate_screen_pos_min.x + (gate_size.x / 2.f), gate_screen_pos_min.y + (15.5f * SCALE.y) + (ICON_SIZE_SCALE / 2.f));
        const auto ICON_POS_MIN = ImVec2(ICON_CENTER_POS.x - (ICON_SIZE_SCALE / 2.f), ICON_CENTER_POS.y - (ICON_SIZE_SCALE / 2.f));
        const auto ICON_POS_MAX = ImVec2(ICON_POS_MIN.x + ICON_SIZE_SCALE, ICON_POS_MIN.y + ICON_SIZE_SCALE);

        auto &APP_ICON_TYPE = app_path.starts_with("emu") ? gui.app_selector.emu_apps_icon : gui.app_selector.vita_apps_icon;
        if (APP_ICON_TYPE.contains(app_path))
            GATE_DRAW_LIST->AddImageRounded(APP_ICON_TYPE[app_path], ICON_POS_MIN, ICON_POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 75.f * SCALE.x, ImDrawFlags_RoundCornersAll);
        else
            GATE_DRAW_LIST->AddCircleFilled(ICON_CENTER_POS, ICON_SIZE_SCALE / 2.f, IM_COL32_WHITE);
    }

    const float FRAME_THICKNESS = (10.f * SCALE.x) * ZOOM_RATIO.x;

    // Draw gate frame
    if (gui.gate_animation.state != GateAnimationState::ReturnApp)
        DRAW_LIST->AddRect(gate_screen_pos_min, gate_screen_pos_max, IM_COL32(192, 192, 192, static_cast<int>(255 * frame_alpha)), (10.f * SCALE.x) * ZOOM_RATIO.x, ImDrawFlags_RoundCornersAll, FRAME_THICKNESS);

    ImGui::SetWindowFontScale(RES_SCALE.x);
    const std::string BUTTON_STR = is_current_app ? gui.lang.live_area.main["continue"] : gui.lang.live_area.main["start"];
    const auto default_font_scale = (25.f * emuenv.manual_dpi_scale) * (ImGui::GetFontSize() / (19.2f * emuenv.manual_dpi_scale));
    const auto font_size_scale = default_font_scale / ImGui::GetFontSize();

    // Interaction (button)
    const auto BUTTON_STR_SIZE = ImGui::CalcTextSize(BUTTON_STR.c_str());
    const auto START_SIZE = ImVec2(BUTTON_STR_SIZE.x * font_size_scale, BUTTON_STR_SIZE.y * font_size_scale);
    const auto START_BUTTON_SIZE = ImVec2(START_SIZE.x + (26.0f * SCALE.x), START_SIZE.y + (5.0f * SCALE.y));
    const auto POS_BUTTON = ImVec2((GATE_POS.x + (GATE_SIZE.x - START_BUTTON_SIZE.x) / 2.0f), (GATE_POS.y + (GATE_SIZE.y - START_BUTTON_SIZE.y) / 1.08f));
    const auto POS_START = set_screen_pos(ImVec2(POS_BUTTON.x + (START_BUTTON_SIZE.x - START_SIZE.x) / 2.f, POS_BUTTON.y + (START_BUTTON_SIZE.y - START_SIZE.y) / 2.f));
    const ImVec2 BUTTON_POS_MIN = set_screen_pos(POS_BUTTON);
    DRAW_LIST->AddRectFilled(BUTTON_POS_MIN, ImVec2(BUTTON_POS_MIN.x + (START_BUTTON_SIZE.x * ZOOM_RATIO.x), BUTTON_POS_MIN.y + (START_BUTTON_SIZE.y * ZOOM_RATIO.y)), IM_COL32(20, 168, 222, 255), (10.0f * SCALE.x) * ZOOM_RATIO.x, ImDrawFlags_RoundCornersAll);
    DRAW_LIST->AddText(gui.vita_font[emuenv.current_font_level], default_font_scale * ZOOM_RATIO.y, POS_START, IM_COL32(255, 255, 255, 255), BUTTON_STR.c_str());

    // Invisible button for gate interaction
    ImGui::SetCursorPos(GATE_POS);
    if (ImGui::InvisibleButton("##gate", GATE_SIZE)) {
        if (emuenv.io.app_path.empty() || is_current_app)
            gui.gate_animation.start(GateAnimationState::EnterApp);
        else
            pre_run_app(gui, emuenv, app_path);
    }

    if (ImGui::IsItemHovered())
        live_area_type_selected = GATE;

    if (live_area_type_selected == GATE) {
        const auto SELECT_THICKNESS = FRAME_THICKNESS / 2.f;
        const auto HALF_SELECT_THICKNESS = SELECT_THICKNESS / 2.f;
        const ImVec2 SELECT_POS_MIN(gate_screen_pos_min.x + HALF_SELECT_THICKNESS, gate_screen_pos_min.y + HALF_SELECT_THICKNESS);
        const ImVec2 SELECT_POS_MAX(SELECT_POS_MIN.x + gate_size.x - SELECT_THICKNESS, SELECT_POS_MIN.y + gate_size.y - SELECT_THICKNESS);

        DRAW_LIST->AddRect(SELECT_POS_MIN, SELECT_POS_MAX, COLOR_PULSE_BORDER, (10.f * SCALE.x) * ZOOM_RATIO.x, ImDrawFlags_RoundCornersAll, SELECT_THICKNESS);
    }

    // Launch app after animation completes
    if (gui.gate_animation.state == GateAnimationState::PreRunApp) {
        pre_run_app(gui, emuenv, gui.live_area_current_open_apps_list[gui.live_area_app_current_open]);
        gui.gate_animation.state = GateAnimationState::None;
    }

    if (app_device == VitaIoDevice::ux0) {
        auto APP_INDEX = get_app_index(gui, app_path);

        const auto widget_scal_size = apply_zoom_size(ImVec2(70.0f * SCALE.x, 70.f * SCALE.y));
        const auto manual_path{ emuenv.pref_path / convert_path(app_path) / "sce_sys/manual/" };
        const auto scal_widget_font_size = (20.0f / ImGui::GetFontSize()) * ZOOM_RATIO.x;
        const auto widget_font_size = (20.f * SCALE.x) * ZOOM_RATIO.x;
        const auto manual_exist = fs::exists(manual_path) && !fs::is_empty(manual_path);

        static std::string donwloading_update_msg;
        const auto process_update = [&]() {
            const auto &id = fs::path(app_path).stem().string();
            if (!gui.updates_install.contains(id) || (gui.updates_install[id].state == UpdateState::NONE)) {
                if (!gui.new_update_infos.contains(id))
                    get_remote_update_info(gui, emuenv, id);
                if ((gui.new_update_infos[id].size * 2) > fs::space(emuenv.pref_path).available) {
                    donwloading_update_msg = fmt::format(fmt::runtime(gui.lang.patch_check["not_enough_space"]), get_unit_size(gui.new_update_infos[id].size * 2));
                    ImGui::OpenPopup("downloading_update_popup");
                } else
                    get_remote_update_history(gui, emuenv, app_path);
            } else if (gui.updates_install[id].state == UpdateState::DOWNLOADING) {
                donwloading_update_msg = gui.lang.patch_check["downloading_app_update"];
                ImGui::OpenPopup("downloading_update_popup");
            } else if (gui.updates_install[id].state == UpdateState::WAITING_INSTALL) {
                update_install(gui, emuenv, id);
            }
        };

        std::vector<std::pair<std::string, std::function<void()>>> buttons;
        if (gui.app_has_update[app_path])
            buttons.emplace_back("Update", process_update);

        buttons.emplace_back("Search", [&]() { open_search(APP_INDEX->title); });

        if (manual_exist)
            buttons.emplace_back("Manual", [&]() { open_manual(gui, emuenv, app_path); });

        buttons.emplace_back("Refresh", [&]() { refresh_and_check_patch(gui, emuenv, app_path); });

        const auto BUTTONS_COUNT = static_cast<int>(buttons.size());
        const float spacing = 42.0f * SCALE.x;
        const ImVec2 BUTTONS_POS = ImVec2(
            (WINDOW_SIZE.x - (BUTTONS_COUNT * widget_scal_size.x) - (spacing * (BUTTONS_COUNT - 1))) / 2.f, WINDOW_SIZE.y - (505.0f * SCALE.y));

        for (int i = 0; i < BUTTONS_COUNT; i++) {
            ImVec2 button_pos_scal = ImVec2(BUTTONS_POS.x + i * (widget_scal_size.x + spacing), BUTTONS_POS.y);

            const auto title = buttons[i].first;
            const char *WIDGET_STR = title.c_str();
            const auto WIDGET_STR_SIZE = ImGui::CalcTextSize(WIDGET_STR);
            const auto WIDGET_STR_SCALE_SIZE = ImVec2((WIDGET_STR_SIZE.x * scal_widget_font_size) * SCALE.x, (WIDGET_STR_SIZE.y * scal_widget_font_size) * SCALE.y);
            const ImVec2 WIDGET_POS_MIN = set_screen_pos(button_pos_scal);
            const auto WIDGET_STR_POS = ImVec2(WIDGET_POS_MIN.x + ((widget_scal_size.x / 2.f) - (WIDGET_STR_SCALE_SIZE.x / 2.f)),
                WIDGET_POS_MIN.y + ((widget_scal_size.x / 2.f) - (WIDGET_STR_SCALE_SIZE.y / 2.f)));

            auto WIDGET_COLOR = IM_COL32(10, 169, 246, 255);
            if (title == "Update")
                WIDGET_COLOR = IM_COL32(249, 165, 4, 255);
            else if (title == "Manual")
                WIDGET_COLOR = IM_COL32(202, 0, 106, 255);

            DRAW_LIST->AddRectFilled(WIDGET_POS_MIN, ImVec2(WIDGET_POS_MIN.x + widget_scal_size.x, WIDGET_POS_MIN.y + widget_scal_size.y), WIDGET_COLOR, 12.0f * SCALE.x, ImDrawFlags_RoundCornersAll);
            DRAW_LIST->AddText(gui.vita_font[emuenv.current_font_level], widget_font_size, WIDGET_STR_POS, IM_COL32(255, 255, 255, 255), WIDGET_STR);
            ImGui::SetCursorPos(button_pos_scal);
            if (ImGui::Selectable(("##" + title).c_str(), false, ImGuiSelectableFlags_None, widget_scal_size)) {
                buttons[i].second();
            }
        }

        // Draw Downloading Popup
        const auto POPUP_SIZE = ImVec2(760.0f * SCALE.x, 436.0f * SCALE.y);
        ImGui::SetNextWindowSize(POPUP_SIZE, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + (VIEWPORT_SIZE.x / 2.f) - (POPUP_SIZE.x / 2.f), VIEWPORT_POS.y + (VIEWPORT_SIZE.y / 2.f) - (POPUP_SIZE.y / 2.f)), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 15.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 8.f * SCALE.x);
        if (ImGui::BeginPopupModal("downloading_update_popup", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration)) {
            ImGui::SetWindowFontScale(1.34f * RES_SCALE.y);
            const auto LARGE_BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);
            const auto str_size = ImGui::CalcTextSize(donwloading_update_msg.c_str(), 0, false, POPUP_SIZE.x - (172.f * SCALE.x));
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - (str_size.x / 2.f), (POPUP_SIZE.y / 2.f) - (str_size.y / 2.f) - (LARGE_BUTTON_SIZE.y / 2.f) - (22.f * SCALE.y)));
            ImGui::PushTextWrapPos(POPUP_SIZE.x - (86.f * SCALE.x));
            ImGui::Text("%s", donwloading_update_msg.c_str());
            ImGui::PopTextWrapPos();
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - (LARGE_BUTTON_SIZE.x / 2.f), POPUP_SIZE.y - LARGE_BUTTON_SIZE.y - (22.0f * SCALE.y)));
            ImGui::SetWindowFontScale(1.54f * RES_SCALE.y);
            if (ImGui::Button(emuenv.common_dialog.lang.common["ok"].c_str(), LARGE_BUTTON_SIZE)) {
                donwloading_update_msg.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(3);
    }
    ImGui::PopID();

    auto &lang = gui.lang.live_area.help;
    auto &common = emuenv.common_dialog.lang.common;

    if (!gui.vita_area.content_manager && !gui.vita_area.manual) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f * SCALE.x);
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - (60.0f * SCALE.x) - BUTTON_SIZE.x, 12.0f * SCALE.y));
        if (ImGui::Button("Esc", BUTTON_SIZE))
            close_live_area_app(gui, emuenv, app_path);
        ImGui::SetCursorPos(ImVec2(60.f * SCALE.x, 12.0f * SCALE.y));
        if (ImGui::Button("Help", BUTTON_SIZE))
            ImGui::OpenPopup("Live Area Help");
        ImGui::SetNextWindowPos(ImVec2(WINDOW_SIZE.x / 2.f, WINDOW_SIZE.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Live Area Help", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
            TextColoredCentered(GUI_COLOR_TEXT_TITLE, gui.lang.main_menubar.help["title"].c_str());
            ImGui::Spacing();
            auto TextColoredCenteredEx = [](GuiState &gui, const ImVec4 &col, const char *text_id) {
                TextColoredCentered(col, gui.lang.live_area.help[text_id].c_str());
                ImGui::Spacing();
            };
            TextColoredCenteredEx(gui, GUI_COLOR_TEXT, "control_setting");
            if (gui.modules.empty())
                TextColoredCenteredEx(gui, GUI_COLOR_TEXT, "firmware_not_detected");
            if (!gui.fw_font)
                TextColoredCenteredEx(gui, GUI_COLOR_TEXT, "firmware_font_not_detected");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["live_area_help"].c_str());
            ImGui::Spacing();
            auto draw_help_table = [](GuiState &gui, const char *const *items, size_t items_count) {
                auto &lang = gui.lang.live_area.help;
                // here I just need any unique value
                if (ImGui::BeginTable(log_hex(reinterpret_cast<uintptr_t>(items)).c_str(), 2)) {
                    ImGui::TableSetupColumn("01");
                    ImGui::TableSetupColumn("02");
                    for (int i = 0; i < items_count; i++) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang[items[i]].c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang[std::string(items[i]) + "_control"].c_str());
                    }
                    ImGui::EndTable();
                }
            };
            constexpr std::array<const char *, 4> help_items_01{ "browse_app", "start_app", "show_hide", "exit_livearea" };
            draw_help_table(gui, help_items_01.data(), help_items_01.size());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["manual_help"].c_str());
            ImGui::Spacing();
            constexpr std::array<const char *, 3> help_items_02{ "browse_page", "hide_show", "exit_manual" };
            draw_help_table(gui, help_items_02.data(), help_items_02.size());

            ImGui::Spacing();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2.f - (BUTTON_SIZE.x / 2.f));
            if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
    }

    const auto SELECTABLE_SIZE = ImVec2(50.f * SCALE.x, 60.f * SCALE.y);

    const auto ARROW_HEIGHT_POS = WINDOW_SIZE.y - (250.f * SCALE.y);
    const auto ARROW_HEIGHT_DRAW_POS = SCREEN_POS.y + ARROW_HEIGHT_POS;
    const auto ARROW_WIDTH_POS = (30.f * SCALE.x);
    const auto ARROW_SELECT_HEIGHT_POS = ARROW_HEIGHT_POS - (SELECTABLE_SIZE.y / 2.f);

    // Draw left arrow
    const auto ARROW_LEFT_CENTER = ImVec2(SCREEN_POS.x + ARROW_WIDTH_POS, ARROW_HEIGHT_DRAW_POS);
    WINDOW_DRAW_LIST->AddTriangleFilled(
        ImVec2(ARROW_LEFT_CENTER.x + (16.f * SCALE.x), ARROW_LEFT_CENTER.y - (20.f * SCALE.y)),
        ImVec2(ARROW_LEFT_CENTER.x - (16.f * SCALE.x), ARROW_LEFT_CENTER.y),
        ImVec2(ARROW_LEFT_CENTER.x + (16.f * SCALE.x), ARROW_LEFT_CENTER.y + (20.f * SCALE.y)), ARROW_COLOR);
    ImGui::SetCursorPos(ImVec2(ARROW_WIDTH_POS - (SELECTABLE_SIZE.x / 2.f), ARROW_SELECT_HEIGHT_POS));
    if (ImGui::InvisibleButton("##left", SELECTABLE_SIZE))
        --gui.live_area_app_current_open;

    // Draw right arrow
    if (gui.live_area_app_current_open < gui.live_area_current_open_apps_list.size() - 1) {
        const auto ARROW_RIGHT_CENTER = ImVec2(SCREEN_POS.x + WINDOW_SIZE.x - ARROW_WIDTH_POS, ARROW_HEIGHT_DRAW_POS);
        WINDOW_DRAW_LIST->AddTriangleFilled(
            ImVec2(ARROW_RIGHT_CENTER.x - (16.f * SCALE.x), ARROW_RIGHT_CENTER.y - (20.f * SCALE.y)),
            ImVec2(ARROW_RIGHT_CENTER.x + (16.f * SCALE.x), ARROW_RIGHT_CENTER.y),
            ImVec2(ARROW_RIGHT_CENTER.x - (16.f * SCALE.x), ARROW_RIGHT_CENTER.y + (20.f * SCALE.y)), ARROW_COLOR);
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - ARROW_WIDTH_POS - (SELECTABLE_SIZE.x / 2.f), ARROW_SELECT_HEIGHT_POS));
        if (ImGui::InvisibleButton("##right", SELECTABLE_SIZE))
            ++gui.live_area_app_current_open;
    }
    ImGui::SetWindowFontScale(1.f);
    ImGui::EndChild();
}

} // namespace gui
