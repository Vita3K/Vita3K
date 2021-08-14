// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <host/state.h>

#include <vector>

static constexpr auto SYSMODULE_COUNT = 0x56;

using SysmodulePaths = std::array<std::vector<const char *>, SYSMODULE_COUNT>;

inline SysmodulePaths init_sysmodule_paths() {
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

const SysmodulePaths sysmodule_paths = init_sysmodule_paths();

bool is_lle_module(SceSysmoduleModuleId module_id, HostState &host);
bool is_module_loaded(KernelState &kernel, SceSysmoduleModuleId module_id);
