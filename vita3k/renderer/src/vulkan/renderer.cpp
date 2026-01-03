// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#ifdef __ANDROID__
// must be first
#define __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__
#endif

#include <renderer/functions.h>
#include <renderer/types.h>
#include <renderer/vulkan/functions.h>
#include <renderer/vulkan/state.h>

#include <config/state.h>
#include <config/version.h>
#include <display/state.h>
#include <shader/spirv_recompiler.h>
#include <util/align.h>
#include <util/log.h>
#include <vkutil/vkutil.h>

#include <SDL3/SDL_vulkan.h>

#ifdef __APPLE__
#include <MoltenVK/mvk_vulkan.h>
#endif

#ifdef __ANDROID__
#include <SDL3/SDL_system.h>
#include <adrenotools/bcenabler.h>
#include <adrenotools/driver.h>
#include <boost/range/iterator_range.hpp>
#include <dlfcn.h>
#include <jni.h>
#include <sys/mman.h>
#include <util/float_to_half.h>

#include <android/hardware_buffer.h>

typedef struct native_handle {
    int version; /* sizeof(native_handle_t) */
    int numFds; /* number of file-descriptors at &data[0] */
    int numInts; /* number of ints at &data[numFds] */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-length-array"
#endif
    int data[0]; /* numFds + numInts ints */
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
} native_handle_t;

typedef const native_handle_t *buffer_handle_t;
// this function is exported by libandroid.so but not defined in the header
const native_handle_t *(*_AHardwareBuffer_getNativeHandle)(const AHardwareBuffer *buffer);

// functions defined in hardware_buffer.h that are dynamically load
decltype(AHardwareBuffer_allocate) *_AHardwareBuffer_allocate;
decltype(AHardwareBuffer_lock) *_AHardwareBuffer_lock;
decltype(AHardwareBuffer_unlock) *_AHardwareBuffer_unlock;
decltype(AHardwareBuffer_release) *_AHardwareBuffer_release;
#endif

static void debug_log_message(std::string_view msg) {
    static const char *ignored_errors[] = {
        "VUID-vkCmdDrawIndexed-None-02721", // using r8g8b8a8 with non-multiple of 4 stride
        "VUID-VkImageViewCreateInfo-usage-02275", // srgb does not support the storage format
        "VUID-VkImageCreateInfo-imageCreateMaxMipLevels-02251", // srgb does not support the storage format
        "VUID-vkCmdPipelineBarrier-pDependencies-02285", // shader write -> vertex input read self-dependency, wrong error
        "VUID-vkCmdDrawIndexed-None-09003", // reading from color attachment, works on most GPUs with a general layout
        "VUID-vkCmdDrawIndexed-None-06538", // reading from color attachment
        "VUID-vkCmdDrawIndexed-None-09000", // reading from color attachment
        "VKDBGUTILWARN003", // Some Adreno warning
        "VK_FORMAT_BC", // BCn patch
        "VUID-vkCmdCopyBufferToImage-dstImage-01997" // BCn patch
    };

    bool log_error = true;
    for (auto ignored_error : ignored_errors) {
        if (msg.find(ignored_error) != std::string_view::npos) {
            log_error = false;
            break;
        }
    }

    if (log_error)
        LOG_ERROR("Validation layer: {}", msg);
}

static vk::DebugUtilsMessengerEXT debug_messenger;
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_util_callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
    vk::DebugUtilsMessageTypeFlagsEXT message_type,
    const vk::DebugUtilsMessengerCallbackDataEXT *callback_data,
    void *pUserData) {
    if (message_severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
        // for now we are not interested by performance warnings
        && (message_type & ~vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)) {
        debug_log_message(callback_data->pMessage);
    }
    return VK_FALSE;
}

static vk::DebugReportCallbackEXT debug_report;
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report_callback(
    vk::DebugReportFlagsEXT flags,
    vk::DebugReportObjectTypeEXT objectType,
    uint64_t object,
    size_t location,
    int32_t messageCode,
    const char *layerPrefix,
    const char *message,
    void *pUserData) {
    std::string msg = fmt::format(
        "Validation layer: Vk{}:{}[0x{:X}]:I{}:L{}: {}",
        layerPrefix,
        vk::to_string(objectType),
        object,
        messageCode,
        location,
        message);

    debug_log_message(msg);

    return VK_FALSE;
}

const static std::vector<const char *> required_device_extensions = {
    vk::KHRSwapchainExtensionName,
    // needed in order to use storage buffers
    vk::KHRStorageBufferStorageClassExtensionName,
    // needed in order to use negative viewport height
    vk::KHRMaintenance1ExtensionName
};

namespace renderer::vulkan {

#ifdef __ANDROID__
static bool detect_patch_bcn(bool *support_dxt) {
    // some Adreno GPUs support BCn textures even though they say they don't
    // and we might need to patch a function for it to work

    // create an instance to get the patch address
    vk::ApplicationInfo application_info{
        .apiVersion = VK_API_VERSION_1_0
    };
    vk::InstanceCreateInfo instance_info{
        .pApplicationInfo = &application_info
    };

    vk::UniqueInstance instance = vk::createInstanceUnique(instance_info);
    // we need these 2 functions for the following part of the code
    VULKAN_HPP_DEFAULT_DISPATCHER.vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(instance.get(), "vkEnumeratePhysicalDevices"));
    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(instance.get(), "vkGetPhysicalDeviceProperties"));
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(instance.get(), "vkDestroyInstance"));

    // assume there is only one gpu
    vk::PhysicalDevice gpu = instance->enumeratePhysicalDevices().front();
    vk::PhysicalDeviceProperties properties = gpu.getProperties();

    const auto type = adrenotools_get_bcn_type(VK_VERSION_MAJOR(properties.driverVersion), VK_VERSION_MINOR(properties.driverVersion), properties.vendorID);
    if (type == ADRENOTOOLS_BCN_PATCH) {
        void *function_to_patch = reinterpret_cast<void *>(VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(instance.get(), "vkGetPhysicalDeviceFormatProperties"));
        if (adrenotools_patch_bcn(function_to_patch)) {
            LOG_INFO("Applied BCeNabler patch");
        } else {
            LOG_INFO("Failed to apply BCeNabler");
            return false;
        }
        *support_dxt = true;
    } else if (type == ADRENOTOOLS_BCN_BLOB) {
        LOG_INFO("BCeNabler skipped, blob BCN support is present");
        *support_dxt = true;
    }

    return true;
}
#endif

static bool device_is_compatible(const vk::PhysicalDevice &device) {
    const std::vector<vk::ExtensionProperties> available_extensions = device.enumerateDeviceExtensionProperties();

    std::set<std::string> required_extensions(required_device_extensions.begin(), required_device_extensions.end());
    for (const auto &extension : available_extensions)
        required_extensions.erase(extension.extensionName);

    return required_extensions.empty();
}

static bool select_queues(VKState &vk_state,
    std::vector<vk::DeviceQueueCreateInfo> &queue_infos, std::vector<std::vector<float>> &queue_priorities) {
    // TODO: Better queue allocation.

    /**
     * Here's the idea:
     *  - Dedicated queues to a task (e.g. with only graphics bit set) are faster.
     *  - Queues that appear first in the list are faster.
     *  - We really just need a queue for Graphics and Transfer right now afaik.
     *  - Multiple queues families can do the same thing.
     *  - Multiple different queues should be chosen if available.
     * The current algorithm only picks the first one it finds, a new algorithm should be made that takes everything into account.
     */

    bool found_graphics = false, found_transfer = false;

    for (uint32_t i = 0; i < vk_state.physical_device_queue_families.size(); i++) {
        const auto &queue_family = vk_state.physical_device_queue_families[i];

        // MoltenVK does not accept nullptr a pPriorities for some reason.
        std::vector<float> &priorities = queue_priorities.emplace_back(queue_family.queueCount, 1.0f);

        // Only one DeviceQueueCreateInfo should be created per family.
        if (!found_graphics && (queue_family.queueFlags & vk::QueueFlagBits::eGraphics)
#ifndef __ANDROID__
            && (queue_family.queueFlags & vk::QueueFlagBits::eTransfer)
#endif
            && vk_state.physical_device.getSurfaceSupportKHR(i, vk_state.screen_renderer.surface)) {
            vk::DeviceQueueCreateInfo queue_create_info{
                .queueFamilyIndex = i,
                .queueCount = queue_family.queueCount,
                .pQueuePriorities = priorities.data()
            };
            queue_infos.emplace_back(std::move(queue_create_info));
            vk_state.general_family_index = i;
            vk_state.transfer_family_index = i;
            found_graphics = true;
            found_transfer = true;
        }
        // for now use the same queue for graphics and transfer, to be improved on later
        /* else if (!found_transfer && queue_family.queueFlags&vk::QueueFlagBits::eTransfer) {
            vk::DeviceQueueCreateInfo queue_create_info{
                .queueFamilyIndex = i,
                .queueCount = queue_family.queueCount,
                .pQueuePriorities = priorities.data()
            };
            queue_infos.emplace_back(std::move(queue_create_info));
            vk_state.transfer_family_index = i;
            found_transfer = true;
        }
        */

        if (found_graphics && found_transfer)
            break;
    }

    return found_graphics && found_transfer;
}

// Adapted from https://github.com/SaschaWillems/vulkan.gpuinfo.org/blob/master/includes/functions.php
static std::string get_driver_version(uint32_t vendor_id, uint32_t version_raw) {
    // NVIDIA
    if (vendor_id == 4318)
        return fmt::format("{}.{}.{}.{}", (version_raw >> 22) & 0x3ff, (version_raw >> 14) & 0x0ff, (version_raw >> 6) & 0x0ff, version_raw & 0x003f);

#ifdef _WIN32
    // Intel drivers on Windows
    if (vendor_id == 0x8086)
        return fmt::format("{}.{}", version_raw >> 14, version_raw & 0x3fff);
#endif

    // Use Vulkan version conventions if vendor mapping is not available
    return fmt::format("{}.{}.{}", (version_raw >> 22) & 0x3ff, (version_raw >> 12) & 0x3ff, version_raw & 0xfff);
}

bool create(SDL_Window *window, std::unique_ptr<renderer::State> &state, const Config &config) {
    auto &vk_state = dynamic_cast<VKState &>(*state);

    return vk_state.create(window, state, config);
}

VKState::VKState(int gpu_idx)
    : gpu_idx(gpu_idx)
    , surface_cache(*this)
    , pipeline_cache(*this)
    , texture_cache(*this)
    , screen_renderer(*this)
    , buffer_trapping(*this) {
}

bool VKState::init() {
    shader_version = fmt::format("v{}", shader::CURRENT_VERSION);
    return true;
}

#ifdef __ANDROID__
static void *load_custom_adreno_driver(const std::string &driver_name) {
    const fs::path driver_path = fs::path(SDL_GetAndroidInternalStoragePath()) / "driver" / driver_name / "/";

    if (!fs::exists(driver_path)) {
        LOG_ERROR("Could not find driver {}", driver_name);
        return nullptr;
    }

    std::string main_so_name;
    {
        fs::path driver_name_file = driver_path / "driver_name.txt";
        if (!fs::exists(driver_name_file)) {
            LOG_ERROR("Could not find driver driver_name.txt");
            return nullptr;
        }

        fs::ifstream name_file(driver_name_file, std::ios_base::in);
        name_file >> main_so_name;
        name_file.close();
    }

    const char *temp_dir = nullptr;
    fs::path temp_dir_path;
    if (SDL_GetAndroidSDKVersion() < 29) {
        temp_dir_path = driver_path / "tmp/";
        fs::create_directory(temp_dir_path);
        temp_dir = temp_dir_path.c_str();
    }

    fs::path lib_dir;
    // retrieve the app lib dir using jni
    {
        // retrieve the JNI environment.
        JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
        env->PushLocalFrame(10);
        // retrieve the Java instance of the SDLActivity
        jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
        // the following calls activity.getApplicationInfo().nativeLibraryDir
        jclass actibity_class = env->GetObjectClass(activity);
        jmethodID getApplicationInfo_method = env->GetMethodID(actibity_class, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
        jobject app_info = env->CallObjectMethod(activity, getApplicationInfo_method);
        jclass app_info_class = env->GetObjectClass(app_info);
        jfieldID app_info_field = env->GetFieldID(app_info_class, "nativeLibraryDir", "Ljava/lang/String;");
        jstring lib_dir_java = reinterpret_cast<jstring>(env->GetObjectField(app_info, app_info_field));
        const char *lib_dir_ptr = env->GetStringUTFChars(lib_dir_java, nullptr);

        // copy the dir path in our local object
        lib_dir = fs::path(lib_dir_ptr) / "/";

        env->ReleaseStringUTFChars(lib_dir_java, lib_dir_ptr);
        // remove all local references
        env->PopLocalFrame(nullptr);
    }

    fs::create_directory(driver_path / "file_redirect");

    void *vulkan_handle = adrenotools_open_libvulkan(
        RTLD_NOW,
        ADRENOTOOLS_DRIVER_FILE_REDIRECT | ADRENOTOOLS_DRIVER_CUSTOM,
        temp_dir,
        lib_dir.c_str(),
        driver_path.c_str(),
        main_so_name.c_str(),
        (driver_path / "file_redirect/").c_str(),
        nullptr);

    if (!vulkan_handle) {
        LOG_ERROR("Could not open handle for custom driver {}", driver_name);
        return nullptr;
    }

    return vulkan_handle;
}
#endif

bool VKState::create(SDL_Window *window, std::unique_ptr<renderer::State> &state, const Config &config) {
    // Create Instance
    {
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

#ifdef __ANDROID__
        if (!config.current_config.custom_driver_name.empty()) {
            void *vulkan_handle = load_custom_adreno_driver(config.current_config.custom_driver_name);
            if (vulkan_handle) {
                vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(vulkan_handle, "vkGetInstanceProcAddr"));
                VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
                LOG_INFO("Custom Adreno driver {} injected successfully", config.current_config.custom_driver_name);
            }
        }

        if (!detect_patch_bcn(&texture_cache.support_dxt))
            return false;
#endif

        vk::ApplicationInfo app_info{
            .pApplicationName = app_name, // App Name
            .applicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 1), // App Version
            .pEngineName = org_name, // Engine Name, using org instead.
            .engineVersion = VK_MAKE_API_VERSION(0, 0, 0, 1), // Engine Version
            .apiVersion = VK_API_VERSION_1_0
        };

        unsigned int instance_req_ext_count;
        auto instance_extensions_str = SDL_Vulkan_GetInstanceExtensions(&instance_req_ext_count);
        std::vector<const char *> instance_extensions;
        instance_extensions.reserve(instance_req_ext_count + 6);
        for (size_t i = 0; i < instance_req_ext_count; i++) {
            instance_extensions.push_back(instance_extensions_str[i]);
        }

#ifdef __APPLE__
        // VK_KHR_portability_enumeration is a Vulkan Loader extension automatically added by SDL_Vulkan_GetInstanceExtensions.
        // When using MoltenVK directly without the Vulkan Loader, this extension causes an instant crash on startup.
        // Remove it from the default instance_extensions and handle it separately in the optional extensions.
        std::erase(instance_extensions, vk::KHRPortabilityEnumerationExtensionName);
#endif

        const std::set<std::string> optional_instance_extensions = {
            vk::KHRGetPhysicalDeviceProperties2ExtensionName,
            vk::KHRExternalMemoryCapabilitiesExtensionName,
            vk::KHRDeviceGroupCreationExtensionName,
#ifdef __APPLE__
            vk::KHRPortabilityEnumerationExtensionName,
            vk::EXTLayerSettingsExtensionName,
#endif
        };
        for (const vk::ExtensionProperties &prop : vk::enumerateInstanceExtensionProperties()) {
            auto ite = optional_instance_extensions.find(prop.extensionName);
            if (ite != optional_instance_extensions.end()) {
                instance_extensions.push_back(ite->c_str());
            }
        }

        // look if we can use the validation layer
        bool has_validation_layer = false;
        const std::array<const std::string, 2> debug_extensions = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            VK_EXT_DEBUG_REPORT_EXTENSION_NAME };
        // use a string, not a string view, on some mali devices the memory gets modified
        std::string found_debug_extension;
        for (const vk::ExtensionProperties &prop : vk::enumerateInstanceExtensionProperties()) {
            const std::string_view extension(prop.extensionName.data());
            for (const auto &debug_ext : debug_extensions) {
                if (debug_ext == extension)
                    found_debug_extension = extension;
            }
        }
        const std::string validation_layer = "VK_LAYER_KHRONOS_validation";
        for (const vk::LayerProperties &layer : vk::enumerateInstanceLayerProperties()) {
            if (std::string_view(layer.layerName.data()) == validation_layer) {
                has_validation_layer = true;
                break;
            }
        }

        std::vector<const char *> instance_layers;
        if (has_validation_layer && !found_debug_extension.empty()) {
            if (config.validation_layer) {
                LOG_INFO("Enabling vulkan validation layers (has a performance impact but allows better error messages)");
                instance_layers.push_back(validation_layer.c_str());
                instance_extensions.push_back(found_debug_extension.data());
            } else {
                LOG_INFO("Disabling Vulkan validation layers (may improve performance but provides limited error messages)");
            }
        }

#ifdef __APPLE__
        const VkBool32 full_image_swizzle = VK_TRUE;
#ifndef NDEBUG
        const VkBool32 debug = VK_TRUE;
        const int32_t log_level = 4;
#endif
        vk::LayerSettingEXT layer_settings[] = {
            { kMVKMoltenVKDriverLayerName, "MVK_CONFIG_FULL_IMAGE_VIEW_SWIZZLE", vk::LayerSettingTypeEXT::eBool32, 1,
                &full_image_swizzle },
#ifndef NDEBUG
            { kMVKMoltenVKDriverLayerName, "MVK_CONFIG_DEBUG", vk::LayerSettingTypeEXT::eBool32, 1, &debug },
            { kMVKMoltenVKDriverLayerName, "MVK_CONFIG_LOG_LEVEL", vk::LayerSettingTypeEXT::eInt32, 1, &log_level },
#endif
        };

        vk::LayerSettingsCreateInfoEXT layer_settings_info = {
            .pNext = nullptr,
            .settingCount = static_cast<uint32_t>(std::size(layer_settings)),
            .pSettings = layer_settings,
        };
#endif

        vk::InstanceCreateInfo instance_info{
#ifdef __APPLE__
            .flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
            .pNext = &layer_settings_info,
#endif
            .pApplicationInfo = &app_info,
        };
        instance_info.setPEnabledLayerNames(instance_layers);
        instance_info.setPEnabledExtensionNames(instance_extensions);

        instance = vk::createInstance(instance_info);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

        if (has_validation_layer && !found_debug_extension.empty() && config.validation_layer) {
            // we support two debugging extensions
            if (found_debug_extension == VK_EXT_DEBUG_UTILS_EXTENSION_NAME) {
                vk::DebugUtilsMessengerCreateInfoEXT debug_info{
                    .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
                    .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
                    .pfnUserCallback = debug_util_callback
                };
                debug_messenger = instance.createDebugUtilsMessengerEXT(debug_info);

            } else if (found_debug_extension == VK_EXT_DEBUG_REPORT_EXTENSION_NAME) {
                vk::DebugReportCallbackCreateInfoEXT report_info{
                    .flags = vk::DebugReportFlagBitsEXT::eError,
                    .pfnCallback = debug_report_callback
                };
                debug_report = instance.createDebugReportCallbackEXT(report_info);
            }
        }
    }

    // Create Surface
    if (!screen_renderer.create(window))
        return false;

    // Select Physical Device
    {
        std::vector<vk::PhysicalDevice> physical_devices = instance.enumeratePhysicalDevices();

        if (gpu_idx != 0 && gpu_idx <= physical_devices.size()) {
            // force choose the gpu
            physical_device = physical_devices[gpu_idx - 1];
        } else {
            // choose a suitable gpu
            for (const auto &device : physical_devices) {
                if (device_is_compatible(device)) {
                    physical_device = device;

                    // if it is an integrated gpu, try to find a discrete one
                    if (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
                        break;
                }
            }
        }

        if (!physical_device) {
            LOG_ERROR("Failed to select Vulkan physical device.");
            return false;
        }

        physical_device_properties = physical_device.getProperties();
        physical_device_features = physical_device.getFeatures();
        physical_device_memory = physical_device.getMemoryProperties();
        physical_device_queue_families = physical_device.getQueueFamilyProperties();

        LOG_INFO("Vulkan device: {}", physical_device_properties.deviceName.data());
        LOG_INFO("Driver version: {}", get_driver_version(physical_device_properties.vendorID, physical_device_properties.driverVersion));
    }

#ifdef __ANDROID__
    if (support_custom_drivers()) {
        // First I was looking for "Turnip" in the device name, however some turnip driver do not have it in their name for whatever reason....
        // so as a ugly workaround, say it is a turnip driver if the major driver version is less than 100
        uint32_t major_driver_version = physical_device_properties.driverVersion >> 22;
        is_adreno_stock = major_driver_version >= 100;
        is_adreno_turnip = major_driver_version < 100;
    }
#endif

    bool support_dedicated_allocations = false;
    // Create Device
    {
        std::vector<vk::DeviceQueueCreateInfo> queue_infos;
        std::vector<std::vector<float>> queue_priorities;
        if (!select_queues(*this, queue_infos, queue_priorities)) {
            LOG_ERROR("Failed to select proper Vulkan queues. This is likely a bug.");
            return false;
        }

        if (!physical_device.getSurfaceSupportKHR(
                general_family_index, screen_renderer.surface)) {
            LOG_ERROR("Failed to select a Vulkan queue that supports presentation. This is likely a bug.");
            return false;
        }

        // use these features (because they are used by the vita GPU) if they are available
        vk::PhysicalDeviceFeatures enabled_features{
            .fillModeNonSolid = physical_device_features.fillModeNonSolid,
            .wideLines = physical_device_features.wideLines,
            .samplerAnisotropy = physical_device_features.samplerAnisotropy,
            .occlusionQueryPrecise = physical_device_features.occlusionQueryPrecise,
            .fragmentStoresAndAtomics = physical_device_features.fragmentStoresAndAtomics,
            .shaderStorageImageExtendedFormats = physical_device_features.shaderStorageImageExtendedFormats,
            .shaderInt16 = physical_device_features.shaderInt16,
        };

        // look for optional extensions
        std::vector<const char *> device_extensions(required_device_extensions);
        bool temp_bool;
        bool support_global_priority = false;
        bool support_buffer_device_address = false;
        bool support_external_memory = false;
        bool support_shader_interlock = false;
        const std::map<std::string_view, bool *> optional_extensions = {
            { vk::KHRGetMemoryRequirements2ExtensionName, &temp_bool },
            // can be used by vma to improve performance
            { vk::KHRDedicatedAllocationExtensionName, &support_dedicated_allocations },
            // used to tell the driver this application is high priority
            { vk::EXTGlobalPriorityExtensionName, &support_global_priority },
            // can be used to specify which format will be used by mutable images
            { vk::KHRImageFormatListExtensionName, &surface_cache.support_image_format_specifier },
            { vk::KHRExternalMemoryExtensionName, &temp_bool },
            { vk::KHRDeviceGroupExtensionName, &temp_bool },
            // can host memory directly be used for gxm memory
            { vk::EXTExternalMemoryHostExtensionName, &support_external_memory },
            // also needed for reading mapped memory in the shader
            { vk::KHRBufferDeviceAddressExtensionName, &support_buffer_device_address },
            // needed for uniform uvec2 arrays not to take twice the size
            { vk::KHRUniformBufferStandardLayoutExtensionName, &support_standard_layout },
            // needed for FSR
            { vk::KHRShaderFloat16Int8ExtensionName, &support_fsr },
            // used for accurate programmable blending on desktop GPUs
            { vk::EXTFragmentShaderInterlockExtensionName, &support_shader_interlock },
#ifdef __APPLE__
            // Needed to create the MoltenVK device
            { vk::KHRPortabilitySubsetExtensionName, &temp_bool },
#endif
            // used for coherent framebuffer fetch
            { VK_EXT_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME, &support_rasterized_order_access },
#ifdef __ANDROID__
            // dependencies of VK_ANDROID_external_memory_android_hardware_buffer
            { VK_KHR_BIND_MEMORY_2_EXTENSION_NAME, &temp_bool },
            { VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME, &temp_bool },
            { VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME, &temp_bool },
            // used for memory trapping in android
            { VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME, &support_android_buffer_import },
            { VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, &support_unix_fd_import },
#endif
        };

        for (const vk::ExtensionProperties &ext : physical_device.enumerateDeviceExtensionProperties()) {
            auto it = optional_extensions.find(ext.extensionName.data());
            if (it != optional_extensions.end()) {
                // this extension is available on the GPU
                *it->second = true;
                device_extensions.push_back(it->first.data());
            }
        }

        bool support_memory_mapping = true;
        if (support_buffer_device_address) {
            auto features = physical_device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceBufferDeviceAddressFeatures>();
            support_buffer_device_address &= static_cast<bool>(features.get<vk::PhysicalDeviceBufferDeviceAddressFeatures>().bufferDeviceAddress);
        }
        support_memory_mapping &= support_buffer_device_address;

        if (support_standard_layout) {
            auto features = physical_device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceUniformBufferStandardLayoutFeatures>();
            support_standard_layout &= static_cast<bool>(features.get<vk::PhysicalDeviceUniformBufferStandardLayoutFeatures>().uniformBufferStandardLayout);
        }
        support_memory_mapping &= support_standard_layout;

#ifdef __APPLE__
        // we need to make a copy of the vertex buffer for moltenvk, so disable memory mapping
        support_memory_mapping = false;
#endif

#ifdef __ANDROID__
        support_android_buffer_import &= SDL_GetAndroidSDKVersion() >= 26;
        support_unix_fd_import &= SDL_GetAndroidSDKVersion() >= 26;
#endif

        // Find which memory mapping methods are supported by the GPU
        supported_mapping_methods_mask = (1 << static_cast<int>(MappingMethod::Disabled));
        if (support_memory_mapping) {
            // No additional check needed for these methods
            mapping_method = MappingMethod::DoubleBuffer;
            supported_mapping_methods_mask |= (1 << static_cast<int>(MappingMethod::DoubleBuffer));
            supported_mapping_methods_mask |= (1 << static_cast<int>(MappingMethod::PageTable));

            if (support_external_memory) {
                // disable this extension on GPUs with an alignment requirement higher than 4096 (should only
                // concern a few intel iGPUs)
                auto props = physical_device.getProperties2KHR<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceExternalMemoryHostPropertiesEXT>();
                support_external_memory = (props.get<vk::PhysicalDeviceExternalMemoryHostPropertiesEXT>().minImportedHostPointerAlignment <= 4096);
            }

            if (support_external_memory)
                supported_mapping_methods_mask |= (1 << static_cast<int>(MappingMethod::ExernalHost));

#ifdef __ANDROID__
            if (support_android_buffer_import || support_unix_fd_import)
                supported_mapping_methods_mask |= (1 << static_cast<int>(MappingMethod::NativeBuffer));
#endif
        }

        if (physical_device_properties.vendorID == 4318) {
            // Nvidia does not allow us to set the device priority higher than normal
            // no need to remove the priority extension
            support_global_priority = false;
        }
        // this is an emulator, tell the system it should have a high priority
        const vk::DeviceQueueGlobalPriorityCreateInfoEXT queue_priority{
            .globalPriority = vk::QueueGlobalPriorityEXT::eHigh
        };
        if (support_global_priority) {
            // add queue_priority to each queue creation info
            for (auto &queue_info : queue_infos) {
                queue_info.pNext = &queue_priority;
            }
        }

        support_fsr &= static_cast<bool>(physical_device_features.shaderInt16);
        if (support_fsr) {
            // double check for FP16 support
            auto props = physical_device.getFeatures2KHR<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceShaderFloat16Int8Features>();
            support_fsr = static_cast<bool>(props.get<vk::PhysicalDeviceShaderFloat16Int8Features>().shaderFloat16);
        }

        if (support_rasterized_order_access) {
            auto props = physical_device.getFeatures2KHR<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT>();
            support_rasterized_order_access = static_cast<bool>(props.get<vk::PhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT>().rasterizationOrderColorAttachmentAccess);
            // although both should never be supported at the same time, rasterized order access is far better than shader interlock
            support_shader_interlock = false;
        }

        support_shader_interlock &= static_cast<bool>(physical_device_features.fragmentStoresAndAtomics);
        if (support_shader_interlock) {
            auto props = physical_device.getFeatures2KHR<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceFragmentShaderInterlockFeaturesEXT>();
            support_shader_interlock = static_cast<bool>(props.get<vk::PhysicalDeviceFragmentShaderInterlockFeaturesEXT>().fragmentShaderSampleInterlock);
            features.support_shader_interlock = support_shader_interlock;
        }

        vk::StructureChain<vk::DeviceCreateInfo,
            vk::PhysicalDeviceBufferDeviceAddressFeatures,
            vk::PhysicalDeviceUniformBufferStandardLayoutFeatures,
            vk::PhysicalDeviceShaderFloat16Int8Features,
            vk::PhysicalDeviceFragmentShaderInterlockFeaturesEXT,
            vk::PhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT>
            device_info{
                vk::DeviceCreateInfo{
                    .pEnabledFeatures = &enabled_features },
                vk::PhysicalDeviceBufferDeviceAddressFeatures{
                    .bufferDeviceAddress = VK_TRUE },
                vk::PhysicalDeviceUniformBufferStandardLayoutFeatures{
                    .uniformBufferStandardLayout = VK_TRUE },
                vk::PhysicalDeviceShaderFloat16Int8Features{
                    // FSR uses float16
                    .shaderFloat16 = VK_TRUE },
                vk::PhysicalDeviceFragmentShaderInterlockFeaturesEXT{
                    .fragmentShaderSampleInterlock = VK_TRUE },
                vk::PhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT{
                    .rasterizationOrderColorAttachmentAccess = VK_TRUE }
            };
        device_info.get().setQueueCreateInfos(queue_infos);
        device_info.get().setPEnabledExtensionNames(device_extensions);

        if (!support_memory_mapping)
            device_info.unlink<vk::PhysicalDeviceBufferDeviceAddressFeatures>();

        if (!support_standard_layout)
            device_info.unlink<vk::PhysicalDeviceUniformBufferStandardLayoutFeatures>();

        if (!support_rasterized_order_access)
            device_info.unlink<vk::PhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT>();

        if (!support_fsr)
            device_info.unlink<vk::PhysicalDeviceShaderFloat16Int8Features>();

        if (!support_shader_interlock)
            device_info.unlink<vk::PhysicalDeviceFragmentShaderInterlockFeaturesEXT>();

        try {
            device = physical_device.createDevice(device_info.get());
        } catch (vk::NotPermittedError &) {
            // according to the vk spec, when using a priority higher than medium
            // we can get this error (although I think it will only possibly happen
            // for realtime priority)
            for (auto &queue_info : queue_infos) {
                queue_info.pNext = nullptr;
            }
            device = physical_device.createDevice(device_info.get());
        }
        VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
    }

    // Get Queues
    general_queue = device.getQueue(general_family_index, 0);
    transfer_queue = device.getQueue(transfer_family_index, 0);

    // Create Command Pools
    {
        vk::CommandPoolCreateInfo general_pool_info{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, // Flags
            .queueFamilyIndex = general_family_index // Queue Family Index
        };

        vk::CommandPoolCreateInfo transfer_pool_info{
            .flags = vk::CommandPoolCreateFlagBits::eTransient, // Flags
            .queueFamilyIndex = transfer_family_index // Queue Family Index
        };

        general_command_pool = device.createCommandPool(general_pool_info);
        transfer_command_pool = device.createCommandPool(transfer_pool_info);

        general_pool_info.flags |= vk::CommandPoolCreateFlagBits::eTransient;
        multithread_command_pool = device.createCommandPool(general_pool_info);
    }

    // Allocate Memory for Images and Buffers
    {
        vma::VulkanFunctions vulkan_functions{
            .vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr
        };

        vma::AllocatorCreateInfo allocator_info = {
            // everything vma-related is done on one thread, no need for thread safety
            .flags = vma::AllocatorCreateFlagBits::eExternallySynchronized,
            .physicalDevice = physical_device,
            .device = device,
            .pVulkanFunctions = &vulkan_functions,
            .instance = instance,
            .vulkanApiVersion = VK_API_VERSION_1_0,
        };

        if (support_dedicated_allocations)
            allocator_info.flags |= vma::AllocatorCreateFlagBits::eKhrDedicatedAllocation;

        // if memory mapping is supported
        if (supported_mapping_methods_mask > 1)
            allocator_info.flags |= vma::AllocatorCreateFlagBits::eBufferDeviceAddress;

        allocator = vma::createAllocator(allocator_info);
        vkutil::init(allocator);
    }

    // create the default image and buffer
    {
        default_buffer = vkutil::Buffer(KiB(4));
        default_buffer.init_buffer(vk::BufferUsageFlagBits::eVertexBuffer);

        // create the default image, it must be cleared then transitioned
        default_image = vkutil::Image(1, 1, vk::Format::eR8G8B8A8Unorm);

        default_image.init_image(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
        vk::CommandBuffer cmd_buffer = vkutil::create_single_time_command(device, general_command_pool);
        default_image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst);
        // make it white
        vk::ClearColorValue white{
            .float32 = std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f }
        };
        cmd_buffer.clearColorImage(default_image.image, vk::ImageLayout::eTransferDstOptimal, white, vkutil::color_subresource_range);
        default_image.transition_to(cmd_buffer, vkutil::ImageLayout::StorageImage);
        vkutil::end_single_time_command(device, general_queue, general_command_pool, cmd_buffer);

        // create the default sampler
        vk::SamplerCreateInfo sampler_info{
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = vk::SamplerAddressMode::eRepeat,
            .addressModeV = vk::SamplerAddressMode::eRepeat,
            .addressModeW = vk::SamplerAddressMode::eRepeat,
            .minLod = 0.0f,
            .maxLod = 0.0f,
        };
        default_image.sampler = device.createSampler(sampler_info);
    }

    // create the frame objects
    for (int i = 0; i < MAX_FRAMES_RENDERING; i++) {
        FrameObject &frame = frames[i];

        vk::CommandPoolCreateInfo pool_info{
            .queueFamilyIndex = general_family_index
        };

        frame.render_pool = device.createCommandPool(pool_info);
        pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        frame.prerender_pool = device.createCommandPool(pool_info);

        frame.destroy_queue.init(device);
    }

    if (!screen_renderer.setup())
        return false;

    support_fsr &= static_cast<bool>(screen_renderer.surface_capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eStorage);

#if defined(__linux__) && !defined(__ANDROID__) // According to my tests (Macdu), mprotect on buffers (mapped with external memory host) only works with Nvidia drivers
    surface_cache.can_mprotect_mapped_memory = std::string_view(physical_device_properties.deviceName).find("NVIDIA") != std::string_view::npos;
#endif

    return true;
}

void VKState::late_init(const Config &cfg, const std::string_view game_id, MemState &mem) {
    this->mem = &mem;

    bool use_high_accuracy = cfg.current_config.high_accuracy;

    // shader interlock is more accurate but slower
    if (features.support_shader_interlock && use_high_accuracy) {
        LOG_INFO("Using shader interlock for accurate framebuffer fetch emulation");
    } else {
        // We use subpass input to get something similar to direct fragcolor access (there is no difference for the shader)
        features.direct_fragcolor = true;
        features.support_shader_interlock = false;
    }

    // texture viewport is faster but not entirely accurate
    if (support_standard_layout && !use_high_accuracy) {
        LOG_INFO("The Vulkan renderer is using texture viewport for better performance");
        features.use_texture_viewport = true;
    }

    // parse the mapping method
    auto &config_mapping = cfg.current_config.memory_mapping;
    MappingMethod request_mapping = MappingMethod::Disabled;
    if (config_mapping == "double-buffer")
        request_mapping = MappingMethod::DoubleBuffer;
    else if (config_mapping == "external-host")
        request_mapping = MappingMethod::ExernalHost;
    else if (config_mapping == "page-table")
        request_mapping = MappingMethod::PageTable;
    else if (config_mapping == "native-buffer")
        request_mapping = MappingMethod::NativeBuffer;
    const std::string_view mapping_string[] = { "Disabled", "Double buffer", "External Host", "Page Table", "Native Buffer" };

    if ((1 << static_cast<int>(request_mapping)) & supported_mapping_methods_mask)
        // we support the requested mapping method
        mapping_method = request_mapping;

    features.enable_memory_mapping = mapping_method != MappingMethod::Disabled;

#ifdef __ANDROID__
    if (mapping_method == MappingMethod::NativeBuffer) {
        // dynamically load the symbols
        void *libandroid = dlopen("libandroid.so", RTLD_LAZY);
        _AHardwareBuffer_getNativeHandle = reinterpret_cast<decltype(_AHardwareBuffer_getNativeHandle)>(dlsym(libandroid, "AHardwareBuffer_getNativeHandle"));
        _AHardwareBuffer_allocate = reinterpret_cast<decltype(_AHardwareBuffer_allocate)>(dlsym(libandroid, "AHardwareBuffer_allocate"));
        _AHardwareBuffer_lock = reinterpret_cast<decltype(_AHardwareBuffer_lock)>(dlsym(libandroid, "AHardwareBuffer_lock"));
        _AHardwareBuffer_unlock = reinterpret_cast<decltype(_AHardwareBuffer_unlock)>(dlsym(libandroid, "AHardwareBuffer_unlock"));
        _AHardwareBuffer_release = reinterpret_cast<decltype(_AHardwareBuffer_release)>(dlsym(libandroid, "AHardwareBuffer_release"));
    }
#endif

    LOG_INFO("Using the following memory mapping method: {}", mapping_string[static_cast<int>(mapping_method)]);

    pipeline_cache.init(support_rasterized_order_access);

    texture_cache.init(true, texture_folder(), game_id);
}

void VKState::cleanup() {
    device.waitIdle();

    screen_renderer.cleanup();

    allocator.destroy();

    device.destroy(general_command_pool);
    device.destroy(transfer_command_pool);

    device.destroy();
    instance.destroy();
}

void VKState::render_frame(const SceFVector2 &viewport_pos, const SceFVector2 &viewport_size, DisplayState &display,
    const GxmState &gxm, MemState &mem) {
    // we are displaying this frame, wait for a new one
    should_display = false;

    DisplayFrameInfo frame;
    {
        std::lock_guard<std::mutex> guard(display.display_info_mutex);
        frame = display.next_rendered_frame;
    }

    if (!frame.base)
        return;

    if (!screen_renderer.acquire_swapchain_image())
        return;

    // Check if the surface exists
    Viewport viewport;
    viewport.width = static_cast<uint32_t>(frame.image_size.x * res_multiplier);
    viewport.height = static_cast<uint32_t>(frame.image_size.y * res_multiplier);

    vk::ImageLayout layout = vk::ImageLayout::eGeneral;
    vk::ImageView surface_handle = surface_cache.sourcing_color_surface_for_presentation(
        frame.base, frame.pitch, viewport);

    if (!surface_handle) {
        vkutil::Image &vita_surface = screen_renderer.vita_surface[screen_renderer.swapchain_image_idx];
        if (frame.image_size.x != vita_surface.width || frame.image_size.y != vita_surface.height) {
            // re-create the image
            vita_surface.destroy();
            vita_surface = vkutil::Image(frame.image_size.x, frame.image_size.y, vk::Format::eR8G8B8A8Unorm);
            vita_surface.init_image(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
        }

        // copy surface to staging buffer
        const vk::DeviceSize texture_data_size = frame.pitch * frame.image_size.y * 4;
        memcpy(screen_renderer.vita_surface_staging_info.pMappedData, frame.base.get(mem), texture_data_size);

        // copy staging buffer to image
        auto &cmd_buffer = screen_renderer.current_cmd_buffer;
        vita_surface.transition_to_discard(cmd_buffer, vkutil::ImageLayout::TransferDst);
        vk::BufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = frame.pitch,
            .bufferImageHeight = static_cast<uint32_t>(frame.image_size.y),
            .imageSubresource = vkutil::color_subresource_layer,
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { static_cast<uint32_t>(frame.image_size.x), static_cast<uint32_t>(frame.image_size.y), 1 }
        };
        cmd_buffer.copyBufferToImage(screen_renderer.vita_surface_staging, vita_surface.image, vk::ImageLayout::eTransferDstOptimal, region);

        vita_surface.transition_to(cmd_buffer, vkutil::ImageLayout::SampledImage);

        surface_handle = vita_surface.view;
        viewport = {
            .offset_x = 0,
            .offset_y = 0,
            .width = static_cast<uint32_t>(frame.image_size.x),
            .height = static_cast<uint32_t>(frame.image_size.y),
            .texture_width = static_cast<uint32_t>(frame.image_size.x),
            .texture_height = static_cast<uint32_t>(frame.image_size.y)
        };
        layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    screen_renderer.render(surface_handle, layout, viewport);
}

void VKState::swap_window(SDL_Window *window) {
    screen_renderer.swap_window();

    // look once a frame if we need to save the pipeline cache
    const auto time_s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (time_s >= pipeline_cache.next_pipeline_cache_save) {
        pipeline_cache.save_pipeline_cache();

        pipeline_cache.next_pipeline_cache_save = std::numeric_limits<uint64_t>::max();
    }
}

std::vector<uint32_t> VKState::dump_frame(DisplayState &display, uint32_t &width, uint32_t &height) {
    DisplayFrameInfo frame;
    {
        std::lock_guard<std::mutex> guard(display.display_info_mutex);
        frame = display.next_rendered_frame;
    }

    width = static_cast<uint32_t>(frame.image_size.x * res_multiplier);
    height = static_cast<uint32_t>(frame.image_size.y * res_multiplier);
    return surface_cache.dump_frame(frame.base, width, height, frame.pitch);
}

uint32_t VKState::get_features_mask() {
    union {
        struct {
            bool use_shader_interlock : 1;
            bool use_texture_viewport : 1;
            bool use_memory_mapping : 1;
            bool use_rgb_attributes : 1;
            bool use_scaled_attributes : 1;
        };
        uint32_t value;
    } features_mask;
    static_assert(sizeof(features_mask) == sizeof(uint32_t));

    features_mask.value = 0;
    features_mask.use_shader_interlock = features.support_shader_interlock;
    features_mask.use_texture_viewport = features.use_texture_viewport;
    features_mask.use_memory_mapping = features.enable_memory_mapping;
    features_mask.use_rgb_attributes = features.support_rgb_attributes;
    features_mask.use_scaled_attributes = pipeline_cache.support_scaled_vertex_attribute;

    return features_mask.value;
}

int VKState::get_supported_filters() {
    int filters = static_cast<int>(Filter::NEAREST) | static_cast<int>(Filter::BILINEAR) | static_cast<int>(Filter::BICUBIC) | static_cast<int>(Filter::FXAA);
    if (support_fsr)
        filters |= static_cast<int>(Filter::FSR);
    return filters;
}

void VKState::set_screen_filter(const std::string_view &filter) {
    if (filter == "FSR" && !support_fsr) {
        LOG_WARN("Trying to enable FSR but the GPU does not support it");
        screen_renderer.set_filter("");
    } else {
        screen_renderer.set_filter(filter);
    }
}

bool VKState::map_memory(MemState &mem, Ptr<void> address, uint32_t size) {
    assert(features.enable_memory_mapping);
    // the address should be 4K aligned
    assert((address.address() & 4095) == 0);
    constexpr vk::BufferUsageFlags mapped_memory_flags = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst;

    auto find_mem_type_with_flag = [&](const vk::MemoryPropertyFlags flags, uint32_t hardware_types) {
        while (hardware_types != 0) {
            // try to find a cached memory type
            int mapped_memory_type = std::countr_zero(hardware_types);
            hardware_types -= (1 << mapped_memory_type);

            if ((physical_device_memory.memoryTypes[mapped_memory_type].propertyFlags & flags) == flags)
                return mapped_memory_type;
        }
        return -1;
    };

    auto find_suitable_mapped_type = [&](uint32_t hardware_types) {
        // first try to find a memory that is both coherent and cached
        int mapped_memory_type = find_mem_type_with_flag(vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached, hardware_types);
        if (mapped_memory_type == -1)
            // then only coherent (lower performance)
            mapped_memory_type = find_mem_type_with_flag(vk::MemoryPropertyFlagBits::eHostCoherent, hardware_types);

        if (mapped_memory_type == -1) {
            static bool has_happened = false;
            LOG_CRITICAL_IF(!has_happened, "No coherent memory available for memory mapping!");
            has_happened = true;
            mapped_memory_type = std::countr_zero(hardware_types);
        }

        return static_cast<uint32_t>(mapped_memory_type);
    };

    switch (mapping_method) {
    case MappingMethod::NativeBuffer: {
#ifdef __ANDROID__
        // if we get there, this means we support the hardware buffer extension
        AHardwareBuffer_Desc buffer_desc{
            .width = static_cast<uint32_t>(size + KiB(4)),
            .height = 1,
            .layers = 1,
            .format = AHARDWAREBUFFER_FORMAT_BLOB,
            .usage = AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER | AHARDWAREBUFFER_USAGE_CPU_READ_MASK | AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK,
        };
        AHardwareBuffer *buffer;
        int err = _AHardwareBuffer_allocate(&buffer_desc, &buffer);
        if (err != 0) {
            LOG_ERROR("Failed to allocate Android hardware buffer, error {}", err);
            return false;
        }
        void *mapped_location;
        err = _AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_READ_MASK | AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK, -1, nullptr, &mapped_location);
        if (err != 0) {
            LOG_ERROR("Failed to lock Android hardware buffer, error {}", err);
            return false;
        }

        vk::DeviceMemory device_memory;
        // prefer this extension
        if (support_android_buffer_import) {
            const vk::AndroidHardwareBufferPropertiesANDROID hardware_props = device.getAndroidHardwareBufferPropertiesANDROID(*buffer);

            uint32_t mapped_memory_type = find_suitable_mapped_type(hardware_props.memoryTypeBits);
            vk::StructureChain<vk::MemoryAllocateInfo, vk::ImportAndroidHardwareBufferInfoANDROID, vk::MemoryAllocateFlagsInfo> alloc_info{
                vk::MemoryAllocateInfo{
                    .allocationSize = size + KiB(4),
                    .memoryTypeIndex = mapped_memory_type },
                vk::ImportAndroidHardwareBufferInfoANDROID{
                    .buffer = buffer },
                vk::MemoryAllocateFlagsInfo{
                    .flags = vk::MemoryAllocateFlagBits::eDeviceAddress }
            };
            device_memory = device.allocateMemory(alloc_info.get());
        } else {
            const native_handle_t *handle = _AHardwareBuffer_getNativeHandle(buffer);
            if (handle == nullptr || handle->numFds == 0 || handle->data[0] == -1) {
                LOG_ERROR("Failed to get native handle");
                return false;
            }

            int fd = handle->data[0];
            const vk::MemoryFdPropertiesKHR fd_props = device.getMemoryFdPropertiesKHR(vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd, fd);
            uint32_t mapped_memory_type = find_suitable_mapped_type(fd_props.memoryTypeBits);
            vk::StructureChain<vk::MemoryAllocateInfo, vk::ImportMemoryFdInfoKHR, vk::MemoryAllocateFlagsInfo> alloc_info{
                vk::MemoryAllocateInfo{
                    .allocationSize = size + KiB(4),
                    .memoryTypeIndex = mapped_memory_type },
                vk::ImportMemoryFdInfoKHR{
                    .handleType = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd,
                    .fd = fd },
                vk::MemoryAllocateFlagsInfo{
                    .flags = vk::MemoryAllocateFlagBits::eDeviceAddress }
            };
            device_memory = device.allocateMemory(alloc_info.get());
        }

        vk::StructureChain<vk::BufferCreateInfo, vk::ExternalMemoryBufferCreateInfoKHR> buffer_info{
            vk::BufferCreateInfo{
                .size = size + KiB(4),
                .usage = mapped_memory_flags,
                .sharingMode = vk::SharingMode::eExclusive },
            vk::ExternalMemoryBufferCreateInfoKHR{
                .handleTypes = support_android_buffer_import ? vk::ExternalMemoryHandleTypeFlagBits::eAndroidHardwareBufferANDROID : vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd }
        };
        const vk::Buffer mapped_buffer = device.createBuffer(buffer_info.get());
        device.bindBufferMemory(mapped_buffer, device_memory, 0);

        vk::BufferDeviceAddressInfoKHR address_info{
            .buffer = mapped_buffer
        };
        const uint64_t buffer_address = device.getBufferAddress(address_info);

        add_external_mapping(mem, address.address(), size, reinterpret_cast<uint8_t *>(mapped_location));
        mapped_memories[address.address()] = { address.address(), ExternalBuffer{ device_memory, buffer }, mapped_buffer, size, buffer_address };
#else
        LOG_ERROR("Native buffer is only supported on Android!\n");
#endif
        break;
    }
    case MappingMethod::PageTable: {
        // add 4 KiB because we can as an easy way to prevent crashes due to memory accesses right after the memory boundary
        // also make sure later the mapped address is 4K aligned
        vkutil::Buffer buffer(size + KiB(4));
        constexpr vma::AllocationCreateInfo memory_mapped_alloc = {
            .flags = vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
            .usage = vma::MemoryUsage::eAutoPreferHost,
            .requiredFlags = vk::MemoryPropertyFlagBits::eHostCoherent,
            .preferredFlags = vk::MemoryPropertyFlagBits::eHostCached,
        };
        buffer.init_buffer(mapped_memory_flags, memory_mapped_alloc);
        const uint64_t buffer_ptr_val = std::bit_cast<uint64_t>(buffer.mapped_data);
        const uint64_t buffer_offset = align(buffer_ptr_val, KiB(4)) - buffer_ptr_val;
        buffer.mapped_data = std::bit_cast<void *>(buffer_ptr_val + buffer_offset);

        vk::BufferDeviceAddressInfoKHR address_info{
            .buffer = buffer.buffer
        };
        const uint64_t buffer_address = device.getBufferAddress(address_info) + buffer_offset;
        const vk::Buffer mapped_buffer = buffer.buffer;

        add_external_mapping(mem, address.address(), size, static_cast<uint8_t *>(buffer.mapped_data));
        mapped_memories[address.address()] = { address.address(), std::move(buffer), mapped_buffer, size, buffer_address };
        break;
    }

    case MappingMethod::ExernalHost: {
        void *host_address = address.get(mem);
        auto host_mem_props = device.getMemoryHostPointerPropertiesEXT(vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT, host_address);
        assert(host_mem_props.memoryTypeBits != 0);

        int mapped_memory_type = -1;
        auto find_mem_type_with_flag = [&](const vk::MemoryPropertyFlags flags) {
            uint32_t host_mem_types = host_mem_props.memoryTypeBits;
            while (host_mem_types != 0) {
                // try to find a cached memory type
                mapped_memory_type = std::countr_zero(host_mem_types);
                host_mem_types -= (1 << mapped_memory_type);

                if ((physical_device_memory.memoryTypes[mapped_memory_type].propertyFlags & flags) == flags)
                    return;
            }

            mapped_memory_type = -1;
        };

        // first try to find a memory that is both coherent and cached
        find_mem_type_with_flag(vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached);
        if (mapped_memory_type == -1)
            // then only coherent (lower performance)
            find_mem_type_with_flag(vk::MemoryPropertyFlagBits::eHostCoherent);

        if (mapped_memory_type == -1) {
            LOG_CRITICAL_ONCE("No coherent memory available for memory mapping, this may be caused by an old driver!");
            mapped_memory_type = std::countr_zero(host_mem_props.memoryTypeBits);
        }

        vk::StructureChain<vk::MemoryAllocateInfo, vk::ImportMemoryHostPointerInfoEXT, vk::MemoryAllocateFlagsInfo> alloc_info{
            vk::MemoryAllocateInfo{
                .allocationSize = size,
                .memoryTypeIndex = static_cast<uint32_t>(mapped_memory_type) },
            vk::ImportMemoryHostPointerInfoEXT{
                .handleType = vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT,
                .pHostPointer = host_address },
            vk::MemoryAllocateFlagsInfo{
                .flags = vk::MemoryAllocateFlagBits::eDeviceAddress }
        };
        const vk::DeviceMemory device_memory = device.allocateMemory(alloc_info.get());

        vk::StructureChain<vk::BufferCreateInfo, vk::ExternalMemoryBufferCreateInfoKHR> buffer_info{
            vk::BufferCreateInfo{
                .size = size,
                .usage = mapped_memory_flags,
                .sharingMode = vk::SharingMode::eExclusive },
            vk::ExternalMemoryBufferCreateInfoKHR{
                .handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT }
        };
        const vk::Buffer mapped_buffer = device.createBuffer(buffer_info.get());
        device.bindBufferMemory(mapped_buffer, device_memory, 0);

        vk::BufferDeviceAddressInfoKHR address_info{
            .buffer = mapped_buffer
        };
        const uint64_t buffer_address = device.getBufferAddress(address_info);

        mapped_memories[address.address()] = { address.address(), ExternalBuffer{ device_memory, nullptr }, mapped_buffer, size, buffer_address };
        break;
    }

    case MappingMethod::DoubleBuffer: {
        vkutil::Buffer buffer(size + KiB(4));
        buffer.init_buffer(mapped_memory_flags, vkutil::vma_mapped_alloc);

        vk::BufferDeviceAddressInfoKHR address_info{
            .buffer = buffer.buffer
        };
        const uint64_t buffer_address = device.getBufferAddress(address_info);
        const vk::Buffer mapped_buffer = buffer.buffer;
        mapped_memories[address.address()] = { address.address(), std::move(buffer), mapped_buffer, size, buffer_address };
        break;
    }

    default:
        LOG_CRITICAL("Mapping method not handled, report it to the devs!");
        break;
    }

    return true;
}

void VKState::unmap_memory(MemState &mem, Ptr<void> address) {
    assert(features.enable_memory_mapping);

    auto ite = mapped_memories.find(address.address());
    if (ite == mapped_memories.end()) {
        LOG_CRITICAL("Could not find mapped memory to erase");
        return;
    }

    // we need to wait in case the buffer is being used
    device.waitIdle();

    switch (mapping_method) {
    case MappingMethod::ExernalHost:
        device.destroyBuffer(ite->second.buffer);
        device.freeMemory(std::get<ExternalBuffer>(ite->second.buffer_impl).memory);
        break;

    case MappingMethod::DoubleBuffer:
        remove_external_mapping(mem, address.cast<uint8_t>().get(mem), ite->second.size);
        // remove all the trapping related to these locations
        buffer_trapping.remove_range(address.address(), address.address() + ite->second.size);
        break;

#ifdef __ANDROID__
    case MappingMethod::NativeBuffer: {
        remove_external_mapping(mem, address.cast<uint8_t>().get(mem), ite->second.size);
        device.destroyBuffer(ite->second.buffer);
        ExternalBuffer &buffer = std::get<ExternalBuffer>(ite->second.buffer_impl);
        device.freeMemory(buffer.memory);

        AHardwareBuffer *hardware_buffer = reinterpret_cast<AHardwareBuffer *>(buffer.extra);
        _AHardwareBuffer_unlock(hardware_buffer, nullptr);
        // When using external fd, it takes ownership of the handle, so don't release it in this case
        if (support_android_buffer_import)
            _AHardwareBuffer_release(hardware_buffer);
        break;
    }
#endif

    case MappingMethod::PageTable:
        remove_external_mapping(mem, address.cast<uint8_t>().get(mem), ite->second.size);
        break;

    default:
        LOG_CRITICAL("Mapping method not handled, report it to the devs!");
        break;
    }
    mapped_memories.erase(ite);
}

std::tuple<vk::Buffer, uint32_t> VKState::get_matching_mapping(const Ptr<void> address) {
    auto mapped_memory = mapped_memories.lower_bound(address.address());
    if (mapped_memory == mapped_memories.end()
        || mapped_memory->first + mapped_memory->second.size < address.address()) {
        LOG_ERROR("Could not find matching mapped buffer for vertex stream");
        return { nullptr, 0 };
    }

    return std::make_tuple(mapped_memory->second.buffer, address.address() - mapped_memory->first);
}

uint64_t VKState::get_matching_device_address(const Address address) {
    auto mapped_memory = mapped_memories.lower_bound(address);
    if (mapped_memory == mapped_memories.end()
        || mapped_memory->first + mapped_memory->second.size < address) {
        LOG_ERROR("Could not find matching mapped buffer for vertex stream");
        return 0;
    }

    return mapped_memory->second.buffer_address + address - mapped_memory->first;
}

int VKState::get_max_anisotropic_filtering() {
    return static_cast<int>(physical_device_properties.limits.maxSamplerAnisotropy);
}

void VKState::set_anisotropic_filtering(int anisotropic_filtering) {
    texture_cache.anisotropic_filtering = anisotropic_filtering;
}

int VKState::get_max_2d_texture_width() {
    return static_cast<int>(physical_device_properties.limits.maxImageDimension2D);
}

void VKState::set_async_compilation(bool enable) {
    pipeline_cache.set_async_compilation(enable);
}

#ifdef __ANDROID__
std::vector<std::string> VKState::get_gpu_list() {
    if (!support_custom_drivers())
        return { physical_device_properties.deviceName.data() };

    // get the stock name
    std::vector<std::string> gpu_list = { "Default" };

    // First value is the stock driver
    fs::path driver_path = fs::path(SDL_GetAndroidInternalStoragePath()) / "driver";
    fs::create_directories(driver_path);

    for (const auto &entry : boost::make_iterator_range(fs::directory_iterator(driver_path), {})) {
        if (fs::is_directory(entry.path()))
            gpu_list.push_back(entry.path().filename().c_str());
    }

    return gpu_list;
}
#else
std::vector<std::string> VKState::get_gpu_list() {
    const std::vector<vk::PhysicalDevice> gpus = instance.enumeratePhysicalDevices();

    std::vector<std::string> gpu_list;
    // First value is always automatic
    gpu_list.emplace_back("Automatic");
    for (const vk::PhysicalDevice gpu : gpus)
        gpu_list.emplace_back(gpu.getProperties().deviceName.data());

    return gpu_list;
}
#endif

uint32_t VKState::get_gpu_version() {
    return physical_device_properties.driverVersion;
}

std::string_view VKState::get_gpu_name() {
    return physical_device_properties.deviceName.data();
}

void VKState::precompile_shader(const ShadersHash &hash) {
    Sha256Hash empty_hash{};
    if (hash.vert != empty_hash) {
        pipeline_cache.precompile_shader(hash.vert);
    }
    if (hash.frag != empty_hash) {
        pipeline_cache.precompile_shader(hash.frag);
    }

    programs_count_pre_compiled++;
    LOG_INFO("Program Compiled {}/{}", programs_count_pre_compiled, shaders_cache_hashs.size());
}

void VKState::preclose_action() {
    // make sure we are in a game
    if (shaders_path.empty())
        return;

    pipeline_cache.save_pipeline_cache();
}

#ifdef __ANDROID__
bool VKState::support_custom_drivers() {
    // vendor ID 0x5143 is Qualcomm, being stock or turnip
    return physical_device_properties.vendorID == 0x5143;
}

void VKState::set_turbo_mode(bool set) {
    if (!support_custom_drivers())
        return;

    adrenotools_set_turbo(set);
}
#endif

BufferTrapping::BufferTrapping(VKState &state)
    : state(state) {}

TrappedBuffer *BufferTrapping::access_buffer(Address addr, uint32_t size, MemState &mem, bool always_trap, bool cover_everything) {
    const bool is_buffer_small = (size < 3 * KiB(4));

    if (is_buffer_small && always_trap) {
        // overwise we may end up with trapping nothing
        cover_everything = true;
    } else if (is_buffer_small) {
        // not big enough to apply buffer trapping
        auto mem_it = state.mapped_memories.lower_bound(addr);
        if (mem_it == state.mapped_memories.end() || mem_it->first + mem_it->second.size < addr + size) {
            LOG_ERROR("Buffer at address {} is not completely mapped", log_hex(addr));
            return &temp_buffer;
        }

        temp_buffer.size = size;
        temp_buffer.mapped_location = reinterpret_cast<uint8_t *>(std::get<vkutil::Buffer>(mem_it->second.buffer_impl).mapped_data);
        temp_buffer.mapped_location += addr - mem_it->first;
        temp_buffer.extra = ~0;

        memcpy(temp_buffer.mapped_location, Ptr<void>(addr).get(mem), size);
        return &temp_buffer;
    }

    auto it = trapped_buffers.find(addr);
    bool is_new = false;
    if (it != trapped_buffers.end()) {
        // must check if everything match
        TrappedBuffer &buffer = it->second;
        if (!buffer.dirty && buffer.size >= size)
            // nothing to change
            return &it->second;
    } else {
        it = trapped_buffers.emplace(std::piecewise_construct, std::forward_as_tuple(addr), std::forward_as_tuple()).first;
        is_new = true;
    }

    {
        // remove the following overlapping dirty buffers
        auto next_it = it;
        next_it++;
        while (next_it != trapped_buffers.end() && next_it->first < addr + size) {
            if (next_it->second.dirty)
                next_it = trapped_buffers.erase(next_it);
            else
                next_it++;
        }
    }
    it->second.size = size;
    it->second.dirty = false;
    it->second.extra = ~0;

    if (is_new) {
        // we must find the matching mapped buffer
        auto mem_it = state.mapped_memories.lower_bound(addr);
        if (mem_it == state.mapped_memories.end() || mem_it->first + mem_it->second.size < addr + size) {
            LOG_ERROR("Buffer at address {} is not completely mapped", log_hex(addr));
            return &it->second;
        }

        it->second.mapped_location = reinterpret_cast<uint8_t *>(std::get<vkutil::Buffer>(mem_it->second.buffer_impl).mapped_data);
        it->second.mapped_location += addr - mem_it->first;
    }

    Address aligned_addr;
    uint32_t aligned_size;
    if (cover_everything) {
        aligned_addr = align_down(addr, KiB(4));
        aligned_size = align(addr + size, KiB(4)) - aligned_addr;
    } else {
        aligned_addr = align(addr, KiB(4));
        aligned_size = align_down(addr + size - aligned_addr, KiB(4));
    }
    add_protect(mem, aligned_addr, aligned_size, MemPerm::ReadOnly, [it](Address addr, bool write) {
        it->second.dirty = true;
        return true;
    });

    // copy back the data as it was non-existent or dirty
    memcpy(it->second.mapped_location, Ptr<void>(addr).get(mem), size);

    return &it->second;
}

void BufferTrapping::remove_range(Address start, Address end) {
    auto it = trapped_buffers.lower_bound(start);
    while (it != trapped_buffers.end() && it->first < end)
        it = trapped_buffers.erase(it);
}

} // namespace renderer::vulkan
