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

#include "interface.h"
#include "archive.h"

#include "module/load_module.h"

#include <app/functions.h>
#include <app/session_controller.h>
#include <audio/state.h>
#include <bgm_player/functions.h>
#include <config/functions.h>
#include <config/state.h>
#include <ctime>
#include <ctrl/functions.h>
#include <ctrl/state.h>
#include <dialog/state.h>
#include <display/functions.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <gui/functions.h>
#include <io/functions.h>
#include <io/vfs.h>
#include <kernel/state.h>
#include <lang/state.h>
#include <motion/event_handler.h>
#include <packages/functions.h>
#include <packages/pkg.h>
#include <packages/sfo.h>
#include <packages/vci.h>
#include <renderer/state.h>
#include <renderer/texture_cache.h>
#include <touch/functions.h>
#include <touch/state.h>

#include <miniz.h>
#include <pugixml.hpp>

#include <modules/module_parent.h>
#include <string>
#include <util/log.h>
#include <util/string_utils.h>
#include <util/vector_utils.h>
#include <util/vita_theme_utils.h>

#include <gui/imgui_impl_sdl.h>
#include <gui/state.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_sensor.h>
#include <SDL3/SDL_video.h>

#include <gdbstub/functions.h>
#include <gxm/state.h>
#include <ime/functions.h>
#include <stb_image_write.h>

#if USE_DISCORD
#include <app/discord.h>
#endif

#include "patch/patch.h"

#include <cstring>
#include <memory>
#include <regex>
#include <set>

typedef std::shared_ptr<mz_zip_archive> ZipPtr;

inline void delete_zip(mz_zip_archive *zip) {
    mz_zip_reader_end(zip);
    delete zip;
}

static size_t write_to_buffer(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n) {
    vfs::FileBuffer *const buffer = static_cast<vfs::FileBuffer *>(pOpaque);
    assert(file_ofs == buffer->size());
    const uint8_t *const first = static_cast<const uint8_t *>(pBuf);
    const uint8_t *const last = &first[n];
    buffer->insert(buffer->end(), first, last);

    return n;
}

static const char *miniz_get_error(const ZipPtr &zip) {
    return mz_zip_get_error_string(mz_zip_get_last_error(zip.get()));
}

static std::string fallback_theme_root_name(const std::string &content_path) {
    std::string trimmed = content_path;
    while (!trimmed.empty() && ((trimmed.back() == '/') || (trimmed.back() == '\\')))
        trimmed.pop_back();

    if (trimmed.empty())
        return {};

    const auto separator = trimmed.find_last_of("/\\");
    return (separator == std::string::npos) ? trimmed : trimmed.substr(separator + 1);
}

static bool is_nonpdrm(EmuEnvState &emuenv, const fs::path &output_path) {
    const auto app_license_path{ emuenv.vita_fs_path / "ux0/license" / emuenv.app_info.app_title_id / fmt::format("{}.rif", emuenv.app_info.app_content_id) };
    const auto is_patch_found_app_license = (emuenv.app_info.app_category == "gp") && fs::exists(app_license_path);
    if (fs::exists(output_path / "sce_sys/package/work.bin") || is_patch_found_app_license) {
        fs::path licpath = is_patch_found_app_license ? app_license_path : output_path / "sce_sys/package/work.bin";
        LOG_INFO("Decrypt layer: {}", output_path);
        if (!decrypt_install_nonpdrm(emuenv, licpath, output_path)) {
            LOG_ERROR("NoNpDrm installation failed, deleting data!");
            fs::remove_all(output_path);
            return false;
        }
        return true;
    }

    return false;
}

static bool set_content_path(EmuEnvState &emuenv, const bool is_theme, fs::path &dest_path) {
    if (emuenv.app_info.app_title_id.empty()) {
        LOG_ERROR("param.sfo has no title ID, not installing it");
        return false;
    }

    const auto app_path = dest_path / "app" / emuenv.app_info.app_title_id;

    if (emuenv.app_info.app_category == "ac") {
        if (is_theme) {
            dest_path /= fs::path("theme") / emuenv.app_info.app_content_id;
            emuenv.app_info.app_title += " (Theme)";
        } else {
            emuenv.app_info.app_content_id = emuenv.app_info.app_content_id.substr(20);
            dest_path /= fs::path("addcont") / emuenv.app_info.app_title_id / emuenv.app_info.app_content_id;
            emuenv.app_info.app_title += " (DLC)";
        }
    } else if (emuenv.app_info.app_category.contains("gp")) {
        if (!fs::exists(app_path) || fs::is_empty(app_path)) {
            LOG_ERROR("Install app before patch");
            return false;
        }
        dest_path /= fs::path("patch") / emuenv.app_info.app_title_id;
        emuenv.app_info.app_title += " (Patch)";
    } else {
        dest_path = app_path;
        emuenv.app_info.app_title += " (App)";
    }

    return true;
}

static void set_theme_name(EmuEnvState &emuenv, const vfs::FileBuffer &buffer, const std::string &fallback_id_hint = {}) {
    std::string content_id;
    std::string title;

    pugi::xml_document doc;
    if (doc.load_buffer(buffer.data(), buffer.size())) {
        const auto info = doc.child("theme").child("InfomationProperty");
        content_id = info.child("m_contentId").text().as_string();
        title = info.child("m_title").child("m_default").text().as_string();
    } else {
        LOG_WARN("Unable to parse theme.xml metadata during install, falling back to folder/title-derived theme identity");
    }

    const std::string resolved_id = vita_theme_utils::resolve_theme_id(
        content_id,
        fallback_id_hint,
        title,
        buffer.empty() ? nullptr : buffer.data(),
        buffer.size());
    emuenv.app_info.app_content_id = resolved_id;
    emuenv.app_info.app_title_id = resolved_id;

    if (!title.empty()) {
        emuenv.app_info.app_title = title;
    } else if (emuenv.app_info.app_title.empty()) {
        emuenv.app_info.app_title = resolved_id;
    }
}

static bool install_archive_content(EmuEnvState &emuenv, const ZipPtr &zip, const std::string &content_path, const std::function<void(ArchiveContents)> &progress_callback, const ReinstallCallback &reinstall_callback) {
    std::string sfo_path = "sce_sys/param.sfo";
    std::string theme_path = "theme.xml";
    vfs::FileBuffer buffer, theme;

    const auto is_theme = mz_zip_reader_extract_file_to_callback(zip.get(), (content_path + theme_path).c_str(), &write_to_buffer, &theme, 0);
    const std::string theme_root_name = fallback_theme_root_name(content_path);

    auto output_path{ emuenv.vita_fs_path / "ux0" };
    if (mz_zip_reader_extract_file_to_callback(zip.get(), (content_path + sfo_path).c_str(), &write_to_buffer, &buffer, 0)) {
        sfo::get_param_info(emuenv.app_info, buffer, emuenv.cfg.sys_lang);
        if (!set_content_path(emuenv, is_theme, output_path))
            return false;
    } else if (is_theme) {
        set_theme_name(emuenv, theme, theme_root_name);
        output_path /= fs::path("theme") / emuenv.app_info.app_content_id;
    } else {
        LOG_CRITICAL("miniz error: {} extracting file: {}", miniz_get_error(zip), sfo_path);
        return false;
    }

    const auto created = fs::create_directories(output_path);
    if (!created) {
        if (reinstall_callback) {
            if (!reinstall_callback(emuenv.app_info.app_title, emuenv.app_info.app_title_id)) {
                LOG_INFO("{} already installed, skipping", emuenv.app_info.app_title_id);
                return true;
            }
        }
        fs::remove_all(output_path);
    }

    float file_progress = 0;
    float decrypt_progress = 0;

    const auto update_progress = [&]() {
        if (progress_callback)
            progress_callback({ {}, {}, { file_progress * 0.7f + decrypt_progress * 0.3f } });
    };

    mz_uint num_files = mz_zip_reader_get_num_files(zip.get());
    for (mz_uint i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat)) {
            continue;
        }
        const std::string m_filename = file_stat.m_filename;
        if (m_filename.contains(content_path)) {
            file_progress = static_cast<float>(i) / num_files * 100.0f;
            update_progress();

            std::string replace_filename = m_filename.substr(content_path.size());
            const fs::path file_output = (output_path / fs_utils::utf8_to_path(replace_filename)).generic_path();
            if (mz_zip_reader_is_file_a_directory(zip.get(), i)) {
                fs::create_directories(file_output);
            } else {
                fs::create_directories(file_output.parent_path());
                LOG_INFO("Extracting {}", file_output);
                mz_zip_reader_extract_to_file(zip.get(), i, fs_utils::path_to_utf8(file_output).c_str(), 0);
            }
        }
    }

    if (fs::exists(output_path / "sce_sys/package/") && emuenv.app_info.app_title_id.starts_with("PCS")) {
        update_progress();
        if (is_nonpdrm(emuenv, output_path))
            decrypt_progress = 100.f;
        else
            return false;
    }
    if (!copy_path(output_path, emuenv.vita_fs_path, emuenv.app_info.app_title_id, emuenv.app_info.app_category))
        return false;

    update_progress();

    LOG_INFO("{} [{}] installed successfully!", emuenv.app_info.app_title, emuenv.app_info.app_title_id);

    return true;
}

static std::vector<std::string> get_archive_contents_path(const ZipPtr &zip) {
    mz_uint num_files = mz_zip_reader_get_num_files(zip.get());
    std::vector<std::string> content_path;
    std::string sfo_path = "sce_sys/param.sfo";
    std::string theme_path = "theme.xml";

    for (mz_uint i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat))
            continue;

        std::string m_filename = std::string(file_stat.m_filename);
        if (m_filename.contains("sce_module/steroid.suprx")) {
            LOG_CRITICAL("A Vitamin dump was detected, aborting installation...");
#ifdef __ANDROID__
            // SDL_ShowAndroidToast("Vitamin dumps are not supported!", 1, -1, 0, 0);
#endif
            content_path.clear();
            break;
        }

        const auto is_content = m_filename.contains(sfo_path) || m_filename.contains(theme_path);
        if (is_content) {
            const auto content_type = m_filename.contains(sfo_path) ? sfo_path : theme_path;
            m_filename.erase(m_filename.find(content_type));
            vector_utils::push_if_not_exists(content_path, m_filename);
        }
    }

    return content_path;
}

std::vector<ContentInfo> install_archive(EmuEnvState &emuenv, const fs::path &archive_path, const std::function<void(ArchiveContents)> &progress_callback, const ReinstallCallback &reinstall_callback) {
    if (string_utils::tolower(archive_path.extension().string()) == ".vci") {
        const auto vci_progress = [&](float pct) {
            if (progress_callback)
                progress_callback({ 1.f, 1.f, pct });
        };
        const bool state = install_vci(archive_path, emuenv, vci_progress);
        std::vector<ContentInfo> content_installed{};
        content_installed.push_back({ emuenv.app_info.app_title, emuenv.app_info.app_title_id, emuenv.app_info.app_category, emuenv.app_info.app_content_id, archive_path.string(), state });
        return content_installed;
    }

    FILE *vpk_fp = FOPEN(archive_path.c_str(), "rb");
    if (!vpk_fp) {
        LOG_CRITICAL("Failed to load archive file in path: {}", fs_utils::path_to_utf8(archive_path));
        return {};
    }

    const ZipPtr zip(new mz_zip_archive, delete_zip);
    std::memset(zip.get(), 0, sizeof(*zip));

    if (!mz_zip_reader_init_cfile(zip.get(), vpk_fp, 0, 0)) {
        LOG_CRITICAL("miniz error reading archive: {}", miniz_get_error(zip));
        fclose(vpk_fp);
        return {};
    }

    const auto content_path = get_archive_contents_path(zip);
    if (content_path.empty()) {
        fclose(vpk_fp);
        return {};
    }

    const auto count = static_cast<float>(content_path.size());
    float current = 0.f;
    const auto update_progress = [&]() {
        if (progress_callback)
            progress_callback({ count, current, {} });
    };
    update_progress();

    std::vector<ContentInfo> content_installed{};
    for (auto &path : content_path) {
        current++;
        update_progress();
        bool state = install_archive_content(emuenv, zip, path, progress_callback, reinstall_callback);
        // Can't use emplace_back due to Clang 15 for macos
        content_installed.push_back({ emuenv.app_info.app_title, emuenv.app_info.app_title_id, emuenv.app_info.app_category, emuenv.app_info.app_content_id, path, state });
    }

    fclose(vpk_fp);
    return content_installed;
}

static std::vector<fs::path> get_contents_path(const fs::path &path) {
    std::vector<fs::path> contents_path;

    for (const auto &p : fs::recursive_directory_iterator(path)) {
        auto filename = p.path().filename();
        const auto is_content = (filename == "param.sfo") || (filename == "theme.xml");
        if (is_content) {
            auto parent_path = p.path().parent_path();
            const auto content_path = (filename == "param.sfo") ? parent_path.parent_path() : parent_path;
            vector_utils::push_if_not_exists(contents_path, content_path);
        }
    }

    return contents_path;
}

static bool install_content(EmuEnvState &emuenv, const fs::path &content_path) {
    const auto sfo_path{ content_path / "sce_sys/param.sfo" };
    const auto theme_path{ content_path / "theme.xml" };
    vfs::FileBuffer buffer;

    const auto is_theme = fs::exists(theme_path);
    auto dst_path{ emuenv.vita_fs_path / "ux0" };
    if (fs_utils::read_data(sfo_path, buffer)) {
        sfo::get_param_info(emuenv.app_info, buffer, emuenv.cfg.sys_lang);
        if (!set_content_path(emuenv, is_theme, dst_path))
            return false;

        if (exists(dst_path))
            fs::remove_all(dst_path);

    } else if (fs_utils::read_data(theme_path, buffer)) {
        set_theme_name(emuenv, buffer, fs_utils::path_to_utf8(content_path.filename()));
        dst_path /= fs::path("theme") / emuenv.app_info.app_title_id;
    } else {
        LOG_ERROR("Param.sfo file is missing in path", sfo_path);
        return false;
    }

    if (!fs_utils::copy_directory_contents(content_path, dst_path)) {
        LOG_ERROR("Failed to copy directory to: {}", dst_path);
        return false;
    }

    if (fs::exists(dst_path / "sce_sys/package/") && !is_nonpdrm(emuenv, dst_path))
        return false;

    if (!copy_path(dst_path, emuenv.vita_fs_path, emuenv.app_info.app_title_id, emuenv.app_info.app_category))
        return false;

    LOG_INFO("{} [{}] installed successfully!", emuenv.app_info.app_title, emuenv.app_info.app_title_id);

    return true;
}

uint32_t install_contents(EmuEnvState &emuenv, const fs::path &path) {
    const auto src_path = get_contents_path(path);

    LOG_WARN_IF(src_path.empty(), "No found any content compatible on this path: {}", path);

    uint32_t installed = 0;
    for (const auto &src : src_path) {
        if (install_content(emuenv, src))
            ++installed;
    }

    if (installed) {
        LOG_INFO("Successfully installed {} content!", installed);
    }

    return installed;
}

static void do_patches(MemState &mem, const Patches &patches, const SceKernelModuleInfo &sceKernelModuleInfo) {
    for (const auto &patch : patches) {
        if (patch.seg < MODULE_INFO_NUM_SEGMENTS) {
            auto &seg = sceKernelModuleInfo.segments[patch.seg];
            auto seg_ptr = seg.vaddr.cast<uint8_t>();
            if (seg_ptr) {
                LOG_INFO("Patching segment {} at offset 0x{:X} with {} values", patch.seg, patch.offset, patch.values.size());
                if (patch.offset + patch.values.size() <= seg.memsz) {
                    memcpy(seg_ptr.get(mem) + patch.offset, patch.values.data(), patch.values.size());
                } else {
                    LOG_ERROR("Patch out of bounds for segment {} at offset 0x{:X}", patch.seg, patch.offset);
                }
            }
        }
    }
}

static ExitCode load_app_impl(SceUID &main_module_id, EmuEnvState &emuenv, const AppLaunchRequest &launch_request) {
    const auto call_import = [&emuenv](CPUState &cpu, uint32_t nid, SceUID thread_id) {
        ::call_import(emuenv, cpu, nid, thread_id);
    };
    emuenv.kernel.process_exit_callback = [&emuenv](int res, std::optional<AppLaunchRequest> relaunch) {
        emuenv.post_app_launch_request(relaunch.value_or(AppLaunchRequest{ .reason = AppLaunchReason::ProcessExit, .exit_code = res }));
    };
    if (!emuenv.kernel.init(emuenv.mem, call_import, emuenv.cfg.current_config.cpu_opt)) {
        LOG_WARN("Failed to init kernel!");
        return KernelInitFailed;
    }

    if (emuenv.cfg.archive_log) {
        const fs::path log_directory{ emuenv.log_path / "logs" };
        fs::create_directory(log_directory);
        const auto log_path{ log_directory / fs_utils::utf8_to_path(emuenv.io.title_id + " - [" + string_utils::remove_special_chars(emuenv.current_app_title) + "].log") };
        if (logging::add_sink(log_path) != Success)
            return InitConfigFailed;
        logging::set_level(static_cast<spdlog::level::level_enum>(emuenv.cfg.log_level));
    }

    LOG_INFO("CPU Optimisation state: {}", emuenv.cfg.current_config.cpu_opt);
    LOG_INFO("ngs state: {}", emuenv.cfg.current_config.ngs_enable);
    LOG_INFO("Resolution multiplier: {}", emuenv.cfg.resolution_multiplier);

    if (emuenv.ctrl.controllers_num) {
        LOG_INFO("{} Controllers Connected", emuenv.ctrl.controllers_num);
        for (auto controller_it = emuenv.ctrl.controllers.begin(); controller_it != emuenv.ctrl.controllers.end(); ++controller_it) {
            LOG_INFO("Controller {}: {}", controller_it->second.port, controller_it->second.name);
        }
        if (emuenv.ctrl.has_motion_support)
            LOG_INFO("Controller has motion support");
    }
    constexpr std::array modules_mode_names{ "Automatic", "Auto & Manual", "Manual" };
    LOG_INFO("modules mode: {}", modules_mode_names.at(emuenv.cfg.current_config.modules_mode));
    if ((emuenv.cfg.current_config.modules_mode != ModulesMode::AUTOMATIC) && !emuenv.cfg.current_config.lle_modules.empty()) {
        std::string modules;
        for (const auto &mod : emuenv.cfg.current_config.lle_modules) {
            modules += mod + ",";
        }
        modules.pop_back();
        LOG_INFO("lle-modules: {}", modules);
    }

    LOG_INFO("Title: {}", emuenv.current_app_title);
    LOG_INFO("Serial: {}", emuenv.io.title_id);
    LOG_INFO("Version: {}", emuenv.app_info.app_version);
    LOG_INFO("Category: {}", emuenv.app_info.app_category);

    init_device_paths(emuenv.io);
    init_savedata_app_path(emuenv.io, emuenv.vita_fs_path);

    // Load param.sfo
    vfs::FileBuffer param_sfo;
    if (vfs::read_app_file(param_sfo, emuenv.vita_fs_path, emuenv.io.app_path, "sce_sys/param.sfo"))
        sfo::load(emuenv.sfo_handle, param_sfo);

    init_exported_vars(emuenv);

    // Load main executable
    if (!launch_request.self_path.empty()) {
        emuenv.self_path = launch_request.self_path;
    } else {
        emuenv.self_path = !emuenv.cfg.self_path.empty() ? emuenv.cfg.self_path : EBOOT_PATH;
    }

    main_module_id = load_module(emuenv, "app0:" + emuenv.self_path);

    if (main_module_id >= 0) {
        const auto module = emuenv.kernel.loaded_modules[main_module_id];
        LOG_INFO("Main executable {} ({}) loaded", module->info.module_name, emuenv.self_path);
        const Patches patches = get_patches(emuenv.patch_path, emuenv.io.title_id, "app0:" + emuenv.self_path);
        if (!patches.empty())
            do_patches(emuenv.mem, patches, module->info);
    } else
        return FileNotFound;
    // Set self name from self path, can contain folder, get file name only
    emuenv.self_name = fs_utils::path_to_utf8(fs::path(emuenv.self_path).filename());

    // get list of preload modules
    SceUInt32 process_preload_disabled = 0;
    auto process_param = emuenv.kernel.process_param.get(emuenv.mem);
    if (process_param) {
        auto preload_disabled_ptr = Ptr<SceUInt32>(process_param->process_preload_disabled);
        if (preload_disabled_ptr) {
            process_preload_disabled = *preload_disabled_ptr.get(emuenv.mem);
        }
    }
    const auto module_app_path{ emuenv.vita_fs_path / "ux0/app" / emuenv.io.app_path / "sce_module" };

    std::vector<std::string> lib_load_list = {};
    // todo: check if module is imported
    auto add_preload_module = [&](uint32_t code, SceSysmoduleModuleId module_id, const std::string &name, bool load_from_app) {
        if ((process_preload_disabled & code) == 0) {
            if (is_lle_module(name, emuenv)) {
                const auto module_name_file = fmt::format("{}.suprx", name);
                if (load_from_app && fs::exists(module_app_path / module_name_file))
                    lib_load_list.emplace_back(fmt::format("app0:sce_module/{}", module_name_file));
                else if (fs::exists(emuenv.vita_fs_path / "vs0/sys/external" / module_name_file))
                    lib_load_list.emplace_back(fmt::format("vs0:sys/external/{}", module_name_file));
            }

            if (module_id != SCE_SYSMODULE_INVALID)
                emuenv.kernel.loaded_sysmodules[module_id] = {};
        }
    };
    lib_load_list.emplace_back("os0:kd/bootimage.skprx");
    lib_load_list.emplace_back("os0:kd/sysmodule.skprx");
    add_preload_module(0x00010000, SCE_SYSMODULE_INVALID, "libc", true);
    add_preload_module(0x00020000, SCE_SYSMODULE_DBG, "libdbg", false);
    add_preload_module(0x00080000, SCE_SYSMODULE_INVALID, "libshellsvc", false);
    add_preload_module(0x00100000, SCE_SYSMODULE_INVALID, "libcdlg", false);
    add_preload_module(0x00200000, SCE_SYSMODULE_FIOS2, "libfios2", true);
    add_preload_module(0x00400000, SCE_SYSMODULE_APPUTIL, "apputil", false);
    add_preload_module(0x00800000, SCE_SYSMODULE_INVALID, "libSceFt2", false);
    add_preload_module(0x01000000, SCE_SYSMODULE_INVALID, "libpvf", false);
    add_preload_module(0x02000000, SCE_SYSMODULE_PERF, "libperf", false); // if DEVELOPMENT_MODE dipsw is set

    for (const auto &module_path : lib_load_list) {
        auto res = load_module(emuenv, module_path);
        LOG_ERROR_IF(res < 0, "Failed to load preloaded module: {}. Ignoring this error.", module_path);
    }

    // Load taiHEN plugins configured for this title
    load_taihen_plugins_for_title(emuenv, emuenv.io.title_id);

    return Success;
}

void toggle_texture_replacement(EmuEnvState &emuenv) {
    emuenv.cfg.current_config.import_textures = !emuenv.cfg.current_config.import_textures;
    emuenv.renderer->get_texture_cache()->set_replacement_state(emuenv.cfg.current_config.import_textures, emuenv.cfg.current_config.export_textures, emuenv.cfg.current_config.export_as_png);
}

static std::vector<uint32_t> get_current_app_frame(EmuEnvState &emuenv, uint32_t &width, uint32_t &height) {
    // Dump the current frame from the emulator display
    std::vector<uint32_t> frame = emuenv.renderer->dump_frame(emuenv.display, width, height);
    if (frame.empty() || (frame.size() != (width * height))) {
        return {};
    }

    // Force alpha channel to 255 (fully opaque) for every pixel
    for (uint32_t &pixel : frame) {
        pixel |= 0xFF000000;
    }

    return frame;
}

static void update_live_area_last_app_frame(EmuEnvState &emuenv, GuiState &gui) {
    uint32_t width = 0;
    uint32_t height = 0;
    auto frame = get_current_app_frame(emuenv, width, height);
    if (frame.empty()) {
        LOG_ERROR("Failed to dump current app frame for live area");
        return;
    }

    gui.live_area_last_app_frame = ImGui_Texture(gui.imgui_state.get(), frame.data(), width, height);
}

void take_screenshot(EmuEnvState &emuenv) {
    if (emuenv.cfg.screenshot_format == None)
        return;

    if (emuenv.io.title_id.empty()) {
        LOG_ERROR("Trying to take a screenshot while not ingame");
        return;
    }

    uint32_t width, height;
    auto frame = get_current_app_frame(emuenv, width, height);
    if (frame.empty()) {
        LOG_ERROR("Failed to take screenshot");
        return;
    }

    const fs::path save_folder = emuenv.shared_path / "screenshots" / fmt::format("{}", string_utils::remove_special_chars(emuenv.current_app_title));
    fs::create_directories(save_folder);

    auto t = std::time(nullptr);
    struct tm localtime;
#ifdef _WIN32
    localtime_s(&localtime, &t);
#else
    localtime_r(&t, &localtime);
#endif

    const auto img_format = emuenv.cfg.screenshot_format == JPEG ? ".jpg" : ".png";
    const fs::path save_file = save_folder / fmt::format("{}_{:%Y-%m-%d-%H%M%OS}{}", string_utils::remove_special_chars(emuenv.current_app_title), localtime, img_format);
    constexpr int quality = 85; // google recommended value
    if (emuenv.cfg.screenshot_format == JPEG) {
        if (stbi_write_jpg(fs_utils::path_to_utf8(save_file).c_str(), width, height, 4, frame.data(), quality) == 1)
            LOG_INFO("Successfully saved screenshot to {}", save_file);
        else
            LOG_INFO("Failed to save screenshot");
    } else {
        if (stbi_write_png(fs_utils::path_to_utf8(save_file).c_str(), width, height, 4, frame.data(), width * 4) == 1)
            LOG_INFO("Successfully saved screenshot to {}", save_file);
        else
            LOG_INFO("Failed to save screenshot");
    }
}

static void switch_full_screen(EmuEnvState &emuenv) {
    emuenv.display.fullscreen = !emuenv.display.fullscreen;
    emuenv.renderer->set_fullscreen(emuenv.display.fullscreen);
    SDL_SetWindowFullscreen(emuenv.window.get(), emuenv.display.fullscreen.load());
    app::update_viewport(emuenv);
}

static input::PhysicalKeyCode physical_key_from_sdl_scancode(const SDL_Scancode scancode) {
    if (scancode <= SDL_SCANCODE_UNKNOWN || scancode >= SDL_SCANCODE_COUNT)
        return input::PhysicalKeyCode::Unbound;

    return static_cast<input::PhysicalKeyCode>(0x00070000u | static_cast<uint32_t>(scancode));
}

static std::set<input::PhysicalKeyCode> sdl_pressed_keys;

static void update_sdl_keyboard_state(EmuEnvState &emuenv, const input::PhysicalKeyCode key, const bool pressed) {
    if (key == input::PhysicalKeyCode::Unbound)
        return;

    if (pressed)
        sdl_pressed_keys.insert(key);
    else
        sdl_pressed_keys.erase(key);

    const auto &cfg = emuenv.cfg;
    const auto is_pressed = [](const input::PhysicalKeyCode primary, const input::PhysicalKeyCode alternate) {
        return (primary != input::PhysicalKeyCode::Unbound && sdl_pressed_keys.contains(primary))
            || (alternate != input::PhysicalKeyCode::Unbound && sdl_pressed_keys.contains(alternate));
    };
    const auto set_bit_if_pressed = [&is_pressed](const input::PhysicalKeyCode primary, const input::PhysicalKeyCode alternate, const uint32_t bit, uint32_t &mask) {
        if (is_pressed(primary, alternate))
            mask |= bit;
    };

    float axes[4] = {
        static_cast<float>(is_pressed(cfg.keyboard_leftstick_right, cfg.keyboard_leftstick_right_alt)) - static_cast<float>(is_pressed(cfg.keyboard_leftstick_left, cfg.keyboard_leftstick_left_alt)),
        static_cast<float>(is_pressed(cfg.keyboard_leftstick_down, cfg.keyboard_leftstick_down_alt)) - static_cast<float>(is_pressed(cfg.keyboard_leftstick_up, cfg.keyboard_leftstick_up_alt)),
        static_cast<float>(is_pressed(cfg.keyboard_rightstick_right, cfg.keyboard_rightstick_right_alt)) - static_cast<float>(is_pressed(cfg.keyboard_rightstick_left, cfg.keyboard_rightstick_left_alt)),
        static_cast<float>(is_pressed(cfg.keyboard_rightstick_down, cfg.keyboard_rightstick_down_alt)) - static_cast<float>(is_pressed(cfg.keyboard_rightstick_up, cfg.keyboard_rightstick_up_alt)),
    };
    uint32_t buttons = 0;
    uint32_t buttons_ext = 0;
    const auto set_common_buttons = [&cfg, &set_bit_if_pressed](uint32_t &mask) {
        set_bit_if_pressed(cfg.keyboard_button_select, cfg.keyboard_button_select_alt, SCE_CTRL_SELECT, mask);
        set_bit_if_pressed(cfg.keyboard_button_start, cfg.keyboard_button_start_alt, SCE_CTRL_START, mask);
        set_bit_if_pressed(cfg.keyboard_button_up, cfg.keyboard_button_up_alt, SCE_CTRL_UP, mask);
        set_bit_if_pressed(cfg.keyboard_button_right, cfg.keyboard_button_right_alt, SCE_CTRL_RIGHT, mask);
        set_bit_if_pressed(cfg.keyboard_button_down, cfg.keyboard_button_down_alt, SCE_CTRL_DOWN, mask);
        set_bit_if_pressed(cfg.keyboard_button_left, cfg.keyboard_button_left_alt, SCE_CTRL_LEFT, mask);
        set_bit_if_pressed(cfg.keyboard_button_triangle, cfg.keyboard_button_triangle_alt, SCE_CTRL_TRIANGLE, mask);
        set_bit_if_pressed(cfg.keyboard_button_circle, cfg.keyboard_button_circle_alt, SCE_CTRL_CIRCLE, mask);
        set_bit_if_pressed(cfg.keyboard_button_cross, cfg.keyboard_button_cross_alt, SCE_CTRL_CROSS, mask);
        set_bit_if_pressed(cfg.keyboard_button_square, cfg.keyboard_button_square_alt, SCE_CTRL_SQUARE, mask);
        set_bit_if_pressed(cfg.keyboard_button_psbutton, cfg.keyboard_button_psbutton_alt, SCE_CTRL_PSBUTTON, mask);
    };
    set_common_buttons(buttons);
    set_common_buttons(buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_l1, cfg.keyboard_button_l1_alt, SCE_CTRL_L, buttons);
    set_bit_if_pressed(cfg.keyboard_button_r1, cfg.keyboard_button_r1_alt, SCE_CTRL_R, buttons);
    set_bit_if_pressed(cfg.keyboard_button_l1, cfg.keyboard_button_l1_alt, SCE_CTRL_L1, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_r1, cfg.keyboard_button_r1_alt, SCE_CTRL_R1, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_l2, cfg.keyboard_button_l2_alt, SCE_CTRL_L2, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_r2, cfg.keyboard_button_r2_alt, SCE_CTRL_R2, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_l3, cfg.keyboard_button_l3_alt, SCE_CTRL_L3, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_r3, cfg.keyboard_button_r3_alt, SCE_CTRL_R3, buttons_ext);

    std::lock_guard<std::mutex> lock(emuenv.ctrl.mutex);
    emuenv.ctrl.keyboard_state.buttons = buttons;
    emuenv.ctrl.keyboard_state.buttons_ext = buttons_ext;
    for (size_t i = 0; i < 4; ++i)
        emuenv.ctrl.keyboard_state.axes[i] = axes[i];
}

static void clear_sdl_keyboard_state(EmuEnvState &emuenv) {
    sdl_pressed_keys.clear();
    std::lock_guard<std::mutex> lock(emuenv.ctrl.mutex);
    emuenv.ctrl.keyboard_state = {};
}

bool handle_events(EmuEnvState &emuenv, GuiState &gui, app::AppSessionController *session) {
    std::lock_guard<std::mutex> render_lock(gui.render_mutex);
    const bool ime_dialog_active = emuenv.common_dialog.type == IME_DIALOG
        && emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING;
    if (ime_dialog_active && !SDL_TextInputActive(emuenv.window.get())) {
        if (gui.imgui_state)
            clear_sdl_keyboard_state(emuenv);
        SDL_StartTextInput(emuenv.window.get());
    } else if (!ime_dialog_active && gui.imgui_state
        && !gui.imgui_state->is_typing && !ImGui::GetIO().WantTextInput
        && SDL_TextInputActive(emuenv.window.get())) {
        SDL_StopTextInput(emuenv.window.get());
    }
    const auto allow_switch_state = !emuenv.io.title_id.empty() && !gui.vita_area.live_area_screen && !gui.vita_area.app_close && !gui.vita_area.online_storage && (!gui.vita_area.home_screen || emuenv.kernel.is_threads_paused()) && !gui.vita_area.user_management && !gui.configuration_menu.custom_settings_dialog && !gui.configuration_menu.settings_dialog && !gui.controls_menu.controls_dialog && gui::get_sys_apps_state(gui);

    const auto toggle_pause = [session]() {
        if (session && session->is_running())
            session->set_pause_reason(app::AppSessionPauseReason::User, !session->is_paused());
    };

    const auto ui_navigation = [&emuenv, &gui, session, allow_switch_state, &toggle_pause](const uint32_t sce_ctrl_btn) {
        switch (sce_ctrl_btn) {
        case SCE_CTRL_CROSS:
        case SCE_CTRL_CIRCLE:
            gui.is_key_locked = true;
            if (gui.vita_area.start_screen)
                gui::close_start_screen(gui, emuenv);
            break;
        case SCE_CTRL_PSBUTTON:
            gui.is_key_locked = true;
            if (allow_switch_state) {
                if (!emuenv.cfg.show_live_area_screen) {
                    toggle_pause();
                } else {
                    const auto live_area_app_index = gui::get_live_area_current_open_apps_list_index(gui, emuenv.io.app_path);
                    if (live_area_app_index == gui.live_area_current_open_apps_list.end())
                        gui::open_live_area(gui, emuenv, emuenv.io.app_path);
                    else {
                        if ((gui.live_area_app_current_open < 0) || (gui.live_area_current_open_apps_list[gui.live_area_app_current_open] != emuenv.io.app_path))
                            gui.live_area_app_current_open = static_cast<int32_t>(std::distance(live_area_app_index, gui.live_area_current_open_apps_list.end()) - 1);
                        gui.vita_area.information_bar = true;
                        gui.vita_area.live_area_screen = true;
                    }
                    emuenv.display.imgui_render = true;
#ifdef __ANDROID__
                    gui::set_controller_overlay_state(0);
#endif

                    if (session && session->is_running()) {
                        const bool was_paused = session->is_paused();
                        if (!was_paused)
                            update_live_area_last_app_frame(emuenv, gui);

                        session->set_pause_reason(app::AppSessionPauseReason::Menu, true);
                        if (!was_paused) {
                            app::update_app_time_used(emuenv, emuenv.io.app_path);
                            gui.gate_animation.start(GateAnimationState::ReturnApp);
                            bgm_player::switch_bgm_state(false);
                        }
                    }
                }
            } else if (!gui::get_sys_apps_state(gui))
                gui::close_system_app(gui, emuenv);
            break;
        default: break;
        }

        if (gui.vita_area.app_close) {
            const auto cancel = [&gui]() {
                gui.vita_area.app_close = false;
            };
            const auto confirm = [&gui, &emuenv]() {
                const auto app_path = gui.vita_area.live_area_screen ? gui.live_area_current_open_apps_list[gui.live_area_app_current_open] : emuenv.app_path;
                gui::close_and_run_new_app(emuenv, app_path);
            };
            switch (sce_ctrl_btn) {
            case SCE_CTRL_CIRCLE:
                if (emuenv.cfg.sys_button == 1)
                    cancel();
                else
                    confirm();
                break;
            case SCE_CTRL_CROSS:
                if (emuenv.cfg.sys_button == 1)
                    confirm();
                else
                    cancel();
                break;
            default: break;
            }
        } else if (gui.vita_area.user_management)
            gui::browse_users_management(gui, emuenv, sce_ctrl_btn);
        else if (gui.vita_area.manual)
            gui::browse_pages_manual(gui, emuenv, sce_ctrl_btn);
        else if (gui.vita_area.home_screen)
            gui::browse_home_apps_list(gui, emuenv, sce_ctrl_btn, session);
        else if (gui.vita_area.live_area_screen)
            gui::browse_live_area_apps_list(gui, emuenv, sce_ctrl_btn, session);
    };

    emuenv.drop_inputs = gui.configuration_menu.settings_dialog || gui.configuration_menu.custom_settings_dialog || gui.controls_menu.controllers_dialog || gui.controls_menu.controls_dialog;
    std::set<uint32_t> last_buttons;

    const auto update_mouse_position = [&emuenv](const float x, const float y) {
        int window_width = 0;
        int window_height = 0;
        int pixel_width = 0;
        int pixel_height = 0;
        SDL_GetWindowSize(emuenv.window.get(), &window_width, &window_height);
        SDL_GetWindowSizeInPixels(emuenv.window.get(), &pixel_width, &pixel_height);
        const float scale_x = window_width > 0 ? static_cast<float>(pixel_width) / window_width : 1.0f;
        const float scale_y = window_height > 0 ? static_cast<float>(pixel_height) / window_height : 1.0f;
        emuenv.touch.mouse_x = x * scale_x;
        emuenv.touch.mouse_y = y * scale_y;

        auto &display = emuenv.display;
        std::lock_guard<std::mutex> lock(display.viewport_mutex);
        if (display.viewport_w > 0.f && display.viewport_h > 0.f) {
            auto &mouse = emuenv.ctrl.overlay_mouse;
            mouse.x.store((emuenv.touch.mouse_x - display.viewport_x) * 960.f / display.viewport_w,
                std::memory_order_relaxed);
            mouse.y.store((emuenv.touch.mouse_y - display.viewport_y) * 544.f / display.viewport_h,
                std::memory_order_relaxed);
        }
    };

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        const bool route_text_to_ime = ime_dialog_active
            && (event.type == SDL_EVENT_TEXT_INPUT || event.type == SDL_EVENT_TEXT_EDITING);
        if (!route_text_to_ime)
            ImGui_ImplSdl_ProcessEvent(gui.imgui_state.get(), &event);
        switch (event.type) {
        case SDL_EVENT_QUIT:
            bgm_player::destroy_bgm_player();
            if (!emuenv.io.app_path.empty())
                app::update_app_time_used(emuenv, emuenv.io.app_path);
            if (emuenv.audio.adapter)
                emuenv.audio.switch_state(true);
            // Wake guest threads blocked on GXM notifications/display work before
            // waiting for process_exit() to delete them.
            emuenv.gxm.display_queue.abort();
            emuenv.display.abort = true;
            emuenv.renderer->notification_ready.notify_all();
            emuenv.kernel.process_exit();
            if (emuenv.display.vblank_thread)
                emuenv.display.vblank_thread->join();
            gui.is_capturing_keys = false;
            return false;

        case SDL_EVENT_KEY_DOWN: {
            if (gui.controller_binding_capture >= 0) {
                gui.controller_binding_capture = -1;
                continue;
            }
            if (ime_dialog_active) {
                switch (event.key.key) {
                case SDLK_BACKSPACE: {
                    std::lock_guard lock(emuenv.ime.mutex);
                    ime_backspace(emuenv.ime);
                    continue;
                }
                case SDLK_LEFT: {
                    std::lock_guard lock(emuenv.ime.mutex);
                    ime_cursor_left(emuenv.ime);
                    continue;
                }
                case SDLK_RIGHT: {
                    std::lock_guard lock(emuenv.ime.mutex);
                    ime_cursor_right(emuenv.ime);
                    continue;
                }
                case SDLK_RETURN:
                case SDLK_KP_ENTER: {
                    auto &dialog = emuenv.common_dialog;
                    std::lock_guard<std::recursive_mutex> dialog_lock(dialog.mutex);
                    std::lock_guard ime_lock(emuenv.ime.mutex);
                    const size_t copy_len = std::min(static_cast<size_t>(emuenv.ime.str.length()), static_cast<size_t>(dialog.ime.max_length));
                    std::memcpy(dialog.ime.result, emuenv.ime.str.c_str(), copy_len * sizeof(uint16_t));
                    dialog.ime.result[copy_len] = 0;
                    const std::string utf8 = string_utils::utf16_to_utf8(emuenv.ime.str);
                    snprintf(dialog.ime.text, sizeof(dialog.ime.text), "%s", utf8.c_str());
                    emuenv.ime.event_id = SCE_IME_EVENT_PRESS_ENTER;
                    dialog.ime.status = SCE_IME_DIALOG_BUTTON_ENTER;
                    dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
                    dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
                    continue;
                }
                case SDLK_ESCAPE: {
                    auto &dialog = emuenv.common_dialog;
                    std::lock_guard<std::recursive_mutex> dialog_lock(dialog.mutex);
                    if (dialog.ime.cancelable) {
                        std::lock_guard ime_lock(emuenv.ime.mutex);
                        emuenv.ime.event_id = SCE_IME_EVENT_PRESS_CLOSE;
                        dialog.ime.status = SCE_IME_DIALOG_BUTTON_CLOSE;
                        dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
                        dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
                    }
                    continue;
                }
                default:
                    continue;
                }
            }
            const auto physical_key = physical_key_from_sdl_scancode(event.key.scancode);
            update_sdl_keyboard_state(emuenv, physical_key, true);
            const auto get_sce_ctrl_btn_from_physical_key = [&emuenv](const input::PhysicalKeyCode key) {
                if (key == emuenv.cfg.keyboard_button_up || key == emuenv.cfg.keyboard_button_up_alt)
                    return SCE_CTRL_UP;
                else if (key == emuenv.cfg.keyboard_button_right || key == emuenv.cfg.keyboard_button_right_alt)
                    return SCE_CTRL_RIGHT;
                else if (key == emuenv.cfg.keyboard_button_down || key == emuenv.cfg.keyboard_button_down_alt)
                    return SCE_CTRL_DOWN;
                else if (key == emuenv.cfg.keyboard_button_left || key == emuenv.cfg.keyboard_button_left_alt)
                    return SCE_CTRL_LEFT;
                else if (key == emuenv.cfg.keyboard_button_l1 || key == emuenv.cfg.keyboard_button_l1_alt)
                    return SCE_CTRL_L1;
                else if (key == emuenv.cfg.keyboard_button_r1 || key == emuenv.cfg.keyboard_button_r1_alt)
                    return SCE_CTRL_R1;
                else if (key == emuenv.cfg.keyboard_button_triangle || key == emuenv.cfg.keyboard_button_triangle_alt)
                    return SCE_CTRL_TRIANGLE;
                else if (key == emuenv.cfg.keyboard_button_circle || key == emuenv.cfg.keyboard_button_circle_alt)
                    return SCE_CTRL_CIRCLE;
                else if (key == emuenv.cfg.keyboard_button_cross || key == emuenv.cfg.keyboard_button_cross_alt)
                    return SCE_CTRL_CROSS;
                else if (key == emuenv.cfg.keyboard_button_psbutton || key == emuenv.cfg.keyboard_button_psbutton_alt)
                    return SCE_CTRL_PSBUTTON;
                else
                    return static_cast<SceCtrlButtons>(0);
            };

            auto sce_ctrl_btn = get_sce_ctrl_btn_from_physical_key(physical_key);
            if (gui.is_capturing_keys && physical_key != input::PhysicalKeyCode::Unbound) {
                gui.is_key_capture_dropped = false;
                if (physical_key == input::PhysicalKeyCode::Escape) {
                    LOG_ERROR("Key is reserved!");
                    gui.captured_key = gui.old_captured_key;
                    gui.is_key_capture_dropped = true;
                } else
                    gui.captured_key = static_cast<int>(physical_key);
                gui.is_capturing_keys = false;
                auto capture_completion = std::move(gui.key_capture_completion);
                gui.key_capture_completion = {};
                if (capture_completion)
                    capture_completion();
            }

            if (ImGui::GetIO().WantTextInput || gui.is_key_locked || (emuenv.drop_inputs && !gui.vita_area.live_area_screen) || gui.gate_animation.state != GateAnimationState::None)
                continue;
#ifdef __ANDROID__
            if (event.key.scancode == SDL_SCANCODE_AC_BACK)
                sce_ctrl_btn = SCE_CTRL_PSBUTTON;
#else
            if (allow_switch_state && (physical_key == emuenv.cfg.keyboard_gui_toggle_gui || physical_key == emuenv.cfg.keyboard_gui_toggle_gui_alt))
                emuenv.display.imgui_render = !emuenv.display.imgui_render;
            if ((physical_key == emuenv.cfg.keyboard_gui_toggle_touch || physical_key == emuenv.cfg.keyboard_gui_toggle_touch_alt) && !gui.is_key_capture_dropped)
                toggle_touchscreen(emuenv.touch);
            if ((physical_key == emuenv.cfg.keyboard_gui_fullscreen || physical_key == emuenv.cfg.keyboard_gui_fullscreen_alt) && !gui.is_key_capture_dropped)
                switch_full_screen(emuenv);
            if ((physical_key == emuenv.cfg.keyboard_toggle_texture_replacement || physical_key == emuenv.cfg.keyboard_toggle_texture_replacement_alt) && !gui.is_key_capture_dropped)
                toggle_texture_replacement(emuenv);
            if ((physical_key == emuenv.cfg.keyboard_take_screenshot || physical_key == emuenv.cfg.keyboard_take_screenshot_alt) && !gui.is_key_capture_dropped)
                take_screenshot(emuenv);
            if ((physical_key == emuenv.cfg.keyboard_pinch_modifier || physical_key == emuenv.cfg.keyboard_pinch_modifier_alt || physical_key == emuenv.cfg.keyboard_alternate_pinch_in || physical_key == emuenv.cfg.keyboard_alternate_pinch_in_alt || physical_key == emuenv.cfg.keyboard_alternate_pinch_out || physical_key == emuenv.cfg.keyboard_alternate_pinch_out_alt) && !gui.is_key_capture_dropped)
                pinch_modifier(emuenv.touch, true);

            constexpr float pinch_amount = 0.5f;
            if ((physical_key == emuenv.cfg.keyboard_alternate_pinch_in || physical_key == emuenv.cfg.keyboard_alternate_pinch_in_alt) && !gui.is_key_capture_dropped)
                pinch_automove(emuenv.touch, -pinch_amount);
            if ((physical_key == emuenv.cfg.keyboard_alternate_pinch_out || physical_key == emuenv.cfg.keyboard_alternate_pinch_out_alt) && !gui.is_key_capture_dropped)
                pinch_automove(emuenv.touch, pinch_amount);
#endif
            if (sce_ctrl_btn != 0) {
                if (last_buttons.contains(sce_ctrl_btn))
                    continue;
                last_buttons.insert(sce_ctrl_btn);
                ui_navigation(sce_ctrl_btn);
            }
            break;
        }

        case SDL_EVENT_KEY_UP: {
            gui.is_key_locked = false;
            const auto physical_key = physical_key_from_sdl_scancode(event.key.scancode);
            if (!ime_dialog_active)
                update_sdl_keyboard_state(emuenv, physical_key, false);
            if (physical_key == emuenv.cfg.keyboard_pinch_modifier || physical_key == emuenv.cfg.keyboard_pinch_modifier_alt || physical_key == emuenv.cfg.keyboard_alternate_pinch_in || physical_key == emuenv.cfg.keyboard_alternate_pinch_in_alt || physical_key == emuenv.cfg.keyboard_alternate_pinch_out || physical_key == emuenv.cfg.keyboard_alternate_pinch_out_alt) {
                pinch_modifier(emuenv.touch, false);
                pinch_automove(emuenv.touch, 0.0f);
            }
            break;
        }

        case SDL_EVENT_TEXT_INPUT: {
            if (ime_dialog_active) {
                std::lock_guard lock(emuenv.ime.mutex);
                ime_commit_text(emuenv.ime, string_utils::utf8_to_utf16(event.text.text));
            }
            break;
        }

        case SDL_EVENT_TEXT_EDITING: {
            if (ime_dialog_active) {
                std::lock_guard lock(emuenv.ime.mutex);
                ime_set_preedit(emuenv.ime, string_utils::utf8_to_utf16(event.edit.text));
            }
            break;
        }

        case SDL_EVENT_MOUSE_WHEEL:
            pinch_move(emuenv.touch, event.wheel.y);
            gui.is_nav_button = false;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            update_mouse_position(event.motion.x, event.motion.y);
            gui.is_nav_button = false;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            update_mouse_position(event.button.x, event.button.y);
            if (event.button.button == SDL_BUTTON_LEFT) {
                emuenv.touch.mouse_button_left = true;
                emuenv.ctrl.overlay_mouse.pressed.store(true, std::memory_order_relaxed);
            }
            if (event.button.button == SDL_BUTTON_RIGHT)
                emuenv.touch.mouse_button_right = true;
            gui.is_nav_button = false;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            update_mouse_position(event.button.x, event.button.y);
            if (event.button.button == SDL_BUTTON_LEFT) {
                emuenv.touch.mouse_button_left = false;
                emuenv.ctrl.overlay_mouse.pressed.store(false, std::memory_order_relaxed);
            }
            if (event.button.button == SDL_BUTTON_RIGHT)
                emuenv.touch.mouse_button_right = false;
            gui.is_nav_button = false;
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (gui.controller_binding_capture >= 0) {
                const auto binding = static_cast<size_t>(gui.controller_binding_capture);
                if (binding < emuenv.cfg.controller_binds.size()) {
                    emuenv.cfg.controller_binds[binding] = event.gbutton.button;
                    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
                }
                gui.controller_binding_capture = -1;
                continue;
            }
            if (!emuenv.kernel.is_threads_paused() && event.gbutton.button == SDL_GAMEPAD_BUTTON_TOUCHPAD)
                toggle_touchscreen(emuenv.touch);
            if (ImGui::GetIO().WantTextInput || gui.is_key_locked || (emuenv.drop_inputs && !gui.vita_area.live_area_screen) || gui.gate_animation.state != GateAnimationState::None)
                continue;
            for (const auto &binding : get_controller_bindings_ext(emuenv)) {
                if (event.gbutton.button == binding.controller) {
                    if (last_buttons.contains(binding.button))
                        continue;
                    last_buttons.insert(binding.button);
                    ui_navigation(binding.button);
                    break;
                }
            }
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            gui.is_key_locked = false;
            break;

        case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
            handle_touchpad_event(emuenv.touch, event.gtouchpad);
            break;
        case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
            handle_motion_event(emuenv, event.gsensor.sensor, event.gsensor);
            break;
        case SDL_EVENT_SENSOR_UPDATE:
            handle_motion_event(emuenv, SDL_GetSensorTypeForID(event.sensor.which), event.sensor);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            refresh_controllers(emuenv.ctrl, emuenv);
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            app::update_viewport(emuenv);
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            clear_sdl_keyboard_state(emuenv);
            pinch_modifier(emuenv.touch, false);
            pinch_automove(emuenv.touch, 0.0f);
            emuenv.touch.renderer_focused = false;
            emuenv.touch.mouse_button_left = false;
            emuenv.touch.mouse_button_right = false;
            emuenv.ctrl.overlay_mouse.pressed.store(false, std::memory_order_relaxed);
            gui.is_key_locked = false;
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            emuenv.touch.renderer_focused = true;
            break;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
            handle_touch_event(emuenv.touch, event.tfinger);
            break;
        case SDL_EVENT_DROP_FILE: {
            const auto drop_file = fs_utils::utf8_to_path(event.drop.data);
            const auto extension = string_utils::tolower(drop_file.extension().string());
            if (extension == ".pup") {
                const std::string fw_version = install_pup(emuenv.vita_fs_path, drop_file);
                if (!fw_version.empty()) {
                    LOG_INFO("Firmware {} installed successfully!", fw_version);
                    gui::refresh_modules_list(emuenv);
                    if (emuenv.cfg.initial_setup)
                        gui::init_theme(gui, emuenv, emuenv.app.user_list.users[emuenv.cfg.user_id].theme_id);
                }
            } else if ((extension == ".vpk") || (extension == ".zip"))
                install_archive(emuenv, drop_file);
            else if ((extension == ".rif") || (drop_file.filename() == "work.bin"))
                copy_license(emuenv, drop_file);
            else if (fs::is_directory(drop_file))
                install_contents(emuenv, drop_file);
            else if (drop_file.filename() == "theme.xml")
                install_content(emuenv, drop_file.parent_path());
            else
                LOG_ERROR("File dropped: [{}] is not supported.", drop_file.filename());
            break;
        }
        default: break;
        }
    }

    return true;
}

ExitCode load_app(int32_t &main_module_id, EmuEnvState &emuenv) {
    return load_app(main_module_id, emuenv, AppLaunchRequest{
                                                .app_path = emuenv.io.app_path,
                                            });
}

ExitCode load_app(int32_t &main_module_id, EmuEnvState &emuenv, const AppLaunchRequest &launch_request) {
    if (load_app_impl(main_module_id, emuenv, launch_request) != Success) {
        std::string message = fmt::format(fmt::runtime(lang::get(lang::str::load_app_failed_msg)), emuenv.vita_fs_path / "ux0/app" / emuenv.io.app_path / emuenv.self_path);
        LOG_ERROR(message);
        return ModuleLoadFailed;
    }

    if (emuenv.cfg.boot_apps_full_screen && !emuenv.display.fullscreen.load())
        switch_full_screen(emuenv);

    if (emuenv.cfg.gdbstub) {
        emuenv.kernel.debugger.wait_for_debugger = true;
        server_open(emuenv);
    }

#if USE_DISCORD
    if (emuenv.cfg.discord_rich_presence)
        discordrpc::update_presence(emuenv.io.title_id, emuenv.current_app_title);
#endif

    return Success;
}

static std::vector<std::string> split(const std::string &input, const std::string &regex) {
    std::regex re(regex);
    std::sregex_token_iterator
        first{ input.begin(), input.end(), re, -1 },
        last;
    return { first, last };
}

ExitCode run_app(EmuEnvState &emuenv, int32_t main_module_id) {
    return run_app(emuenv, main_module_id, AppLaunchRequest{
                                               .app_path = emuenv.io.app_path,
                                           });
}

ExitCode run_app(EmuEnvState &emuenv, int32_t main_module_id, const AppLaunchRequest &launch_request) {
    auto entry_point = emuenv.kernel.loaded_modules[main_module_id]->info.start_entry;
    auto process_param = emuenv.kernel.process_param.get(emuenv.mem);

    SceInt32 priority = SCE_KERNEL_DEFAULT_PRIORITY_USER;
    SceInt32 stack_size = SCE_KERNEL_STACK_SIZE_USER_MAIN;
    SceInt32 affinity = SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT;
    if (process_param) {
        auto priority_ptr = Ptr<int32_t>(process_param->main_thread_priority);
        if (priority_ptr) {
            priority = *priority_ptr.get(emuenv.mem);
        }

        auto stack_size_ptr = Ptr<int32_t>(process_param->main_thread_stacksize);
        if (stack_size_ptr) {
            stack_size = *stack_size_ptr.get(emuenv.mem);
        }

        auto affinity_ptr = Ptr<SceInt32>(process_param->main_thread_cpu_affinity_mask);
        if (affinity_ptr) {
            affinity = *affinity_ptr.get(emuenv.mem);
        }
    }
    const ThreadStatePtr main_thread = emuenv.kernel.create_thread(emuenv.mem, emuenv.io.title_id.c_str(), entry_point, priority, affinity, stack_size, nullptr);
    if (!main_thread) {
        LOG_ERROR("Failed to init main thread.");
        return InitThreadFailed;
    }
    emuenv.main_thread_id = main_thread->id;

    // Run `module_start` export (entry point) of loaded libraries
    for (auto &[_, module] : emuenv.kernel.loaded_modules) {
        if (module->info.modid != main_module_id)
            start_module(emuenv, module->info);
    }

    SceKernelThreadOptParam param{ 0, 0 };
    std::vector<std::string> cfg_args;
    const auto *args = &launch_request.argv;
    if (args->empty() && !emuenv.cfg.app_args.empty()) {
        cfg_args = split(emuenv.cfg.app_args, ",\\s+");
        args = &cfg_args;
    }
    if (!args->empty()) {
        // why is this flipped
        std::vector<uint8_t> buf;
        for (const auto &arg : *args)
            buf.insert(buf.end(), arg.c_str(), arg.c_str() + arg.size() + 1);
        auto arr = Ptr<uint8_t>(alloc(emuenv.mem, static_cast<uint32_t>(buf.size()), "arg"));
        memcpy(arr.get(emuenv.mem), buf.data(), buf.size());
        param.size = static_cast<SceSize>(buf.size());
        param.attr = arr.address();
    }
    if (main_thread->start(param.size, Ptr<void>(param.attr), true) < 0) {
        LOG_ERROR("Failed to run main thread.");
        return RunThreadFailed;
    }

    start_sync_thread(emuenv);

    return Success;
}
