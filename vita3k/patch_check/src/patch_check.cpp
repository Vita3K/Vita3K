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

#include <patch_check/functions.h>

#include <emuenv/state.h>

#include <packages/sce_types.h>

#include <util/lock_and_find.h>
#include <util/log.h>
#include <util/net_utils.h>

#include <boost/algorithm/string/trim.hpp>

#include <pugixml.hpp>

#include <regex>
#include <sstream>

namespace patch_check {

static PatchCheckState patch_check_state{};

PatchCheckState &get_state() {
    return patch_check_state;
}

void PatchCheckState::erase_cached_update_info(const std::string &app_path) {
    std::lock_guard<std::mutex> lock(mutex);
    new_update_infos.erase(app_path);
}

RemoteUpdateInfo *PatchCheckState::find_cached_update_info(const std::string &app_path) {
    const auto info = lock_and_find(app_path, new_update_infos, mutex);
    return info ? info.get() : nullptr;
}

UpdateInstall *PatchCheckState::find_update_install(const std::string &app_path) {
    const auto install = lock_and_find(app_path, updates_install, mutex);
    return install ? install.get() : nullptr;
}

bool *PatchCheckState::find_app_has_update(const std::string &app_path) {
    const auto has_update = lock_and_find(app_path, app_has_update, mutex);
    return has_update ? has_update.get() : nullptr;
}

bool PatchCheckState::has_app_update(const std::string &app_path) {
    const auto has_update = lock_and_find(app_path, app_has_update, mutex);
    return has_update && *has_update;
}

void PatchCheckState::set_app_has_update(const std::string &app_path, bool has_update) {
    std::lock_guard<std::mutex> lock(mutex);
    auto &app_has_update_state = app_has_update[app_path];
    if (!app_has_update_state)
        app_has_update_state = std::make_shared<bool>(has_update);
    else
        *app_has_update_state = has_update;
}

void PatchCheckState::erase_app_has_update(const std::string &app_path) {
    std::lock_guard<std::mutex> lock(mutex);
    app_has_update.erase(app_path);
}

bool PatchCheckState::sync_app_has_update_from_local(const fs::path &pref_path, const std::string &app_path, const std::string &title_id, const std::string &current_version) {
    const bool has_update = has_patch(pref_path, title_id, current_version);
    set_app_has_update(app_path, has_update);
    return has_update;
}

UpdateInstall &PatchCheckState::begin_update_download(const std::string &app_path) {
    std::lock_guard<std::mutex> lock(mutex);
    auto &update_install = updates_install[app_path];
    if (!update_install)
        update_install = std::make_shared<UpdateInstall>();
    update_install->state = UpdateState::DOWNLOADING;
    return *update_install;
}

UpdateInstall &PatchCheckState::queue_update_install(const std::string &app_path, const std::string &content_id, const fs::path &pkg_path) {
    std::lock_guard<std::mutex> lock(mutex);
    auto &update_install = updates_install[app_path];
    if (!update_install)
        update_install = std::make_shared<UpdateInstall>();
    update_install->content_id = content_id;
    update_install->pkg_path = pkg_path;
    update_install->state = UpdateState::WAITING_INSTALL;
    return *update_install;
}

void PatchCheckState::mark_update_install_result(const std::string &app_path, bool success, bool erase_on_failure) {
    std::lock_guard<std::mutex> lock(mutex);
    auto &install = updates_install[app_path];
    if (!install)
        install = std::make_shared<UpdateInstall>();
    auto &app_has_update_state = app_has_update[app_path];
    if (!app_has_update_state)
        app_has_update_state = std::make_shared<bool>(false);
    else
        *app_has_update_state = false;
    if (success) {
        install->state = UpdateState::SUCCESS;
    } else if (erase_on_failure) {
        updates_install.erase(app_path);
    } else {
        install->state = UpdateState::FAILED;
    }
}

void PatchCheckState::erase_update_install(const std::string &app_path) {
    std::lock_guard<std::mutex> lock(mutex);
    updates_install.erase(app_path);
}

void PatchCheckState::set_update_install_state(const std::string &app_path, UpdateState state_value) {
    std::lock_guard<std::mutex> lock(mutex);
    auto &update_install = updates_install[app_path];
    if (!update_install)
        update_install = std::make_shared<UpdateInstall>();
    update_install->state = state_value;
}

bool PatchCheckState::has_update_install_state(const std::string &app_path, UpdateState state_value) {
    const auto install = lock_and_find(app_path, updates_install, mutex);
    return install && (install->state == state_value);
}

void PatchCheckState::cache_remote_update_info(const std::string &app_path, RemoteUpdateInfo info) {
    std::lock_guard<std::mutex> lock(mutex);
    new_update_infos[app_path] = std::make_shared<RemoteUpdateInfo>(std::move(info));
}

static std::pair<uint32_t, uint32_t> split_version(const std::string &version) {
    uint32_t major = 0;
    uint32_t minor = 0;
    const auto pos = version.find('.');
    if (pos != std::string::npos) {
        major = std::stoi(version.substr(0, pos));
        minor = std::stoi(version.substr(pos + 1));
    }
    return std::make_pair(major, minor);
}

bool parse_update_history(const std::string &update_history_xml, const std::string &current_version, std::map<std::string, std::vector<UpdateHistoryLine>> &history_lines, bool &is_new_version) {
    history_lines.clear();
    is_new_version = current_version != "0";

    pugi::xml_document history_doc;
    if (!history_doc.load_string(update_history_xml.c_str())) {
        LOG_ERROR("Failed to parse update history XML: \n{}", update_history_xml);
        return false;
    }

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

    const std::string bullet = "\u30FB";
    const auto current_ver = split_version(current_version);

    auto html_decode_regex = [&](std::string text) {
        for (const auto &[key, value] : entities)
            text = std::regex_replace(text, std::regex(key), value);
        return text;
    };

    for (const auto &info : history_doc.child("changeinfo")) {
        std::string app_ver = info.attribute("app_ver").as_string();
        if (app_ver.empty()) {
            LOG_ERROR("App version is empty in update history XML.");
            continue;
        }
        if (app_ver[0] == '0')
            app_ver.erase(app_ver.begin());

        if (is_new_version) {
            auto version = split_version(app_ver);
            if ((version.first < current_ver.first) || ((version.first == current_ver.first) && (version.second <= current_ver.second)))
                continue;
        }

        std::string text = info.text().as_string();
        text = std::regex_replace(text, std::regex(R"(\r\n?)"), "\n");
        text = std::regex_replace(text, std::regex(R"(\n\s*)"), " ");
        text = std::regex_replace(text, std::regex(R"(<li>)"), bullet);
        text = std::regex_replace(text, std::regex(R"(<br\s*/?>|</li>|</?p>)"), "\n");
        text = std::regex_replace(text, std::regex(R"(<h([1-6])>)"), "\n__H_START_$1__");
        text = std::regex_replace(text, std::regex(R"(</h[1-6]>)"), "\n__H_END__");
        const std::string ulstart_token = "__UL_START__";
        const std::string ulend_token = "__UL_END__";
        text = std::regex_replace(text, std::regex(R"(<ul>)"), "\n" + ulstart_token);
        text = std::regex_replace(text, std::regex(R"(</ul>)"), ulend_token);

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

        text = std::regex_replace(text, std::regex(R"(<[^>]+>)"), "");
        text = std::regex_replace(text, std::regex(R"(\xC2\xA0)"), "");
        text = std::regex_replace(text, std::regex(R"(\xE2\x80\x99)"), "'");
        text = html_decode_regex(text);

        std::istringstream stream(text);
        std::string line;
        std::vector<UpdateHistoryLine> update_lines;
        auto hl = HistoryHeadingLevel::None;
        bool in_list = false;

        while (std::getline(stream, line)) {
            bool is_bullet = false;
            bool is_start_list = false;
            bool is_end_list = false;
            std::string color;
            uint32_t font_size = 3;

            std::smatch local_match;
            if (std::regex_search(line, local_match, std::regex(R"(__H_START_(\d)__)", std::regex_constants::icase))) {
                int level = std::stoi(local_match[1].str());
                if (level >= 1 && level <= 6) {
                    hl = static_cast<HistoryHeadingLevel>(level);
                    line = std::regex_replace(line, std::regex(R"(__H_START_\d__)"), "");
                }
            } else if (line.find("__H_END__") != std::string::npos) {
                hl = HistoryHeadingLevel::None;
                line = std::regex_replace(line, std::regex(R"(__H_END__)"), "");
            }

            if (std::regex_search(line, local_match, std::regex(R"(__COLOR_([0-9a-fA-F]{6})__)", std::regex_constants::icase))) {
                color = local_match[1].str();
                line = std::regex_replace(line, std::regex(R"(__COLOR_[0-9a-fA-F]{6}__)"), "");
            }

            if (std::regex_search(line, local_match, std::regex(R"(__SIZE_([1-7])__)", std::regex_constants::icase))) {
                font_size = std::stoi(local_match[1].str());
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

            in_list = is_start_list || (in_list && !is_end_list);
            if (line.find(bullet) != std::string::npos) {
                is_bullet = true;
                line = std::regex_replace(line, std::regex(bullet), "");
            }

            boost::trim(line);
            update_lines.push_back({ color, font_size, hl, in_list, is_bullet, line });
        }

        history_lines[app_ver] = update_lines;
    }

    return !history_lines.empty();
}

bool has_patch(const fs::path &pref_path, const std::string &title_id, const std::string &current_version) {
    const auto ver_xml_path = pref_path / "ux0/bgdl" / fmt::format("{}-ver.xml", title_id);

    pugi::xml_document doc;
    if (!doc.load_file(ver_xml_path.string().c_str()))
        return false;

    auto tag = doc.child("titlepatch").child("tag");
    const auto last_package = tag.last_child();
    if (last_package.empty() || (std::string(last_package.name()) != "package")) {
        LOG_ERROR("No package found in the version XML for title ID: {}", title_id);
        return false;
    }

    std::string latest_version = last_package.attribute("version").as_string();
    if (!latest_version.empty() && (latest_version[0] == '0'))
        latest_version.erase(latest_version.begin());

    if (latest_version != current_version) {
        LOG_INFO("New update {} is available", latest_version);
        return true;
    }

    return false;
}

bool refresh_update_xml(const fs::path &pref_path, const std::string &title_id) {
    const auto resolved_ver_xml_url = resolve_ver_xml_url(title_id);
    if (resolved_ver_xml_url.empty()) {
        LOG_ERROR("Failed to resolve version XML URL for title ID: {}", title_id);
        return false;
    }

    const auto bgdl_path = pref_path / "ux0/bgdl";
    fs::create_directories(bgdl_path);
    const auto ver_xml_path = bgdl_path / fs::path(resolved_ver_xml_url).filename();
    fs::remove(ver_xml_path);

    const auto has_downloaded = net_utils::download_file(resolved_ver_xml_url, ver_xml_path.string());
    LOG_WARN_IF(!has_downloaded, "Failed to download or find ver.xml for title ID: {}", title_id);
    return has_downloaded;
}

bool refresh_app_update_state(const fs::path &pref_path, const std::string &app_path, const std::string &title_id, const std::string &current_version) {
    const bool has_downloaded = refresh_update_xml(pref_path, title_id);
    const bool has_update = has_downloaded && has_patch(pref_path, title_id, current_version);
    patch_check_state.set_app_has_update(app_path, has_update);
    return has_update;
}

bool cache_remote_update_info(const fs::path &pref_path, const std::string &app_path, const std::string &title_id, const std::string &current_version, uint32_t sys_lang) {
    RemoteUpdateInfo info;
    if (!get_remote_update_info(pref_path, title_id, current_version, sys_lang, info)) {
        patch_check_state.erase_cached_update_info(app_path);
        return false;
    }

    patch_check_state.cache_remote_update_info(app_path, std::move(info));
    return true;
}

RemoteUpdateInfo *ensure_cached_remote_update_info(const fs::path &pref_path, const std::string &app_path, const std::string &title_id, const std::string &current_version, uint32_t sys_lang) {
    if (!patch_check_state.find_cached_update_info(app_path) && !cache_remote_update_info(pref_path, app_path, title_id, current_version, sys_lang))
        return nullptr;

    return patch_check_state.find_cached_update_info(app_path);
}

bool get_remote_update_info(const fs::path &pref_path, const std::string &title_id, const std::string &current_version, uint32_t sys_lang, RemoteUpdateInfo &info) {
    const auto ver_xml_path = pref_path / "ux0/bgdl" / fmt::format("{}-ver.xml", title_id);

    pugi::xml_document doc;
    if (!doc.load_file(ver_xml_path.string().c_str())) {
        LOG_ERROR("Failed to load update info XML for title ID: {} from {}", title_id, ver_xml_path.string());
        return false;
    }

    const auto titlepatch = doc.child("titlepatch");
    info.titleid = titlepatch.attribute("titleid").as_string();

    const auto tag_child = titlepatch.child("tag");
    std::vector<std::string> package_updates_version;
    for (const auto package : tag_child.children("package")) {
        std::string version = package.attribute("version").as_string();
        if (version.empty()) {
            LOG_ERROR("Package version is empty for title ID: {}", title_id);
            return false;
        }
        if (version[0] == '0')
            version.erase(version.begin());
        package_updates_version.emplace_back(version);
    }

    if (package_updates_version.empty()) {
        LOG_ERROR("No package update versions found for title ID: {}", title_id);
        return false;
    }

    info.version = package_updates_version.back();

    const auto last_package_child = tag_child.last_child();
    const auto hybrid_package_child = last_package_child.child("hybrid_package");
    if (((package_updates_version.size() >= 2) && (package_updates_version[package_updates_version.size() - 2] == current_version)) || hybrid_package_child.empty()) {
        info.size = last_package_child.attribute("size").as_uint();
        info.url = last_package_child.attribute("url").as_string();
        info.content_id = last_package_child.attribute("content_id").as_string();
    } else {
        info.size = hybrid_package_child.attribute("size").as_uint();
        info.url = hybrid_package_child.attribute("url").as_string();
        info.content_id = hybrid_package_child.attribute("content_id").as_string();
    }

    if (info.content_id.empty() || info.url.empty()) {
        LOG_ERROR("Content ID or URL is empty for title ID: {}, version: {}", title_id, info.version);
        return false;
    }

    const auto paramsfo_child = last_package_child.child("paramsfo");
    if (paramsfo_child.empty()) {
        LOG_ERROR("Paramsfo child is empty for title ID: {}", title_id);
        return false;
    }

    const auto title_lang = fmt::format("title_{:02x}", sys_lang);
    const auto title = !paramsfo_child.child(title_lang).empty() ? title_lang : "title";
    info.title = paramsfo_child.child(title).text().as_string();
    if (info.title.empty()) {
        LOG_ERROR("Title string is empty for title ID: {}", title_id);
        return false;
    }

    return true;
}

bool get_update_history(const fs::path &pref_path, const std::string &title_id, const std::string &current_version, uint32_t sys_lang, std::map<std::string, std::vector<UpdateHistoryLine>> &history_lines, bool &is_new_version) {
    history_lines.clear();
    is_new_version = current_version != "0";

    const auto ver_xml_path = pref_path / "ux0/bgdl" / fmt::format("{}-ver.xml", title_id);
    pugi::xml_document doc;
    if (!doc.load_file(ver_xml_path.string().c_str()))
        return false;

    const auto tag_child = doc.child("titlepatch").child("tag");
    const auto last_package_child = tag_child.last_child();
    const auto changeinfo_lang = fmt::format("changeinfo_{:02x}", sys_lang);
    const auto changeinfo = !last_package_child.child(changeinfo_lang).empty() ? changeinfo_lang : "changeinfo";
    const std::string changeinfo_url = last_package_child.child(changeinfo).attribute("url").as_string();
    if (changeinfo_url.empty()) {
        LOG_ERROR("Change info URL is empty for title ID: {}", title_id);
        return false;
    }

    const auto changeinfo_str = net_utils::get_web_response(changeinfo_url);
    if (changeinfo_str.empty()) {
        LOG_ERROR("Failed to fetch change info for title ID: {}", title_id);
        return false;
    }

    return parse_update_history(changeinfo_str, current_version, history_lines, is_new_version);
}

bool get_local_update_history(const fs::path &pref_path, const std::string &app_path, uint32_t sys_lang, std::map<std::string, std::vector<UpdateHistoryLine>> &history_lines, bool &is_new_version) {
    const auto change_info_path = pref_path / "ux0/app" / app_path / "sce_sys/changeinfo";
    const auto filename = fs::exists(change_info_path / fmt::format("changeinfo_{:0>2d}.xml", sys_lang)) ? fmt::format("changeinfo_{:0>2d}.xml", sys_lang) : "changeinfo.xml";

    pugi::xml_document doc;
    if (!doc.load_file((change_info_path / filename).string().c_str()))
        return false;

    std::ostringstream oss;
    doc.save(oss);
    return parse_update_history(oss.str(), "0", history_lines, is_new_version);
}

} // namespace patch_check
