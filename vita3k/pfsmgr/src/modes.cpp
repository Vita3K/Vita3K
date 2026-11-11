// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <pfsmgr/modes.h>
#include <util/log.h>

namespace pfsmgr {

std::optional<ModeInfo> mode_info(Mode mode) {
    uint16_t filesystem_type;
    switch (mode) {
    case Mode::Mode0:
    case Mode::Mode20:
    case Mode::Mode21:
    case Mode::Mode22:
    case Mode::Mode23: filesystem_type = 0; break;
    case Mode::Mode2:
    case Mode::Mode3:
    case Mode::Mode10: filesystem_type = 1; break;
    case Mode::Mode5:
    case Mode::Mode6:
    case Mode::Mode7:
    case Mode::Mode9: filesystem_type = 2; break;
    case Mode::Mode4:
    case Mode::Mode8:
    case Mode::Mode12: filesystem_type = 3; break;
    case Mode::Mode11:
    case Mode::Mode13:
    case Mode::Mode32:
    case Mode::Mode33: filesystem_type = 4; break;
    default: return std::nullopt;
    }

    ModeInfo info{};
    info.filesystem_type = filesystem_type;
    switch (mode) {
    case Mode::Mode0:
    case Mode::Mode21:
    case Mode::Mode22:
    case Mode::Mode23:
        info.type = 2;
        info.integrity = IntegrityStorage::None;
        break;
    case Mode::Mode20:
        info.type = 2;
        info.integrity = IntegrityStorage::None;
        info.no_create = true;
        info.no_sync = true;
        break;
    case Mode::Mode2:
    case Mode::Mode13:
    case Mode::Mode32:
    case Mode::Mode33:
        info.type = 1;
        info.integrity = IntegrityStorage::Unified;
        info.unknown_flag = 1;
        info.icv_open_flags = 1;
        info.unicv_open_flags = 1;
        info.no_create = true;
        info.no_sync = true;
        info.read_only = true;
        break;
    case Mode::Mode3:
        info.type = 1;
        info.integrity = IntegrityStorage::Unified;
        info.unknown_flag = 1;
        info.icv_open_flags = 1;
        info.unicv_open_flags = 1;
        info.file_mode = 6;
        info.directory_mode = 6;
        info.no_create = true;
        info.read_only = true;
        break;
    case Mode::Mode4:
    case Mode::Mode5:
    case Mode::Mode6:
    case Mode::Mode7:
    case Mode::Mode8:
    case Mode::Mode9:
        info.integrity = IntegrityStorage::PerFile;
        info.unknown_flag = 1;
        info.icv_open_flags = 0x203;
        info.unicv_open_flags = 0x203;
        info.file_mode = 6;
        info.directory_mode = 6;
        break;
    case Mode::Mode10:
    case Mode::Mode11:
        info.type = 1;
        info.integrity = IntegrityStorage::Unified;
        info.unknown_flag = 1;
        info.icv_open_flags = 0x203;
        info.unicv_open_flags = 0x203;
        info.file_mode = 6;
        info.directory_mode = 6;
        info.read_only = true;
        break;
    case Mode::Mode12:
        info.integrity = IntegrityStorage::PerFile;
        info.unknown_flag = 1;
        info.icv_open_flags = 1;
        info.unicv_open_flags = 1;
        info.no_create = true;
        info.no_sync = true;
        break;
    default: return std::nullopt;
    }
    return info;
}

bool supports_reading(Mode mode) {
    return mode == Mode::GameData || mode == Mode::ThemeData || mode == Mode::AcidDirectory;
}

bool validate_read_mode(Mode mode) {
    const auto info = mode_info(mode);
    if (!info) {
        LOG_ERROR("Unknown PFS mode {}, please report it to the developers", static_cast<uint32_t>(mode));
        return false;
    }
    if (!supports_reading(mode)) {
        LOG_ERROR("PFS mode {} is not implemented", static_cast<uint32_t>(mode));
        return false;
    }
    return true;
}

bool validate_read_mode(Mode mode, uint16_t filesystem_type) {
    if (!validate_read_mode(mode))
        return false;
    const auto info = mode_info(mode);
    if (info->filesystem_type != filesystem_type) {
        LOG_ERROR("PFS filesystem type {} does not match mode {} (expected {})", filesystem_type,
            static_cast<uint32_t>(mode), info->filesystem_type);
        return false;
    }
    return true;
}

} // namespace pfsmgr