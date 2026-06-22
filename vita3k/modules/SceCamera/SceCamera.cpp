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

#include <module/module.h>

#include "camera/state.h"

#include <camera/camera.h>
#include <kernel/state.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceCamera);

EXPORT(int, sceCameraOpen, SceCameraDevice devnum, SceCameraInfo *pInfo) {
    TRACY_FUNC(sceCameraOpen, devnum, pInfo);
    if (emuenv.cfg.pstv_mode)
        return RET_ERROR(SCE_CAMERA_ERROR_NOT_MOUNTED);
    if (devnum != SCE_CAMERA_DEVICE_BACK && devnum != SCE_CAMERA_DEVICE_FRONT)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    if (!pInfo)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);

    // Validate pInfo size
    // in disasmed code 40 - 48 is correct
    if (pInfo->size > sizeof(SceCameraInfo) || pInfo->size < sizeof(SceCameraInfo) - 4)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);

    // Validate priority
    if (pInfo->priority != SCE_CAMERA_PRIORITY_SHARE && pInfo->priority != SCE_CAMERA_PRIORITY_EXCLUSIVE)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);

    // Validate format
    if (pInfo->format == SCE_CAMERA_FORMAT_INVALID || pInfo->format > SCE_CAMERA_FORMAT_RAW8)
        return RET_ERROR(SCE_CAMERA_ERROR_FORMAT_UNKNOWN);

    // Validate framerate
    switch (pInfo->framerate) {
    case SCE_CAMERA_FRAMERATE_3_FPS:
    case SCE_CAMERA_FRAMERATE_5_FPS:
    case SCE_CAMERA_FRAMERATE_7_FPS:
    case SCE_CAMERA_FRAMERATE_10_FPS:
    case SCE_CAMERA_FRAMERATE_15_FPS:
    case SCE_CAMERA_FRAMERATE_20_FPS:
    case SCE_CAMERA_FRAMERATE_30_FPS:
    case SCE_CAMERA_FRAMERATE_60_FPS:
    case SCE_CAMERA_FRAMERATE_120_FPS:
        break;
    default:
        return RET_ERROR(SCE_CAMERA_ERROR_BAD_FRAMERATE);
    }

    // Validate buffer type
    if (pInfo->buffer != SCE_CAMERA_BUFFER_SETBYOPEN && pInfo->buffer != SCE_CAMERA_BUFFER_SETBYREAD)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);

    // Validate buffer pointers and sizes for SETBYOPEN mode
    if (pInfo->size < sizeof(SceCameraInfo) || pInfo->buffer == SCE_CAMERA_BUFFER_SETBYOPEN) {
        if (!pInfo->pIBase || pInfo->sizeIBase == 0)
            return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
        if (pInfo->format == SCE_CAMERA_FORMAT_YUV420_PLANE || pInfo->format == SCE_CAMERA_FORMAT_YUV422_PLANE) {
            if (!pInfo->pUBase || pInfo->sizeUBase == 0 || !pInfo->pVBase || pInfo->sizeVBase == 0)
                return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
        }
    }

    // Validate data range
    if (pInfo->range != SCE_CAMERA_DATA_RANGE_FULL && pInfo->range != SCE_CAMERA_DATA_RANGE_BT601)
        return RET_ERROR(SCE_CAMERA_ERROR_DATA_RANGE_UNKNOWN);

    // Validate resolution and set width/height
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
        return RET_ERROR(SCE_CAMERA_ERROR_RESOLUTION_UNKNOWN);
    }

    auto &camera = emuenv.camera.cameras[devnum];
    if (camera.is_opened) {
        return RET_ERROR(SCE_CAMERA_ERROR_ALREADY_OPEN);
    }
    init_default_cameras(emuenv.cfg);
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
    // conditions from disassembled code
    if (pRead->size <= 24 || pRead->size > sizeof(SceCameraRead)) {
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    }
    SceCameraRead read{};
    if (pRead->size != sizeof(SceCameraRead)) {
        memcpy(&read, pRead, std::min<size_t>(pRead->size, sizeof(SceCameraRead)));
        pRead = &read;
    }
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
        if (!pIBase || sizeIBase == 0)
            return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
        if (camera.info.format == SCE_CAMERA_FORMAT_YUV420_PLANE || camera.info.format == SCE_CAMERA_FORMAT_YUV422_PLANE) {
            if (!pUBase || sizeUBase == 0 || !pVBase || sizeVBase == 0)
                return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
        }
    } else {
        pIBase = camera.info.pIBase.get(emuenv.mem);
        pUBase = camera.info.pUBase.get(emuenv.mem);
        pVBase = camera.info.pVBase.get(emuenv.mem);
        sizeIBase = camera.info.sizeIBase;
        sizeUBase = camera.info.sizeUBase;
        sizeVBase = camera.info.sizeVBase;
    }
    auto res = camera.read(pRead, pIBase, pUBase, pVBase, sizeIBase, sizeUBase, sizeVBase);
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
    // code from disasm
    if (devnum != SCE_CAMERA_DEVICE_BACK && devnum != SCE_CAMERA_DEVICE_FRONT)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    if (!pValue)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    return RET_ERROR(SCE_CAMERA_ERROR_ATTRIBUTE_UNKNOWN);
}

EXPORT(int, sceCameraSetGain, SceCameraDevice devnum, int value) {
    TRACY_FUNC(sceCameraSetGain, devnum, value);
    // code from disasm
    if (devnum != SCE_CAMERA_DEVICE_BACK && devnum != SCE_CAMERA_DEVICE_FRONT)
        return RET_ERROR(SCE_CAMERA_ERROR_PARAM);
    return RET_ERROR(SCE_CAMERA_ERROR_ATTRIBUTE_UNKNOWN);
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
