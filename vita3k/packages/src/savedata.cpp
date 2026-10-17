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

#include <packages/savedata.h>

#include <emuenv/app_util.h>
#include <pfsmgr/functions.h>
#include <util/log.h>

#include <cstring>
#include <cwctype>
#include <fstream>
#include <set>
#include <stdexcept>

namespace savedata {

static fs::path unique_sibling(const fs::path &dst, std::string_view suffix, std::error_code &ec) {
    for (int i = 0; i < 64; i++) {
        const fs::path candidate = fs_utils::path_concat(dst, fmt::format(".{}{}", i, suffix));
        if (!fs::exists(candidate, ec) && !ec)
            return candidate;
        if (ec)
            return {};
    }
    ec = std::make_error_code(std::errc::file_exists);
    return {};
}

static SlotInfo to_slot_info(uint32_t id, const SceAppUtilSaveDataSlotParam &param) {
    const auto str = [](const char *s, size_t n) { return std::string(s, strnlen(s, n)); };
    return { id, str(param.title, sizeof(param.title)), str(param.subTitle, sizeof(param.subTitle)),
        str(param.detail, sizeof(param.detail)), param.modifiedTime };
}

bool is_encrypted(const fs::path &save_path) {
    return fs::exists(save_path / "sce_pfs" / "files.db");
}

std::vector<SlotInfo> read_slots(const fs::path &save_path) {
    std::vector<SlotInfo> slots;

    std::unique_ptr<pfsmgr::Layer> layer;
    if (is_encrypted(save_path)) {
        try {
            layer = std::make_unique<pfsmgr::Layer>(save_path);
        } catch (const std::exception &error) {
            LOG_ERROR("Cannot open protected savedata {}: {}", save_path, error.what());
            return slots;
        }
    }
    bool invalid_size = false;
    const auto read_file = [&](const fs::path &relative, size_t max_size) -> std::optional<std::vector<uint8_t>> {
        if (layer) {
            auto data = layer->read(relative);
            if (data && data->size() > max_size) {
                LOG_ERROR("Cannot read savedata slots from {}: slot file {} exceeds the expected size", save_path, relative);
                invalid_size = true;
                return std::nullopt;
            }
            return data;
        }
        std::ifstream input(save_path / relative, std::ios::binary);
        if (!input)
            return std::nullopt;
        std::vector<uint8_t> data(max_size);
        input.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size()));
        data.resize(static_cast<size_t>(input.gcount()));
        return data;
    };
    try {
        const auto buf = read_file(fs::path("sce_sys") / "sdslot.dat", SDSLOT_TOTAL_SIZE).value_or(std::vector<uint8_t>{});
        if (invalid_size)
            return {};
        const bool valid_sdslot = buf.size() >= SDSLOT_DATA_BASE && std::memcmp(buf.data(), SDSLOT_MAGIC, sizeof(SDSLOT_MAGIC)) == 0;

        for (uint32_t i = 0; i < SDSLOT_MAX_SLOTS; i++) {
            SceAppUtilSaveDataSlotParam param{};
            const size_t offset = SDSLOT_DATA_BASE + static_cast<size_t>(i) * SDSLOT_BLOCK_SIZE;
            if (valid_sdslot && offset + sizeof(param) <= buf.size()) {
                if (buf[SDSLOT_FLAG_BASE + i] == 0)
                    continue;
                std::memcpy(&param, buf.data() + offset, sizeof(param));
            } else {
                const auto legacy = read_file(fmt::format("SlotParam_{}.bin", i), sizeof(param));
                if (invalid_size)
                    return {};
                if (!legacy || legacy->size() != sizeof(param))
                    continue;
                std::memcpy(&param, legacy->data(), sizeof(param));
            }
            slots.push_back(to_slot_info(i, param));
        }
    } catch (const std::exception &error) {
        LOG_ERROR("Cannot read savedata slots from {}: {}", save_path, error.what());
        slots.clear();
    }
    return slots;
}

// Both directions write next to the destination first, so a failure never leaves a half written folder behind
static bool run_to_temp(const fs::path &dst, const std::function<bool(const fs::path &)> &job, std::string &error) {
    const auto fail = [&error](const std::string &message, const fs::path &tmp) {
        error = message;
        LOG_ERROR("Savedata copy failed: {}", error);
        std::error_code ec;
        if (!tmp.empty())
            fs::remove_all(tmp, ec);
        LOG_WARN_IF(ec, "Failed to remove {}: {}", tmp, ec.message());
        return false;
    };

    std::error_code ec;
    const fs::path tmp = unique_sibling(dst, ".tmp", ec);
    if (ec || !fs::create_directories(tmp, ec))
        return fail(ec ? ec.message() : "Failed to create " + tmp.string(), {});

    try {
        if (!job(tmp))
            return fail(error, tmp);
    } catch (const std::exception &e) {
        return fail(e.what(), tmp);
    }

    fs::path backup;
    const bool destination_exists = fs::exists(dst, ec);
    if (ec)
        return fail(fmt::format("Failed to inspect {}: {}", dst, ec.message()), tmp);
    if (destination_exists) {
        backup = unique_sibling(dst, ".bak", ec);
        if (!ec)
            fs::rename(dst, backup, ec);
        if (ec)
            return fail(fmt::format("Failed to move the existing {} aside: {}", dst, ec.message()), tmp);
    }

    fs::rename(tmp, dst, ec);
    if (ec) {
        const std::string message = fmt::format("Failed to move the new save data into {}: {}", dst, ec.message());
        if (!backup.empty()) {
            std::error_code restore_ec;
            fs::rename(backup, dst, restore_ec);
            if (restore_ec)
                return fail(fmt::format("{}\nThe previous save data is kept at {}", message, backup), tmp);
        }
        return fail(message, tmp);
    }

    if (!backup.empty()) {
        fs::remove_all(backup, ec);
        LOG_WARN_IF(ec, "Failed to remove {}: {}", backup, ec.message());
    }
    return true;
}

static bool path_inside(const fs::path &path, const fs::path &base) {
    const auto resolved_path = fs::weakly_canonical(path);
    const auto resolved_base = fs::weakly_canonical(base);
    auto part = resolved_path.begin();
    for (auto parent = resolved_base.begin(); parent != resolved_base.end(); ++parent, ++part) {
        if (part == resolved_path.end())
            return false;
#ifdef _WIN32
        auto left = part->native();
        auto right = parent->native();
        std::transform(left.begin(), left.end(), left.begin(), [](wchar_t c) { return std::towlower(c); });
        std::transform(right.begin(), right.end(), right.begin(), [](wchar_t c) { return std::towlower(c); });
        if (left != right)
#else
        if (*part != *parent)
#endif
            return false;
    }
    return true;
}

static bool verify_savedata(const fs::path &root, std::string &error) {
    const pfsmgr::Layer layer(root);
    std::vector<fs::path> directories{ {} };
    std::set<uint32_t> visited;
    while (!directories.empty()) {
        const auto directory = std::move(directories.back());
        directories.pop_back();
        for (const auto &entry : layer.children(directory)) {
            const auto path = directory / fs_utils::utf8_to_path(entry.name);
            if (!visited.insert(entry.index).second) {
                error = "PFS: duplicate or cyclic savedata entry";
                return false;
            }
            if (entry.attributes & 0x8000) {
                if (!fs::is_directory(fs::symlink_status(layer.savedata_host_path(path)))) {
                    error = "PFS: savedata directory is missing or invalid";
                    return false;
                }
                directories.push_back(path);
            } else if (!layer.read(path)) {
                error = "PFS: file disappeared during verification";
                return false;
            }
        }
    }
    return true;
}

bool copy_protected(const fs::path &src, const fs::path &dst, const std::function<void(float)> &progress, std::string &error) {
    try {
        if (!fs::is_directory(fs::symlink_status(src)) || !is_encrypted(src)) {
            error = "Source is not a protected savedata directory";
            LOG_ERROR("Cannot copy savedata: {}", error);
            return false;
        }
        if (path_inside(src, dst) || path_inside(dst, src)) {
            error = "Source and destination savedata directories overlap";
            LOG_ERROR("Cannot copy savedata: {}", error);
            return false;
        }
    } catch (const std::exception &e) {
        error = e.what();
        LOG_ERROR("Cannot copy savedata: {}", error);
        return false;
    }
    LOG_INFO("Copying protected savedata {} into {}", src, dst);
    return run_to_temp(dst, [&](const fs::path &tmp) {
        std::vector<fs::path> files;
        for (const auto &entry : fs::recursive_directory_iterator(src)) {
            const auto status = entry.symlink_status();
            const auto relative = entry.path().lexically_relative(src);
            if (fs::is_directory(status))
                fs::create_directory(tmp / relative);
            else if (fs::is_regular_file(status))
                files.push_back(relative);
            else {
                error = "Savedata contains a symbolic link or unsupported file";
                return false;
            }
        }
        for (size_t i = 0; i < files.size(); ++i) {
            fs::copy_file(src / files[i], tmp / files[i]);
            if (progress)
                progress(0.9f * static_cast<float>(i + 1) / static_cast<float>(files.size()));
        }
        if (!verify_savedata(tmp, error))
            return false;
        if (progress)
            progress(1.f);
        return true; }, error);
}

} // namespace savedata
