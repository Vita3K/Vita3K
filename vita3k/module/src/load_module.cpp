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

#include <module/load_module.h>

#include <config/state.h>
#include <emuenv/state.h>
#include <kernel/state.h>
#include <util/vector_utils.h>

// The module table of sysmodule: each module's file name and SYSMODULE_MODULE_* flags
constinit const SysmoduleModules sysmodule_modules = { {
    { nullptr, 0x0 }, // 0
    { nullptr, 0x40 }, // 1
    { "libnet", 0x11 }, // 2
    { "libnetctl", 0x11 }, // 3
    { "libhttp", 0x11 }, // 4
    { "libssl", 0x11 }, // 5
    { "libperf", 0x85 }, // 6
    { "libfiber", 0x1 }, // 7
    { "libult", 0x20 }, // 8
    { "libult", 0x1 }, // 9
    { "librazorcapture_es4", 0x85 }, // 10
    { "librazorhud_es4", 0x85 }, // 11
    { "libngs", 0x1 }, // 12
    { "libsulpha", 0x105 }, // 13
    { "libsas", 0x1 }, // 14
    { "libpgf", 0x1 }, // 15
    { "liblocation", 0x1 }, // 16
    { "np_common", 0x11 }, // 17
    { "np_manager", 0x11 }, // 18
    { "np_basic", 0x1 }, // 19
    { "libScePhotoExport", 0x11 }, // 20
    { "libime", 0x1 }, // 21
    { "libsystemgesture", 0x1 }, // 22
    { "libSceXml", 0x1 }, // 23
    { "np_commerce2", 0x1 }, // 24
    { "np_utility", 0x11 }, // 25
    { "libvoice", 0x1 }, // 26
    { "libvoiceqos", 0x1 }, // 27
    { "np_matching2", 0x1 }, // 28
    { "libSceScreenShot", 0x1 }, // 29
    { "np_ranking", 0x1 }, // 30
    { "libSceSqlite", 0x1 }, // 31
    { "trigger_util", 0x1 }, // 32
    { "librudp", 0x1 }, // 33
    { "libcodecengine_perf", 0x5 }, // 34
    { "livearea_util", 0x11 }, // 35
    { "np_activity_sdk", 0x1 }, // 36
    { "np_trophy", 0x1 }, // 37
    { "np_message_padding", 0x1 }, // 38
    { "np_message", 0x1 }, // 39
    { "libSceShutterSound", 0x1 }, // 40
    { "libclipboard", 0x1 }, // 41
    { "np_party", 0x1 }, // 42
    { "adhoc_matching", 0x1 }, // 43
    { "libSceNearUtil", 0x1 }, // 44
    { "np_tus", 0x1 }, // 45
    { "libscemp4", 0x1 }, // 46
    { "libnaac", 0x1 }, // 47
    { "libhandwriting", 0x1 }, // 48
    { "libatrac", 0x1 }, // 49
    { "np_sns_facebook", 0x1 }, // 50
    { "libSceVideoExport", 0x1 }, // 51
    { "notification_util", 0x1 }, // 52
    { "bgapputil", 0x1 }, // 53
    { "incoming_dialog", 0x1 }, // 54
    { "libipmi_nongame", 0x1 }, // 55
    { "audiocodec", 0x11 }, // 56
    { "libface", 0x20 }, // 57
    { "libsmart", 0x20 }, // 58
    { "libmln", 0x1 }, // 59
    { "libmlndownloader", 0x1 }, // 60
    { "libmlnapplib", 0x1 }, // 61
    { "libSceTelephonyUtil", 0x1 }, // 62
    { "libshacccg", 0x1 }, // 63
    { "libmono_bridge", 0x1 }, // 64
    { "libmono", 0x1 }, // 65
    { "libpsm", 0x1 }, // 66
    { "libpsm_devagent", 0x1 }, // 67
    { "pspnet_adhoc", 0x1 }, // 68
    { "libSceDtcpIp", 0x1 }, // 69
    { "libSceVideoSearchEmpr", 0x1 }, // 70
    { "np_signaling", 0x1 }, // 71
    { "libSceBeisobmf", 0x1 }, // 72
    { "libSceBemp2sys", 0x1 }, // 73
    { "libSceMusicExport", 0x1 }, // 74
    { "libSceNearDialogUtil", 0x1 }, // 75
    { "liblocation_extension", 0x1 }, // 76
    { "libsceavplayer", 0x1 }, // 77
    { "libgameupdate", 0x1 }, // 78
    { "mail_api_for_local_libc", 0x1 }, // 79
    { "libSceTeleportClient", 0x1 }, // 80
    { "libSceTeleportServer", 0x1 }, // 81
    { "libSceMp4Rec", 0x1 }, // 82
    { "apputil_ext", 0x1 }, // 83
    { "np_common_ps4", 0x11 }, // 84
    { "np_webapi", 0x11 }, // 85
    { "avcdec_for_player", 0x1 }, // 86
    { "libSceJson", 0x1 }, // 87
    { "libscejpegencarm", 0x11 }, // 88
    { "libscejpegarm", 0x1 }, // 89
    { "libg729", 0x1 }, // 90
    { "bXCe", 0x11 }, // 91
    { "ini_file_processor", 0x11 }, // 92
    { "np_activity", 0x11 }, // 93
    { "libpaf", 0x11 }, // 94
    { "sqlite", 0x11 }, // 95
    { "dbutil", 0x11 }, // 96
    { "activity_db", 0x11 }, // 97
    { "common_gui_dialog", 0x11 }, // 98
    { "store_checkout_plugin", 0x1 }, // 99
    { "libcdlg_ime", 0x1 }, // 100
    { "libcdlg_photoimport", 0x1 }, // 101
    { "libcdlg_photoreview", 0x1 }, // 102
    { "libcdlg_checkout", 0x1 }, // 103
    { "libcdlg_main", 0x1 }, // 104
    { "libcdlg_msg", 0x1 }, // 105
    { "libcdlg_netcheck", 0x1 }, // 106
    { "libcdlg_savedata", 0x1 }, // 107
    { "libcdlg_np_message", 0x1 }, // 108
    { "libcdlg_np_trophy_setup", 0x1 }, // 109
    { "libcdlg_friendlist", 0x1 }, // 110
    { "libcdlg_npprofile", 0x1 }, // 111
    { "near_profile", 0x1 }, // 112
    { "np_friend_privacylevel", 0x1 }, // 113
    { "libpaf_debug", 0x1 }, // 114
    { "np_commerce2", 0x1 }, // 115
    { "np_kdc", 0x1 }, // 116
    { "np_message_dialog_impl", 0x1 }, // 117
    { "np_message_contacts", 0x1 }, // 118
    { "dbrecovery_utility", 0x1 }, // 119
    { "libScePromoterUtil", 0x1 }, // 120
    { "party_member_list", 0x1 }, // 121
    { "psmkdc", 0x1 }, // 122
    { "liblocation_internal", 0x1 }, // 123
    { "liblocation_factory", 0x1 }, // 124
} };

static constexpr uint8_t net_modules[] = { 3, 2 }; // libnet, libnetctl
static constexpr uint8_t http_modules[] = { 4, 3, 2 }; // libnet, libnetctl, libhttp
static constexpr uint8_t ssl_modules[] = { 5, 2 }; // libnet, libssl
static constexpr uint8_t https_modules[] = { 5, 2, 4, 3, 2 }; // libnet, libnetctl, libhttp, libnet, libssl
static constexpr uint8_t perf_modules[] = { 6 }; // libperf
static constexpr uint8_t fiber_modules[] = { 7 }; // libfiber
static constexpr uint8_t ult_modules[] = { 8, 7 }; // libfiber, libult
static constexpr uint8_t razor_capture_modules[] = { 10 }; // librazorcapture_es4
static constexpr uint8_t razor_hud_modules[] = { 11 }; // librazorhud_es4
static constexpr uint8_t ngs_modules[] = { 12 }; // libngs
static constexpr uint8_t sulpha_modules[] = { 13 }; // libsulpha
static constexpr uint8_t sas_modules[] = { 14 }; // libsas
static constexpr uint8_t pgf_modules[] = { 15 }; // libpgf
static constexpr uint8_t ime_modules[] = { 21 }; // libime
static constexpr uint8_t np_basic_modules[] = { 19, 17, 18, 3, 2 }; // libnet, libnetctl, np_manager, np_common, np_basic
static constexpr uint8_t system_gesture_modules[] = { 22 }; // libsystemgesture
static constexpr uint8_t location_modules[] = { 16 }; // liblocation
static constexpr uint8_t np_modules[] = { 17, 18, 3, 2 }; // libnet, libnetctl, np_manager, np_common
static constexpr uint8_t photo_export_modules[] = { 20 }; // libScePhotoExport
static constexpr uint8_t xml_modules[] = { 23 }; // libSceXml
static constexpr uint8_t np_commerce2_modules[] = { 24, 17, 18, 3, 2, 5, 2, 4, 3, 2 }; // libnet, libnetctl, libhttp, libnet, libssl, libnet, libnetctl, np_manager, np_common, np_commerce2
static constexpr uint8_t np_utility_modules[] = { 25, 17, 18, 3, 2, 4, 3, 2 }; // libnet, libnetctl, libhttp, libnet, libnetctl, np_manager, np_common, np_utility
static constexpr uint8_t voice_modules[] = { 26 }; // libvoice
static constexpr uint8_t voiceqos_modules[] = { 27, 26 }; // libvoice, libvoiceqos
static constexpr uint8_t np_matching2_modules[] = { 28, 17, 18, 3, 2, 5, 2, 4, 3, 2 }; // libnet, libnetctl, libhttp, libnet, libssl, libnet, libnetctl, np_manager, np_common, np_matching2
static constexpr uint8_t screen_shot_modules[] = { 29 }; // libSceScreenShot
static constexpr uint8_t np_score_ranking_modules[] = { 30, 17, 18, 3, 2 }; // libnet, libnetctl, np_manager, np_common, np_ranking
static constexpr uint8_t sqlite_modules[] = { 31 }; // libSceSqlite
static constexpr uint8_t trigger_util_modules[] = { 32 }; // trigger_util
static constexpr uint8_t rudp_modules[] = { 33 }; // librudp
static constexpr uint8_t codecengine_perf_modules[] = { 34 }; // libcodecengine_perf
static constexpr uint8_t livearea_modules[] = { 35 }; // livearea_util
static constexpr uint8_t np_activity_modules[] = { 36 }; // np_activity_sdk
static constexpr uint8_t np_trophy_modules[] = { 37, 17, 18, 3, 2 }; // libnet, libnetctl, np_manager, np_common, np_trophy
static constexpr uint8_t np_message_modules[] = { 39, 38 }; // np_message_padding, np_message
static constexpr uint8_t shutter_sound_modules[] = { 40 }; // libSceShutterSound
static constexpr uint8_t clipboard_modules[] = { 41 }; // libclipboard
static constexpr uint8_t np_party_modules[] = { 42 }; // np_party
static constexpr uint8_t net_adhoc_matching_modules[] = { 43, 3, 2 }; // libnet, libnetctl, adhoc_matching
static constexpr uint8_t near_util_modules[] = { 44, 17, 18, 3, 2 }; // libnet, libnetctl, np_manager, np_common, libSceNearUtil
static constexpr uint8_t np_tus_modules[] = { 45, 17, 18, 3, 2 }; // libnet, libnetctl, np_manager, np_common, np_tus
static constexpr uint8_t mp4_modules[] = { 46 }; // libscemp4
static constexpr uint8_t aacenc_modules[] = { 47 }; // libnaac
static constexpr uint8_t handwriting_modules[] = { 48 }; // libhandwriting
static constexpr uint8_t atrac_modules[] = { 49 }; // libatrac
static constexpr uint8_t np_sns_facebook_modules[] = { 50 }; // np_sns_facebook
static constexpr uint8_t video_export_modules[] = { 51 }; // libSceVideoExport
static constexpr uint8_t notification_util_modules[] = { 52 }; // notification_util
static constexpr uint8_t bg_app_util_modules[] = { 53 }; // bgapputil
static constexpr uint8_t incoming_dialog_modules[] = { 54 }; // incoming_dialog
static constexpr uint8_t ipmi_modules[] = { 55 }; // libipmi_nongame
static constexpr uint8_t audiocodec_modules[] = { 56 }; // audiocodec
static constexpr uint8_t face_modules[] = { 57 }; // libface
static constexpr uint8_t smart_modules[] = { 58 }; // libsmart
static constexpr uint8_t marlin_modules[] = { 59 }; // libmln
static constexpr uint8_t marlin_downloader_modules[] = { 60 }; // libmlndownloader
static constexpr uint8_t marlin_app_lib_modules[] = { 61 }; // libmlnapplib
static constexpr uint8_t telephony_util_modules[] = { 62 }; // libSceTelephonyUtil
static constexpr uint8_t shacccg_modules[] = { 63 }; // libshacccg
static constexpr uint8_t mono_bridge_modules[] = { 64 }; // libmono_bridge
static constexpr uint8_t mono_modules[] = { 65, 64 }; // libmono_bridge, libmono
static constexpr uint8_t psm_modules[] = { 66, 65, 63 }; // libshacccg, libmono, libpsm
static constexpr uint8_t psm_devagent_modules[] = { 67, 66 }; // libpsm, libpsm_devagent
static constexpr uint8_t pspnet_adhoc_modules[] = { 68, 3, 2 }; // libnet, libnetctl, pspnet_adhoc
static constexpr uint8_t dtcp_ip_modules[] = { 69 }; // libSceDtcpIp
static constexpr uint8_t video_search_empr_modules[] = { 70 }; // libSceVideoSearchEmpr
static constexpr uint8_t np_signaling_modules[] = { 71, 17, 18, 3, 2 }; // libnet, libnetctl, np_manager, np_common, np_signaling
static constexpr uint8_t beisobmf_modules[] = { 72 }; // libSceBeisobmf
static constexpr uint8_t bemp2sys_modules[] = { 73 }; // libSceBemp2sys
static constexpr uint8_t music_export_modules[] = { 74 }; // libSceMusicExport
static constexpr uint8_t near_dialog_util_modules[] = { 75, 17, 18, 3, 2 }; // libnet, libnetctl, np_manager, np_common, libSceNearDialogUtil
static constexpr uint8_t location_extension_modules[] = { 76, 16 }; // liblocation, liblocation_extension
static constexpr uint8_t avplayer_modules[] = { 77, 46 }; // libscemp4, libsceavplayer
static constexpr uint8_t game_update_modules[] = { 78, 5, 2, 4, 3, 2 }; // libnet, libnetctl, libhttp, libnet, libssl, libgameupdate
static constexpr uint8_t mail_api_modules[] = { 79 }; // mail_api_for_local_libc
static constexpr uint8_t teleport_client_modules[] = { 80 }; // libSceTeleportClient
static constexpr uint8_t teleport_server_modules[] = { 81 }; // libSceTeleportServer
static constexpr uint8_t mp4_recorder_modules[] = { 82, 47, 51 }; // libSceVideoExport, libnaac, libSceMp4Rec
static constexpr uint8_t apputil_ext_modules[] = { 83 }; // apputil_ext
static constexpr uint8_t np_webapi_modules[] = { 85, 84, 17, 18, 3, 2, 5, 2, 4, 3, 2 }; // libnet, libnetctl, libhttp, libnet, libssl, libnet, libnetctl, np_manager, np_common, np_common_ps4, np_webapi
static constexpr uint8_t avcdec_modules[] = { 86 }; // avcdec_for_player
static constexpr uint8_t json_modules[] = { 87 }; // libSceJson

static constexpr SysmoduleIds make_sysmodule_ids() {
    SysmoduleIds ids{};
    ids[SCE_SYSMODULE_NET] = { 0x1, net_modules };
    ids[SCE_SYSMODULE_HTTP] = { 0x1, http_modules };
    ids[SCE_SYSMODULE_SSL] = { 0x1, ssl_modules };
    ids[SCE_SYSMODULE_HTTPS] = { 0x1, https_modules };
    ids[SCE_SYSMODULE_PERF] = { 0x21, perf_modules };
    ids[SCE_SYSMODULE_FIBER] = { 0x1, fiber_modules };
    ids[SCE_SYSMODULE_ULT] = { 0x1, ult_modules };
    ids[SCE_SYSMODULE_DBG] = { 0x1, {} };
    ids[SCE_SYSMODULE_RAZOR_CAPTURE] = { 0x21, razor_capture_modules };
    ids[SCE_SYSMODULE_RAZOR_HUD] = { 0x21, razor_hud_modules };
    ids[SCE_SYSMODULE_NGS] = { 0x1, ngs_modules };
    ids[SCE_SYSMODULE_SULPHA] = { 0x21, sulpha_modules };
    ids[SCE_SYSMODULE_SAS] = { 0x1, sas_modules };
    ids[SCE_SYSMODULE_PGF] = { 0x1, pgf_modules };
    ids[SCE_SYSMODULE_APPUTIL] = { 0x1, {} };
    ids[SCE_SYSMODULE_FIOS2] = { 0x1, {} };
    ids[SCE_SYSMODULE_IME] = { 0x1, ime_modules };
    ids[SCE_SYSMODULE_NP_BASIC] = { 0x1, np_basic_modules };
    ids[SCE_SYSMODULE_SYSTEM_GESTURE] = { 0x1, system_gesture_modules };
    ids[SCE_SYSMODULE_LOCATION] = { 0x1, location_modules };
    ids[SCE_SYSMODULE_NP] = { 0x1, np_modules };
    ids[SCE_SYSMODULE_PHOTO_EXPORT] = { 0x1, photo_export_modules };
    ids[SCE_SYSMODULE_XML] = { 0x1, xml_modules };
    ids[SCE_SYSMODULE_NP_COMMERCE2] = { 0x1, np_commerce2_modules };
    ids[SCE_SYSMODULE_NP_UTILITY] = { 0x1, np_utility_modules };
    ids[SCE_SYSMODULE_VOICE] = { 0x1, voice_modules };
    ids[SCE_SYSMODULE_VOICEQOS] = { 0x1, voiceqos_modules };
    ids[SCE_SYSMODULE_NP_MATCHING2] = { 0x1, np_matching2_modules };
    ids[SCE_SYSMODULE_SCREEN_SHOT] = { 0x1, screen_shot_modules };
    ids[SCE_SYSMODULE_NP_SCORE_RANKING] = { 0x1, np_score_ranking_modules };
    ids[SCE_SYSMODULE_SQLITE] = { 0x1, sqlite_modules };
    ids[SCE_SYSMODULE_TRIGGER_UTIL] = { 0x4, trigger_util_modules };
    ids[SCE_SYSMODULE_RUDP] = { 0x1, rudp_modules };
    ids[SCE_SYSMODULE_CODECENGINE_PERF] = { 0x21, codecengine_perf_modules };
    ids[SCE_SYSMODULE_LIVEAREA] = { 0x1, livearea_modules };
    ids[SCE_SYSMODULE_NP_ACTIVITY] = { 0x1, np_activity_modules };
    ids[SCE_SYSMODULE_NP_TROPHY] = { 0x1, np_trophy_modules };
    ids[SCE_SYSMODULE_NP_MESSAGE] = { 0x1, np_message_modules };
    ids[SCE_SYSMODULE_SHUTTER_SOUND] = { 0x1, shutter_sound_modules };
    ids[SCE_SYSMODULE_CLIPBOARD] = { 0x1, clipboard_modules };
    ids[SCE_SYSMODULE_NP_PARTY] = { 0x1, np_party_modules };
    ids[SCE_SYSMODULE_NET_ADHOC_MATCHING] = { 0x1, net_adhoc_matching_modules };
    ids[SCE_SYSMODULE_NEAR_UTIL] = { 0x1, near_util_modules };
    ids[SCE_SYSMODULE_NP_TUS] = { 0x1, np_tus_modules };
    ids[SCE_SYSMODULE_MP4] = { 0x1, mp4_modules };
    ids[SCE_SYSMODULE_AACENC] = { 0x1, aacenc_modules };
    ids[SCE_SYSMODULE_HANDWRITING] = { 0x1, handwriting_modules };
    ids[SCE_SYSMODULE_ATRAC] = { 0x1, atrac_modules };
    ids[SCE_SYSMODULE_NP_SNS_FACEBOOK] = { 0x1, np_sns_facebook_modules };
    ids[SCE_SYSMODULE_VIDEO_EXPORT] = { 0x4, video_export_modules };
    ids[SCE_SYSMODULE_NOTIFICATION_UTIL] = { 0x1, notification_util_modules };
    ids[SCE_SYSMODULE_BG_APP_UTIL] = { 0x4, bg_app_util_modules };
    ids[SCE_SYSMODULE_INCOMING_DIALOG] = { 0x4, incoming_dialog_modules };
    ids[SCE_SYSMODULE_IPMI] = { 0x4, ipmi_modules };
    ids[SCE_SYSMODULE_AUDIOCODEC] = { 0x4, audiocodec_modules };
    ids[SCE_SYSMODULE_FACE] = { 0x1, face_modules };
    ids[SCE_SYSMODULE_SMART] = { 0x1, smart_modules };
    ids[SCE_SYSMODULE_MARLIN] = { 0x4, marlin_modules };
    ids[SCE_SYSMODULE_MARLIN_DOWNLOADER] = { 0x4, marlin_downloader_modules };
    ids[SCE_SYSMODULE_MARLIN_APP_LIB] = { 0x4, marlin_app_lib_modules };
    ids[SCE_SYSMODULE_TELEPHONY_UTIL] = { 0x4, telephony_util_modules };
    ids[SCE_SYSMODULE_SHACCCG] = { 0x8, shacccg_modules };
    ids[SCE_SYSMODULE_MONO_BRIDGE] = { 0x8, mono_bridge_modules };
    ids[SCE_SYSMODULE_MONO] = { 0x8, mono_modules };
    ids[SCE_SYSMODULE_PSM] = { 0x8, psm_modules };
    ids[SCE_SYSMODULE_PSM_DEVAGENT] = { 0x8, psm_devagent_modules };
    ids[SCE_SYSMODULE_PSPNET_ADHOC] = { 0x1, pspnet_adhoc_modules };
    ids[SCE_SYSMODULE_DTCP_IP] = { 0x4, dtcp_ip_modules };
    ids[SCE_SYSMODULE_VIDEO_SEARCH_EMPR] = { 0x4, video_search_empr_modules };
    ids[SCE_SYSMODULE_NP_SIGNALING] = { 0x1, np_signaling_modules };
    ids[SCE_SYSMODULE_BEISOBMF] = { 0x1, beisobmf_modules };
    ids[SCE_SYSMODULE_BEMP2SYS] = { 0x1, bemp2sys_modules };
    ids[SCE_SYSMODULE_MUSIC_EXPORT] = { 0x1, music_export_modules };
    ids[SCE_SYSMODULE_NEAR_DIALOG_UTIL] = { 0x1, near_dialog_util_modules };
    ids[SCE_SYSMODULE_LOCATION_EXTENSION] = { 0x1, location_extension_modules };
    ids[SCE_SYSMODULE_AVPLAYER] = { 0x1, avplayer_modules };
    ids[SCE_SYSMODULE_GAME_UPDATE] = { 0x1, game_update_modules };
    ids[SCE_SYSMODULE_MAIL_API] = { 0x4, mail_api_modules };
    ids[SCE_SYSMODULE_TELEPORT_CLIENT] = { 0x1, teleport_client_modules };
    ids[SCE_SYSMODULE_TELEPORT_SERVER] = { 0x1, teleport_server_modules };
    ids[SCE_SYSMODULE_MP4_RECORDER] = { 0x1, mp4_recorder_modules };
    ids[SCE_SYSMODULE_APPUTIL_EXT] = { 0x1, apputil_ext_modules };
    ids[SCE_SYSMODULE_NP_WEBAPI] = { 0x1, np_webapi_modules };
    ids[SCE_SYSMODULE_AVCDEC] = { 0x4, avcdec_modules };
    ids[SCE_SYSMODULE_JSON] = { 0x1, json_modules };
    return ids;
}

constinit const SysmoduleIds sysmodule_ids = make_sysmodule_ids();

static constexpr uint8_t jpeg_enc_arm_internal_modules[] = { 88 }; // libscejpegencarm
static constexpr uint8_t audiocodec_internal_modules[] = { 56 }; // audiocodec
static constexpr uint8_t jpeg_arm_internal_modules[] = { 89 }; // libscejpegarm
static constexpr uint8_t g729_internal_modules[] = { 90 }; // libg729
static constexpr uint8_t bxce_internal_modules[] = { 91 }; // bXCe
static constexpr uint8_t ini_file_processor_internal_modules[] = { 92 }; // ini_file_processor
static constexpr uint8_t np_activity_net_internal_modules[] = { 93 }; // np_activity
static constexpr uint8_t paf_internal_modules[] = { 94 }; // libpaf
static constexpr uint8_t sqlite_vsh_internal_modules[] = { 95 }; // sqlite
static constexpr uint8_t dbutil_internal_modules[] = { 96 }; // dbutil
static constexpr uint8_t activity_db_internal_modules[] = { 97 }; // activity_db
static constexpr uint8_t common_gui_dialog_internal_modules[] = { 98 }; // common_gui_dialog
static constexpr uint8_t store_checkout_internal_modules[] = { 99 }; // store_checkout_plugin
static constexpr uint8_t ime_dialog_internal_modules[] = { 100 }; // libcdlg_ime
static constexpr uint8_t photo_import_dialog_internal_modules[] = { 101 }; // libcdlg_photoimport
static constexpr uint8_t photo_review_dialog_internal_modules[] = { 102 }; // libcdlg_photoreview
static constexpr uint8_t checkout_dialog_internal_modules[] = { 103 }; // libcdlg_checkout
static constexpr uint8_t common_dialog_main_internal_modules[] = { 104 }; // libcdlg_main
static constexpr uint8_t msg_dialog_internal_modules[] = { 105 }; // libcdlg_msg
static constexpr uint8_t net_check_dialog_internal_modules[] = { 106 }; // libcdlg_netcheck
static constexpr uint8_t savedata_dialog_internal_modules[] = { 107 }; // libcdlg_savedata
static constexpr uint8_t np_message_dialog_internal_modules[] = { 108 }; // libcdlg_np_message
static constexpr uint8_t trophy_setup_dialog_internal_modules[] = { 109 }; // libcdlg_np_trophy_setup
static constexpr uint8_t friend_list_dialog_internal_modules[] = { 110 }; // libcdlg_friendlist
static constexpr uint8_t internal_19_modules[] = { 111 }; // libcdlg_npprofile
static constexpr uint8_t near_profile_internal_modules[] = { 112 }; // near_profile
static constexpr uint8_t np_friend_privacy_level_internal_modules[] = { 113 }; // np_friend_privacylevel
static constexpr uint8_t internal_1c_modules[] = { 114 }; // libpaf_debug
static constexpr uint8_t np_commerce2_internal_modules[] = { 115 }; // np_commerce2
static constexpr uint8_t np_kdc_internal_modules[] = { 116 }; // np_kdc
static constexpr uint8_t music_export_internal_modules[] = { 74 }; // libSceMusicExport
static constexpr uint8_t video_export_internal_modules[] = { 51 }; // libSceVideoExport
static constexpr uint8_t np_message_dialog_impl_internal_modules[] = { 117 }; // np_message_dialog_impl
static constexpr uint8_t np_message_contacts_internal_modules[] = { 118 }; // np_message_contacts
static constexpr uint8_t db_recovery_utility_internal_modules[] = { 119 }; // dbrecovery_utility
static constexpr uint8_t promoter_util_internal_modules[] = { 120 }; // libScePromoterUtil
static constexpr uint8_t ult_internal_modules[] = { 9, 7 }; // libfiber, libult
static constexpr uint8_t party_member_list_internal_modules[] = { 121 }; // party_member_list
static constexpr uint8_t drm_psm_kdc_internal_modules[] = { 122 }; // psmkdc
static constexpr uint8_t location_internal_internal_modules[] = { 123 }; // liblocation_internal
static constexpr uint8_t location_factory_internal_modules[] = { 124 }; // liblocation_factory

static constexpr SysmoduleInternalIds make_sysmodule_internal_ids() {
    SysmoduleInternalIds ids{};
    ids[0x01] = { 0x42, jpeg_enc_arm_internal_modules }; // SCE_SYSMODULE_INTERNAL_JPEG_ENC_ARM
    ids[0x02] = { 0x42, audiocodec_internal_modules }; // SCE_SYSMODULE_INTERNAL_AUDIOCODEC
    ids[0x03] = { 0x42, jpeg_arm_internal_modules }; // SCE_SYSMODULE_INTERNAL_JPEG_ARM
    ids[0x04] = { 0x42, g729_internal_modules }; // SCE_SYSMODULE_INTERNAL_G729
    ids[0x05] = { 0x42, bxce_internal_modules }; // SCE_SYSMODULE_INTERNAL_BXCE
    ids[0x06] = { 0x42, ini_file_processor_internal_modules }; // SCE_SYSMODULE_INTERNAL_INI_FILE_PROCESSOR
    ids[0x07] = { 0x42, np_activity_net_internal_modules }; // SCE_SYSMODULE_INTERNAL_NP_ACTIVITY_NET
    ids[0x08] = { 0x42, paf_internal_modules }; // SCE_SYSMODULE_INTERNAL_PAF
    ids[0x09] = { 0x42, sqlite_vsh_internal_modules }; // SCE_SYSMODULE_INTERNAL_SQLITE_VSH
    ids[0x0A] = { 0x42, dbutil_internal_modules }; // SCE_SYSMODULE_INTERNAL_DBUTIL
    ids[0x0B] = { 0x42, activity_db_internal_modules }; // SCE_SYSMODULE_INTERNAL_ACTIVITY_DB
    ids[0x0C] = { 0x42, common_gui_dialog_internal_modules }; // SCE_SYSMODULE_INTERNAL_COMMON_GUI_DIALOG
    ids[0x0D] = { 0x42, store_checkout_internal_modules }; // SCE_SYSMODULE_INTERNAL_STORE_CHECKOUT
    ids[0x0E] = { 0x42, ime_dialog_internal_modules }; // SCE_SYSMODULE_INTERNAL_IME_DIALOG
    ids[0x0F] = { 0x42, photo_import_dialog_internal_modules }; // SCE_SYSMODULE_INTERNAL_PHOTO_IMPORT_DIALOG
    ids[0x10] = { 0x42, photo_review_dialog_internal_modules }; // SCE_SYSMODULE_INTERNAL_PHOTO_REVIEW_DIALOG
    ids[0x11] = { 0x42, checkout_dialog_internal_modules }; // SCE_SYSMODULE_INTERNAL_CHECKOUT_DIALOG
    ids[0x12] = { 0x42, common_dialog_main_internal_modules }; // SCE_SYSMODULE_INTERNAL_COMMON_DIALOG_MAIN
    ids[0x13] = { 0x42, msg_dialog_internal_modules }; // SCE_SYSMODULE_INTERNAL_MSG_DIALOG
    ids[0x14] = { 0x42, net_check_dialog_internal_modules }; // SCE_SYSMODULE_INTERNAL_NET_CHECK_DIALOG
    ids[0x15] = { 0x42, savedata_dialog_internal_modules }; // SCE_SYSMODULE_INTERNAL_SAVEDATA_DIALOG
    ids[0x16] = { 0x42, np_message_dialog_internal_modules }; // SCE_SYSMODULE_INTERNAL_NP_MESSAGE_DIALOG
    ids[0x17] = { 0x42, trophy_setup_dialog_internal_modules }; // SCE_SYSMODULE_INTERNAL_TROPHY_SETUP_DIALOG
    ids[0x18] = { 0x42, friend_list_dialog_internal_modules }; // SCE_SYSMODULE_INTERNAL_FRIEND_LIST_DIALOG
    ids[0x19] = { 0x42, internal_19_modules };
    ids[0x1A] = { 0x42, near_profile_internal_modules }; // SCE_SYSMODULE_INTERNAL_NEAR_PROFILE
    ids[0x1B] = { 0x42, np_friend_privacy_level_internal_modules }; // SCE_SYSMODULE_INTERNAL_NP_FRIEND_PRIVACY_LEVEL
    ids[0x1C] = { 0x42, internal_1c_modules };
    ids[0x1D] = { 0x42, np_commerce2_internal_modules }; // SCE_SYSMODULE_INTERNAL_NP_COMMERCE2
    ids[0x1E] = { 0x42, np_kdc_internal_modules }; // SCE_SYSMODULE_INTERNAL_NP_KDC
    ids[0x1F] = { 0x42, music_export_internal_modules }; // SCE_SYSMODULE_INTERNAL_MUSIC_EXPORT
    ids[0x20] = { 0x42, video_export_internal_modules }; // SCE_SYSMODULE_INTERNAL_VIDEO_EXPORT
    ids[0x21] = { 0x42, np_message_dialog_impl_internal_modules }; // SCE_SYSMODULE_INTERNAL_NP_MESSAGE_DIALOG_IMPL
    ids[0x22] = { 0x42, np_message_contacts_internal_modules }; // SCE_SYSMODULE_INTERNAL_NP_MESSAGE_CONTACTS
    ids[0x23] = { 0x42, db_recovery_utility_internal_modules }; // SCE_SYSMODULE_INTERNAL_DB_RECOVERY_UTILITY
    ids[0x24] = { 0x42, promoter_util_internal_modules }; // SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL
    ids[0x25] = { 0x2, ult_internal_modules }; // SCE_SYSMODULE_INTERNAL_ULT
    ids[0x26] = { 0x42, party_member_list_internal_modules }; // SCE_SYSMODULE_INTERNAL_PARTY_MEMBER_LIST
    ids[0x27] = { 0x42, drm_psm_kdc_internal_modules }; // SCE_SYSMODULE_INTERNAL_DRM_PSM_KDC
    ids[0x28] = { 0x42, location_internal_internal_modules }; // SCE_SYSMODULE_INTERNAL_LOCATION_INTERNAL
    ids[0x29] = { 0x42, location_factory_internal_modules }; // SCE_SYSMODULE_INTERNAL_LOCATION_FACTORY
    return ids;
}

constinit const SysmoduleInternalIds sysmodule_internal_ids = make_sysmodule_internal_ids();

// Current modules works for loading
static constexpr auto auto_lle_modules = {
    SCE_SYSMODULE_HTTP,
    SCE_SYSMODULE_SSL,
    SCE_SYSMODULE_HTTPS,
    SCE_SYSMODULE_FIBER,
    SCE_SYSMODULE_ULT,
    SCE_SYSMODULE_SAS,
    SCE_SYSMODULE_PGF,
    SCE_SYSMODULE_FIOS2,
    SCE_SYSMODULE_SYSTEM_GESTURE,
    SCE_SYSMODULE_XML,
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
    const SysmoduleId &id = sysmodule_ids[module_id];

    // Do we know the module and its dependencies?
    if (!id.modules.empty()) {
        if (emuenv.cfg.current_config.modules_mode != ModulesMode::MANUAL) {
            if (std::ranges::contains(auto_lle_modules, module_id))
                return true;
        }

        if (emuenv.cfg.current_config.modules_mode != ModulesMode::AUTOMATIC) {
            for (const uint8_t index : id.modules) {
                if (std::ranges::contains(emuenv.cfg.current_config.lle_modules, sysmodule_modules[index].name))
                    return true;
            }
        }
    }

    return false;
}

// The modules of auto_lle_modules that run LLE, the other modules those ids load stay HLE
static constexpr auto auto_lle_module_names = {
    "libc", "libSceFt2", "libpvf", "libfiber", "libhttp", "libssl", "libult", "libsas", "libpgf", "libfios2",
    "libsystemgesture", "libSceXml", "libSceSqlite", "librudp", "adhoc_matching", "libscemp4", "libatrac", "libface",
    "libsmart", "libsceavplayer", "libSceJson"
};

bool is_lle_module(const std::string &module_name, EmuEnvState &emuenv) {
    if (emuenv.cfg.current_config.modules_mode != ModulesMode::AUTOMATIC) {
        if (std::ranges::contains(emuenv.cfg.current_config.lle_modules, module_name))
            return true;
    }
    if (emuenv.cfg.current_config.modules_mode != ModulesMode::MANUAL) {
        if (std::ranges::contains(auto_lle_module_names, std::string_view(module_name)))
            return true;
    }
    return false;
}
