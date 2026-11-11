// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <pfsmgr/functions.h>
#include <pfsmgr/mounts.h>
#include <util/fs.h>
#include <util/log.h>

#include <limits>
#include <system_error>

namespace pfsmgr {

bool has_databases(const std::filesystem::path &root, bool &found) {
    found = false;
    for (const auto *name : { "files.db", "unicv.db" }) {
        std::error_code error;
        const bool exists = std::filesystem::exists(root / "sce_pfs" / name, error);
        if (error) {
            LOG_ERROR("Cannot inspect PFS database '{}': {}", root / "sce_pfs" / name, error.message());
            return false;
        }
        found |= exists;
    }
    return true;
}

bool has_appmeta_pfs(const std::filesystem::path &root, bool &found) {
    found = false;
    const auto path = root / "sce_pfs";
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory || (!error && !std::filesystem::exists(status)))
        return true;
    if (error || !std::filesystem::is_directory(status)) {
        LOG_ERROR("Cannot inspect appmeta PFS '{}': {}", path, error ? error.message() : "not a directory");
        return false;
    }
    found = true;
    return true;
}

bool Mounts::mount_savedata_directory(const std::string &name, const std::filesystem::path &base, const std::optional<AesKey> &key, MountScope scope) {
    if (name.empty()) {
        LOG_ERROR("PFS: empty mount name");
        return false;
    }
    try {
        std::error_code error;
        const auto source = std::filesystem::absolute(base, error);
        if (error) {
            LOG_ERROR("Cannot resolve savedata path '{}': {}", base, error.message());
            return false;
        }
        const auto canonical_source = std::filesystem::canonical(source, error);
        if (error) {
            LOG_ERROR("Cannot canonicalize savedata path '{}': {}", source, error.message());
            return false;
        }
        auto layer = key ? std::make_shared<Layer>(source, *key) : std::make_shared<Layer>(source);
        std::lock_guard lock(mutex_);
        if (next_id_ == no_mount) {
            LOG_ERROR("PFS: mount identifiers exhausted");
            return false;
        }
        const auto id = next_id_++;
        layers_.emplace(id, Entry{ std::move(layer), scope, key });
        try {
            MountedDirectory directory{ source, canonical_source, {}, {}, { id, no_mount }, Mode::Mode5,
                Mode::Mode5, scope, std::make_shared<int>(0) };
            if (!directories_.emplace(name, std::move(directory)).second) {
                layers_.erase(id);
                LOG_ERROR("PFS: mount name already exists: {}", name);
                return false;
            }
        } catch (const std::exception &error) {
            layers_.erase(id);
            LOG_ERROR("Cannot publish savedata mount '{}': {}", name, error.what());
            return false;
        }
        return true;
    } catch (const std::exception &error) {
        LOG_ERROR("Cannot mount savedata '{}': {}", base, error.what());
        return false;
    }
}

bool Mounts::refresh_savedata_directory(const std::string &name) {
    std::lock_guard lock(mutex_);
    const auto directory = directories_.find(name);
    if (directory == directories_.end() || directory->second.mode != Mode::Mode5) {
        LOG_ERROR("PFS: missing savedata mount '{}'", name);
        return false;
    }
    const auto entry = layers_.find(directory->second.layers.base);
    if (entry == layers_.end()) {
        LOG_ERROR("PFS: missing savedata layer for '{}'", name);
        return false;
    }
    try {
        entry->second.layer = entry->second.savedata_key
            ? std::make_shared<Layer>(directory->second.source, *entry->second.savedata_key)
            : std::make_shared<Layer>(directory->second.source);
        return true;
    } catch (const std::exception &error) {
        LOG_ERROR("Cannot refresh savedata mount '{}': {}", name, error.what());
        return false;
    }
}

std::optional<MountedLayers> Mounts::mount(const std::filesystem::path &base, const std::filesystem::path &patch,
    const std::filesystem::path &license_directory, const std::string &title_id, Mode mode, const std::string &content_id, MountScope scope) {
    if (!validate_read_mode(mode))
        return std::nullopt;
    try {
        auto base_layer = open_layer(base, license_directory, title_id, mode, content_id);
        auto patch_layer = patch.empty() ? nullptr : open_layer(patch, license_directory, title_id, mode, content_id);

        std::lock_guard lock(mutex_);
        const auto count = static_cast<MountId>(!!base_layer) + static_cast<MountId>(!!patch_layer);
        if (count && (next_id_ == no_mount || count - 1 > std::numeric_limits<MountId>::max() - next_id_)) {
            LOG_ERROR("PFS: mount identifiers exhausted");
            return std::nullopt;
        }
        MountedLayers result;
        if (base_layer) {
            result.base = next_id_++;
            layers_.emplace(result.base, Entry{ std::move(base_layer), scope });
        }

        try {
            if (patch_layer) {
                result.patch = next_id_++;
                layers_.emplace(result.patch, Entry{ std::move(patch_layer), scope });
            }
        } catch (const std::exception &error) {
            layers_.erase(result.base);
            LOG_ERROR("Cannot publish PFS patch layer: {}", error.what());
            return std::nullopt;
        }
        return result;
    } catch (const std::exception &error) {
        LOG_ERROR("Cannot mount PFS layers for '{}': {}", base, error.what());
        return std::nullopt;
    }
}

bool Mounts::mount_directory(const std::string &name, const std::filesystem::path &base,
    const std::filesystem::path &patch, const std::filesystem::path &license_directory,
    const std::string &title_id, Mode mode, const std::string &content_id, MountScope scope, bool track_open_handles) {
    if (name.empty()) {
        LOG_ERROR("PFS: empty mount name");
        return false;
    }
    if (mode != Mode::Mode20 && !validate_read_mode(mode))
        return false;
    MountedLayers layers;
    try {
        std::error_code error;
        const auto source = std::filesystem::absolute(base, error);
        if (error) {
            LOG_ERROR("Cannot resolve PFS path '{}': {}", base, error.message());
            return false;
        }
        const auto canonical_source = std::filesystem::canonical(source, error);
        if (error) {
            LOG_ERROR("Cannot canonicalize PFS path '{}': {}", source, error.message());
            return false;
        }
        const auto patch_path = patch.empty() ? std::filesystem::path{} : std::filesystem::absolute(patch, error);
        if (error) {
            LOG_ERROR("Cannot resolve PFS patch '{}': {}", patch, error.message());
            return false;
        }
        const auto canonical_patch = patch.empty() ? std::filesystem::path{} : std::filesystem::canonical(patch_path, error);
        if (error) {
            LOG_ERROR("Cannot canonicalize PFS patch '{}': {}", patch_path, error.message());
            return false;
        }
        if (mode != Mode::Mode20) {
            const auto mounted = mount(source, patch_path, license_directory, title_id, mode, content_id, scope);
            if (!mounted)
                return false;
            layers = *mounted;
        }
        std::lock_guard lock(mutex_);
        if ((layers.base != no_mount && !layers_.contains(layers.base)) || (layers.patch != no_mount && !layers_.contains(layers.patch))) {
            layers_.erase(layers.base);
            layers_.erase(layers.patch);
            LOG_ERROR("PFS: mount '{}' cleared before publication", name);
            return false;
        }
        const auto base_mode = mode == Mode::GameData && layers.base == no_mount ? Mode::Mode20 : mode;
        const auto patch_mode = mode == Mode::GameData && layers.patch == no_mount ? Mode::Mode20 : mode;
        MountedDirectory directory{ source, canonical_source, patch_path, canonical_patch, layers, base_mode, patch_mode, scope,
            track_open_handles ? std::make_shared<int>(0) : std::shared_ptr<void>{} };
        if (!directories_.emplace(name, std::move(directory)).second) {
            layers_.erase(layers.base);
            layers_.erase(layers.patch);
            LOG_ERROR("PFS: mount name already exists: {}", name);
            return false;
        }
        return true;
    } catch (const std::exception &error) {
        unmount(layers);
        LOG_ERROR("Cannot mount PFS directory '{}': {}", name, error.what());
        return false;
    }
}

std::optional<MountedDirectory> Mounts::get_directory(const std::string &name) const {
    std::lock_guard lock(mutex_);
    const auto it = directories_.find(name);
    if (it == directories_.end())
        return std::nullopt;
    return it->second;
}

bool Mounts::unmount_directory(const std::string &name, bool require_idle) {
    std::lock_guard lock(mutex_);
    const auto it = directories_.find(name);
    if (it == directories_.end() || (require_idle && it->second.lease && it->second.lease.use_count() > 1))
        return false;
    layers_.erase(it->second.layers.base);
    layers_.erase(it->second.layers.patch);
    directories_.erase(it);
    return true;
}

bool Mounts::split_patch(const std::string &base_name, const std::string &patch_name) {
    std::lock_guard lock(mutex_);
    const auto base = directories_.find(base_name);
    if (base == directories_.end() || base->second.patch.empty() || directories_.contains(patch_name))
        return false;
    const auto &entry = base->second;
    MountedDirectory patch{ entry.patch, entry.canonical_patch, {}, {}, { entry.layers.patch, no_mount }, entry.patch_mode, entry.patch_mode, entry.scope, entry.lease };
    directories_.emplace(patch_name, std::move(patch));
    // Insertion can rehash the table, so reacquire the base entry.
    auto &updated_base = directories_.at(base_name);
    updated_base.patch.clear();
    updated_base.canonical_patch.clear();
    updated_base.layers.patch = no_mount;
    return true;
}

std::shared_ptr<const Layer> Mounts::get(MountId id) const {
    std::lock_guard lock(mutex_);
    const auto it = layers_.find(id);
    return it == layers_.end() ? nullptr : it->second.layer;
}

void Mounts::unmount(MountedLayers layers) {
    std::lock_guard lock(mutex_);
    layers_.erase(layers.base);
    layers_.erase(layers.patch);
}

void Mounts::clear(MountScope scope) {
    std::lock_guard lock(mutex_);
    std::erase_if(directories_, [scope](const auto &entry) { return entry.second.scope == scope; });
    std::erase_if(layers_, [scope](const auto &entry) { return entry.second.scope == scope; });
}

size_t Mounts::size() const {
    std::lock_guard lock(mutex_);
    return layers_.size();
}

} // namespace pfsmgr