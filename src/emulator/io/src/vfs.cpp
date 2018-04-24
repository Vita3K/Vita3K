#include <io/vfs.h>
#include <random>

std::string get_dvc(const std::string& path) {
    return path.substr(0, path.find_first_of(':'));
}

DeviceName gen_temp_device_name(DeviceName dvc) {
    std::random_device rdvc;
    std::mt19937 tw(rdvc());
    std::uniform_int_distribution<uint8_t> rdg(0, 255);
    std::string randomid;

    randomid.resize(11);

    for (auto& randomidc: randomid) {
        randomidc = std::to_string(rdg(tw))[0];
    }

    return dvc + randomid;
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

    // UNFINISHED

    return true;
}
