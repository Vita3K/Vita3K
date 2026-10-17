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

#include <config/state.h>
#include <ctrl/ctrl.h>
#include <gui-imgui/functions.h>
#include <util/log.h>
#include <util/net_utils.h>
#include <vfs/mounts.h>

#include <boost/algorithm/string/trim.hpp>
#include <pugixml.hpp>

#include <cstdlib>
#include <limits>
#include <mutex>
#include <regex>
#include <sstream>

namespace gui {

void push_update_notification(GuiState &gui, const std::string &id, const std::string &title, const std::string &message) {
    std::lock_guard lock(gui.notifications_mutex);
    gui.notifications.push_back({ id, title, message });
}

std::optional<bool> check_has_patch(EmuEnvState &emuenv, const std::string &title_id, const std::string &current_version) {
    const auto version_xml_path = emuenv.vita_fs_path / "ux0/bgdl" / fmt::format("{}-ver.xml", title_id);
    pugi::xml_document doc;
    if (!doc.load_file(version_xml_path.string().c_str())) {
        std::error_code error;
        if (fs::exists(version_xml_path, error) && !error)
            LOG_ERROR("Failed to parse update metadata for {} from '{}'.", title_id, version_xml_path);
        return std::nullopt;
    }

    const auto last_package = doc.child("titlepatch").child("tag").last_child();
    if (last_package.empty() || std::string(last_package.name()) != "package") {
        LOG_ERROR("No package found in version XML for title ID: {}", title_id);
        return std::nullopt;
    }

    std::string latest_version = last_package.attribute("version").as_string();
    if (latest_version.empty()) {
        LOG_ERROR("Latest package version is empty for title ID: {}", title_id);
        return std::nullopt;
    }
    if (latest_version.front() == '0')
        latest_version.erase(latest_version.begin());
    if (latest_version != current_version) {
        LOG_INFO("New app update {} is available for {}", latest_version, title_id);
        return true;
    }
    return false;
}

bool get_remote_update_info(GuiState &gui, EmuEnvState &emuenv, const std::string &id) {
    const auto app = get_app_index(gui, emuenv, id);
    if (!app) {
        LOG_ERROR("App index not found for update title ID: {}", id);
        return false;
    }

    const auto version_xml_path = emuenv.vita_fs_path / "ux0/bgdl" / fmt::format("{}-ver.xml", app->title_id);
    pugi::xml_document doc;
    if (!doc.load_file(version_xml_path.string().c_str())) {
        LOG_ERROR("Failed to load update metadata for {} from '{}'.", app->title_id, version_xml_path);
        return false;
    }

    const auto title_patch = doc.child("titlepatch");
    const auto title_id = std::string(title_patch.attribute("titleid").as_string());
    const auto tag = title_patch.child("tag");
    std::vector<std::string> versions;
    for (const auto package : tag.children("package")) {
        std::string version = package.attribute("version").as_string();
        if (version.empty()) {
            LOG_ERROR("Package version is empty for title ID: {}", title_id);
            return false;
        }
        if (version.front() == '0')
            version.erase(version.begin());
        versions.push_back(std::move(version));
    }
    if (versions.empty()) {
        LOG_ERROR("No package versions found in update metadata for {}.", app->title_id);
        return false;
    }

    const auto last_package = tag.last_child();
    if (last_package.empty() || std::string(last_package.name()) != "package") {
        LOG_ERROR("Last package entry is missing in update metadata for {}.", app->title_id);
        return false;
    }
    const std::string version = versions.back();
    auto selected_package = last_package;
    auto hybrid_package = last_package.child("hybrid_package");
    if ((versions.size() >= 2 && versions[versions.size() - 2] != app->app_ver) && !hybrid_package.empty()) {
        selected_package = hybrid_package;
        LOG_INFO("Using hybrid update package for {} version {}.", app->title_id, version);
    }

    const uint64_t size = selected_package.attribute("size").as_ullong();
    const std::string url = selected_package.attribute("url").as_string();
    const std::string content_id = selected_package.attribute("content_id").as_string();
    if (url.empty() || content_id.empty() || size == 0) {
        LOG_ERROR("Update package metadata is incomplete for {} version {}.", app->title_id, version);
        return false;
    }

    const auto paramsfo = last_package.child("paramsfo");
    if (paramsfo.empty()) {
        LOG_ERROR("Update paramsfo is missing for {}.", app->title_id);
        return false;
    }
    const auto title_lang = fmt::format("title_{:02x}", emuenv.cfg.sys_lang);
    const auto title_node = paramsfo.child(title_lang.c_str());
    const std::string title = title_node.empty() ? paramsfo.child("title").text().as_string() : title_node.text().as_string();
    if (title.empty()) {
        LOG_ERROR("Update title is empty for {}.", app->title_id);
        return false;
    }

    gui.new_update_infos[id] = { title_id.empty() ? app->title_id : title_id, version, size, url, content_id, title };
    return true;
}

struct UpdateHistoryLine {
    ImVec4 color;
    unsigned size;
    unsigned heading;
    bool in_list;
    bool bullet;
    std::string text;
};

static std::map<std::string, std::vector<UpdateHistoryLine>> update_history_infos;
static std::string remote_update_id;
static bool update_history_just_opened = false;

static bool get_update_history_infos(const pugi::xml_document &doc) {
    update_history_infos.clear();
    static const std::map<std::string, std::string> entities = {
        { "&lt;", "<" }, { "&gt;", ">" }, { "&quot;", "\"" }, { "&apos;", "'" },
        { "&copy;", "\xC2\xA9" }, { "&reg;", "\xC2\xAE" }, { "&trade;", "\xE2\x84\xA2" },
        { "&hellip;", "\xE2\x80\xA6" }, { "&mdash;", "\xE2\x80\x94" },
        { "&ndash;", "\xE2\x80\x93" }, { "&nbsp;", "\xC2\xA0" }, { "&amp;", "&" }
    };
    static const std::string bullet = "\u30FB";
    for (const auto &info : doc.child("changeinfo")) {
        std::string version = info.attribute("app_ver").as_string();
        if (version.empty()) {
            LOG_ERROR("App version is empty in update history XML.");
            continue;
        }
        if (version.front() == '0')
            version.erase(version.begin());
        std::string text = info.text().as_string();
        text = std::regex_replace(text, std::regex(R"(\r\n?)"), "\n");
        text = std::regex_replace(text, std::regex(R"(\n\s*)"), " ");
        text = std::regex_replace(text, std::regex(R"(<li>)"), bullet);
        text = std::regex_replace(text, std::regex(R"(<br\s*/?>|</li>|</?p>)"), "\n");
        text = std::regex_replace(text, std::regex(R"(<h([1-6])>)"), "\n__H_START_$1__");
        text = std::regex_replace(text, std::regex(R"(</h[1-6]>)"), "\n__H_END__");
        text = std::regex_replace(text, std::regex(R"(<ul>)"), "\n__UL_START__");
        text = std::regex_replace(text, std::regex(R"(</ul>)"), "__UL_END__");
        const std::regex font_regex(R"(<font\s*(?:color=["']#([0-9a-fA-F]{6})["'])?\s*(?:size=["']([1-7])["'])?\s*>)");
        std::smatch match;
        while (std::regex_search(text, match, font_regex)) {
            std::string replacement;
            if (match[1].matched)
                replacement += "__COLOR_" + match[1].str() + "__";
            if (match[2].matched)
                replacement += "__SIZE_" + match[2].str() + "__";
            text = std::regex_replace(text, font_regex, replacement, std::regex_constants::format_first_only);
        }
        text = std::regex_replace(text, std::regex(R"(<[^>]+>)"), "");
        text = std::regex_replace(text, std::regex(R"(\xC2\xA0)"), "");
        text = std::regex_replace(text, std::regex(R"(\xE2\x80\x99)"), "'");
        for (const auto &[entity, value] : entities)
            text = std::regex_replace(text, std::regex(entity), value);

        std::istringstream stream(text);
        std::string line;
        unsigned heading = 0;
        bool in_list = false;
        auto &lines = update_history_infos[version];
        while (std::getline(stream, line)) {
            ImVec4 color = GUI_COLOR_TEXT;
            unsigned size = 3;
            if (std::regex_search(line, match, std::regex(R"(__H_START_([1-6])__)", std::regex_constants::icase))) {
                heading = static_cast<unsigned>(std::stoi(match[1].str()));
                line = std::regex_replace(line, std::regex(R"(__H_START_\d__)"), "");
            } else if (line.find("__H_END__") != std::string::npos) {
                heading = 0;
                line = std::regex_replace(line, std::regex(R"(__H_END__)"), "");
            }
            if (std::regex_search(line, match, std::regex(R"(__COLOR_([0-9a-fA-F]{6})__)", std::regex_constants::icase))) {
                const auto rgb = std::strtoul(match[1].str().c_str(), nullptr, 16);
                color = ImVec4(float((rgb >> 16) & 0xFF) / 255.f, float((rgb >> 8) & 0xFF) / 255.f, float(rgb & 0xFF) / 255.f, 1.f);
                line = std::regex_replace(line, std::regex(R"(__COLOR_[0-9a-fA-F]{6}__)"), "");
            }
            if (std::regex_search(line, match, std::regex(R"(__SIZE_([1-7])__)", std::regex_constants::icase))) {
                size = static_cast<unsigned>(std::stoi(match[1].str()));
                line = std::regex_replace(line, std::regex(R"(__SIZE_[1-7]__)"), "");
            }
            const bool start_list = line.find("__UL_START__") != std::string::npos;
            const bool end_list = line.find("__UL_END__") != std::string::npos;
            line = std::regex_replace(line, std::regex("__UL_START__|__UL_END__"), "");
            in_list = start_list || (in_list && !end_list);
            const bool is_bullet = line.find(bullet) != std::string::npos;
            if (is_bullet)
                line = std::regex_replace(line, std::regex(bullet), "");
            boost::trim(line);
            lines.push_back({ color, size, heading, in_list, is_bullet, std::move(line) });
        }
    }
    return !update_history_infos.empty();
}

bool get_local_update_history(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path, const std::string &title_id) {
    std::vector<uint8_t> xml;
    if (!pfsmgr::read_update_history(emuenv.io, emuenv.vita_fs_path, app_path, title_id, emuenv.cfg.sys_lang, xml))
        return false;

    pugi::xml_document doc;
    if (!doc.load_buffer(xml.data(), xml.size()) || !get_update_history_infos(doc)) {
        LOG_WARN("Failed to parse update history for {}.", title_id);
        return false;
    }
    remote_update_id.clear();
    update_history_just_opened = true;
    gui.vita_area.update_history_button = 0;
    gui.vita_area.update_history_info = true;
    return true;
}

static bool get_remote_update_history(GuiState &gui, EmuEnvState &emuenv, const std::string &id) {
    const auto app = get_app_index(gui, emuenv, id);
    if (!app) {
        LOG_ERROR("App index not found for update history: {}", id);
        return false;
    }

    const auto version_path = emuenv.vita_fs_path / "ux0/bgdl" / fmt::format("{}-ver.xml", app->title_id);
    pugi::xml_document version_doc;
    if (!version_doc.load_file(version_path.string().c_str())) {
        LOG_ERROR("Failed to load update metadata from '{}'.", version_path);
        return false;
    }
    const auto package = version_doc.child("titlepatch").child("tag").last_child();
    const auto localized = fmt::format("changeinfo_{:02x}", emuenv.cfg.sys_lang);
    const auto changeinfo = package.child(localized.c_str()).empty() ? package.child("changeinfo") : package.child(localized.c_str());
    const std::string url = changeinfo.attribute("url").as_string();
    if (url.empty()) {
        LOG_ERROR("Change info URL is empty for title ID: {}", app->title_id);
        return false;
    }
    const auto response = net_utils::get_web_response(url);
    if (response.empty()) {
        LOG_ERROR("Failed to fetch change info for title ID: {}", app->title_id);
        return false;
    }
    pugi::xml_document doc;
    if (!doc.load_string(response.c_str()) || !get_update_history_infos(doc)) {
        LOG_ERROR("Failed to parse update history for title ID: {}", app->title_id);
        return false;
    }
    remote_update_id = id;
    update_history_just_opened = true;
    gui.vita_area.update_history_button = 0;
    gui.vita_area.update_history_info = true;
    return true;
}

void draw_update_history_infos(GuiState &gui, EmuEnvState &emuenv) {
    const auto update_it = gui.new_update_infos.find(remote_update_id);
    const bool is_remote = !remote_update_id.empty();
    if (is_remote && update_it == gui.new_update_infos.end()) {
        LOG_ERROR("Update metadata disappeared while displaying history for {}.", remote_update_id);
        remote_update_id.clear();
        gui.vita_area.update_history_info = false;
        return;
    }

    const auto &viewport = sdl::get_viewport();
    const ImVec2 res_scale(viewport.scale_x, viewport.scale_y);
    const ImVec2 scale(res_scale.x * viewport.manual_dpi_scale, res_scale.y * viewport.manual_dpi_scale);
    const ImVec2 window_size(756.f * scale.x, 436.f * scale.y);
    const ImVec2 button_size(320.f * scale.x, 46.f * scale.y);
    ImGui::SetNextWindowPos(ImVec2(viewport.logical_x, viewport.logical_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport.logical_width, viewport.logical_height), ImGuiCond_Always);
    ImGui::Begin("##update_history_infos", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowBgAlpha(0.999f);
    ImGui::SetNextWindowPos(ImVec2(viewport.logical_x + (viewport.logical_width - window_size.x) / 2.f, viewport.logical_y + (viewport.logical_height - window_size.y) / 2.f), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * scale.x);
    ImGui::BeginChild("##update_history_infos_child", window_size, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * scale.x);
    const ImVec2 padding(24.f * scale.x, 56.f * scale.y);
    const ImVec2 list_size(window_size.x - padding.x * 2.f, window_size.y / 1.51f);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowPos().x + padding.x, ImGui::GetWindowPos().y + padding.y));
    ImGui::BeginChild("##info_update_list", list_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(1.48f * res_scale.y);
    if (is_remote) {
        ImGui::TextWrapped("%s", gui.lang.patch_check["new_app_version_available"].c_str());
        auto size_str = std::regex_replace(get_unit_size(update_it->second.size), std::regex("\\s+"), "");
        ImGui::NewLine();
        ImGui::TextColored(GUI_COLOR_TEXT, "%s(%s)", fmt::format(fmt::runtime(gui.lang.patch_check["version"]), update_it->second.version).c_str(), size_str.c_str());
        ImGui::NewLine();
    }
    static constexpr float heading_sizes[] = { 0.f, 30.f, 24.f, 20.f, 16.f, 14.f, 12.f };
    static constexpr float font_sizes[] = { 0.f, 13.3f, 16.7f, 20.f, 23.3f, 30.f, 40.f, 60.f };
    for (auto it = update_history_infos.rbegin(); it != update_history_infos.rend(); ++it) {
        ImGui::SetWindowFontScale(1.48f * res_scale.y);
        const auto version = is_remote ? it->first : fmt::format(fmt::runtime(gui.lang.app_context.main["history_version"]), it->first);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", version.c_str());
        for (const auto &line : it->second) {
            const float size = line.heading ? heading_sizes[line.heading] : font_sizes[line.size];
            ImGui::SetWindowFontScale((size / ImGui::GetFont()->FontSize) * res_scale.y);
            if (line.in_list || line.bullet || line.text.find('*') != std::string::npos) {
                const auto y = ImGui::GetCursorPosY();
                const auto bullet_size = ImGui::CalcTextSize("\u30FB");
                if (line.bullet)
                    ImGui::TextUnformatted("\u30FB");
                ImGui::SetCursorPos(ImVec2(bullet_size.x + ImGui::GetStyle().ItemSpacing.x, y));
            }
            ImGui::PushTextWrapPos(list_size.x - ImGui::GetStyle().ScrollbarSize);
            ImGui::PushStyleColor(ImGuiCol_Text, line.color);
            ImGui::TextWrapped("%s", line.text.c_str());
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
        }
        if (std::next(it) != update_history_infos.rend())
            ImGui::NewLine();
    }
    ImGui::ScrollWhenDragging();
    ImGui::EndChild();
    ImGui::SetWindowFontScale(1.25f * res_scale.y);
    const auto controller_button = gui.vita_area.update_history_button;
    gui.vita_area.update_history_button = 0;
    if (is_remote) {
        ImGui::SetCursorPos(ImVec2(window_size.x / 2.f - button_size.x - 10.f * scale.x, window_size.y - button_size.y - 22.f * scale.y));
        if (ImGui::Button(::lang::get(::lang::str::cancel).c_str(), button_size) || (!update_history_just_opened && (controller_button == SCE_CTRL_CIRCLE || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_circle, false)))) {
            remote_update_id.clear();
            gui.vita_area.update_history_info = false;
        }
        ImGui::SameLine(0, 20.f * scale.x);
        if ((ImGui::Button(gui.lang.patch_check["download"].c_str(), button_size) || (!update_history_just_opened && (controller_button == SCE_CTRL_CROSS || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_cross, false)))) && !remote_update_id.empty()) {
            begin_update_download(gui, emuenv, remote_update_id);
            remote_update_id.clear();
            gui.vita_area.update_history_info = false;
        }
    } else {
        ImGui::SetCursorPos(ImVec2((window_size.x - button_size.x) / 2.f, window_size.y - button_size.y - 22.f * scale.y));
        if (ImGui::Button(::lang::get(::lang::str::ok).c_str(), button_size) || (!update_history_just_opened && (controller_button == SCE_CTRL_CROSS || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_cross, false))))
            gui.vita_area.update_history_info = false;
    }
    update_history_just_opened = false;
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
}

void process_live_area_update(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const auto app = get_app_index(gui, emuenv, app_path);
    if (!app) {
        LOG_ERROR("Cannot update unknown application '{}'.", app_path);
        return;
    }
    const auto &id = app->title_id;
    const auto state_it = gui.updates_install.find(id);
    if (state_it != gui.updates_install.end() && state_it->second.state == UpdateState::WAITING_INSTALL) {
        update_install(gui, emuenv, id);
        return;
    }
    if (state_it != gui.updates_install.end() && state_it->second.state == UpdateState::DOWNLOADING) {
        const auto title = gui.new_update_infos.contains(id) ? gui.new_update_infos.at(id).title : id;
        push_update_notification(gui, id, title, gui.lang.indicator["waiting_download"]);
        return;
    }
    if (!gui.new_update_infos.contains(id) && !get_remote_update_info(gui, emuenv, id)) {
        return;
    }

    const auto &update = gui.new_update_infos.at(id);
    std::error_code error;
    const auto space = fs::space(emuenv.vita_fs_path, error);
    if (error) {
        LOG_ERROR("Failed to check free disk space at '{}': {}", emuenv.vita_fs_path, error.message());
        push_update_notification(gui, id, update.title, "Unable to check free disk space.");
        return;
    }
    const auto required_space = update.size > (std::numeric_limits<uint64_t>::max() / 2) ? std::numeric_limits<uint64_t>::max() : update.size * 2;
    if (required_space > space.available) {
        push_update_notification(gui, id, update.title, fmt::format(fmt::runtime(gui.lang.patch_check["not_enough_space"]), get_unit_size(required_space)));
        return;
    }
    get_remote_update_history(gui, emuenv, id);
}

} // namespace gui