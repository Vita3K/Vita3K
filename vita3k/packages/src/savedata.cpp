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
#include <filesystem>
#include <sstream>

namespace savedata {

static constexpr uint32_t SDSLOT_MAX_SLOTS = 256;
static constexpr uint32_t SDSLOT_FLAG_BASE = 0x200;
static constexpr uint32_t SDSLOT_DATA_BASE = 0x400;
static constexpr uint32_t SDSLOT_BLOCK_SIZE = 0x400;

static std::filesystem::path to_std_path(const fs::path &path) {
    return std::filesystem::path(path.native());
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
    return fs::exists(save_path / "sce_sys" / "sealedkey") && !is_encrypted(save_path);
}

std::vector<SlotInfo> read_slots(const fs::path &save_path) {
    std::vector<SlotInfo> slots;

    fs::ifstream sdslot(save_path / "sce_sys" / "sdslot.dat", std::ios::binary);
    if (sdslot) {
        std::vector<char> buf((std::istreambuf_iterator<char>(sdslot)), {});
        if (buf.size() < SDSLOT_DATA_BASE || std::memcmp(buf.data(), "SDSL", 4) != 0)
            return slots;
        for (uint32_t i = 0; i < SDSLOT_MAX_SLOTS; i++) {
            const size_t offset = SDSLOT_DATA_BASE + static_cast<size_t>(i) * SDSLOT_BLOCK_SIZE;
            if (buf[SDSLOT_FLAG_BASE + i] == 0 || offset + sizeof(SceAppUtilSaveDataSlotParam) > buf.size())
                continue;
            SceAppUtilSaveDataSlotParam param;
            std::memcpy(&param, buf.data() + offset, sizeof(param));
            slots.push_back(to_slot_info(i, param));
        }
        return slots;
    }

    for (uint32_t i = 0; i < SDSLOT_MAX_SLOTS; i++) {
        fs::ifstream legacy(save_path / fmt::format("SlotParam_{}.bin", i), std::ios::binary);
        SceAppUtilSaveDataSlotParam param{};
        if (legacy && legacy.read(reinterpret_cast<char *>(&param), sizeof(param)))
            slots.push_back(to_slot_info(i, param));
    }
    return slots;
}

// Both directions write next to the destination first, so a failure never leaves a half written folder behind
static bool run_to_temp(const fs::path &dst, const std::function<int(const std::filesystem::path &, std::ostream &)> &job, std::string &error) {
    const fs::path tmp = fs_utils::path_concat(dst, ".tmp");
    fs::remove_all(tmp);

    std::ostringstream log;
    int result = -1;
    try {
        result = job(to_std_path(tmp), log);
    } catch (const std::exception &e) {
        log << e.what() << '\n';
    }

    if (result < 0) {
        error = last_line(log.str());
        LOG_ERROR("Savedata conversion failed: {}", error);
        fs::remove_all(tmp);
        return false;
    }

    fs::remove_all(dst);
    fs::rename(tmp, dst);
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
    return run_to_temp(dst, [&](const std::filesystem::path &tmp, std::ostream &log) { return decrypt_savedata(to_std_path(src), tmp, to_pfs_progress(progress), log); }, error);
}

bool export_encrypted(const fs::path &src, const fs::path &dst, const std::function<void(float)> &progress, std::string &error) {
    LOG_INFO("Encrypting savedata {} into {}", src, dst);
    return run_to_temp(dst, [&](const std::filesystem::path &tmp, std::ostream &log) { return encrypt_savedata(to_std_path(src), tmp, to_pfs_progress(progress), log); }, error);
}

} // namespace savedata
