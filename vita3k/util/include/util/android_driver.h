#pragma once

#include <util/fs.h>

#include <optional>
#include <string>
#include <string_view>
#include <vulkan/vulkan.h>

namespace android_driver {

bool is_safe_driver_relative_path(const fs::path &relative_path);
std::optional<std::string> read_driver_library_name_from_meta(const fs::path &meta_path);
std::optional<std::string> resolve_custom_driver_library_name(const fs::path &driver_path);
bool is_custom_driver_loaded(const std::string &driver_name, uint32_t vendor_id, uint32_t driver_version, std::string_view device_name);
PFN_vkGetInstanceProcAddr resolve_vk_get_instance_proc_addr(const std::string &driver_name);

} // namespace android_driver
