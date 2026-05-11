#ifdef __ANDROID__

#include <util/android_driver.h>

#include <util/fs.h>
#include <util/log.h>

#include <SDL3/SDL_system.h>
#include <adrenotools/driver.h>
#include <android/api-level.h>
#include <dlfcn.h>
#include <jni.h>

#include <algorithm>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct DriverFingerprint {
    uint32_t vendor_id = 0;
    uint32_t driver_version = 0;
    std::string device_name;

    bool operator==(const DriverFingerprint &other) const {
        return vendor_id == other.vendor_id
            && driver_version == other.driver_version
            && device_name == other.device_name;
    }
};

std::string jni_string_to_utf8(JNIEnv *env, jstring str) {
    const char *utf = env->GetStringUTFChars(str, nullptr);
    if (!utf)
        return {};

    std::string result(utf);
    env->ReleaseStringUTFChars(str, utf);
    return result;
}

static jobject get_android_application(JNIEnv *env) {
    jclass activity_thread_class = env->FindClass("android/app/ActivityThread");
    if (!activity_thread_class)
        return nullptr;

    jmethodID current_application = env->GetStaticMethodID(activity_thread_class, "currentApplication", "()Landroid/app/Application;");
    if (!current_application)
        return nullptr;

    return env->CallStaticObjectMethod(activity_thread_class, current_application);
}

std::optional<fs::path> get_android_files_dir(JNIEnv *env) {
    if (!env)
        return std::nullopt;

    env->PushLocalFrame(8);

    jobject application = get_android_application(env);
    if (!application) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve Android application while resolving Vulkan driver files dir");
        return std::nullopt;
    }

    jclass context_class = env->GetObjectClass(application);
    jmethodID get_files_dir = env->GetMethodID(context_class, "getFilesDir", "()Ljava/io/File;");
    if (!get_files_dir) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not find Context.getFilesDir while resolving Vulkan driver files dir");
        return std::nullopt;
    }

    jobject files_dir = env->CallObjectMethod(application, get_files_dir);
    if (!files_dir) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve Android files dir for Vulkan driver probing");
        return std::nullopt;
    }

    jclass file_class = env->GetObjectClass(files_dir);
    jmethodID get_absolute_path = env->GetMethodID(file_class, "getAbsolutePath", "()Ljava/lang/String;");
    if (!get_absolute_path) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not find File.getAbsolutePath while resolving Vulkan driver files dir");
        return std::nullopt;
    }

    auto *files_dir_str = reinterpret_cast<jstring>(env->CallObjectMethod(files_dir, get_absolute_path));
    if (!files_dir_str) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve Android files dir string for Vulkan driver probing");
        return std::nullopt;
    }

    const fs::path resolved = fs::path(jni_string_to_utf8(env, files_dir_str)) / "";
    env->PopLocalFrame(nullptr);
    return resolved;
}

std::optional<fs::path> get_android_native_library_dir(JNIEnv *env) {
    if (!env)
        return std::nullopt;

    env->PushLocalFrame(8);

    jobject application = get_android_application(env);
    if (!application) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve Android application while resolving native library dir");
        return std::nullopt;
    }

    jclass context_class = env->GetObjectClass(application);
    jmethodID get_application_info = env->GetMethodID(context_class, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
    if (!get_application_info) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not find Context.getApplicationInfo while resolving native library dir");
        return std::nullopt;
    }

    jobject app_info = env->CallObjectMethod(application, get_application_info);
    if (!app_info) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve ApplicationInfo while resolving native library dir");
        return std::nullopt;
    }

    jclass app_info_class = env->GetObjectClass(app_info);
    jfieldID native_library_dir_field = env->GetFieldID(app_info_class, "nativeLibraryDir", "Ljava/lang/String;");
    if (!native_library_dir_field) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not find ApplicationInfo.nativeLibraryDir");
        return std::nullopt;
    }

    auto *native_library_dir = reinterpret_cast<jstring>(env->GetObjectField(app_info, native_library_dir_field));
    if (!native_library_dir) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve ApplicationInfo.nativeLibraryDir");
        return std::nullopt;
    }

    const fs::path resolved = fs::path(jni_string_to_utf8(env, native_library_dir)) / "";
    env->PopLocalFrame(nullptr);
    return resolved;
}

std::optional<std::string> parse_driver_library_name_from_meta_contents(const std::string &meta_contents) {
    constexpr std::string_view key = "\"libraryName\"";

    const size_t key_pos = meta_contents.find(key);
    if (key_pos == std::string::npos)
        return std::nullopt;

    const size_t colon_pos = meta_contents.find(':', key_pos + key.size());
    if (colon_pos == std::string::npos)
        return std::nullopt;

    const size_t opening_quote = meta_contents.find('"', colon_pos + 1);
    if (opening_quote == std::string::npos)
        return std::nullopt;

    const size_t closing_quote = meta_contents.find('"', opening_quote + 1);
    if (closing_quote == std::string::npos || closing_quote <= opening_quote + 1)
        return std::nullopt;

    return meta_contents.substr(opening_quote + 1, closing_quote - opening_quote - 1);
}

std::string ensure_trailing_separator(const fs::path &path) {
    std::string value = fs_utils::path_to_utf8(path);
    if (!value.empty() && value.back() != '/' && value.back() != '\\')
        value.push_back('/');
    return value;
}

PFN_vkGetInstanceProcAddr load_system_vk_get_instance_proc_addr() {
    static void *vulkan_loader_handle = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!vulkan_loader_handle) {
        LOG_ERROR("Failed to open Android Vulkan loader: {}", dlerror());
        return nullptr;
    }

    auto vk_get_instance_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(vulkan_loader_handle, "vkGetInstanceProcAddr"));
    if (!vk_get_instance_proc_addr)
        LOG_ERROR("Failed to resolve vkGetInstanceProcAddr from Android Vulkan loader");

    return vk_get_instance_proc_addr;
}

std::optional<fs::path> resolve_custom_driver_library_path(JNIEnv *env, const std::string &driver_name) {
    const auto files_dir = get_android_files_dir(env);
    if (!files_dir)
        return std::nullopt;

    const fs::path driver_path = *files_dir / "driver" / driver_name / "";
    if (!fs::exists(driver_path))
        return std::nullopt;

    const auto main_so_name = android_driver::resolve_custom_driver_library_name(driver_path);
    if (!main_so_name)
        return std::nullopt;

    return (driver_path / *main_so_name).lexically_normal();
}

bool is_library_mapped(const fs::path &library_path) {
    std::ifstream proc_maps("/proc/self/maps", std::ios_base::in);
    if (!proc_maps.is_open())
        return false;

    const std::string library_path_utf8 = fs_utils::path_to_utf8(library_path);
    for (std::string line; std::getline(proc_maps, line);) {
        if (line.find(library_path_utf8) != std::string::npos)
            return true;
    }

    return false;
}

PFN_vkCreateInstance resolve_vk_create_instance(PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr) {
    if (!vk_get_instance_proc_addr)
        return nullptr;

    return reinterpret_cast<PFN_vkCreateInstance>(vk_get_instance_proc_addr(VkInstance{}, "vkCreateInstance"));
}

PFN_vkEnumeratePhysicalDevices resolve_vk_enumerate_physical_devices(PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr, VkInstance instance) {
    if (!vk_get_instance_proc_addr || !instance)
        return nullptr;

    return reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(vk_get_instance_proc_addr(instance, "vkEnumeratePhysicalDevices"));
}

PFN_vkGetPhysicalDeviceProperties resolve_vk_get_physical_device_properties(PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr, VkInstance instance) {
    if (!vk_get_instance_proc_addr || !instance)
        return nullptr;

    return reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(vk_get_instance_proc_addr(instance, "vkGetPhysicalDeviceProperties"));
}

PFN_vkDestroyInstance resolve_vk_destroy_instance(PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr, VkInstance instance) {
    if (!vk_get_instance_proc_addr || !instance)
        return nullptr;

    return reinterpret_cast<PFN_vkDestroyInstance>(vk_get_instance_proc_addr(instance, "vkDestroyInstance"));
}

std::optional<DriverFingerprint> probe_driver_fingerprint(PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr) {
    const PFN_vkCreateInstance vk_create_instance = resolve_vk_create_instance(vk_get_instance_proc_addr);
    if (!vk_create_instance)
        return std::nullopt;

    const VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vita3K",
        .applicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 1),
        .pEngineName = "Vita3K",
        .engineVersion = VK_MAKE_API_VERSION(0, 0, 0, 1),
        .apiVersion = VK_API_VERSION_1_0,
    };
    const VkInstanceCreateInfo instance_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
    };

    VkInstance instance = VK_NULL_HANDLE;
    if (vk_create_instance(&instance_info, nullptr, &instance) != VK_SUCCESS || !instance)
        return std::nullopt;

    const PFN_vkDestroyInstance vk_destroy_instance = resolve_vk_destroy_instance(vk_get_instance_proc_addr, instance);
    const PFN_vkEnumeratePhysicalDevices vk_enumerate_physical_devices = resolve_vk_enumerate_physical_devices(vk_get_instance_proc_addr, instance);
    const PFN_vkGetPhysicalDeviceProperties vk_get_physical_device_properties = resolve_vk_get_physical_device_properties(vk_get_instance_proc_addr, instance);

    std::optional<DriverFingerprint> fingerprint;
    if (vk_enumerate_physical_devices && vk_get_physical_device_properties) {
        uint32_t physical_device_count = 0;
        if (vk_enumerate_physical_devices(instance, &physical_device_count, nullptr) == VK_SUCCESS && physical_device_count > 0) {
            std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
            if (vk_enumerate_physical_devices(instance, &physical_device_count, physical_devices.data()) == VK_SUCCESS && !physical_devices.empty()) {
                VkPhysicalDeviceProperties properties{};
                vk_get_physical_device_properties(physical_devices.front(), &properties);
                fingerprint = DriverFingerprint{
                    .vendor_id = properties.vendorID,
                    .driver_version = properties.driverVersion,
                    .device_name = properties.deviceName,
                };
            }
        }
    }

    if (vk_destroy_instance)
        vk_destroy_instance(instance, nullptr);

    return fingerprint;
}

std::optional<DriverFingerprint> get_default_driver_fingerprint() {
    static std::mutex fingerprint_mutex;
    static std::optional<DriverFingerprint> cached_fingerprint;
    static bool is_initialized = false;

    std::lock_guard<std::mutex> lock(fingerprint_mutex);
    if (is_initialized)
        return cached_fingerprint;

    is_initialized = true;
    cached_fingerprint = probe_driver_fingerprint(load_system_vk_get_instance_proc_addr());
    if (!cached_fingerprint)
        LOG_WARN("Default Vulkan driver fingerprint probe failed");

    return cached_fingerprint;
}

} // namespace

namespace android_driver {

bool is_safe_driver_relative_path(const fs::path &relative_path) {
    if (relative_path.empty() || relative_path == "." || relative_path.is_absolute())
        return false;

    return std::none_of(relative_path.begin(), relative_path.end(), [](const fs::path &part) {
        return part == "..";
    });
}

std::optional<std::string> read_driver_library_name_from_meta(const fs::path &meta_path) {
    if (!fs::exists(meta_path))
        return std::nullopt;

    std::ifstream meta_file(fs_utils::path_to_utf8(meta_path), std::ios_base::in);
    if (!meta_file.is_open())
        return std::nullopt;

    std::ostringstream meta_stream;
    meta_stream << meta_file.rdbuf();

    auto library_name = parse_driver_library_name_from_meta_contents(meta_stream.str());
    if (!library_name)
        return std::nullopt;

    const fs::path normalized_library_path = fs::path(*library_name).lexically_normal();
    if (!is_safe_driver_relative_path(normalized_library_path)) {
        LOG_WARN("Ignoring custom driver metadata with unsafe library path {}", *library_name);
        return std::nullopt;
    }

    return normalized_library_path.generic_string();
}

std::optional<std::string> resolve_custom_driver_library_name(const fs::path &driver_path) {
    if (const auto meta_library_name = read_driver_library_name_from_meta(driver_path / "meta.json")) {
        const fs::path meta_library_path = driver_path / *meta_library_name;
        if (fs::exists(meta_library_path))
            return *meta_library_name;

        LOG_WARN("Custom driver metadata references missing library {}", *meta_library_name);
    }

    const fs::path driver_name_file = driver_path / "driver_name.txt";
    if (!fs::exists(driver_name_file))
        return std::nullopt;

    std::ifstream name_file(fs_utils::path_to_utf8(driver_name_file), std::ios_base::in);
    if (!name_file.is_open())
        return std::nullopt;

    std::string main_so_name;
    name_file >> main_so_name;
    if (main_so_name.empty())
        return std::nullopt;

    const fs::path normalized_library_path = fs::path(main_so_name).lexically_normal();
    if (!is_safe_driver_relative_path(normalized_library_path)) {
        LOG_WARN("Ignoring custom driver record with unsafe library path {}", main_so_name);
        return std::nullopt;
    }

    return normalized_library_path.generic_string();
}

bool is_custom_driver_loaded(const std::string &driver_name, uint32_t vendor_id, uint32_t driver_version, std::string_view device_name) {
    if (driver_name.empty())
        return false;

    if (const auto default_fingerprint = get_default_driver_fingerprint()) {
        const DriverFingerprint current_fingerprint{
            .vendor_id = vendor_id,
            .driver_version = driver_version,
            .device_name = std::string(device_name),
        };
        if (!(current_fingerprint == *default_fingerprint))
            return true;
    }

    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    const auto library_path = resolve_custom_driver_library_path(env, driver_name);
    return library_path && is_library_mapped(*library_path);
}

} // namespace android_driver

void *open_custom_vulkan_driver(const std::string &driver_name) {
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    const auto files_dir = get_android_files_dir(env);
    if (!files_dir)
        return nullptr;

    const fs::path driver_path = *files_dir / "driver" / driver_name / "";
    if (!fs::exists(driver_path)) {
        LOG_ERROR("Could not find driver {}", driver_name);
        return nullptr;
    }

    const fs::path driver_name_file = driver_path / "driver_name.txt";
    if (!fs::exists(driver_name_file) && !fs::exists(driver_path / "meta.json")) {
        LOG_ERROR("Could not find driver driver_name.txt");
        return nullptr;
    }

    const auto main_so_name = android_driver::resolve_custom_driver_library_name(driver_path);
    if (!main_so_name) {
        LOG_ERROR("Could not resolve the main shared library for custom driver {}", driver_name);
        return nullptr;
    }

    const char *temp_dir = nullptr;
    fs::path temp_dir_path;
    if (android_get_device_api_level() < 29) {
        temp_dir_path = driver_path / "tmp/";
        fs::create_directory(temp_dir_path);
        temp_dir = temp_dir_path.c_str();
    }

    const auto lib_dir = get_android_native_library_dir(env);
    if (!lib_dir)
        return nullptr;

    const std::string custom_driver_dir = ensure_trailing_separator(driver_path);
    const std::string file_redirect_dir = ensure_trailing_separator(driver_path / "file_redirect");

    fs::create_directory(file_redirect_dir);

    void *vulkan_handle = adrenotools_open_libvulkan(
        RTLD_NOW,
        ADRENOTOOLS_DRIVER_FILE_REDIRECT | ADRENOTOOLS_DRIVER_CUSTOM,
        temp_dir,
        lib_dir->c_str(),
        custom_driver_dir.c_str(),
        main_so_name->c_str(),
        file_redirect_dir.c_str(),
        nullptr);

    if (!vulkan_handle) {
        LOG_ERROR("Could not open handle for custom driver {} using library {}", driver_name, *main_so_name);
        return nullptr;
    }

    return vulkan_handle;
}

namespace android_driver {

PFN_vkGetInstanceProcAddr resolve_vk_get_instance_proc_addr(const std::string &driver_name) {
    const PFN_vkGetInstanceProcAddr system_vk_get_instance_proc_addr = load_system_vk_get_instance_proc_addr();

    if (!driver_name.empty()) {
        void *vulkan_handle = open_custom_vulkan_driver(driver_name);
        if (!vulkan_handle) {
            LOG_WARN("Failed to load custom driver {}; falling back to the system Vulkan loader", driver_name);
        } else {
            auto vk_get_instance_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(vulkan_handle, "vkGetInstanceProcAddr"));
            if (!vk_get_instance_proc_addr) {
                LOG_WARN("Custom driver {} loaded without vkGetInstanceProcAddr; falling back to the system Vulkan loader", driver_name);
            } else {
                return vk_get_instance_proc_addr;
            }
        }
    }

    return system_vk_get_instance_proc_addr;
}

} // namespace android_driver

#endif
