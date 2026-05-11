#include "android_state.h"

#include <miniz.h>
#include <util/android_driver.h>
#include <util/log.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <optional>
#include <string_view>
#include <vector>

namespace {

std::optional<fs::path> get_android_files_dir(JNIEnv *env) {
    env->PushLocalFrame(8);

    jclass activity_thread_class = env->FindClass("android/app/ActivityThread");
    if (!activity_thread_class) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not find ActivityThread class while resolving Android files dir");
        return std::nullopt;
    }

    jmethodID current_application = env->GetStaticMethodID(activity_thread_class, "currentApplication", "()Landroid/app/Application;");
    if (!current_application) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not find ActivityThread.currentApplication while resolving Android files dir");
        return std::nullopt;
    }

    jobject application = env->CallStaticObjectMethod(activity_thread_class, current_application);
    if (!application) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve Android application while resolving files dir");
        return std::nullopt;
    }

    jclass context_class = env->GetObjectClass(application);
    jmethodID get_files_dir = env->GetMethodID(context_class, "getFilesDir", "()Ljava/io/File;");
    if (!get_files_dir) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not find Context.getFilesDir while resolving Android files dir");
        return std::nullopt;
    }

    jobject files_dir = env->CallObjectMethod(application, get_files_dir);
    if (!files_dir) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve Android files dir");
        return std::nullopt;
    }

    jclass file_class = env->GetObjectClass(files_dir);
    jmethodID get_absolute_path = env->GetMethodID(file_class, "getAbsolutePath", "()Ljava/lang/String;");
    if (!get_absolute_path) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not find File.getAbsolutePath while resolving Android files dir");
        return std::nullopt;
    }

    auto *files_dir_str = reinterpret_cast<jstring>(env->CallObjectMethod(files_dir, get_absolute_path));
    if (!files_dir_str) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not convert Android files dir to string");
        return std::nullopt;
    }

    const fs::path resolved = fs::path(jstring_to_string(env, files_dir_str)) / "";
    env->PopLocalFrame(nullptr);
    return resolved;
}

std::optional<fs::path> get_custom_driver_root(JNIEnv *env) {
    const auto files_dir = get_android_files_dir(env);
    if (!files_dir)
        return std::nullopt;

    return *files_dir / "driver" / "";
}

std::vector<std::string> list_installed_custom_drivers(const fs::path &driver_root) {
    std::vector<std::string> drivers;
    if (!fs::exists(driver_root))
        return drivers;

    for (const auto &entry : fs::directory_iterator(driver_root)) {
        if (entry.is_directory())
            drivers.push_back(entry.path().filename().string());
    }

    std::sort(drivers.begin(), drivers.end());
    return drivers;
}

bool extract_custom_driver_archive(const fs::path &archive_path, const fs::path &driver_path, std::string &driver_so_name) {
    const std::string archive_path_utf8 = fs_utils::path_to_utf8(archive_path);
    std::unique_ptr<FILE, int (*)(FILE *)> archive_file(std::fopen(archive_path_utf8.c_str(), "rb"), std::fclose);
    if (!archive_file) {
        LOG_ERROR("Could not open custom driver archive: {}", archive_path_utf8);
        return false;
    }

    mz_zip_archive zip{};
    if (!mz_zip_reader_init_cfile(&zip, archive_file.get(), 0, 0)) {
        LOG_ERROR("miniz error reading custom driver archive: {}", mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
        return false;
    }

    const mz_uint file_count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint index = 0; index < file_count; ++index) {
        mz_zip_archive_file_stat file_stat{};
        if (!mz_zip_reader_file_stat(&zip, index, &file_stat))
            continue;

        const std::string_view entry_name(file_stat.m_filename);
        if (entry_name.empty() || entry_name.ends_with('/'))
            continue;

        const fs::path relative_path = fs::path(file_stat.m_filename).lexically_normal();
        if (!android_driver::is_safe_driver_relative_path(relative_path)) {
            LOG_ERROR("Rejecting unsafe custom driver archive entry: {}", file_stat.m_filename);
            mz_zip_reader_end(&zip);
            return false;
        }

        if (relative_path.extension() == ".so" && entry_name.find("vulkan") != std::string_view::npos) {
            if (!driver_so_name.empty())
                LOG_WARN("Two possible Vulkan driver files were found in {}: {} and {}", archive_path_utf8, driver_so_name, entry_name);

            driver_so_name = relative_path.generic_string();
        }

        const fs::path output_path = driver_path / relative_path;
        fs::create_directories(output_path.parent_path());

        if (!mz_zip_reader_extract_to_file(&zip, index, fs_utils::path_to_utf8(output_path).c_str(), 0)) {
            LOG_ERROR("Failed to extract custom driver archive entry: {}", file_stat.m_filename);
            mz_zip_reader_end(&zip);
            return false;
        }
    }

    mz_zip_reader_end(&zip);

    if (const auto meta_library_name = android_driver::read_driver_library_name_from_meta(driver_path / "meta.json")) {
        const fs::path meta_library_path = driver_path / *meta_library_name;
        if (!fs::exists(meta_library_path)) {
            LOG_ERROR("Custom driver archive metadata references missing library {}", *meta_library_name);
            return false;
        }

        driver_so_name = *meta_library_name;
    }

    return !driver_so_name.empty();
}

} // namespace

extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_org_vita3k_emulator_NativeLib_getInstalledCustomDrivers(JNIEnv *env, jclass) {
    const auto driver_root = get_custom_driver_root(env);
    const auto drivers = driver_root ? list_installed_custom_drivers(*driver_root) : std::vector<std::string>{};

    jclass string_class = env->FindClass("java/lang/String");
    jobjectArray result = env->NewObjectArray(static_cast<jsize>(drivers.size()), string_class, nullptr);
    for (jsize index = 0; index < static_cast<jsize>(drivers.size()); ++index) {
        jstring value = env->NewStringUTF(drivers[index].c_str());
        env->SetObjectArrayElement(result, index, value);
        env->DeleteLocalRef(value);
    }

    return result;
}

JNIEXPORT jstring JNICALL
Java_org_vita3k_emulator_NativeLib_installCustomDriver(JNIEnv *env, jclass, jstring path_str) {
    const auto driver_root = get_custom_driver_root(env);
    if (!driver_root)
        return env->NewStringUTF("");

    const fs::path archive_path = fs::path(jstring_to_string(env, path_str));
    const std::string driver_name = archive_path.filename().stem().string();
    if (driver_name.empty()) {
        LOG_ERROR("Could not derive custom driver name from archive path: {}", archive_path.string());
        return env->NewStringUTF("");
    }

    fs::create_directories(*driver_root);

    const fs::path driver_path = *driver_root / driver_name / "";
    if (fs::exists(driver_path)) {
        if (!fs::is_empty(driver_path)) {
            LOG_ERROR("Custom driver {} already exists", driver_name);
            return env->NewStringUTF("");
        }

        fs::remove_all(driver_path);
    }

    fs::create_directories(driver_path);

    std::string driver_so_name;
    if (!extract_custom_driver_archive(archive_path, driver_path, driver_so_name)) {
        fs::remove_all(driver_path);
        LOG_ERROR("Custom driver installation failed for archive: {}", archive_path.string());
        return env->NewStringUTF("");
    }

    std::ofstream driver_name_file(fs_utils::path_to_utf8(driver_path / "driver_name.txt"), std::ios_base::out | std::ios_base::trunc);
    if (!driver_name_file.is_open()) {
        fs::remove_all(driver_path);
        LOG_ERROR("Could not write driver_name.txt for custom driver {}", driver_name);
        return env->NewStringUTF("");
    }

    driver_name_file << driver_so_name << '\n';
    driver_name_file.close();

    LOG_INFO("Successfully installed custom driver {}", driver_name);
    return env->NewStringUTF(driver_name.c_str());
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_removeCustomDriver(JNIEnv *env, jclass, jstring driver_name_str) {
    const auto driver_root = get_custom_driver_root(env);
    if (!driver_root)
        return JNI_FALSE;

    const std::string driver_name = jstring_to_string(env, driver_name_str);
    if (driver_name.empty()) {
        LOG_ERROR("Refusing to remove a custom driver with an empty name");
        return JNI_FALSE;
    }

    const fs::path driver_path = *driver_root / driver_name / "";
    if (!fs::exists(driver_path)) {
        LOG_ERROR("Custom driver {} does not exist", driver_name);
        return JNI_FALSE;
    }

    fs::remove_all(driver_path);
    LOG_INFO("Removed custom driver {}", driver_name);
    return JNI_TRUE;
}

} // extern "C"
