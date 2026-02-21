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

#pragma once

#include <mem/ptr.h>
#include <util/log.h>
#include <util/types.h>

enum SceCameraErrorCode : uint32_t {
    SCE_CAMERA_ERROR_PARAM = 0x802E0000,
    SCE_CAMERA_ERROR_ALREADY_INIT = 0x802E0001,
    SCE_CAMERA_ERROR_NOT_INIT = 0x802E0002,
    SCE_CAMERA_ERROR_ALREADY_OPEN = 0x802E0003,
    SCE_CAMERA_ERROR_NOT_OPEN = 0x802E0004,
    SCE_CAMERA_ERROR_ALREADY_START = 0x802E0005,
    SCE_CAMERA_ERROR_NOT_START = 0x802E0006,
    SCE_CAMERA_ERROR_FORMAT_UNKNOWN = 0x802E0007,
    SCE_CAMERA_ERROR_RESOLUTION_UNKNOWN = 0x802E0008,
    SCE_CAMERA_ERROR_BAD_FRAMERATE = 0x802E0009,
    SCE_CAMERA_ERROR_TIMEOUT = 0x802E000A,
    SCE_CAMERA_ERROR_EXCLUSIVE = 0x802E000B,
    SCE_CAMERA_ERROR_ATTRIBUTE_UNKNOWN = 0x802E000C,
    SCE_CAMERA_ERROR_MAX_PROCESS = 0x802E000D,
    SCE_CAMERA_ERROR_NOT_ACTIVE = 0x802E000E,
    SCE_CAMERA_ERROR_ALREADY_READ = 0x802E000F,
    SCE_CAMERA_ERROR_NOT_MOUNTED = 0x802E0010,
    SCE_CAMERA_ERROR_DATA_RANGE_UNKNOWN = 0x802E0011,
    SCE_CAMERA_ERROR_OTHER_ALREADY_START = 0x802E0012,
    SCE_CAMERA_ERROR_FATAL = 0x802E00FF
};

enum SceCameraDevice {
    SCE_CAMERA_DEVICE_UNKNOWN = -1,
    SCE_CAMERA_DEVICE_FRONT = 0, // Front camera
    SCE_CAMERA_DEVICE_BACK = 1, // Retro camera
};
BOOST_DESCRIBE_ENUM(SceCameraDevice, SCE_CAMERA_DEVICE_FRONT, SCE_CAMERA_DEVICE_BACK, SCE_CAMERA_DEVICE_UNKNOWN);

enum SceCameraPriority : uint16_t {
    SCE_CAMERA_PRIORITY_SHARE = 0, // Share mode
    SCE_CAMERA_PRIORITY_EXCLUSIVE = 1 // Exclusive mode
};
BOOST_DESCRIBE_ENUM(SceCameraPriority, SCE_CAMERA_PRIORITY_SHARE, SCE_CAMERA_PRIORITY_EXCLUSIVE);

enum SceCameraFormat : uint16_t {
    SCE_CAMERA_FORMAT_INVALID = 0, // Invalid format
    SCE_CAMERA_FORMAT_YUV422_PLANE = 1, // YUV422 planes
    SCE_CAMERA_FORMAT_YUV422_PACKED = 2, // YUV422 pixels packed
    SCE_CAMERA_FORMAT_YUV420_PLANE = 3, // YUV420 planes
    SCE_CAMERA_FORMAT_ARGB = 4, // ARGB pixels
    SCE_CAMERA_FORMAT_ABGR = 5, // ABGR pixels
    SCE_CAMERA_FORMAT_RAW8 = 6 // 8 bit raw data
};
BOOST_DESCRIBE_ENUM(SceCameraFormat, SCE_CAMERA_FORMAT_INVALID, SCE_CAMERA_FORMAT_YUV422_PLANE, SCE_CAMERA_FORMAT_YUV422_PACKED, SCE_CAMERA_FORMAT_YUV420_PLANE, SCE_CAMERA_FORMAT_ARGB, SCE_CAMERA_FORMAT_ABGR, SCE_CAMERA_FORMAT_RAW8);

enum SceCameraResolution : uint16_t {
    SCE_CAMERA_RESOLUTION_0_0 = 0, // Invalid resolution
    SCE_CAMERA_RESOLUTION_640_480 = 1, // VGA resolution
    SCE_CAMERA_RESOLUTION_320_240 = 2, // QVGA resolution
    SCE_CAMERA_RESOLUTION_160_120 = 3, // QQVGA resolution
    SCE_CAMERA_RESOLUTION_352_288 = 4, // CIF resolution
    SCE_CAMERA_RESOLUTION_176_144 = 5, // QCIF resolution
    SCE_CAMERA_RESOLUTION_480_272 = 6, // PSP resolution
    SCE_CAMERA_RESOLUTION_640_360 = 8 // NGP resolution
};
BOOST_DESCRIBE_ENUM(SceCameraResolution, SCE_CAMERA_RESOLUTION_0_0, SCE_CAMERA_RESOLUTION_640_480, SCE_CAMERA_RESOLUTION_320_240, SCE_CAMERA_RESOLUTION_160_120, SCE_CAMERA_RESOLUTION_352_288, SCE_CAMERA_RESOLUTION_176_144, SCE_CAMERA_RESOLUTION_480_272, SCE_CAMERA_RESOLUTION_640_360);

enum SceCameraFrameRate : uint16_t {
    SCE_CAMERA_FRAMERATE_3_FPS = 3, // 3.75 fps
    SCE_CAMERA_FRAMERATE_5_FPS = 5, // 5 fps
    SCE_CAMERA_FRAMERATE_7_FPS = 7, // 7.5 fps
    SCE_CAMERA_FRAMERATE_10_FPS = 10, // 10 fps
    SCE_CAMERA_FRAMERATE_15_FPS = 15, // 15 fps
    SCE_CAMERA_FRAMERATE_20_FPS = 20, // 20 fps
    SCE_CAMERA_FRAMERATE_30_FPS = 30, // 30 fps
    SCE_CAMERA_FRAMERATE_60_FPS = 60, // 60 fps
    SCE_CAMERA_FRAMERATE_120_FPS = 120 // 120 fps (@note Resolution must be QVGA or lower)
};
BOOST_DESCRIBE_ENUM(SceCameraFrameRate, SCE_CAMERA_FRAMERATE_3_FPS, SCE_CAMERA_FRAMERATE_5_FPS, SCE_CAMERA_FRAMERATE_7_FPS, SCE_CAMERA_FRAMERATE_10_FPS, SCE_CAMERA_FRAMERATE_15_FPS, SCE_CAMERA_FRAMERATE_20_FPS, SCE_CAMERA_FRAMERATE_30_FPS, SCE_CAMERA_FRAMERATE_60_FPS, SCE_CAMERA_FRAMERATE_120_FPS);

enum SceCameraExposureCompensation {
    SCE_CAMERA_EV_NEGATIVE_20 = -20, // -2.0
    SCE_CAMERA_EV_NEGATIVE_17 = -17, // -1.7
    SCE_CAMERA_EV_NEGATIVE_15 = -15, // -1.5
    SCE_CAMERA_EV_NEGATIVE_13 = -13, // -1.3
    SCE_CAMERA_EV_NEGATIVE_10 = -10, // -1.0
    SCE_CAMERA_EV_NEGATIVE_7 = -7, // -0.7
    SCE_CAMERA_EV_NEGATIVE_5 = -5, // -0.5
    SCE_CAMERA_EV_NEGATIVE_3 = -3, // -0.3
    SCE_CAMERA_EV_POSITIVE_0 = 0, // +0.0
    SCE_CAMERA_EV_POSITIVE_3 = 3, // +0.3
    SCE_CAMERA_EV_POSITIVE_5 = 5, // +0.5
    SCE_CAMERA_EV_POSITIVE_7 = 7, // +0.7
    SCE_CAMERA_EV_POSITIVE_10 = 10, // +1.0
    SCE_CAMERA_EV_POSITIVE_13 = 13, // +1.3
    SCE_CAMERA_EV_POSITIVE_15 = 15, // +1.5
    SCE_CAMERA_EV_POSITIVE_17 = 17, // +1.7
    SCE_CAMERA_EV_POSITIVE_20 = 20 // +2.0
};
BOOST_DESCRIBE_ENUM(SceCameraExposureCompensation, SCE_CAMERA_EV_NEGATIVE_20, SCE_CAMERA_EV_NEGATIVE_17, SCE_CAMERA_EV_NEGATIVE_15, SCE_CAMERA_EV_NEGATIVE_13, SCE_CAMERA_EV_NEGATIVE_10, SCE_CAMERA_EV_NEGATIVE_7, SCE_CAMERA_EV_NEGATIVE_5, SCE_CAMERA_EV_NEGATIVE_3, SCE_CAMERA_EV_POSITIVE_0, SCE_CAMERA_EV_POSITIVE_3, SCE_CAMERA_EV_POSITIVE_5, SCE_CAMERA_EV_POSITIVE_7, SCE_CAMERA_EV_POSITIVE_10, SCE_CAMERA_EV_POSITIVE_13, SCE_CAMERA_EV_POSITIVE_15, SCE_CAMERA_EV_POSITIVE_17, SCE_CAMERA_EV_POSITIVE_20);

enum SceCameraEffect {
    SCE_CAMERA_EFFECT_NORMAL = 0,
    SCE_CAMERA_EFFECT_NEGATIVE = 1,
    SCE_CAMERA_EFFECT_BLACKWHITE = 2,
    SCE_CAMERA_EFFECT_SEPIA = 3,
    SCE_CAMERA_EFFECT_BLUE = 4,
    SCE_CAMERA_EFFECT_RED = 5,
    SCE_CAMERA_EFFECT_GREEN = 6
};
BOOST_DESCRIBE_ENUM(SceCameraEffect, SCE_CAMERA_EFFECT_NORMAL, SCE_CAMERA_EFFECT_NEGATIVE, SCE_CAMERA_EFFECT_BLACKWHITE, SCE_CAMERA_EFFECT_SEPIA, SCE_CAMERA_EFFECT_BLUE, SCE_CAMERA_EFFECT_RED, SCE_CAMERA_EFFECT_GREEN);

enum SceCameraReverse {
    SCE_CAMERA_REVERSE_OFF = 0, // Reverse mode off
    SCE_CAMERA_REVERSE_MIRROR = 1, // Mirror mode
    SCE_CAMERA_REVERSE_FLIP = 2, // Flip mode
    SCE_CAMERA_REVERSE_MIRROR_FLIP = (SCE_CAMERA_REVERSE_MIRROR | SCE_CAMERA_REVERSE_FLIP) // Mirror + Flip mode
};
BOOST_DESCRIBE_ENUM(SceCameraReverse, SCE_CAMERA_REVERSE_OFF, SCE_CAMERA_REVERSE_MIRROR, SCE_CAMERA_REVERSE_FLIP, SCE_CAMERA_REVERSE_MIRROR_FLIP);

enum SceCameraSaturation {
    SCE_CAMERA_SATURATION_0 = 0, // 0.0
    SCE_CAMERA_SATURATION_5 = 5, // 0.5
    SCE_CAMERA_SATURATION_10 = 10, // 1.0
    SCE_CAMERA_SATURATION_20 = 20, // 2.0
    SCE_CAMERA_SATURATION_30 = 30, // 3.0
    SCE_CAMERA_SATURATION_40 = 40 // 4.0
};
BOOST_DESCRIBE_ENUM(SceCameraSaturation, SCE_CAMERA_SATURATION_0, SCE_CAMERA_SATURATION_5, SCE_CAMERA_SATURATION_10, SCE_CAMERA_SATURATION_20, SCE_CAMERA_SATURATION_30, SCE_CAMERA_SATURATION_40);

enum SceCameraSharpness {
    SCE_CAMERA_SHARPNESS_100 = 1, // 100%
    SCE_CAMERA_SHARPNESS_200 = 2, // 200%
    SCE_CAMERA_SHARPNESS_300 = 3, // 300%
    SCE_CAMERA_SHARPNESS_400 = 4 // 400%
};
BOOST_DESCRIBE_ENUM(SceCameraSharpness, SCE_CAMERA_SHARPNESS_100, SCE_CAMERA_SHARPNESS_200, SCE_CAMERA_SHARPNESS_300, SCE_CAMERA_SHARPNESS_400);

enum SceCameraAntiFlicker {
    SCE_CAMERA_ANTIFLICKER_AUTO = 1, // Automatic mode
    SCE_CAMERA_ANTIFLICKER_50HZ = 2, // 50 Hz mode
    SCE_CAMERA_ANTIFLICKER_60HZ = 3 // 50 Hz mode
};
BOOST_DESCRIBE_ENUM(SceCameraAntiFlicker, SCE_CAMERA_ANTIFLICKER_AUTO, SCE_CAMERA_ANTIFLICKER_50HZ, SCE_CAMERA_ANTIFLICKER_60HZ);

enum SceCameraISO {
    SCE_CAMERA_ISO_AUTO = 1, // Automatic mode
    SCE_CAMERA_ISO_100 = 100, // ISO100/21?
    SCE_CAMERA_ISO_200 = 200, // ISO200/24?
    SCE_CAMERA_ISO_400 = 400 // ISO400/27?
};
BOOST_DESCRIBE_ENUM(SceCameraISO, SCE_CAMERA_ISO_AUTO, SCE_CAMERA_ISO_100, SCE_CAMERA_ISO_200, SCE_CAMERA_ISO_400);

enum SceCameraGain {
    SCE_CAMERA_GAIN_AUTO = 0,
    SCE_CAMERA_GAIN_1 = 1,
    SCE_CAMERA_GAIN_2 = 2,
    SCE_CAMERA_GAIN_3 = 3,
    SCE_CAMERA_GAIN_4 = 4,
    SCE_CAMERA_GAIN_5 = 5,
    SCE_CAMERA_GAIN_6 = 6,
    SCE_CAMERA_GAIN_7 = 7,
    SCE_CAMERA_GAIN_8 = 8,
    SCE_CAMERA_GAIN_9 = 9,
    SCE_CAMERA_GAIN_10 = 10,
    SCE_CAMERA_GAIN_11 = 11,
    SCE_CAMERA_GAIN_12 = 12,
    SCE_CAMERA_GAIN_13 = 13,
    SCE_CAMERA_GAIN_14 = 14,
    SCE_CAMERA_GAIN_15 = 15,
    SCE_CAMERA_GAIN_16 = 16
};
BOOST_DESCRIBE_ENUM(SceCameraGain, SCE_CAMERA_GAIN_AUTO, SCE_CAMERA_GAIN_1, SCE_CAMERA_GAIN_2, SCE_CAMERA_GAIN_3, SCE_CAMERA_GAIN_4, SCE_CAMERA_GAIN_5, SCE_CAMERA_GAIN_6, SCE_CAMERA_GAIN_7, SCE_CAMERA_GAIN_8, SCE_CAMERA_GAIN_9, SCE_CAMERA_GAIN_10, SCE_CAMERA_GAIN_11, SCE_CAMERA_GAIN_12, SCE_CAMERA_GAIN_13, SCE_CAMERA_GAIN_14, SCE_CAMERA_GAIN_15, SCE_CAMERA_GAIN_16);

enum SceCameraWhiteBalance {
    SCE_CAMERA_WB_AUTO = 0, // Automatic mode
    SCE_CAMERA_WB_DAY = 1, // Daylight mode
    SCE_CAMERA_WB_CWF = 2, // Cool White Fluorescent mode
    SCE_CAMERA_WB_SLSA = 4 // Standard Light Source A mode
};
BOOST_DESCRIBE_ENUM(SceCameraWhiteBalance, SCE_CAMERA_WB_AUTO, SCE_CAMERA_WB_DAY, SCE_CAMERA_WB_CWF, SCE_CAMERA_WB_SLSA);

enum SceCameraBacklight {
    SCE_CAMERA_BACKLIGHT_OFF = 0, // Disabled
    SCE_CAMERA_BACKLIGHT_ON = 1 // Enabled
};
BOOST_DESCRIBE_ENUM(SceCameraBacklight, SCE_CAMERA_BACKLIGHT_OFF, SCE_CAMERA_BACKLIGHT_ON);

enum SceCameraNightmode {
    SCE_CAMERA_NIGHTMODE_OFF = 0, // Disabled
    SCE_CAMERA_NIGHTMODE_LESS10 = 1, // 10 lux or below
    SCE_CAMERA_NIGHTMODE_LESS100 = 2, // 100 lux or below
    SCE_CAMERA_NIGHTMODE_OVER100 = 3 // 100 lux or over
};
BOOST_DESCRIBE_ENUM(SceCameraNightmode, SCE_CAMERA_NIGHTMODE_OFF, SCE_CAMERA_NIGHTMODE_LESS10, SCE_CAMERA_NIGHTMODE_LESS100, SCE_CAMERA_NIGHTMODE_OVER100);

/* Camera Status */
enum SceCameraStatus {
    SCE_CAMERA_STATUS_IS_ACTIVE = 0,
    SCE_CAMERA_STATUS_IS_NOT_ACTIVE = -1,
    SCE_CAMERA_STATUS_IS_FORCED_STOP = -2,
    SCE_CAMERA_STATUS_IS_FORCED_STOP_POWER_CONFIG_CHANGE = -3,
    SCE_CAMERA_STATUS_IS_ALREADY_READ = 1,
    SCE_CAMERA_STATUS_IS_NOT_STABLE = 2
};
BOOST_DESCRIBE_ENUM(SceCameraStatus, SCE_CAMERA_STATUS_IS_ACTIVE, SCE_CAMERA_STATUS_IS_NOT_ACTIVE, SCE_CAMERA_STATUS_IS_FORCED_STOP, SCE_CAMERA_STATUS_IS_FORCED_STOP_POWER_CONFIG_CHANGE, SCE_CAMERA_STATUS_IS_ALREADY_READ, SCE_CAMERA_STATUS_IS_NOT_STABLE);

/* Camera Read Mode */
enum SceCameraReadMode {
    SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_ON = 0,
    SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_OFF = 1
};
BOOST_DESCRIBE_ENUM(SceCameraReadMode, SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_ON, SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_OFF);

/* Camera Read Exposure Time Mode */
enum SceCameraReadGetExposureTime {
    SCE_CAMERA_READ_GET_EXPOSURE_TIME_OFF = 0,
    SCE_CAMERA_READ_GET_EXPOSURE_TIME_ON = 1
};
BOOST_DESCRIBE_ENUM(SceCameraReadGetExposureTime, SCE_CAMERA_READ_GET_EXPOSURE_TIME_OFF, SCE_CAMERA_READ_GET_EXPOSURE_TIME_ON);

/* Camera Data Range */

enum SceCameraDataRange {
    SCE_CAMERA_DATA_RANGE_FULL = 0,
    SCE_CAMERA_DATA_RANGE_BT601 = 1
};
BOOST_DESCRIBE_ENUM(SceCameraDataRange, SCE_CAMERA_DATA_RANGE_FULL, SCE_CAMERA_DATA_RANGE_BT601);

/* Camera Buffer */
enum SceCameraBuffer {
    SCE_CAMERA_BUFFER_SETBYOPEN = 0,
    SCE_CAMERA_BUFFER_SETBYREAD = 1
};
BOOST_DESCRIBE_ENUM(SceCameraBuffer, SCE_CAMERA_BUFFER_SETBYOPEN, SCE_CAMERA_BUFFER_SETBYREAD);

/* Camera Raw8 Format Pattern */
enum SceCameraRaw8Format {
    SCE_CAMERA_RAW8_FORMAT_UNKNOWN = 0,
    SCE_CAMERA_RAW8_FORMAT_BGGR = 1,
    SCE_CAMERA_RAW8_FORMAT_GRBG = 2,
    SCE_CAMERA_RAW8_FORMAT_RGGB = 3,
    SCE_CAMERA_RAW8_FORMAT_GBRG = 4
};

struct SceCameraInfo {
    SceSize size; // sizeof(SceCameraInfo)
    SceCameraPriority priority; // Process priority
    SceCameraFormat format; // Output format
    SceCameraResolution resolution; // Resolution
    SceCameraFrameRate framerate; // Framerate
    uint16_t width;
    uint16_t height;
    uint16_t range; // Data range of color components. (SceCameraDataRange)
    uint16_t pad; // Structure padding
    SceSize sizeIBase;
    SceSize sizeUBase;
    SceSize sizeVBase;
    Ptr<void> pIBase;
    Ptr<void> pUBase;
    Ptr<void> pVBase;
    uint16_t pitch; // Byte count difference of user buffer and resolution width
    uint16_t buffer; // User Buffer Setting Method (SceCameraBuffer)
};

struct SceCameraRead {
    SceSize size; // sizeof(SceCameraRead)
    int mode;
    int pad;
    int status;
    uint64_t frame;
    uint64_t timestamp;
    uint32_t exposure_time;
    uint32_t exposure_time_gap;
    uint32_t raw8_format;
    uint32_t unk_01;
    SceSize sizeIBase;
    SceSize sizeUBase;
    SceSize sizeVBase;
    Ptr<void> pIBase;
    Ptr<void> pUBase;
    Ptr<void> pVBase;
};

// numbers from disasmed code
enum class CameraAttributes {
    Saturation = 0xa,
    Brightness = 0xb,
    Contrast = 0xc,
    Sharpness = 0xd,
    Reverse = 0xe,
    Effect = 0xf,
    EV = 0x10,
    Zoom = 0x11,
    AntiFlicker = 0x12,
    Gain, // unused. can't be get/set in original vita
    ISO = 0x14,
    WhiteBalance = 0x16,
    Backlight = 0x17,
    Nightmode = 0x18,
    NoiseReduction = 0x19, // another get/set functions
    AutoControlHold = 0x1a,
    ExposureCeiling = 0x1b,
    ImageQuality = 0x1c,
    SharpnessOff // also 0xd, but another get/set functions
};

enum CameraType {
    SolidColor,
    Image,
    WebCamera
};

class Camera {
public:
    class CameraImpl;
    std::unique_ptr<CameraImpl> pImpl; // opaque type here
    CameraType type;
    std::string id;
    std::string image;
    uint32_t color;

    SceCameraInfo info;
    bool is_opened{};
    bool is_started{};
    uint64_t frame_idx{};
    uint64_t last_frame_timestamp_us = 0; // timestamp of last acquired frame in microseconds
    uint64_t next_frame_timestamp_us = 0; // time, when read should acquire and return next frame. in microseconds
    uint64_t frame_interval_us = 0; // Time interval between frames in microseconds
    uint64_t tick_diff_us = 0; // Difference between emuenv ticks and SDL ticks in microseconds
    int get_attribute(CameraAttributes attribute);
    int set_attribute(CameraAttributes attribute, int value);
    int open(SceCameraInfo *info, uint64_t base_tick, uint64_t start_tick);
    int close();
    int start();
    int stop();
    int read(SceCameraRead *read, void *pIBase, void *pUBase, void *pVBase, SceSize sizeIBase, SceSize sizeUBase, SceSize sizeVBase);
    Camera();
    ~Camera();

    // disable copy
    Camera(const Camera &) = delete;
    Camera &operator=(Camera const &) = delete;
    void update_config(int type, const std::string &id, const std::string &image, uint32_t color);
};

void init_default_cameras(Config &cfg);
