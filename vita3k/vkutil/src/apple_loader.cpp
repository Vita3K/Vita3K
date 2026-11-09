// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#ifdef __APPLE__

#include <vkutil/apple_loader.h>

#include <util/log.h>

#include <dlfcn.h>
#include <mach-o/dyld.h>

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <vector>

namespace vkutil::apple {

namespace {

std::string executable_dir() {
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size + 1, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        return {};

    char resolved[PATH_MAX];
    const std::string path = realpath(buffer.data(), resolved) ? resolved : buffer.data();
    const auto slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

struct Candidate {
    std::string path;
    bool is_loader;
};

std::vector<Candidate> make_candidates(bool prefer_loader) {
    const std::string exe_dir = executable_dir();

    std::vector<Candidate> moltenvk;
    if (!exe_dir.empty()) {
        // app bundle: Contents/MacOS/vita3k -> Contents/Frameworks/libMoltenVK.dylib
        moltenvk.push_back({ exe_dir + "/../Frameworks/libMoltenVK.dylib", false });
        moltenvk.push_back({ exe_dir + "/libMoltenVK.dylib", false });
    }
    moltenvk.push_back({ "@executable_path/../Frameworks/libMoltenVK.dylib", false });
    moltenvk.push_back({ "libMoltenVK.dylib", false });
    moltenvk.push_back({ "/usr/local/lib/libMoltenVK.dylib", false });
    moltenvk.push_back({ "/opt/homebrew/lib/libMoltenVK.dylib", false });
    moltenvk.push_back({ "MoltenVK.framework/MoltenVK", false });

    std::vector<Candidate> loader = {
        { "libvulkan.dylib", true },
        { "libvulkan.1.dylib", true },
        { "/usr/local/lib/libvulkan.dylib", true },
        { "/opt/homebrew/lib/libvulkan.dylib", true },
        { "vulkan.framework/vulkan", true },
    };

    std::vector<Candidate> candidates;
    if (const char *override_path = std::getenv("VITA3K_VULKAN_LIBRARY"); override_path && *override_path)
        candidates.push_back({ override_path, std::string_view(override_path).find("MoltenVK") == std::string_view::npos });

    if (prefer_loader) {
        candidates.insert(candidates.end(), loader.begin(), loader.end());
        candidates.insert(candidates.end(), moltenvk.begin(), moltenvk.end());
    } else {
        candidates.insert(candidates.end(), moltenvk.begin(), moltenvk.end());
        candidates.insert(candidates.end(), loader.begin(), loader.end());
    }
    return candidates;
}

// Create and destroy a minimal instance to make sure a driver is reachable through this library
bool probe_library(PFN_vkGetInstanceProcAddr get_instance_proc_addr, std::string &error) {
    const auto create_instance = reinterpret_cast<PFN_vkCreateInstance>(get_instance_proc_addr(nullptr, "vkCreateInstance"));
    const auto enumerate_extensions = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(get_instance_proc_addr(nullptr, "vkEnumerateInstanceExtensionProperties"));
    if (!create_instance || !enumerate_extensions) {
        error = "missing vkCreateInstance or vkEnumerateInstanceExtensionProperties";
        return false;
    }

    bool has_portability_enumeration = false;
    uint32_t extension_count = 0;
    if (enumerate_extensions(nullptr, &extension_count, nullptr) == VK_SUCCESS && extension_count > 0) {
        std::vector<VkExtensionProperties> extensions(extension_count);
        if (enumerate_extensions(nullptr, &extension_count, extensions.data()) == VK_SUCCESS) {
            for (const auto &extension : extensions) {
                if (strcmp(extension.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0)
                    has_portability_enumeration = true;
            }
        }
    }

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = VK_API_VERSION_1_0;

    const char *portability_extension = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    if (has_portability_enumeration) {
        instance_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        instance_info.enabledExtensionCount = 1;
        instance_info.ppEnabledExtensionNames = &portability_extension;
    }

    VkInstance instance = VK_NULL_HANDLE;
    const VkResult result = create_instance(&instance_info, nullptr, &instance);
    if (result != VK_SUCCESS) {
        error = fmt::format("vkCreateInstance failed with error {}", static_cast<int>(result));
        return false;
    }

    const auto enumerate_devices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(get_instance_proc_addr(instance, "vkEnumeratePhysicalDevices"));
    const auto destroy_instance = reinterpret_cast<PFN_vkDestroyInstance>(get_instance_proc_addr(instance, "vkDestroyInstance"));

    uint32_t device_count = 0;
    const bool has_device = enumerate_devices && enumerate_devices(instance, &device_count, nullptr) == VK_SUCCESS && device_count > 0;
    if (!has_device)
        error = "no Vulkan physical device is available";

    if (destroy_instance)
        destroy_instance(instance, nullptr);

    return has_device;
}

std::optional<VulkanLibrary> loaded_library;
std::mutex load_mutex;

} // namespace

const VulkanLibrary *load_vulkan_library(bool prefer_loader) {
    std::lock_guard<std::mutex> lock(load_mutex);
    if (loaded_library)
        return &*loaded_library;

    for (const Candidate &candidate : make_candidates(prefer_loader)) {
        void *handle = dlopen(candidate.path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle)
            continue;

        const auto get_instance_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(handle, "vkGetInstanceProcAddr"));
        std::string error;
        if (!get_instance_proc_addr) {
            error = "vkGetInstanceProcAddr is missing";
        } else if (probe_library(get_instance_proc_addr, error)) {
            // promote the library to global visibility so SDL (dlsym(RTLD_DEFAULT, "vkGetInstanceProcAddr"))
            // uses the same implementation
            dlopen(candidate.path.c_str(), RTLD_NOW | RTLD_GLOBAL);
            loaded_library = VulkanLibrary{
                .path = candidate.path,
                .is_loader = candidate.is_loader,
                .get_instance_proc_addr = get_instance_proc_addr,
            };
            LOG_INFO("Loaded Vulkan implementation from {} ({})", candidate.path, candidate.is_loader ? "Khronos loader" : "MoltenVK");
            return &*loaded_library;
        }

        LOG_WARN("Skipping Vulkan library {}: {}", candidate.path, error);
        dlclose(handle);
    }

    LOG_ERROR("No usable Vulkan implementation was found. MoltenVK should be bundled with Vita3K in Contents/Frameworks.");
    return nullptr;
}

} // namespace vkutil::apple

#endif
