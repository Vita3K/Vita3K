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
#include <util/log.h>

#include <PfsSaveData.h>

#include <cstring>
#include <fstream>
#include <sstream>

namespace savedata {

static fs::path unique_sibling(const fs::path &dst, std::string_view suffix, std::error_code &ec) {
    for (int i = 0; i < 64; i++) {
        const fs::path candidate = fs_utils::path_concat(dst, fmt::format(".{}{}", i, suffix));
        if (!fs::exists(candidate, ec) && !ec)
            return candidate;
    }
    ec = std::make_error_code(std::errc::file_exists);
    return {};
}

static std::string last_line(const std::string &log) {
    std::istringstream lines(log);
    std::string line, last;
    while (std::getline(lines, line))
        if (!line.empty() && !line.starts_with("[WARNING]"))
            last = line;
    return last;
}

static SlotInfo to_slot_info(uint32_t id, const SceAppUtilSaveDataSlotParam &param) {
    const auto str = [](const char *s, size_t n) { return std::string(s, strnlen(s, n)); };
    return { id, str(param.title, sizeof(param.title)), str(param.subTitle, sizeof(param.subTitle)),
        str(param.detail, sizeof(param.detail)), param.modifiedTime };
}

bool is_encrypted(const fs::path &save_path) {
    return fs::exists(save_path / "sce_pfs" / "files.db");
}

bool can_export(const fs::path &save_path) {
    const fs::path sce_sys = save_path / "sce_sys";
    return fs::exists(sce_sys / "sealedkey") && fs::exists(sce_sys / "keystone") && fs::exists(sce_sys / "param.sfo")
        && !is_encrypted(save_path);
}

std::vector<SlotInfo> read_slots(const fs::path &save_path) {
    std::vector<SlotInfo> slots;

    std::vector<char> buf;
    if (std::ifstream sdslot{ save_path / "sce_sys" / "sdslot.dat", std::ios::binary }) {
        buf.resize(SDSLOT_TOTAL_SIZE);
        sdslot.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        buf.resize(static_cast<size_t>(sdslot.gcount()));
    }
    const bool valid_sdslot = buf.size() >= SDSLOT_DATA_BASE && std::memcmp(buf.data(), SDSLOT_MAGIC, sizeof(SDSLOT_MAGIC)) == 0;

    for (uint32_t i = 0; i < SDSLOT_MAX_SLOTS; i++) {
        SceAppUtilSaveDataSlotParam param{};
        const size_t offset = SDSLOT_DATA_BASE + static_cast<size_t>(i) * SDSLOT_BLOCK_SIZE;
        if (valid_sdslot && offset + sizeof(param) <= buf.size()) {
            if (buf[SDSLOT_FLAG_BASE + i] == 0)
                continue;
            std::memcpy(&param, buf.data() + offset, sizeof(param));
        } else {
            std::ifstream legacy(save_path / fmt::format("SlotParam_{}.bin", i), std::ios::binary);
            if (!legacy.read(reinterpret_cast<char *>(&param), sizeof(param)))
                continue;
        }
        slots.push_back(to_slot_info(i, param));
    }
    return slots;
}

// Both directions write next to the destination first, so a failure never leaves a half written folder behind
static bool run_to_temp(const fs::path &dst, const std::function<int(const fs::path &, std::ostream &)> &job, std::string &error) {
    const auto fail = [&error](const std::string &message, const fs::path &tmp) {
        error = message;
        LOG_ERROR("Savedata conversion failed: {}", error);
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

    std::ostringstream log;
    int result = -1;
    try {
        result = job(tmp, log);
    } catch (const std::exception &e) {
        log << e.what() << '\n';
    }
    if (result < 0)
        return fail(last_line(log.str()), tmp);

    fs::path backup;
    if (fs::exists(dst, ec)) {
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

static PfsProgressCallback to_pfs_progress(const std::function<void(float)> &progress) {
    if (!progress)
        return nullptr;
    return [progress](uint64_t done, uint64_t total, const std::string &) {
        progress(total ? static_cast<float>(done) / static_cast<float>(total) : 1.f);
    };
}

bool import_encrypted(const fs::path &src, const fs::path &dst, const std::function<void(float)> &progress, std::string &error) {
    LOG_INFO("Decrypting savedata {} into {}", src, dst);
    return run_to_temp(dst, [&](const fs::path &tmp, std::ostream &log) { return decrypt_savedata(src, tmp, to_pfs_progress(progress), log); }, error);
}

bool export_encrypted(const fs::path &src, const fs::path &dst, const std::function<void(float)> &progress, std::string &error) {
    LOG_INFO("Encrypting savedata {} into {}", src, dst);
    return run_to_temp(dst, [&](const fs::path &tmp, std::ostream &log) { return encrypt_savedata(src, tmp, to_pfs_progress(progress), log); }, error);
}

} // namespace savedata
