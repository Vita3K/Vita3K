// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <pfsmgr/functions.h>

#include <span>

namespace pfsmgr::detail {

struct SavedataChange {
    std::filesystem::path path;
    std::optional<std::vector<uint8_t>> data;
};

void validate_savedata_directory_path(const std::filesystem::path &directory);

// The caller must exclusively lock the savedata root against readers, writers and recovery.
// Failure can leave a journal which must be recovered before accessing the root.
[[nodiscard]] bool publish_savedata_transaction(const std::filesystem::path &root, const Hash &key,
    std::span<const SavedataChange> changes);
[[nodiscard]] bool publish_savedata_directory_transaction(const std::filesystem::path &root, const Hash &key,
    const std::filesystem::path &directory, std::span<const SavedataChange> changes);
[[nodiscard]] bool publish_savedata_directory_removal(const std::filesystem::path &root, const Hash &key,
    const std::filesystem::path &directory, std::span<const SavedataChange> changes);
[[nodiscard]] bool recover_savedata_transaction(const std::filesystem::path &root, const Hash &key);

} // namespace pfsmgr::detail