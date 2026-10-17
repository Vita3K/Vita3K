// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <app/functions.h>
#include <pfsmgr/functions.h>
#include <regmgr/functions.h>
#include <util/log.h>

#include <stdexcept>

namespace app {

static void ensure_identity_registry(RegMgrState &registry, const fs::path &vita_fs_path) {
    if (registry.system_dreg_path.empty())
        regmgr::init_regmgr(registry, vita_fs_path);
}

static bool recover_identity(RegMgrState &registry, const fs::path &vita_fs_path, std::string &error) {
    const auto ux0 = vita_fs_path / "ux0";
    const auto pending = pfsmgr::pending_savedata_identity(ux0);
    if (!pending)
        return true;
    ensure_identity_registry(registry, vita_fs_path);
    if (pending->remove_id_dat) {
        const auto id_dat = ux0 / "id.dat";
        std::error_code ec;
        const auto status = fs::symlink_status(id_dat, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) {
            error = "Cannot inspect ux0:id.dat: " + ec.message();
            LOG_ERROR("{}", error);
            return false;
        }
        if (fs::exists(status)) {
            if (!fs::is_regular_file(status) && !fs::is_symlink(status)) {
                error = "Cannot remove ux0:id.dat: not a file";
                LOG_ERROR("{}", error);
                return false;
            }
            if (!fs::remove(id_dat, ec) && ec) {
                error = "Cannot remove ux0:id.dat: " + ec.message();
                LOG_ERROR("{}", error);
                return false;
            }
        }
    }
    if (!regmgr::set_account_id(registry, pending->identity.account_id)) {
        error = "Failed to update registry account ID";
        LOG_ERROR("{}", error);
        return false;
    }
    pfsmgr::commit_savedata_identity(ux0);
    return true;
}

void recover_savedata_identity(RegMgrState &registry, const fs::path &vita_fs_path) {
    std::string error;
    if (!recover_identity(registry, vita_fs_path, error))
        throw std::runtime_error(error);
}

pfsmgr::SavedataIdentity get_savedata_identity(RegMgrState &registry, const fs::path &vita_fs_path) {
    recover_savedata_identity(registry, vita_fs_path);
    ensure_identity_registry(registry, vita_fs_path);
    const auto account_id = regmgr::get_account_id(registry);
    if (!account_id)
        throw std::runtime_error("Failed to read registry account ID");
    return { pfsmgr::get_or_create_open_psid(vita_fs_path / "ux0"), *account_id };
}

bool import_savedata_identity(RegMgrState &registry, const fs::path &vita_fs_path, const pfsmgr::SavedataIdentity &identity, std::string &error) {
    try {
        const auto previous = get_savedata_identity(registry, vita_fs_path);
        const auto ux0 = vita_fs_path / "ux0";
        pfsmgr::stage_savedata_identity(ux0, identity, previous);
        if (recover_identity(registry, vita_fs_path, error))
            return true;
    } catch (const std::runtime_error &exception) {
        error = exception.what();
        LOG_ERROR("Cannot change savedata identity: {}", error);
    }
    error += " (the identity transaction may be pending and must be recovered before launching an application)";
    return false;
}

bool reset_savedata_identity(RegMgrState &registry, const fs::path &vita_fs_path, std::string &error) {
    const auto open_psid = pfsmgr::generate_open_psid();
    if (!open_psid) {
        error = "Cannot generate emulated OpenPSID";
        return false;
    }
    return import_savedata_identity(registry, vita_fs_path, { *open_psid, {} }, error);
}

} // namespace app