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

#include "camera/state.h"

#include <camera/camera.h>
#include <kernel/state.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceCamera);

template <typename T>
std::string_view enum_name(T value);

template <>
std::string_view enum_name<SceCameraDevice>(SceCameraDevice value) {
    switch (value) {
    case SCE_CAMERA_DEVICE_FRONT: return "SCE_CAMERA_DEVICE_FRONT";
    case SCE_CAMERA_DEVICE_BACK: return "SCE_CAMERA_DEVICE_BACK";
    case SCE_CAMERA_DEVICE_UNKNOWN: return "SCE_CAMERA_DEVICE_UNKNOWN";
    }
    return {};
}
template <>
std::string_view enum_name<SceCameraPriority>(SceCameraPriority value) {
    switch (value) {
    case SCE_CAMERA_PRIORITY_SHARE: return "SCE_CAMERA_PRIORITY_SHARE";
    case SCE_CAMERA_PRIORITY_EXCLUSIVE: return "SCE_CAMERA_PRIORITY_EXCLUSIVE";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraFormat>(SceCameraFormat value) {
    switch (value) {
    case SCE_CAMERA_FORMAT_INVALID: return "SCE_CAMERA_FORMAT_INVALID";
    case SCE_CAMERA_FORMAT_YUV422_PLANE: return "SCE_CAMERA_FORMAT_YUV422_PLANE";
    case SCE_CAMERA_FORMAT_YUV422_PACKED: return "SCE_CAMERA_FORMAT_YUV422_PACKED";
    case SCE_CAMERA_FORMAT_YUV420_PLANE: return "SCE_CAMERA_FORMAT_YUV420_PLANE";
    case SCE_CAMERA_FORMAT_ARGB: return "SCE_CAMERA_FORMAT_ARGB";
    case SCE_CAMERA_FORMAT_ABGR: return "SCE_CAMERA_FORMAT_ABGR";
    case SCE_CAMERA_FORMAT_RAW8: return "SCE_CAMERA_FORMAT_RAW8";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraResolution>(SceCameraResolution value) {
    switch (value) {
    case SCE_CAMERA_RESOLUTION_0_0: return "SCE_CAMERA_RESOLUTION_0_0";
    case SCE_CAMERA_RESOLUTION_640_480: return "SCE_CAMERA_RESOLUTION_640_480";
    case SCE_CAMERA_RESOLUTION_320_240: return "SCE_CAMERA_RESOLUTION_320_240";
    case SCE_CAMERA_RESOLUTION_160_120: return "SCE_CAMERA_RESOLUTION_160_120";
    case SCE_CAMERA_RESOLUTION_352_288: return "SCE_CAMERA_RESOLUTION_352_288";
    case SCE_CAMERA_RESOLUTION_176_144: return "SCE_CAMERA_RESOLUTION_176_144";
    case SCE_CAMERA_RESOLUTION_480_272: return "SCE_CAMERA_RESOLUTION_480_272";
    case SCE_CAMERA_RESOLUTION_640_360: return "SCE_CAMERA_RESOLUTION_640_360";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraFrameRate>(SceCameraFrameRate value) {
    switch (value) {
    case SCE_CAMERA_FRAMERATE_3_FPS: return "SCE_CAMERA_FRAMERATE_3_FPS";
    case SCE_CAMERA_FRAMERATE_5_FPS: return "SCE_CAMERA_FRAMERATE_5_FPS";
    case SCE_CAMERA_FRAMERATE_7_FPS: return "SCE_CAMERA_FRAMERATE_7_FPS";
    case SCE_CAMERA_FRAMERATE_10_FPS: return "SCE_CAMERA_FRAMERATE_10_FPS";
    case SCE_CAMERA_FRAMERATE_15_FPS: return "SCE_CAMERA_FRAMERATE_15_FPS";
    case SCE_CAMERA_FRAMERATE_20_FPS: return "SCE_CAMERA_FRAMERATE_20_FPS";
    case SCE_CAMERA_FRAMERATE_30_FPS: return "SCE_CAMERA_FRAMERATE_30_FPS";
    case SCE_CAMERA_FRAMERATE_60_FPS: return "SCE_CAMERA_FRAMERATE_60_FPS";
    case SCE_CAMERA_FRAMERATE_120_FPS: return "SCE_CAMERA_FRAMERATE_120_FPS";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraExposureCompensation>(SceCameraExposureCompensation value) {
    switch (value) {
    case SCE_CAMERA_EV_NEGATIVE_20: return "SCE_CAMERA_EV_NEGATIVE_20";
    case SCE_CAMERA_EV_NEGATIVE_17: return "SCE_CAMERA_EV_NEGATIVE_17";
    case SCE_CAMERA_EV_NEGATIVE_15: return "SCE_CAMERA_EV_NEGATIVE_15";
    case SCE_CAMERA_EV_NEGATIVE_13: return "SCE_CAMERA_EV_NEGATIVE_13";
    case SCE_CAMERA_EV_NEGATIVE_10: return "SCE_CAMERA_EV_NEGATIVE_10";
    case SCE_CAMERA_EV_NEGATIVE_7: return "SCE_CAMERA_EV_NEGATIVE_7";
    case SCE_CAMERA_EV_NEGATIVE_5: return "SCE_CAMERA_EV_NEGATIVE_5";
    case SCE_CAMERA_EV_NEGATIVE_3: return "SCE_CAMERA_EV_NEGATIVE_3";
    case SCE_CAMERA_EV_POSITIVE_0: return "SCE_CAMERA_EV_POSITIVE_0";
    case SCE_CAMERA_EV_POSITIVE_3: return "SCE_CAMERA_EV_POSITIVE_3";
    case SCE_CAMERA_EV_POSITIVE_5: return "SCE_CAMERA_EV_POSITIVE_5";
    case SCE_CAMERA_EV_POSITIVE_7: return "SCE_CAMERA_EV_POSITIVE_7";
    case SCE_CAMERA_EV_POSITIVE_10: return "SCE_CAMERA_EV_POSITIVE_10";
    case SCE_CAMERA_EV_POSITIVE_13: return "SCE_CAMERA_EV_POSITIVE_13";
    case SCE_CAMERA_EV_POSITIVE_15: return "SCE_CAMERA_EV_POSITIVE_15";
    case SCE_CAMERA_EV_POSITIVE_17: return "SCE_CAMERA_EV_POSITIVE_17";
    case SCE_CAMERA_EV_POSITIVE_20: return "SCE_CAMERA_EV_POSITIVE_20";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraEffect>(SceCameraEffect value) {
    switch (value) {
    case SCE_CAMERA_EFFECT_NORMAL: return "SCE_CAMERA_EFFECT_NORMAL";
    case SCE_CAMERA_EFFECT_NEGATIVE: return "SCE_CAMERA_EFFECT_NEGATIVE";
    case SCE_CAMERA_EFFECT_BLACKWHITE: return "SCE_CAMERA_EFFECT_BLACKWHITE";
    case SCE_CAMERA_EFFECT_SEPIA: return "SCE_CAMERA_EFFECT_SEPIA";
    case SCE_CAMERA_EFFECT_BLUE: return "SCE_CAMERA_EFFECT_BLUE";
    case SCE_CAMERA_EFFECT_RED: return "SCE_CAMERA_EFFECT_RED";
    case SCE_CAMERA_EFFECT_GREEN: return "SCE_CAMERA_EFFECT_GREEN";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraReverse>(SceCameraReverse value) {
    switch (value) {
    case SCE_CAMERA_REVERSE_OFF: return "SCE_CAMERA_REVERSE_OFF";
    case SCE_CAMERA_REVERSE_MIRROR: return "SCE_CAMERA_REVERSE_MIRROR";
    case SCE_CAMERA_REVERSE_FLIP: return "SCE_CAMERA_REVERSE_FLIP";
    case SCE_CAMERA_REVERSE_MIRROR_FLIP: return "SCE_CAMERA_REVERSE_MIRROR_FLIP";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraSaturation>(SceCameraSaturation value) {
    switch (value) {
    case SCE_CAMERA_SATURATION_0: return "SCE_CAMERA_SATURATION_0";
    case SCE_CAMERA_SATURATION_5: return "SCE_CAMERA_SATURATION_5";
    case SCE_CAMERA_SATURATION_10: return "SCE_CAMERA_SATURATION_10";
    case SCE_CAMERA_SATURATION_20: return "SCE_CAMERA_SATURATION_20";
    case SCE_CAMERA_SATURATION_30: return "SCE_CAMERA_SATURATION_30";
    case SCE_CAMERA_SATURATION_40: return "SCE_CAMERA_SATURATION_40";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraSharpness>(SceCameraSharpness value) {
    switch (value) {
    case SCE_CAMERA_SHARPNESS_100: return "SCE_CAMERA_SHARPNESS_100";
    case SCE_CAMERA_SHARPNESS_200: return "SCE_CAMERA_SHARPNESS_200";
    case SCE_CAMERA_SHARPNESS_300: return "SCE_CAMERA_SHARPNESS_300";
    case SCE_CAMERA_SHARPNESS_400: return "SCE_CAMERA_SHARPNESS_400";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraAntiFlicker>(SceCameraAntiFlicker value) {
    switch (value) {
    case SCE_CAMERA_ANTIFLICKER_AUTO: return "SCE_CAMERA_ANTIFLICKER_AUTO";
    case SCE_CAMERA_ANTIFLICKER_50HZ: return "SCE_CAMERA_ANTIFLICKER_50HZ";
    case SCE_CAMERA_ANTIFLICKER_60HZ: return "SCE_CAMERA_ANTIFLICKER_60HZ";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraISO>(SceCameraISO value) {
    switch (value) {
    case SCE_CAMERA_ISO_AUTO: return "SCE_CAMERA_ISO_AUTO";
    case SCE_CAMERA_ISO_100: return "SCE_CAMERA_ISO_100";
    case SCE_CAMERA_ISO_200: return "SCE_CAMERA_ISO_200";
    case SCE_CAMERA_ISO_400: return "SCE_CAMERA_ISO_400";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraGain>(SceCameraGain value) {
    switch (value) {
    case SCE_CAMERA_GAIN_AUTO: return "SCE_CAMERA_GAIN_AUTO";
    case SCE_CAMERA_GAIN_1: return "SCE_CAMERA_GAIN_1";
    case SCE_CAMERA_GAIN_2: return "SCE_CAMERA_GAIN_2";
    case SCE_CAMERA_GAIN_3: return "SCE_CAMERA_GAIN_3";
    case SCE_CAMERA_GAIN_4: return "SCE_CAMERA_GAIN_4";
    case SCE_CAMERA_GAIN_5: return "SCE_CAMERA_GAIN_5";
    case SCE_CAMERA_GAIN_6: return "SCE_CAMERA_GAIN_6";
    case SCE_CAMERA_GAIN_7: return "SCE_CAMERA_GAIN_7";
    case SCE_CAMERA_GAIN_8: return "SCE_CAMERA_GAIN_8";
    case SCE_CAMERA_GAIN_9: return "SCE_CAMERA_GAIN_9";
    case SCE_CAMERA_GAIN_10: return "SCE_CAMERA_GAIN_10";
    case SCE_CAMERA_GAIN_11: return "SCE_CAMERA_GAIN_11";
    case SCE_CAMERA_GAIN_12: return "SCE_CAMERA_GAIN_12";
    case SCE_CAMERA_GAIN_13: return "SCE_CAMERA_GAIN_13";
    case SCE_CAMERA_GAIN_14: return "SCE_CAMERA_GAIN_14";
    case SCE_CAMERA_GAIN_15: return "SCE_CAMERA_GAIN_15";
    case SCE_CAMERA_GAIN_16: return "SCE_CAMERA_GAIN_16";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraWhiteBalance>(SceCameraWhiteBalance value) {
    switch (value) {
    case SCE_CAMERA_WB_AUTO: return "SCE_CAMERA_WB_AUTO";
    case SCE_CAMERA_WB_DAY: return "SCE_CAMERA_WB_DAY";
    case SCE_CAMERA_WB_CWF: return "SCE_CAMERA_WB_CWF";
    case SCE_CAMERA_WB_SLSA: return "SCE_CAMERA_WB_SLSA";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraBacklight>(SceCameraBacklight value) {
    switch (value) {
    case SCE_CAMERA_BACKLIGHT_OFF: return "SCE_CAMERA_BACKLIGHT_OFF";
    case SCE_CAMERA_BACKLIGHT_ON: return "SCE_CAMERA_BACKLIGHT_ON";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraNightmode>(SceCameraNightmode value) {
    switch (value) {
    case SCE_CAMERA_NIGHTMODE_OFF: return "SCE_CAMERA_NIGHTMODE_OFF";
    case SCE_CAMERA_NIGHTMODE_LESS10: return "SCE_CAMERA_NIGHTMODE_LESS10";
    case SCE_CAMERA_NIGHTMODE_LESS100: return "SCE_CAMERA_NIGHTMODE_LESS100";
    case SCE_CAMERA_NIGHTMODE_OVER100: return "SCE_CAMERA_NIGHTMODE_OVER100";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraBuffer>(SceCameraBuffer value) {
    switch (value) {
    case SceCameraBuffer::SCE_CAMERA_BUFFER_SETBYOPEN: return "SCE_CAMERA_BUFFER_SETBYOPEN";
    case SceCameraBuffer::SCE_CAMERA_BUFFER_SETBYREAD: return "SCE_CAMERA_BUFFER_SETBYREAD";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraReadMode>(SceCameraReadMode value) {
    switch (value) {
    case SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_ON: return "SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_ON";
    case SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_OFF: return "SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_OFF";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraReadGetExposureTime>(SceCameraReadGetExposureTime value) {
    switch (value) {
    case SCE_CAMERA_READ_GET_EXPOSURE_TIME_OFF: return "SCE_CAMERA_READ_GET_EXPOSURE_TIME_OFF";
    case SCE_CAMERA_READ_GET_EXPOSURE_TIME_ON: return "SCE_CAMERA_READ_GET_EXPOSURE_TIME_ON";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraStatus>(SceCameraStatus value) {
    switch (value) {
    case SCE_CAMERA_STATUS_IS_ACTIVE: return "SCE_CAMERA_STATUS_IS_ACTIVE";
    case SCE_CAMERA_STATUS_IS_NOT_ACTIVE: return "SCE_CAMERA_STATUS_IS_NOT_ACTIVE";
    case SCE_CAMERA_STATUS_IS_FORCED_STOP: return "SCE_CAMERA_STATUS_IS_FORCED_STOP";
    case SCE_CAMERA_STATUS_IS_FORCED_STOP_POWER_CONFIG_CHANGE: return "SCE_CAMERA_STATUS_IS_FORCED_STOP_POWER_CONFIG_CHANGE";
    case SCE_CAMERA_STATUS_IS_ALREADY_READ: return "SCE_CAMERA_STATUS_IS_ALREADY_READ";
    case SCE_CAMERA_STATUS_IS_NOT_STABLE: return "SCE_CAMERA_STATUS_IS_NOT_STABLE";
    }
    return {};
}

template <>
std::string_view enum_name<SceCameraDataRange>(SceCameraDataRange value) {
    switch (value) {
    case SCE_CAMERA_DATA_RANGE_FULL: return "SCE_CAMERA_DATA_RANGE_FULL";
    case SCE_CAMERA_DATA_RANGE_BT601: return "SCE_CAMERA_DATA_RANGE_BT601";
    }
    return {};
}

template <typename T>
    requires std::is_enum_v<T>
static std::string to_debug_str(const MemState &mem, T type) {
    auto name = enum_name(type);
    if (name.empty())
        return std::to_string(static_cast<std::underlying_type_t<T>>(type));
    return std::string(name);
}

EXPORT(int, sceCameraOpen, SceCameraDevice devnum, SceCameraInfo *pInfo) {
    TRACY_FUNC(sceCameraOpen, devnum, pInfo);
    if (emuenv.cfg.pstv_mode)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_MOUNTED);
    if (devnum != SCE_CAMERA_DEVICE_BACK && devnum != SCE_CAMERA_DEVICE_FRONT)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    if (!pInfo)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    switch (pInfo->resolution) {
    case SCE_CAMERA_RESOLUTION_640_480:
        pInfo->width = 640;
        pInfo->height = 480;
        break;
    case SCE_CAMERA_RESOLUTION_320_240:
        pInfo->width = 320;
        pInfo->height = 240;
        break;
    case SCE_CAMERA_RESOLUTION_160_120:
        pInfo->width = 160;
        pInfo->height = 120;
        break;
    case SCE_CAMERA_RESOLUTION_352_288:
        pInfo->width = 352;
        pInfo->height = 288;
        break;
    case SCE_CAMERA_RESOLUTION_176_144:
        pInfo->width = 176;
        pInfo->height = 144;
        break;
    case SCE_CAMERA_RESOLUTION_480_272:
        pInfo->width = 480;
        pInfo->height = 272;
        break;
    case SCE_CAMERA_RESOLUTION_640_360:
        pInfo->width = 640;
        pInfo->height = 360;
        break;
    default:
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
        break;
    }
    auto &camera = emuenv.camera.cameras[devnum];
    if (camera.is_opened) {
        return RET_ERROR(SCE_CAMERA_ERROR_ALREADY_OPEN);
    }
    if (devnum == SCE_CAMERA_DEVICE_FRONT)
        camera.update_config(emuenv.cfg.front_camera_type, emuenv.cfg.front_camera_id, emuenv.cfg.front_camera_image, emuenv.cfg.front_camera_color);
    else
        camera.update_config(emuenv.cfg.back_camera_type, emuenv.cfg.back_camera_id, emuenv.cfg.back_camera_image, emuenv.cfg.back_camera_color);
    auto res = camera.open(pInfo, emuenv.kernel.base_tick.tick, emuenv.kernel.start_tick);

    return res;
}

EXPORT(int, sceCameraClose, SceCameraDevice devnum) {
    TRACY_FUNC(sceCameraClose, devnum);
    if (emuenv.cfg.pstv_mode)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_MOUNTED);
    if (devnum != SCE_CAMERA_DEVICE_BACK && devnum != SCE_CAMERA_DEVICE_FRONT)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    auto &camera = emuenv.camera.cameras[devnum];
    if (!camera.is_opened) {
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_OPEN);
    }
    return camera.close();
}

EXPORT(int, sceCameraStart, SceCameraDevice devnum) {
    TRACY_FUNC(sceCameraStart, devnum);
    if (emuenv.cfg.pstv_mode)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_MOUNTED);
    if (devnum != SCE_CAMERA_DEVICE_BACK && devnum != SCE_CAMERA_DEVICE_FRONT)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    auto &camera = emuenv.camera.cameras[devnum];
    if (!camera.is_opened) {
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_OPEN);
    }
    if (camera.is_started) {
        return RET_ERROR(SCE_CAMERA_ERROR_ALREADY_START);
    }
    return camera.start();
}

EXPORT(int, sceCameraStop, SceCameraDevice devnum) {
    TRACY_FUNC(sceCameraStop, devnum);
    if (emuenv.cfg.pstv_mode)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_MOUNTED);
    if (devnum != SCE_CAMERA_DEVICE_BACK && devnum != SCE_CAMERA_DEVICE_FRONT)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    auto &camera = emuenv.camera.cameras[devnum];
    if (!camera.is_opened) {
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_OPEN);
    }
    if (!camera.is_started) {
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_START);
    }
    return camera.stop();
}

EXPORT(int, sceCameraRead, SceCameraDevice devnum, SceCameraRead *pRead) {
    TRACY_FUNC(sceCameraRead, devnum, pRead);
    if (emuenv.cfg.pstv_mode)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_MOUNTED);
    if (devnum != SCE_CAMERA_DEVICE_BACK && devnum != SCE_CAMERA_DEVICE_FRONT)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    if (!pRead)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    if (pRead->size != sizeof(SceCameraRead))
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    auto &camera = emuenv.camera.cameras[devnum];
    if (!camera.is_opened)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_OPEN);
    if (!camera.is_started)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_START);
    void *pIBase;
    void *pUBase;
    void *pVBase;
    SceSize sizeIBase;
    SceSize sizeUBase;
    SceSize sizeVBase;
    if (camera.info.buffer == SCE_CAMERA_BUFFER_SETBYREAD) {
        pIBase = pRead->pIBase.get(emuenv.mem);
        pUBase = pRead->pUBase.get(emuenv.mem);
        pVBase = pRead->pVBase.get(emuenv.mem);
        sizeIBase = pRead->sizeIBase;
        sizeUBase = pRead->sizeUBase;
        sizeVBase = pRead->sizeVBase;
    } else {
        pIBase = camera.info.pIBase.get(emuenv.mem);
        pUBase = camera.info.pUBase.get(emuenv.mem);
        pVBase = camera.info.pVBase.get(emuenv.mem);
        sizeIBase = camera.info.sizeIBase;
        sizeUBase = camera.info.sizeUBase;
        sizeVBase = camera.info.sizeVBase;
    }
    auto res = camera.read(emuenv, pRead, pIBase, pUBase, pVBase, sizeIBase, sizeUBase, sizeVBase);
    return res;
}

EXPORT(int, sceCameraIsActive, SceCameraDevice devnum) {
    TRACY_FUNC(sceCameraIsActive, devnum);
    if (emuenv.cfg.pstv_mode)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_MOUNTED);
    if (devnum != SCE_CAMERA_DEVICE_BACK && devnum != SCE_CAMERA_DEVICE_FRONT)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    auto &camera = emuenv.camera.cameras[devnum];
    if (!camera.is_opened) {
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_OPEN);
    }
    if (!camera.is_started) {
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_START);
    }
    // 0 means camera is used by system and camera image cannot be obtained. 1 means that camera image can be obtained.
    return 1;
}

static int camera_get_attribute(EmuEnvState &emuenv, SceUID thread_id, const char *export_name, SceCameraDevice devnum, CameraAttributes attribute, int *pValue) {
    if (emuenv.cfg.pstv_mode)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_MOUNTED);
    if (devnum != SCE_CAMERA_DEVICE_BACK && devnum != SCE_CAMERA_DEVICE_FRONT)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    if (!pValue)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    auto &camera = emuenv.camera.cameras[devnum];
    if (!camera.is_opened)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_OPEN);
    if (!camera.is_started)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_START);

    *pValue = camera.get_attribute(attribute);
    return STUBBED("Default value");
}

static int camera_set_attribute(EmuEnvState &emuenv, SceUID thread_id, const char *export_name, SceCameraDevice devnum, CameraAttributes attribute, int value) {
    if (emuenv.cfg.pstv_mode)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_MOUNTED);
    if (devnum != SCE_CAMERA_DEVICE_BACK && devnum != SCE_CAMERA_DEVICE_FRONT)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    auto &camera = emuenv.camera.cameras[devnum];
    if (!camera.is_opened)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_OPEN);
    if (!camera.is_started)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_START);

    UNIMPLEMENTED();
    return camera.set_attribute(attribute, value);
}

EXPORT(int, sceCameraGetSaturation, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetSaturation, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetSaturation", devnum, CameraAttributes::Saturation, pValue);
}

EXPORT(int, sceCameraSetSaturation, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetSaturation, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetSaturation", devnum, CameraAttributes::Saturation, value);
}
EXPORT(int, sceCameraGetBrightness, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetBrightness, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetBrightness", devnum, CameraAttributes::Brightness, pValue);
}

EXPORT(int, sceCameraSetBrightness, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetBrightness, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetBrightness", devnum, CameraAttributes::Brightness, value);
}

EXPORT(int, sceCameraGetContrast, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetContrast, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetContrast", devnum, CameraAttributes::Contrast, pValue);
}

EXPORT(int, sceCameraSetContrast, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetContrast, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetContrast", devnum, CameraAttributes::Contrast, value);
}

EXPORT(int, sceCameraGetSharpness, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetSharpness, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetSharpness", devnum, CameraAttributes::Sharpness, pValue);
}

EXPORT(int, sceCameraSetSharpness, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetSharpness, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetSharpness", devnum, CameraAttributes::Sharpness, value);
}

EXPORT(int, sceCameraGetReverse, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetReverse, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetReverse", devnum, CameraAttributes::Reverse, pValue);
}

EXPORT(int, sceCameraSetReverse, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetReverse, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetReverse", devnum, CameraAttributes::Reverse, value);
}

EXPORT(int, sceCameraGetEffect, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetEffect, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetEffect", devnum, CameraAttributes::Effect, pValue);
}

EXPORT(int, sceCameraSetEffect, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetEffect, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetEffect", devnum, CameraAttributes::Effect, value);
}

EXPORT(int, sceCameraGetEV, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetEV, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetEV", devnum, CameraAttributes::EV, pValue);
}

EXPORT(int, sceCameraSetEV, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetEV, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetEV", devnum, CameraAttributes::EV, value);
}

EXPORT(int, sceCameraGetZoom, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetZoom, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetZoom", devnum, CameraAttributes::Zoom, pValue);
}

EXPORT(int, sceCameraSetZoom, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetZoom, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetZoom", devnum, CameraAttributes::Zoom, value);
}

EXPORT(int, sceCameraGetAntiFlicker, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetAntiFlicker, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetAntiFlicker", devnum, CameraAttributes::AntiFlicker, pValue);
}

EXPORT(int, sceCameraSetAntiFlicker, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetAntiFlicker, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetAntiFlicker", devnum, CameraAttributes::AntiFlicker, value);
}

EXPORT(int, sceCameraGetISO, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetISO, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetISO", devnum, CameraAttributes::ISO, pValue);
}

EXPORT(int, sceCameraSetISO, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetISO, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetISO", devnum, CameraAttributes::ISO, value);
}

EXPORT(int, sceCameraGetGain, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetGain, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetGain", devnum, CameraAttributes::Gain, pValue);
}

EXPORT(int, sceCameraSetGain, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetGain, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetGain", devnum, CameraAttributes::Gain, value);
}

EXPORT(int, sceCameraGetWhiteBalance, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetWhiteBalance, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetWhiteBalance", devnum, CameraAttributes::WhiteBalance, pValue);
}

EXPORT(int, sceCameraSetWhiteBalance, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetWhiteBalance, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetWhiteBalance", devnum, CameraAttributes::WhiteBalance, value);
}

EXPORT(int, sceCameraGetBacklight, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetBacklight, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetBacklight", devnum, CameraAttributes::Backlight, pValue);
}

EXPORT(int, sceCameraSetBacklight, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetBacklight, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetBacklight", devnum, CameraAttributes::Backlight, value);
}

EXPORT(int, sceCameraGetNightmode, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetNightmode, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetNightmode", devnum, CameraAttributes::Nightmode, pValue);
}

EXPORT(int, sceCameraSetNightmode, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetNightmode, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetNightmode", devnum, CameraAttributes::Nightmode, value);
}

EXPORT(int, sceCameraGetExposureCeiling, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetExposureCeiling, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetExposureCeiling", devnum, CameraAttributes::ExposureCeiling, pValue);
}

EXPORT(int, sceCameraSetExposureCeiling, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetExposureCeiling, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetExposureCeiling", devnum, CameraAttributes::ExposureCeiling, value);
}

EXPORT(int, sceCameraGetAutoControlHold, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetAutoControlHold, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetAutoControlHold", devnum, CameraAttributes::AutoControlHold, pValue);
}

EXPORT(int, sceCameraSetAutoControlHold, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetAutoControlHold, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetAutoControlHold", devnum, CameraAttributes::AutoControlHold, value);
}

EXPORT(int, sceCameraGetImageQuality, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetImageQuality, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetImageQuality", devnum, CameraAttributes::ImageQuality, pValue);
}

EXPORT(int, sceCameraSetImageQuality, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetImageQuality, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetImageQuality", devnum, CameraAttributes::ImageQuality, value);
}

EXPORT(int, sceCameraGetNoiseReduction, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetNoiseReduction, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetNoiseReduction", devnum, CameraAttributes::NoiseReduction, pValue);
}

EXPORT(int, sceCameraSetNoiseReduction, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetNoiseReduction, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetNoiseReduction", devnum, CameraAttributes::NoiseReduction, value);
}
EXPORT(int, sceCameraGetSharpnessOff, SceCameraDevice devnum, int *pValue) {
    TRACY_FUNC(sceCameraGetSharpnessOff, devnum, pValue);
    return camera_get_attribute(emuenv, thread_id, "sceCameraGetSharpnessOff", devnum, CameraAttributes::SharpnessOff, pValue);
}

EXPORT(int, sceCameraSetSharpnessOff, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetSharpnessOff, devnum, value);
    return camera_set_attribute(emuenv, thread_id, "sceCameraSetSharpnessOff", devnum, CameraAttributes::SharpnessOff, value);
}

EXPORT(int, sceCameraGetDeviceLocation, SceCameraDevice devnum, SceFVector3 *pLocation) {
    TRACY_FUNC(sceCameraGetDeviceLocation, devnum, pLocation);
    if (emuenv.cfg.pstv_mode)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_MOUNTED);
    if (devnum != SCE_CAMERA_DEVICE_BACK && devnum != SCE_CAMERA_DEVICE_FRONT)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    if (!pLocation)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    memset(pLocation, 0, sizeof(*pLocation));
    return UNIMPLEMENTED();
}
