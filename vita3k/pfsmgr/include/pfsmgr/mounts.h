// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <pfsmgr/modes.h>

#include <array>
#include <compare>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace pfsmgr {

class Layer;
using MountId = uint64_t;
constexpr MountId no_mount = 0;

// Lifetime in the emulator, not the Vita's AppMgr mount type.
enum class MountScope {
    Application,
    Shell
};

enum class MountType {
    GameData,
    AppMeta,
    ThemeData,
    AddCont
};

struct MountKey {
    MountType type;
    std::string id;

    auto operator<=>(const MountKey &) const = default;
};

struct MountedLayers {
    MountId base = no_mount;
    MountId patch = no_mount;
};

struct MountedDirectory {
    std::filesystem::path source;
    std::filesystem::path canonical_source;
    std::filesystem::path patch;
    std::filesystem::path canonical_patch;
    MountedLayers layers;
    Mode mode;
    Mode patch_mode;
    MountScope scope;
    std::shared_ptr<void> lease;
};

// Owns PFS pseudo-drives and authenticated layers; callers own their virtual aliases.
class Mounts {
public:
    [[nodiscard]] std::optional<MountedLayers> mount(const std::filesystem::path &base, const std::filesystem::path &patch,
        const std::filesystem::path &license_directory, const std::string &title_id,
        Mode mode, const std::string &content_id = {}, MountScope scope = MountScope::Application);
    [[nodiscard]] bool mount_directory(const std::string &name, const std::filesystem::path &base,
        const std::filesystem::path &patch, const std::filesystem::path &license_directory,
        const std::string &title_id, Mode mode, const std::string &content_id = {},
        MountScope scope = MountScope::Application, bool track_open_handles = false);
    [[nodiscard]] bool mount_savedata_directory(const std::string &name, const std::filesystem::path &base,
        const std::optional<std::array<uint8_t, 16>> &key = std::nullopt, MountScope scope = MountScope::Application);
    [[nodiscard]] bool refresh_savedata_directory(const std::string &name);
    std::optional<MountedDirectory> get_directory(const std::string &name) const;
    bool unmount_directory(const std::string &name, bool require_idle = false);
    bool split_patch(const std::string &base_name, const std::string &patch_name);
    std::shared_ptr<const Layer> get(MountId id) const;
    void unmount(MountedLayers layers);
    void clear(MountScope scope);
    size_t size() const;

private:
    mutable std::mutex mutex_;
    MountId next_id_ = 1;
    struct Entry {
        std::shared_ptr<const Layer> layer;
        MountScope scope;
        std::optional<std::array<uint8_t, 16>> savedata_key;
    };
    std::unordered_map<MountId, Entry> layers_;
    std::unordered_map<std::string, MountedDirectory> directories_;
};

// Return false on inspection failure; found is valid only on success.
// Inspect the databases without opening or authenticating the layer.
[[nodiscard]] bool has_databases(const std::filesystem::path &root, bool &found);
// Treat even an incomplete sce_pfs directory as protected appmeta.
[[nodiscard]] bool has_appmeta_pfs(const std::filesystem::path &root, bool &found);
std::string allocate_mount_name(std::string_view prefix);
bool is_mount_name(const std::string &name, std::string_view prefix);

} // namespace pfsmgr