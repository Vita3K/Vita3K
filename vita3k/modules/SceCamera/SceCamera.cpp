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

BOOST_DESCRIBE_ENUM(SceCameraDevice, SCE_CAMERA_DEVICE_FRONT, SCE_CAMERA_DEVICE_BACK, SCE_CAMERA_DEVICE_UNKNOWN)
BOOST_DESCRIBE_ENUM(SceCameraPriority, SCE_CAMERA_PRIORITY_SHARE, SCE_CAMERA_PRIORITY_EXCLUSIVE)
BOOST_DESCRIBE_ENUM(SceCameraFormat, SCE_CAMERA_FORMAT_INVALID, SCE_CAMERA_FORMAT_YUV422_PLANE, SCE_CAMERA_FORMAT_YUV422_PACKED, SCE_CAMERA_FORMAT_YUV420_PLANE, SCE_CAMERA_FORMAT_ARGB, SCE_CAMERA_FORMAT_ABGR, SCE_CAMERA_FORMAT_RAW8)
BOOST_DESCRIBE_ENUM(SceCameraResolution, SCE_CAMERA_RESOLUTION_0_0, SCE_CAMERA_RESOLUTION_640_480, SCE_CAMERA_RESOLUTION_320_240, SCE_CAMERA_RESOLUTION_160_120, SCE_CAMERA_RESOLUTION_352_288, SCE_CAMERA_RESOLUTION_176_144, SCE_CAMERA_RESOLUTION_480_272, SCE_CAMERA_RESOLUTION_640_360)
BOOST_DESCRIBE_ENUM(SceCameraFrameRate, SCE_CAMERA_FRAMERATE_3_FPS, SCE_CAMERA_FRAMERATE_5_FPS, SCE_CAMERA_FRAMERATE_7_FPS, SCE_CAMERA_FRAMERATE_10_FPS, SCE_CAMERA_FRAMERATE_15_FPS, SCE_CAMERA_FRAMERATE_20_FPS, SCE_CAMERA_FRAMERATE_30_FPS, SCE_CAMERA_FRAMERATE_60_FPS, SCE_CAMERA_FRAMERATE_120_FPS)
BOOST_DESCRIBE_ENUM(SceCameraExposureCompensation, SCE_CAMERA_EV_NEGATIVE_20, SCE_CAMERA_EV_NEGATIVE_17, SCE_CAMERA_EV_NEGATIVE_15, SCE_CAMERA_EV_NEGATIVE_13, SCE_CAMERA_EV_NEGATIVE_10, SCE_CAMERA_EV_NEGATIVE_7, SCE_CAMERA_EV_NEGATIVE_5, SCE_CAMERA_EV_NEGATIVE_3, SCE_CAMERA_EV_POSITIVE_0, SCE_CAMERA_EV_POSITIVE_3, SCE_CAMERA_EV_POSITIVE_5, SCE_CAMERA_EV_POSITIVE_7, SCE_CAMERA_EV_POSITIVE_10, SCE_CAMERA_EV_POSITIVE_13, SCE_CAMERA_EV_POSITIVE_15, SCE_CAMERA_EV_POSITIVE_17, SCE_CAMERA_EV_POSITIVE_20)
BOOST_DESCRIBE_ENUM(SceCameraEffect, SCE_CAMERA_EFFECT_NORMAL, SCE_CAMERA_EFFECT_NEGATIVE, SCE_CAMERA_EFFECT_BLACKWHITE, SCE_CAMERA_EFFECT_SEPIA, SCE_CAMERA_EFFECT_BLUE, SCE_CAMERA_EFFECT_RED, SCE_CAMERA_EFFECT_GREEN)
BOOST_DESCRIBE_ENUM(SceCameraReverse, SCE_CAMERA_REVERSE_OFF, SCE_CAMERA_REVERSE_MIRROR, SCE_CAMERA_REVERSE_FLIP, SCE_CAMERA_REVERSE_MIRROR_FLIP)
BOOST_DESCRIBE_ENUM(SceCameraSaturation, SCE_CAMERA_SATURATION_0, SCE_CAMERA_SATURATION_5, SCE_CAMERA_SATURATION_10, SCE_CAMERA_SATURATION_20, SCE_CAMERA_SATURATION_30, SCE_CAMERA_SATURATION_40)
BOOST_DESCRIBE_ENUM(SceCameraSharpness, SCE_CAMERA_SHARPNESS_100, SCE_CAMERA_SHARPNESS_200, SCE_CAMERA_SHARPNESS_300, SCE_CAMERA_SHARPNESS_400)
BOOST_DESCRIBE_ENUM(SceCameraAntiFlicker, SCE_CAMERA_ANTIFLICKER_AUTO, SCE_CAMERA_ANTIFLICKER_50HZ, SCE_CAMERA_ANTIFLICKER_60HZ)
BOOST_DESCRIBE_ENUM(SceCameraISO, SCE_CAMERA_ISO_AUTO, SCE_CAMERA_ISO_100, SCE_CAMERA_ISO_200, SCE_CAMERA_ISO_400)
BOOST_DESCRIBE_ENUM(SceCameraGain, SCE_CAMERA_GAIN_AUTO, SCE_CAMERA_GAIN_1, SCE_CAMERA_GAIN_2, SCE_CAMERA_GAIN_3, SCE_CAMERA_GAIN_4, SCE_CAMERA_GAIN_5, SCE_CAMERA_GAIN_6, SCE_CAMERA_GAIN_7, SCE_CAMERA_GAIN_8, SCE_CAMERA_GAIN_9, SCE_CAMERA_GAIN_10, SCE_CAMERA_GAIN_11, SCE_CAMERA_GAIN_12, SCE_CAMERA_GAIN_13, SCE_CAMERA_GAIN_14, SCE_CAMERA_GAIN_15, SCE_CAMERA_GAIN_16)
BOOST_DESCRIBE_ENUM(SceCameraWhiteBalance, SCE_CAMERA_WB_AUTO, SCE_CAMERA_WB_DAY, SCE_CAMERA_WB_CWF, SCE_CAMERA_WB_SLSA)
BOOST_DESCRIBE_ENUM(SceCameraBacklight, SCE_CAMERA_BACKLIGHT_OFF, SCE_CAMERA_BACKLIGHT_ON)
BOOST_DESCRIBE_ENUM(SceCameraNightmode, SCE_CAMERA_NIGHTMODE_OFF, SCE_CAMERA_NIGHTMODE_LESS10, SCE_CAMERA_NIGHTMODE_LESS100, SCE_CAMERA_NIGHTMODE_OVER100)
BOOST_DESCRIBE_ENUM(SceCameraBuffer, SCE_CAMERA_BUFFER_SETBYOPEN, SCE_CAMERA_BUFFER_SETBYREAD)
BOOST_DESCRIBE_ENUM(SceCameraReadMode, SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_ON, SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_OFF)
BOOST_DESCRIBE_ENUM(SceCameraReadGetExposureTime, SCE_CAMERA_READ_GET_EXPOSURE_TIME_OFF, SCE_CAMERA_READ_GET_EXPOSURE_TIME_ON)
BOOST_DESCRIBE_ENUM(SceCameraStatus, SCE_CAMERA_STATUS_IS_ACTIVE, SCE_CAMERA_STATUS_IS_NOT_ACTIVE, SCE_CAMERA_STATUS_IS_FORCED_STOP, SCE_CAMERA_STATUS_IS_FORCED_STOP_POWER_CONFIG_CHANGE, SCE_CAMERA_STATUS_IS_ALREADY_READ, SCE_CAMERA_STATUS_IS_NOT_STABLE)
BOOST_DESCRIBE_ENUM(SceCameraDataRange, SCE_CAMERA_DATA_RANGE_FULL, SCE_CAMERA_DATA_RANGE_BT601)

EXPORT(int, sceCameraOpen, SceCameraDevice devnum, SceCameraInfo *pInfo) {
    TRACY_FUNC(sceCameraOpen, devnum, pInfo);
    if (emuenv.cfg.pstv_mode)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_MOUNTED);
    if (devnum != SCE_CAMERA_DEVICE_BACK && devnum != SCE_CAMERA_DEVICE_FRONT)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    if (!pInfo)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    auto &camera = emuenv.camera.cameras[devnum];
    if (camera.is_opened) {
        return RET_ERROR(SCE_CAMERA_ERROR_ALREADY_OPEN);
    }
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
return RET_ERROR(SCE_CAMERA_ERROR_PARAM);        break;
    }
    camera.base_ticks = emuenv.kernel.base_tick.tick;
    if (devnum == SCE_CAMERA_DEVICE_FRONT)
        camera.update_config(emuenv.cfg.front_camera_type, emuenv.cfg.front_camera_id, emuenv.cfg.front_camera_image, emuenv.cfg.front_camera_color);
    else
        camera.update_config(emuenv.cfg.back_camera_type, emuenv.cfg.back_camera_id, emuenv.cfg.back_camera_image, emuenv.cfg.back_camera_color);
    auto res = camera.open(pInfo);

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
