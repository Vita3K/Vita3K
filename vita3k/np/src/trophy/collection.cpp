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

#include <np/trophy/collection.h>

#include <io/functions.h>
#include <util/log.h>

#include <fmt/format.h>
#include <pugixml.hpp>

namespace np::trophy {
namespace {

fs::path trophy_base_path(const CollectionSource &source) {
    return source.vita_fs_path / "ux0/user" / source.user_id / "trophy";
}

void initialize_locked_progress(Context &context) {
    std::fill_n(context.trophy_progress, MAX_TROPHIES >> 5, 0);
    std::fill_n(context.trophy_availability, MAX_TROPHIES >> 5, 0);
    context.group_count = 0;
    context.trophy_count = 0;
    context.platinum_trophy_id = SCE_NP_TROPHY_INVALID_TROPHY_ID;
    context.trophy_count_by_group.fill(0);
    context.unlock_timestamps.fill(0);
    context.trophy_kinds.fill(SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_UNKNOWN);
}

int parse_grade(const char trophy_type) {
    using G = SceNpTrophyGrade;
    switch (trophy_type) {
    case 'B': return static_cast<int>(G::SCE_NP_TROPHY_GRADE_BRONZE);
    case 'S': return static_cast<int>(G::SCE_NP_TROPHY_GRADE_SILVER);
    case 'G': return static_cast<int>(G::SCE_NP_TROPHY_GRADE_GOLD);
    case 'P': return static_cast<int>(G::SCE_NP_TROPHY_GRADE_PLATINUM);
    default: return static_cast<int>(G::SCE_NP_TROPHY_GRADE_UNKNOWN);
    }
}

bool is_valid_trophy_id(const int trophy_id) {
    return trophy_id >= 0 && trophy_id < static_cast<int>(MAX_TROPHIES);
}

bool is_valid_grade_index(const int grade) {
    return grade >= 0 && grade < static_cast<int>(CollectionSnapshot{}.grade_unlocked.size());
}

} // namespace

std::vector<std::string> list_collection_ids(const CollectionSource &source) {
    std::vector<std::string> ids;
    const fs::path conf_root = trophy_base_path(source) / "conf";

    if (!fs::exists(conf_root) || fs::is_empty(conf_root))
        return ids;

    for (const auto &entry : fs::directory_iterator(conf_root)) {
        if (fs::is_directory(entry))
            ids.push_back(entry.path().filename().string());
    }

    return ids;
}

bool load_collection(const CollectionSource &source, const std::string &np_com_id, CollectionSnapshot &snapshot) {
    if (!source.io) {
        LOG_ERROR("Failed to load trophy collection {}: missing IO state", np_com_id);
        return false;
    }

    const fs::path base = trophy_base_path(source);
    const fs::path conf_path = base / "conf" / np_com_id;
    const fs::path dat_path = base / "data" / np_com_id / "TROPUSR.DAT";

    CollectionSnapshot loaded;
    loaded.np_com_id = np_com_id;
    loaded.context.io = source.io;
    loaded.context.vita_fs_path = source.vita_fs_path;
    loaded.context.trophy_progress_output_file_path = "ux0:user/" + source.user_id
        + "/trophy/data/" + np_com_id + "/TROPUSR.DAT";

    initialize_locked_progress(loaded.context);

    if (fs::exists(dat_path)) {
        const SceUID fh = open_file(*source.io,
            loaded.context.trophy_progress_output_file_path.c_str(),
            SCE_O_RDONLY, source.vita_fs_path, "trophy_collection");
        if (fh < 0) {
            LOG_WARN("Failed to open TROPUSR.DAT for {}; showing all trophies as locked", np_com_id);
        } else {
            const bool progress_loaded = loaded.context.load_trophy_progress_file(fh);
            close_file(*source.io, fh, "trophy_collection");
            if (!progress_loaded) {
                LOG_WARN("Failed to parse TROPUSR.DAT for {}; showing all trophies as locked", np_com_id);
                initialize_locked_progress(loaded.context);
            }
        }
    } else {
        LOG_INFO("TROPUSR.DAT missing for {}; showing all trophies as locked", np_com_id);
    }

    const auto localized_sfm = fmt::format("TROP_{:0>2d}.SFM", source.lang);
    const fs::path sfm_path = fs::exists(conf_path / localized_sfm)
        ? conf_path / localized_sfm
        : conf_path / "TROP.SFM";

    pugi::xml_document doc;
    if (!doc.load_file(sfm_path.c_str())) {
        LOG_ERROR("Failed to parse {}", sfm_path);
        return false;
    }

    const auto root = doc.child("trophyconf");
    loaded.title = root.child("title-name").text().as_string();

    const auto localized_icon = fmt::format("ICON0_{:0>2d}.PNG", source.lang);
    loaded.icon_path = fs::exists(conf_path / localized_icon)
        ? fs_utils::path_to_utf8(conf_path / localized_icon)
        : fs_utils::path_to_utf8(conf_path / "ICON0.PNG");

    for (const auto &node : root.children("trophy")) {
        TrophyRecord trophy;
        trophy.id = node.attribute("id").as_int();
        trophy.hidden = node.attribute("hidden").as_string()[0] == 'y';
        trophy.name = node.child("name").text().as_string();
        trophy.detail = node.child("detail").text().as_string();
        trophy.grade = parse_grade(node.attribute("ttype").as_string()[0]);
        if (!is_valid_trophy_id(trophy.id)) {
            LOG_WARN("Ignoring out-of-range trophy id {} in {}", trophy.id, np_com_id);
            trophy.earned = false;
            trophy.timestamp = 0;
        } else {
            trophy.earned = loaded.context.is_trophy_unlocked(static_cast<uint32_t>(trophy.id));
            trophy.timestamp = trophy.earned ? loaded.context.unlock_timestamps[static_cast<size_t>(trophy.id)] : 0;
        }
        trophy.icon_path = fs_utils::path_to_utf8(conf_path / fmt::format("TROP{:0>3d}.PNG", trophy.id));

        if (trophy.earned) {
            loaded.unlocked++;
            if (is_valid_grade_index(trophy.grade)) {
                loaded.grade_unlocked[static_cast<size_t>(trophy.grade)]++;
            } else {
                LOG_WARN("Ignoring out-of-range trophy grade {} for trophy {} in {}", trophy.grade, trophy.id, np_com_id);
            }
        }

        loaded.trophies.push_back(std::move(trophy));
    }

    loaded.total = static_cast<int>(loaded.trophies.size());
    snapshot = std::move(loaded);
    return true;
}

} // namespace np::trophy
