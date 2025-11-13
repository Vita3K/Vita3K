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

#include <module/load_module.h>

#include <config/state.h>
#include <emuenv/state.h>
#include <kernel/state.h>
#include <util/vector_utils.h>

static SysmodulePaths init_sysmodule_paths() {
    SysmodulePaths p;

    // Below dependencies are roughly derived from strings order in sysmodule.skprx
    // Not guaranteed to be correct

    p[SCE_SYSMODULE_NET] = { "libnet", "libnetctl" };
    p[SCE_SYSMODULE_HTTP] = { "libhttp" };
    p[SCE_SYSMODULE_SSL] = { "libssl" };
    p[SCE_SYSMODULE_HTTPS] = { "libhttp", "libssl" };
    p[SCE_SYSMODULE_PERF] = { "libperf" };
    p[SCE_SYSMODULE_FIBER] = { "libfiber" };
    p[SCE_SYSMODULE_ULT] = { "libult" };
    p[SCE_SYSMODULE_DBG] = { "librazorcapture_es4", "librazorhud_es4" };
    p[SCE_SYSMODULE_RAZOR_CAPTURE] = { "librazorcapture_es4" };
    p[SCE_SYSMODULE_RAZOR_HUD] = { "librazorhud_es4" };
    p[SCE_SYSMODULE_NGS] = { "libngs" };
    p[SCE_SYSMODULE_SULPHA] = { "libsulpha" };
    p[SCE_SYSMODULE_SAS] = { "libsas" };
    p[SCE_SYSMODULE_PGF] = { "libpgf" };
    p[SCE_SYSMODULE_APPUTIL] = { "apputil" };
    p[SCE_SYSMODULE_FIOS2] = { "libfios2" };
    p[SCE_SYSMODULE_IME] = { "libime" };
    p[SCE_SYSMODULE_NP_BASIC] = { "np_basic" };
    p[SCE_SYSMODULE_SYSTEM_GESTURE] = { "libsystemgesture" };
    p[SCE_SYSMODULE_LOCATION] = { "liblocation" };
    p[SCE_SYSMODULE_NP] = { "np_common", "np_manager", "np_basic" };
    p[SCE_SYSMODULE_PHOTO_EXPORT] = { "libScePhotoExport" };
    p[SCE_SYSMODULE_XML] = { "libSceXml" };
    p[SCE_SYSMODULE_NP_COMMERCE2] = { "np_commerce2" };
    p[SCE_SYSMODULE_NP_UTILITY] = { "np_utility" };
    p[SCE_SYSMODULE_VOICE] = { "libvoice" };
    p[SCE_SYSMODULE_VOICEQOS] = { "libvoiceqos" };
    p[SCE_SYSMODULE_NP_MATCHING2] = { "np_matching2" };
    p[SCE_SYSMODULE_SCREEN_SHOT] = { "libSceScreenShot" };
    p[SCE_SYSMODULE_NP_SCORE_RANKING] = { "np_ranking" };
    p[SCE_SYSMODULE_SQLITE] = { "libSceSqlite" };
    p[SCE_SYSMODULE_TRIGGER_UTIL] = { "trigger_util" };
    p[SCE_SYSMODULE_RUDP] = { "librudp" };
    p[SCE_SYSMODULE_CODECENGINE_PERF] = { "libcodecengine_perf" };
    p[SCE_SYSMODULE_LIVEAREA] = { "livearea_util" };
    p[SCE_SYSMODULE_NP_ACTIVITY] = { "np_activity_sdk" };
    p[SCE_SYSMODULE_NP_TROPHY] = { "np_trophy" };
    p[SCE_SYSMODULE_NP_MESSAGE] = { "np_message_padding", "np_message" };
    p[SCE_SYSMODULE_SHUTTER_SOUND] = { "libSceShutterSound" };
    p[SCE_SYSMODULE_CLIPBOARD] = { "libclipboard" };
    p[SCE_SYSMODULE_NP_PARTY] = { "np_party" };
    p[SCE_SYSMODULE_NET_ADHOC_MATCHING] = { "adhoc_matching" };
    p[SCE_SYSMODULE_NEAR_UTIL] = { "libScenNearUtil" };
    p[SCE_SYSMODULE_NP_TUS] = { "np_tus" };
    p[SCE_SYSMODULE_MP4] = { "libscemp4" };
    p[SCE_SYSMODULE_AACENC] = { "libnaac" };
    p[SCE_SYSMODULE_HANDWRITING] = { "libhandwriting" };
    p[SCE_SYSMODULE_ATRAC] = { "libatrac" };
    p[SCE_SYSMODULE_NP_SNS_FACEBOOK] = { "np_sns_facebook" };
    p[SCE_SYSMODULE_VIDEO_EXPORT] = { "libSceVideoExport" };
    p[SCE_SYSMODULE_NOTIFICATION_UTIL] = { "notification_util" };
    p[SCE_SYSMODULE_BG_APP_UTIL] = { "bgapputil" };
    p[SCE_SYSMODULE_INCOMING_DIALOG] = { "incoming_dialog" };
    p[SCE_SYSMODULE_IPMI] = { "libipmi_nongame" };
    p[SCE_SYSMODULE_AUDIOCODEC] = { "libaudiocodec" };
    p[SCE_SYSMODULE_FACE] = { "libface" };
    p[SCE_SYSMODULE_SMART] = { "libsmart" };
    p[SCE_SYSMODULE_MARLIN] = { "libmln" };
    p[SCE_SYSMODULE_MARLIN_DOWNLOADER] = { "libmlndownloader" };
    p[SCE_SYSMODULE_MARLIN_APP_LIB] = { "libmlnapplib" };
    p[SCE_SYSMODULE_TELEPHONY_UTIL] = { "libSceTelephonyUtil" };
    p[SCE_SYSMODULE_PSPNET_ADHOC] = { "pspnet_adhoc" };
    p[SCE_SYSMODULE_DTCP_IP] = { "libSceDtcpIp" };
    p[SCE_SYSMODULE_VIDEO_SEARCH_EMPR] = { "libSceVideoSearchEmpr" };
    p[SCE_SYSMODULE_NP_SIGNALING] = { "np_signaling" };
    p[SCE_SYSMODULE_BEISOBMF] = { "libSceBeisobmf" };
    p[SCE_SYSMODULE_BEMP2SYS] = { "libSceBemp2sys" };
    p[SCE_SYSMODULE_MUSIC_EXPORT] = { "libSceMusicExport" };
    p[SCE_SYSMODULE_NEAR_DIALOG_UTIL] = { "libSceNearDialogUtil" };
    p[SCE_SYSMODULE_LOCATION_EXTENSION] = { "liblocation_extension" };
    p[SCE_SYSMODULE_AVPLAYER] = { "libsceavplayer", "libscemp4" };
    p[SCE_SYSMODULE_MAIL_API] = { "mail_api_for_local_libc" };
    p[SCE_SYSMODULE_TELEPORT_CLIENT] = { "libSceTeleportClient" };
    p[SCE_SYSMODULE_TELEPORT_SERVER] = { "libSceTeleportServer" };
    p[SCE_SYSMODULE_MP4_RECORDER] = { "libSceMp4Rec" };
    p[SCE_SYSMODULE_APPUTIL_EXT] = { "apputil_ext" };
    p[SCE_SYSMODULE_NP_WEBAPI] = { "np_webapi" };
    p[SCE_SYSMODULE_AVCDEC] = { "avcdec_for_player" };
    p[SCE_SYSMODULE_JSON] = { "libSceJson" };

    return p;
}

static SysmoduleInternalPaths init_sysmodule_internal_paths() {
    SysmoduleInternalPaths p;

    p[SCE_SYSMODULE_INTERNAL_JPEG_ENC_ARM] = { "libscejpegencarm" };
    p[SCE_SYSMODULE_INTERNAL_AUDIOCODEC] = { "audiocodec" };
    p[SCE_SYSMODULE_INTERNAL_BXCE] = { "bXCe" };
    p[SCE_SYSMODULE_INTERNAL_INI_FILE_PROCESSOR] = { "ini_file_processor" };
    p[SCE_SYSMODULE_INTERNAL_NP_ACTIVITY_NET] = { "np_activity" };
    p[SCE_SYSMODULE_INTERNAL_PAF] = { "libpaf" };
    p[SCE_SYSMODULE_INTERNAL_SQLITE_VSH] = { "sqlite" };
    p[SCE_SYSMODULE_INTERNAL_DBUTIL] = { "dbutil" };
    p[SCE_SYSMODULE_INTERNAL_ACTIVITY_DB] = { "activity_db" };
    p[SCE_SYSMODULE_INTERNAL_COMMON_GUI_DIALOG] = { "common_gui_dialog" };
    p[SCE_SYSMODULE_INTERNAL_MSG_DIALOG] = { "libcdlg_msg" };
    p[SCE_SYSMODULE_INTERNAL_SAVEDATA_DIALOG] = { "libcdlg_savedata" };
    p[SCE_SYSMODULE_INTERNAL_IME_DIALOG] = { "libcdlg_ime" };
    p[SCE_SYSMODULE_INTERNAL_COMMON_DIALOG_MAIN] = { "libcdlg_main" };
    p[SCE_SYSMODULE_INTERNAL_DB_RECOVERY_UTILITY] = { "dbrecovery_utility" };
    p[SCE_SYSMODULE_INTERNAL_DRM_PSM_KDC] = { "psmkdc" };
    p[SCE_SYSMODULE_INTERNAL_LOCATION_INTERNAL] = { "liblocation_internal" };

    return p;
}

extern const SysmodulePaths sysmodule_paths = init_sysmodule_paths();
extern const SysmoduleInternalPaths sysmodule_internal_paths = init_sysmodule_internal_paths();

// Current modules works for loading
static constexpr auto auto_lle_modules = {
    SCE_SYSMODULE_HTTP,
    SCE_SYSMODULE_SSL,
    SCE_SYSMODULE_HTTPS,
    SCE_SYSMODULE_ULT,
    SCE_SYSMODULE_SAS,
    SCE_SYSMODULE_PGF,
    SCE_SYSMODULE_FIOS2,
    SCE_SYSMODULE_SYSTEM_GESTURE,
    SCE_SYSMODULE_XML,
    SCE_SYSMODULE_NP_MATCHING2,
    SCE_SYSMODULE_SQLITE,
    SCE_SYSMODULE_RUDP,
    SCE_SYSMODULE_NET_ADHOC_MATCHING,
    SCE_SYSMODULE_MP4,
    SCE_SYSMODULE_ATRAC,
    SCE_SYSMODULE_FACE,
    SCE_SYSMODULE_SMART,
    SCE_SYSMODULE_AVPLAYER,
    SCE_SYSMODULE_JSON
};

bool is_lle_module(SceSysmoduleModuleId module_id, EmuEnvState &emuenv) {
    const auto &paths = sysmodule_paths[module_id];

    // Do we know the module and its dependencies' paths?
    const bool have_paths = !paths.empty();

    if (have_paths) {
        if (emuenv.cfg.current_config.modules_mode != ModulesMode::MANUAL) {
            if (vector_utils::contains(auto_lle_modules, module_id))
                return true;
        }

        if (emuenv.cfg.current_config.modules_mode != ModulesMode::AUTOMATIC) {
            for (auto path : paths) {
                if (vector_utils::contains(emuenv.cfg.current_config.lle_modules, path))
                    return true;
            }
        }
    }

    return false;
}

static std::vector<std::string> init_auto_lle_module_names() {
    std::vector<std::string> auto_lle_module_names = { "libc", "libSceFt2", "libpvf" };
    for (const auto module_id : auto_lle_modules) {
        for (const auto module : sysmodule_paths[module_id]) {
            auto_lle_module_names.emplace_back(module);
        }
    }
    return auto_lle_module_names;
}

bool is_lle_module(const std::string &module_name, EmuEnvState &emuenv) {
    static std::vector<std::string> auto_lle_module_names{};
    if (auto_lle_module_names.empty())
        auto_lle_module_names = init_auto_lle_module_names();
    if (emuenv.cfg.current_config.modules_mode != ModulesMode::AUTOMATIC) {
        if (vector_utils::contains(emuenv.cfg.current_config.lle_modules, module_name))
            return true;
    }
    if (emuenv.cfg.current_config.modules_mode != ModulesMode::MANUAL) {
        if (vector_utils::contains(auto_lle_module_names, module_name))
            return true;
    }
    return false;
}

bool is_module_loaded(KernelState &kernel, SceSysmoduleModuleId module_id) {
    std::lock_guard<std::mutex> guard(kernel.mutex);
    return kernel.loaded_sysmodules.contains(module_id);
}
