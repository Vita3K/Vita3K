// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <optional>

namespace pfsmgr {

enum class Mode : uint32_t {
    Mode0 = 0,
    Mode2 = 2,
    Mode3 = 3,
    Mode4 = 4,
    Mode5 = 5,
    Mode6 = 6,
    Mode7 = 7,
    Mode8 = 8,
    Mode9 = 9,
    Mode10 = 10,
    Mode11 = 11,
    Mode12 = 12,
    Mode13 = 13,
    Mode20 = 20,
    Mode21 = 21,
    Mode22 = 22,
    Mode23 = 23,
    Mode32 = 32,
    Mode33 = 33,
    GameData = Mode2,
    ThemeData = Mode13,
    AcidDirectory = Mode32
};

enum class IntegrityStorage : uint32_t {
    Unified = 0,
    PerFile = 1,
    None = 2
};

// Vita mode properties, not host open flags or a claim that the backend is implemented.
// https://github.com/shoui520/psp2/blob/main/sys/internal/kernel/src/pfsmgr/handler/mode_setting.c
struct ModeInfo {
    uint16_t filesystem_type;
    uint32_t type;
    IntegrityStorage integrity;
    uint32_t unknown_flag;
    uint32_t icv_open_flags;
    uint32_t unicv_open_flags;
    uint32_t file_mode;
    uint32_t directory_mode;
    bool no_create;
    bool no_sync;
    bool read_only;
};

std::optional<ModeInfo> mode_info(Mode mode);
bool supports_reading(Mode mode);
[[nodiscard]] bool validate_read_mode(Mode mode);
[[nodiscard]] bool validate_read_mode(Mode mode, uint16_t filesystem_type);

} // namespace pfsmgr