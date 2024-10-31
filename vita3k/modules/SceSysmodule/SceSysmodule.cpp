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

#include <module/module.h>

#include <kernel/state.h>
#include <module/load_module.h>
#include <modules/module_parent.h>
#include <util/tracy.h>
#include <util/vector_utils.h>

TRACY_MODULE_NAME(SceSysmodule);

template <>
std::string to_debug_str<SceSysmoduleModuleId>(const MemState &mem, SceSysmoduleModuleId type) {
    switch (type) {
    case SCE_SYSMODULE_INVALID: return "SCE_SYSMODULE_INVALID";
    case SCE_SYSMODULE_NET: return "SCE_SYSMODULE_NET";
    case SCE_SYSMODULE_HTTP: return "SCE_SYSMODULE_HTTP";
    case SCE_SYSMODULE_SSL: return "SCE_SYSMODULE_SSL";
    case SCE_SYSMODULE_HTTPS: return "SCE_SYSMODULE_HTTPS";
    case SCE_SYSMODULE_PERF: return "SCE_SYSMODULE_PERF";
    case SCE_SYSMODULE_FIBER: return "SCE_SYSMODULE_FIBER";
    case SCE_SYSMODULE_ULT: return "SCE_SYSMODULE_ULT";
    case SCE_SYSMODULE_DBG: return "SCE_SYSMODULE_DBG";
    case SCE_SYSMODULE_RAZOR_CAPTURE: return "SCE_SYSMODULE_RAZOR_CAPTURE";
    case SCE_SYSMODULE_RAZOR_HUD: return "SCE_SYSMODULE_RAZOR_HUD";
    case SCE_SYSMODULE_NGS: return "SCE_SYSMODULE_NGS";
    case SCE_SYSMODULE_SULPHA: return "SCE_SYSMODULE_SULPHA";
    case SCE_SYSMODULE_SAS: return "SCE_SYSMODULE_SAS";
    case SCE_SYSMODULE_PGF: return "SCE_SYSMODULE_PGF";
    case SCE_SYSMODULE_APPUTIL: return "SCE_SYSMODULE_APPUTIL";
    case SCE_SYSMODULE_FIOS2: return "SCE_SYSMODULE_FIOS2";
    case SCE_SYSMODULE_IME: return "SCE_SYSMODULE_IME";
    case SCE_SYSMODULE_NP_BASIC: return "SCE_SYSMODULE_NP_BASIC";
    case SCE_SYSMODULE_SYSTEM_GESTURE: return "SCE_SYSMODULE_SYSTEM_GESTURE";
    case SCE_SYSMODULE_LOCATION: return "SCE_SYSMODULE_LOCATION";
    case SCE_SYSMODULE_NP: return "SCE_SYSMODULE_NP";
    case SCE_SYSMODULE_PHOTO_EXPORT: return "SCE_SYSMODULE_PHOTO_EXPORT";
    case SCE_SYSMODULE_XML: return "SCE_SYSMODULE_XML";
    case SCE_SYSMODULE_NP_COMMERCE2: return "SCE_SYSMODULE_NP_COMMERCE2";
    case SCE_SYSMODULE_NP_UTILITY: return "SCE_SYSMODULE_NP_UTILITY";
    case SCE_SYSMODULE_VOICE: return "SCE_SYSMODULE_VOICE";
    case SCE_SYSMODULE_VOICEQOS: return "SCE_SYSMODULE_VOICEQOS";
    case SCE_SYSMODULE_NP_MATCHING2: return "SCE_SYSMODULE_NP_MATCHING2";
    case SCE_SYSMODULE_SCREEN_SHOT: return "SCE_SYSMODULE_SCREEN_SHOT";
    case SCE_SYSMODULE_NP_SCORE_RANKING: return "SCE_SYSMODULE_NP_SCORE_RANKING";
    case SCE_SYSMODULE_SQLITE: return "SCE_SYSMODULE_SQLITE";
    case SCE_SYSMODULE_TRIGGER_UTIL: return "SCE_SYSMODULE_TRIGGER_UTIL";
    case SCE_SYSMODULE_RUDP: return "SCE_SYSMODULE_RUDP";
    case SCE_SYSMODULE_CODECENGINE_PERF: return "SCE_SYSMODULE_CODECENGINE_PERF";
    case SCE_SYSMODULE_LIVEAREA: return "SCE_SYSMODULE_LIVEAREA";
    case SCE_SYSMODULE_NP_ACTIVITY: return "SCE_SYSMODULE_NP_ACTIVITY";
    case SCE_SYSMODULE_NP_TROPHY: return "SCE_SYSMODULE_NP_TROPHY";
    case SCE_SYSMODULE_NP_MESSAGE: return "SCE_SYSMODULE_NP_MESSAGE";
    case SCE_SYSMODULE_SHUTTER_SOUND: return "SCE_SYSMODULE_SHUTTER_SOUND";
    case SCE_SYSMODULE_CLIPBOARD: return "SCE_SYSMODULE_CLIPBOARD";
    case SCE_SYSMODULE_NP_PARTY: return "SCE_SYSMODULE_NP_PARTY";
    case SCE_SYSMODULE_NET_ADHOC_MATCHING: return "SCE_SYSMODULE_NET_ADHOC_MATCHING";
    case SCE_SYSMODULE_NEAR_UTIL: return "SCE_SYSMODULE_NEAR_UTIL";
    case SCE_SYSMODULE_NP_TUS: return "SCE_SYSMODULE_NP_TUS";
    case SCE_SYSMODULE_MP4: return "SCE_SYSMODULE_MP4";
    case SCE_SYSMODULE_AACENC: return "SCE_SYSMODULE_AACENC";
    case SCE_SYSMODULE_HANDWRITING: return "SCE_SYSMODULE_HANDWRITING";
    case SCE_SYSMODULE_ATRAC: return "SCE_SYSMODULE_ATRAC";
    case SCE_SYSMODULE_NP_SNS_FACEBOOK: return "SCE_SYSMODULE_NP_SNS_FACEBOOK";
    case SCE_SYSMODULE_VIDEO_EXPORT: return "SCE_SYSMODULE_VIDEO_EXPORT";
    case SCE_SYSMODULE_NOTIFICATION_UTIL: return "SCE_SYSMODULE_NOTIFICATION_UTIL";
    case SCE_SYSMODULE_BG_APP_UTIL: return "SCE_SYSMODULE_BG_APP_UTIL";
    case SCE_SYSMODULE_INCOMING_DIALOG: return "SCE_SYSMODULE_INCOMING_DIALOG";
    case SCE_SYSMODULE_IPMI: return "SCE_SYSMODULE_IPMI";
    case SCE_SYSMODULE_AUDIOCODEC: return "SCE_SYSMODULE_AUDIOCODEC";
    case SCE_SYSMODULE_FACE: return "SCE_SYSMODULE_FACE";
    case SCE_SYSMODULE_SMART: return "SCE_SYSMODULE_SMART";
    case SCE_SYSMODULE_MARLIN: return "SCE_SYSMODULE_MARLIN";
    case SCE_SYSMODULE_MARLIN_DOWNLOADER: return "SCE_SYSMODULE_MARLIN_DOWNLOADER";
    case SCE_SYSMODULE_MARLIN_APP_LIB: return "SCE_SYSMODULE_MARLIN_APP_LIB";
    case SCE_SYSMODULE_TELEPHONY_UTIL: return "SCE_SYSMODULE_TELEPHONY_UTIL";
    case SCE_SYSMODULE_PSPNET_ADHOC: return "SCE_SYSMODULE_PSPNET_ADHOC";
    case SCE_SYSMODULE_DTCP_IP: return "SCE_SYSMODULE_DTCP_IP";
    case SCE_SYSMODULE_VIDEO_SEARCH_EMPR: return "SCE_SYSMODULE_VIDEO_SEARCH_EMPR";
    case SCE_SYSMODULE_NP_SIGNALING: return "SCE_SYSMODULE_NP_SIGNALING";
    case SCE_SYSMODULE_BEISOBMF: return "SCE_SYSMODULE_BEISOBMF";
    case SCE_SYSMODULE_BEMP2SYS: return "SCE_SYSMODULE_BEMP2SYS";
    case SCE_SYSMODULE_MUSIC_EXPORT: return "SCE_SYSMODULE_MUSIC_EXPORT";
    case SCE_SYSMODULE_NEAR_DIALOG_UTIL: return "SCE_SYSMODULE_NEAR_DIALOG_UTIL";
    case SCE_SYSMODULE_LOCATION_EXTENSION: return "SCE_SYSMODULE_LOCATION_EXTENSION";
    case SCE_SYSMODULE_AVPLAYER: return "SCE_SYSMODULE_AVPLAYER";
    case SCE_SYSMODULE_GAME_UPDATE: return "SCE_SYSMODULE_GAME_UPDATE";
    case SCE_SYSMODULE_MAIL_API: return "SCE_SYSMODULE_MAIL_API";
    case SCE_SYSMODULE_TELEPORT_CLIENT: return "SCE_SYSMODULE_TELEPORT_CLIENT";
    case SCE_SYSMODULE_TELEPORT_SERVER: return "SCE_SYSMODULE_TELEPORT_SERVER";
    case SCE_SYSMODULE_MP4_RECORDER: return "SCE_SYSMODULE_MP4_RECORDER";
    case SCE_SYSMODULE_APPUTIL_EXT: return "SCE_SYSMODULE_APPUTIL_EXT";
    case SCE_SYSMODULE_NP_WEBAPI: return "SCE_SYSMODULE_NP_WEBAPI";
    case SCE_SYSMODULE_AVCDEC: return "SCE_SYSMODULE_AVCDEC";
    case SCE_SYSMODULE_JSON: return "SCE_SYSMODULE_JSON";
    }
    return std::to_string(type);
}

template <>
std::string to_debug_str<SceSysmoduleInternalModuleId>(const MemState &mem, SceSysmoduleInternalModuleId type) {
    switch (type) {
    case SCE_SYSMODULE_INTERNAL_JPEG_ENC_ARM: return "SCE_SYSMODULE_INTERNAL_JPEG_ENC_ARM";
    case SCE_SYSMODULE_INTERNAL_AUDIOCODEC: return "SCE_SYSMODULE_INTERNAL_AUDIOCODEC";
    case SCE_SYSMODULE_INTERNAL_JPEG_ARM: return "SCE_SYSMODULE_INTERNAL_JPEG_ARM";
    case SCE_SYSMODULE_INTERNAL_G729: return "SCE_SYSMODULE_INTERNAL_G729";
    case SCE_SYSMODULE_INTERNAL_BXCE: return "SCE_SYSMODULE_INTERNAL_BXCE";
    case SCE_SYSMODULE_INTERNAL_INI_FILE_PROCESSOR: return "SCE_SYSMODULE_INTERNAL_INI_FILE_PROCESSOR";
    case SCE_SYSMODULE_INTERNAL_NP_ACTIVITY_NET: return "SCE_SYSMODULE_INTERNAL_NP_ACTIVITY_NET";
    case SCE_SYSMODULE_INTERNAL_PAF: return "SCE_SYSMODULE_INTERNAL_PAF";
    case SCE_SYSMODULE_INTERNAL_SQLITE_VSH: return "SCE_SYSMODULE_INTERNAL_SQLITE_VSH";
    case SCE_SYSMODULE_INTERNAL_DBUTIL: return "SCE_SYSMODULE_INTERNAL_DBUTIL";
    case SCE_SYSMODULE_INTERNAL_ACTIVITY_DB: return "SCE_SYSMODULE_INTERNAL_ACTIVITY_DB";
    case SCE_SYSMODULE_INTERNAL_COMMON_GUI_DIALOG: return "SCE_SYSMODULE_INTERNAL_COMMON_GUI_DIALOG";
    case SCE_SYSMODULE_INTERNAL_STORE_CHECKOUT: return "SCE_SYSMODULE_INTERNAL_STORE_CHECKOUT";
    case SCE_SYSMODULE_INTERNAL_IME_DIALOG: return "SCE_SYSMODULE_INTERNAL_IME_DIALOG";
    case SCE_SYSMODULE_INTERNAL_PHOTO_IMPORT_DIALOG: return "SCE_SYSMODULE_INTERNAL_PHOTO_IMPORT_DIALOG";
    case SCE_SYSMODULE_INTERNAL_PHOTO_REVIEW_DIALOG: return "SCE_SYSMODULE_INTERNAL_PHOTO_REVIEW_DIALOG";
    case SCE_SYSMODULE_INTERNAL_CHECKOUT_DIALOG: return "SCE_SYSMODULE_INTERNAL_CHECKOUT_DIALOG";
    case SCE_SYSMODULE_INTERNAL_COMMON_DIALOG_MAIN: return "SCE_SYSMODULE_INTERNAL_COMMON_DIALOG_MAIN";
    case SCE_SYSMODULE_INTERNAL_MSG_DIALOG: return "SCE_SYSMODULE_INTERNAL_MSG_DIALOG";
    case SCE_SYSMODULE_INTERNAL_NET_CHECK_DIALOG: return "SCE_SYSMODULE_INTERNAL_NET_CHECK_DIALOG";
    case SCE_SYSMODULE_INTERNAL_SAVEDATA_DIALOG: return "SCE_SYSMODULE_INTERNAL_SAVEDATA_DIALOG";
    case SCE_SYSMODULE_INTERNAL_NP_MESSAGE_DIALOG: return "SCE_SYSMODULE_INTERNAL_NP_MESSAGE_DIALOG";
    case SCE_SYSMODULE_INTERNAL_TROPHY_SETUP_DIALOG: return "SCE_SYSMODULE_INTERNAL_TROPHY_SETUP_DIALOG";
    case SCE_SYSMODULE_INTERNAL_FRIEND_LIST_DIALOG: return "SCE_SYSMODULE_INTERNAL_FRIEND_LIST_DIALOG";
    case SCE_SYSMODULE_INTERNAL_NEAR_PROFILE: return "SCE_SYSMODULE_INTERNAL_NEAR_PROFILE";
    case SCE_SYSMODULE_INTERNAL_NP_FRIEND_PRIVACY_LEVEL: return "SCE_SYSMODULE_INTERNAL_NP_FRIEND_PRIVACY_LEVEL";
    case SCE_SYSMODULE_INTERNAL_NP_COMMERCE2: return "SCE_SYSMODULE_INTERNAL_NP_COMMERCE2";
    case SCE_SYSMODULE_INTERNAL_NP_KDC: return "SCE_SYSMODULE_INTERNAL_NP_KDC";
    case SCE_SYSMODULE_INTERNAL_MUSIC_EXPORT: return "SCE_SYSMODULE_INTERNAL_MUSIC_EXPORT";
    case SCE_SYSMODULE_INTERNAL_VIDEO_EXPORT: return "SCE_SYSMODULE_INTERNAL_VIDEO_EXPORT";
    case SCE_SYSMODULE_INTERNAL_NP_MESSAGE_DIALOG_IMPL: return "SCE_SYSMODULE_INTERNAL_NP_MESSAGE_DIALOG_IMPL";
    case SCE_SYSMODULE_INTERNAL_NP_MESSAGE_CONTACTS: return "SCE_SYSMODULE_INTERNAL_NP_MESSAGE_CONTACTS";
    case SCE_SYSMODULE_INTERNAL_DB_RECOVERY_UTILITY: return "SCE_SYSMODULE_INTERNAL_DB_RECOVERY_UTILITY";
    case SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL: return "SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL";
    case SCE_SYSMODULE_INTERNAL_PARTY_MEMBER_LIST: return "SCE_SYSMODULE_INTERNAL_PARTY_MEMBER_LIST";
    case SCE_SYSMODULE_INTERNAL_ULT: return "SCE_SYSMODULE_INTERNAL_ULT";
    case SCE_SYSMODULE_INTERNAL_DRM_PSM_KDC: return "SCE_SYSMODULE_INTERNAL_DRM_PSM_KDC";
    case SCE_SYSMODULE_INTERNAL_LOCATION_INTERNAL: return "SCE_SYSMODULE_INTERNAL_LOCATION_INTERNAL";
    case SCE_SYSMODULE_INTERNAL_LOCATION_FACTORY: return "SCE_SYSMODULE_INTERNAL_LOCATION_FACTORY";
    }
    return std::to_string(type);
}

static bool is_module_enabled(EmuEnvState &emuenv, SceSysmoduleModuleId module_id) {
    if (emuenv.cfg.current_config.modules_mode == ModulesMode::MANUAL)
        return !emuenv.cfg.current_config.lle_modules.empty() && is_lle_module(module_id, emuenv);
    else
        return is_lle_module(module_id, emuenv);
}

EXPORT(int, sceSysmoduleIsLoaded, SceSysmoduleModuleId module_id) {
    TRACY_FUNC(sceSysmoduleIsLoaded, module_id);
    if (module_id > SYSMODULE_COUNT)
        return RET_ERROR(SCE_SYSMODULE_ERROR_INVALID_VALUE);
    LOG_INFO("Checking if module ID: {} is loaded", to_debug_str(emuenv.mem, module_id));
    if (is_module_loaded(emuenv.kernel, module_id))
        return SCE_SYSMODULE_LOADED;
    else
        return RET_ERROR(SCE_SYSMODULE_ERROR_UNLOADED);
}

EXPORT(int, sceSysmoduleIsLoadedInternal, SceSysmoduleInternalModuleId module_id) {
    TRACY_FUNC(sceSysmoduleIsLoadedInternal, module_id);
    if (static_cast<int>(module_id) >= 0)
        return CALL_EXPORT(sceSysmoduleIsLoaded, static_cast<SceSysmoduleModuleId>(module_id));

    if (((module_id & 0xFFFF) == 0) || ((module_id & 0xFFFF) > 0x29))
        return RET_ERROR(SCE_SYSMODULE_ERROR_INVALID_VALUE);

    std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);
    if (vector_utils::contains(emuenv.kernel.loaded_internal_sysmodules, module_id))
        return SCE_SYSMODULE_LOADED;
    else
        return RET_ERROR(SCE_SYSMODULE_ERROR_UNLOADED);
}

EXPORT(int, sceSysmoduleLoadModule, SceSysmoduleModuleId module_id) {
    TRACY_FUNC(sceSysmoduleLoadModule, module_id);
    if (module_id > SYSMODULE_COUNT)
        return RET_ERROR(SCE_SYSMODULE_ERROR_INVALID_VALUE);

    LOG_INFO("Loading module ID: {}", to_debug_str(emuenv.mem, module_id));
    {
        std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);
        if (emuenv.kernel.loaded_sysmodules.contains(module_id))
            return SCE_SYSMODULE_LOADED;
    }
    if (is_module_enabled(emuenv, module_id)) {
        if (load_sys_module(emuenv, module_id))
            return SCE_SYSMODULE_LOADED;
        else
            return RET_ERROR(SCE_SYSMODULE_ERROR_FATAL);
    } else {
        std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);
        emuenv.kernel.loaded_sysmodules[module_id] = {};
        return SCE_SYSMODULE_LOADED;
    }
}

EXPORT(int, sceSysmoduleLoadModuleInternal, SceSysmoduleInternalModuleId module_id) {
    TRACY_FUNC(sceSysmoduleLoadModuleInternal, module_id);
    LOG_TRACE("sceSysmoduleLoadModuleInternal(module_id:{})", to_debug_str(emuenv.mem, module_id));

    if (static_cast<int>(module_id) >= 0) {
        // apparently you can load non-internal modules with this function
        return CALL_EXPORT(sceSysmoduleLoadModule, static_cast<SceSysmoduleModuleId>(module_id));
    }

    const bool loaded = load_sys_module_internal_with_arg(emuenv, thread_id, module_id, 0, Ptr<void>(), nullptr);
    return loaded ? SCE_SYSMODULE_LOADED : RET_ERROR(SCE_SYSMODULE_ERROR_FATAL);
}

EXPORT(int, sceSysmoduleLoadModuleInternalWithArg, SceSysmoduleInternalModuleId module_id, SceSize args, Ptr<void> argp, const SceSysmoduleOpt *option) {
    TRACY_FUNC(sceSysmoduleLoadModuleInternalWithArg, module_id, args, argp, option);
    LOG_TRACE("sceSysmoduleLoadModuleInternalWithArg(module_id:{}, args:{}, argp:{},option:{})", to_debug_str(emuenv.mem, module_id),
        to_debug_str(emuenv.mem, args), to_debug_str(emuenv.mem, argp), to_debug_str(emuenv.mem, option));

    const bool loaded = load_sys_module_internal_with_arg(emuenv, thread_id, module_id, args, argp, option ? option->result.get(emuenv.mem) : nullptr);
    return loaded ? SCE_SYSMODULE_LOADED : RET_ERROR(SCE_SYSMODULE_ERROR_FATAL);
}

EXPORT(int, sceSysmoduleUnloadModule, SceSysmoduleModuleId module_id) {
    TRACY_FUNC(sceSysmoduleUnloadModule, module_id);

    if (is_module_enabled(emuenv, module_id)) {
        return unload_sys_module(emuenv, module_id);
    } else {
        std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);
        emuenv.kernel.loaded_sysmodules.erase(module_id);

        return 0;
    }
}

EXPORT(int, sceSysmoduleUnloadModuleInternal, SceSysmoduleInternalModuleId module_id) {
    TRACY_FUNC(sceSysmoduleUnloadModuleInternal, module_id);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSysmoduleUnloadModuleInternalWithArg, SceSysmoduleInternalModuleId module_id, SceSize args, void *argp, const SceSysmoduleOpt *option) {
    TRACY_FUNC(sceSysmoduleUnloadModuleInternalWithArg, module_id, args, argp, option);
    return UNIMPLEMENTED();
}
