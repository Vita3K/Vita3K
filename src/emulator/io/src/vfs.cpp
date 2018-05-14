#include <io/vfs.h>

#include <random>
#include <optional>

std::string get_dvc(const std::string& path) {
    return path.substr(0, path.find_first_of(':'));
}

std::optional<DeviceName> gen_mount_dvc_name(MountID id, VfsState& state) {
    auto random_dvc_gen = [=](std::string dvc_name) -> std::string {
        auto res = gen_temp_device_name(dvc_name) + "0:";
        return res;
    };

    // AD drive
    if (id <= 0x70 || (id >= 0x190 && id <= 0x1F5))
        return random_dvc_gen("ad");

    switch (id) {
        case 0xC8: case 0xC9: case 0xCB: case 0xCC: case 0xCE: case 0xCF: {
            return random_dvc_gen("td");
        }

        case 0xCA: {
            auto entry = get_entry(id, state);

            // Don't mount ms0 yet. Don't have the method of checking auth id
            return random_dvc_gen("ms");
        }

        case 0xCD: {
            return "cache0:";
        }

        case 0x12E: {
            return "trophy_sys0:";
        }

        case 0x12F: {
            return "trophy_dat0:";
        }

        case 0x130: {
            return "trophy_dbk0:";
        }

        case 0x1F8: {
            return "sdimport0:";
        }

        case 0x1F9: {
            return "sdimport_tmp0:";
        }

        case 0x258: {
            return random_dvc_gen("lm");
        }

        case 0x3E8: case 0x3E9: {
            return "app0";
        }

        case 0x3EA: case 0x3EB: {
            return "appcont0:";
        }

        case 0x3ED: case 0x3EE: {
            return "savedata0:";
        }

        case 0x3EF: case 0x3F0: {
            return random_dvc_gen("sd");
        }

        case 0x3F1: {
            return random_dvc_gen("ud");
        }

        default:
            break;
    }

    // fallback
    return std::optional<DeviceName>{};
}

DeviceName gen_temp_device_name(DeviceName dvc) {
    std::random_device rdvc;
    std::mt19937 tw(rdvc());
    std::uniform_int_distribution<char> rdg(0, 255);
    std::string randomid;

    randomid.resize(6);

    for (auto& randomidc: randomid) {
        randomidc = std::to_string(rdg(tw))[0];
    }

    return dvc + randomid;
}

std::optional<MountPointDataEntry> get_entry(const MountID id, VfsState& state) {
    auto res = state.mount_entries.find(id);

    if (res != state.mount_entries.end()) {
        return std::optional<MountPointDataEntry>{};
    }

    return res->second;
}

MountID add_entry(const std::string& mount_point, const std::string& path, const std::string& title_id, uint64_t auth_id[], VfsState& state) {
    MountID this_mid = 0;

    for (;;) {
        this_mid = ++state.next_mount_id;
        auto res = state.mount_entries.emplace(this_mid, MountPointDataEntry(this_mid, path, mount_point, title_id, auth_id));

        if (res.second == true) {
            break;
        }
    }

    return this_mid;
}

bool add_entry_force_id(const MountID mount_id, const std::string& mount_point, const std::string& path, const std::string& title_id, uint64_t auth_id[], VfsState& state) {
    auto res = state.mount_entries.emplace(mount_id, MountPointDataEntry(mount_id, mount_point, path, title_id, auth_id));

    if (res.second == true) {
        return true;
    }

    return false;
}

void remove_entry(MountID mount_id, VfsState& state) {
    state.mount_entries.erase(mount_id);
}

bool mount_gd(const std::string& path, const std::string& patch_path, const std::string& rif_file_path, std::string mp, VfsState& state) {
    const std::string dvc = get_dvc(mp);

    if (mp.substr(dvc.length() + 1) == "data") {
        return false;
    }

    // What are you doing here naughty system device!
    if (dvc == "host0:" || dvc == "vs0:") {
        return false;
    }

    if (!rif_file_path.empty()) {
        // Riffing (no pun intended)

    }

    DeviceName rd_dvc_gamedata = gen_temp_device_name("gp");
    DeviceName rd_dvc_patch = gen_temp_device_name("gp");

    rd_dvc_gamedata += "d:";
    rd_dvc_patch += "d:";

    // Currently the auth id is not needed
    uint64_t auth_id[64];
    add_entry_force_id(0xE38, rd_dvc_gamedata + "/" + mp, path, state.crr_title_id, auth_id, state);

    if (!patch_path.empty()) {
        add_entry_force_id(0xE39, rd_dvc_patch + "/" + mp, patch_path, state.crr_title_id, auth_id, state);
    }

    return true;
}
