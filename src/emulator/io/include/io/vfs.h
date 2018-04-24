#pragma once

#include <string>
#include <map>

using DeviceName = std::string;
using MountID = uint64_t;

struct MountPointDataEntry {
  int mount_id;
  std::string path; // original path
  std::string mount_point;
  std::string title_id;
  std::string auth_id; // array of auth ids (auth id is required for PFS mount of NPDRM apps)
};

struct VfsState {
    std::map<MountID, MountPointDataEntry> mount_entries;
    uint64_t next_mount_id;
};

DeviceName gen_temp_device_name(DeviceName dvc);

MountID add_entry(const std::string& mount_point, const std::string& title_id, const std::string& auth_id, VfsState& state);
void remove_entry(MountID mount_id, VfsState& state);
