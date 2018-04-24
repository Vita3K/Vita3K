#include <io/vfs.h>
#include <random>

DeviceName gen_temp_device_name(DeviceName dvc) {
    std::random_device rdvc;
    std::mt19937 tw(rdvc);
    std::uniform_int_distribution rdg(0, 255);
    std::string randomid(8);

    for (auto& randomidc: randomid) {
        randomidc = rdg(tw);
    }

    return dvc + randomidc;
}

MountID add_entry(const std::string& mount_point, const std::string& title_id, const std::string& auth_id, VfsState& state) {
    MountID this_mid = ++state.next_mount_id;
    state.mount_entries.emplace(this_mid, MountPointDataEntry(this_mid, mount_point, title_id, auth_id));

    return this_mid;
}

void remove_entry(MountID mount_id, VfsState& state) {
    state.mount_entries.erase(mount_id);
}
