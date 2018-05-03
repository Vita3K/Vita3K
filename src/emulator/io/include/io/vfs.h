#pragma once

#include <cstring>
#include <string>
#include <map>

using DeviceName = std::string;
using MountID = uint64_t;

struct MountPointDataEntry {
  MountID mount_id;
  std::string path; // original path
  std::string mount_point;
  std::string title_id;
  uint64_t auth_id[64]; // array of auth ids (auth id is required for PFS mount of NPDRM apps)

  MountPointDataEntry(MountID mount_id, const std::string& path,
                      const std::string& mount_point, const std::string& titleid,
                      uint64_t aauth_id[])
    : mount_id(mount_id), path(path), mount_point(mount_point), title_id(titleid)
  {
     memcpy(auth_id, aauth_id, 64 * sizeof(uint64_t));
  }
};

struct VfsState {
    std::map<MountID, MountPointDataEntry> mount_entries;
    MountID next_mount_id;

    // Although this was in the entry, it was the one and only
    std::string crr_title_id;
};

DeviceName gen_temp_device_name(DeviceName dvc);

MountID add_entry(const std::string& mount_point, const std::string& path, const std::string& title_id, uint64_t auth_id[], VfsState& state);
void remove_entry(MountID mount_id, VfsState& state);

// Force an entry to take a id
bool add_entry_force_id(const MountID mount_id, const std::string& mount_point, const std::string& title_id, uint64_t auth_id[], VfsState& state);

MountPointDataEntry get_entry(const MountID id, VfsState& state);

bool mount_gd(const std::string& path, const std::string& patch_path, const std::string& rif_file_path, std::string mp, VfsState& state);
