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

#include "config/state.h"

#include <array>
#include <camera/camera.h>
#include <rtc/rtc.h>
#include <stb_image.h>
#include <util/log.h>

#include <SDL3/SDL_camera.h>
#include <SDL3/SDL_timer.h>

struct stbi_deleter {
    void operator()(stbi_uc *d) const { stbi_image_free(d); }
};

struct SDL_SurfaceDeleter {
    void operator()(SDL_Surface *s) const { SDL_DestroySurface(s); }
};

struct SDL_CameraDeleter {
    void operator()(SDL_Camera *c) const { SDL_CloseCamera(c); }
};

struct SDL_Deleter {
    void operator()(void *c) const { SDL_free(c); }
};

using SDL_SurfacePtr = std::unique_ptr<SDL_Surface, SDL_SurfaceDeleter>;
using stbi_uc_ptr = std::unique_ptr<stbi_uc, stbi_deleter>;
using SDL_CameraPtr = std::unique_ptr<SDL_Camera, SDL_CameraDeleter>;
using SDL_CameraListPtr = std::unique_ptr<SDL_CameraID[], SDL_Deleter>;

class Camera::CameraImpl {
public:
    SDL_CameraPtr sdl_camera;
    SDL_CameraID device_id;
    SDL_SurfacePtr frame;
    SDL_PixelFormat format;
    SDL_Colorspace colorspace;
    std::mutex frame_mutex;
};

Camera::Camera()
    : pImpl(std::make_unique<CameraImpl>()) {}
Camera::~Camera() = default;

static bool init_web_camera(Camera *self) {
    // open web camera via SDL3
    int num_cameras;
    SDL_CameraListPtr cameras(SDL_GetCameras(&num_cameras));
    if (!cameras) {
        LOG_ERROR("SDL_GetCameras failed. Error: {}", SDL_GetError());
        return false;
    }
    self->pImpl->device_id = 0;
    for (int i = 0; i < num_cameras; i++) {
        auto camera_name = SDL_GetCameraName(cameras[i]);
        if (camera_name && self->id == camera_name) {
            self->pImpl->device_id = cameras[i];
            break;
        }
    }
    if (self->pImpl->device_id == 0) {
        LOG_ERROR("Web camera with id '{}' not found.", self->id);
        return false;
    }
    uint16_t framerate_numerator = self->info.framerate;
    uint16_t framerate_denominator = 1;
    if (self->info.framerate == SCE_CAMERA_FRAMERATE_3_FPS) {
        framerate_numerator = 375;
        framerate_denominator = 100;
    } else if (self->info.framerate == SCE_CAMERA_FRAMERATE_7_FPS) {
        framerate_numerator = 75;
        framerate_denominator = 10;
    }
    self->frame_interval_us = SDL_US_PER_SECOND * framerate_denominator / framerate_numerator;
    SDL_CameraSpec camera_spec{
        .format = self->pImpl->format, /**< Frame format */
        .colorspace = self->pImpl->colorspace, /**< Frame colorspace */
        .width = self->info.width, /**< Frame width */
        .height = self->info.height, /**< Frame height */
        .framerate_numerator = framerate_numerator, /**< Frame rate numerator ((num / denom) == FPS, (denom / num) == duration in seconds) */
        .framerate_denominator = framerate_denominator /**< Frame rate demoninator ((num / denom) == FPS, (denom / num) == duration in seconds) */
    };
    self->pImpl->sdl_camera.reset(SDL_OpenCamera(self->pImpl->device_id, &camera_spec));
    if (!self->pImpl->sdl_camera) {
        LOG_ERROR("Failed to open camera: {}", SDL_GetError());
        return false;
    }
    return true;
}

static bool init_image(Camera *self) {
    FILE *f = FOPEN(fs_utils::utf8_to_path(self->image).c_str(), "rb");
    if (!f) {
        LOG_ERROR("Failed to open camera image file: {} Error: {}", self->image, strerror(errno));
        return false;
    }

    int32_t width = 0, height = 0;
    stbi_uc_ptr data(stbi_load_from_file(f, &width, &height, nullptr, STBI_rgb));
    fclose(f);
    if (!data) {
        LOG_ERROR("Failed to load camera image from {}: {}", self->image, stbi_failure_reason());
        return false;
    }

    SDL_SurfacePtr image_surface(SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGB24, data.get(), width * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_RGB24)));
    if (!image_surface) {
        LOG_ERROR("Failed to create SDL surface from image data: {}", SDL_GetError());
        return false;
    }

    if (width != self->info.width || height != self->info.height) {
        image_surface.reset(SDL_ScaleSurface(image_surface.get(), self->info.width, self->info.height, SDL_SCALEMODE_LINEAR));
        if (!image_surface) {
            LOG_ERROR("Failed to scale SDL surface: {}", SDL_GetError());
            return false;
        }
    }

    image_surface.reset(SDL_ConvertSurfaceAndColorspace(image_surface.get(), self->pImpl->format, nullptr, self->pImpl->colorspace, 0));
    if (!image_surface) {
        LOG_ERROR("Failed to convert SDL surface: {}", SDL_GetError());
        return false;
    }

    // If frame does not own its data then duplicate it
    if (image_surface->flags & SDL_SURFACE_PREALLOCATED) {
        image_surface.reset(SDL_DuplicateSurface(image_surface.get()));
        if (!image_surface) {
            LOG_ERROR("Failed to duplicate SDL surface: {}", SDL_GetError());
            return false;
        }
    }
    self->pImpl->frame = std::move(image_surface);

    return true;
}

static bool init_solid_color(Camera *self) {
    SDL_SurfacePtr image_surface(SDL_CreateSurface(self->info.width, self->info.height, SDL_PIXELFORMAT_ABGR8888));
    if (!image_surface) {
        LOG_ERROR("Failed to create SDL surface for solid color: {}", SDL_GetError());
        return false;
    }
    if (!SDL_FillSurfaceRect(image_surface.get(), nullptr, self->color)) {
        LOG_ERROR("Failed to fill SDL surface with solid color: {}", SDL_GetError());
        return false;
    }
    SDL_SurfacePtr converted_surface(SDL_ConvertSurfaceAndColorspace(image_surface.get(), self->pImpl->format, nullptr, self->pImpl->colorspace, 0));
    if (!converted_surface) {
        LOG_ERROR("Failed to convert SDL surface for solid color: {}", SDL_GetError());
        return false;
    }
    self->pImpl->frame = std::move(converted_surface);
    return true;
}

static void refresh_camera_image(Camera *self) {
    const std::lock_guard lock(self->pImpl->frame_mutex);

    self->pImpl->sdl_camera.reset();
    self->pImpl->frame.reset();
    if (self->type == CameraType::WebCamera) {
        if (!init_web_camera(self))
            self->type = CameraType::Image;
    }
    if (self->type == CameraType::Image) {
        if (!init_image(self))
            self->type = CameraType::SolidColor;
    }
    if (self->type == CameraType::SolidColor) {
        if (!init_solid_color(self))
            self->pImpl->frame.reset();
    }
}

int Camera::get_attribute(CameraAttributes attribute) {
    switch (attribute) {
    case CameraAttributes::Saturation: return SCE_CAMERA_SATURATION_10;
    case CameraAttributes::Brightness: return 128; // 0-255
    case CameraAttributes::Contrast: return 128; // random default value  0-255
    case CameraAttributes::Sharpness: return SCE_CAMERA_SHARPNESS_100;
    case CameraAttributes::Reverse: return SCE_CAMERA_REVERSE_OFF;
    case CameraAttributes::Effect: return SCE_CAMERA_EFFECT_NORMAL;
    case CameraAttributes::EV: return 0;
    case CameraAttributes::Zoom: return 10; // 10-40
    case CameraAttributes::AntiFlicker: return SCE_CAMERA_ANTIFLICKER_AUTO;
    case CameraAttributes::ISO: return SCE_CAMERA_ISO_AUTO;
    case CameraAttributes::Gain: return SCE_CAMERA_GAIN_AUTO;
    case CameraAttributes::WhiteBalance: return SCE_CAMERA_WB_AUTO;
    case CameraAttributes::Backlight: return SCE_CAMERA_BACKLIGHT_OFF;
    case CameraAttributes::Nightmode: return SCE_CAMERA_NIGHTMODE_OFF;
    case CameraAttributes::ExposureCeiling: return 0;
    case CameraAttributes::AutoControlHold: return 0;
    // undescribed parameters
    case CameraAttributes::ImageQuality: return 0;
    case CameraAttributes::NoiseReduction: return 0;
    case CameraAttributes::SharpnessOff: return 0;
    }
    return 0;
}

int Camera::set_attribute(CameraAttributes attribute, int value) {
    return 0;
}

int Camera::open(SceCameraInfo *info, uint64_t base_tick, uint64_t start_tick) {
    tick_diff_us = rtc_get_ticks(base_tick) - start_tick - (SDL_GetTicksNS() / SDL_NS_PER_US);
    this->is_opened = true;
    this->info = {};
    memcpy(&(this->info), info, std::min<size_t>(sizeof(SceCameraInfo), info->size));
    switch (this->info.format) {
    case SCE_CAMERA_FORMAT_YUV420_PLANE: // YUV 4:2:0 planar
        this->pImpl->format = SDL_PIXELFORMAT_YV12;
        this->pImpl->colorspace = SDL_COLORSPACE_YUV_DEFAULT;
        break;
    case SCE_CAMERA_FORMAT_YUV422_PLANE: // YUV 4:2:2 planar
        this->pImpl->format = SDL_PIXELFORMAT_YUY2; // use packed format cause SDL does not have YUV 4:2:2 planar
        this->pImpl->colorspace = SDL_COLORSPACE_YUV_DEFAULT;
        break;
    case SCE_CAMERA_FORMAT_YUV422_PACKED: // YUV 4:2:2 packed
        this->pImpl->format = SDL_PIXELFORMAT_YUY2;
        this->pImpl->colorspace = SDL_COLORSPACE_YUV_DEFAULT;
        break;
    case SCE_CAMERA_FORMAT_ARGB: // ARGB format
        this->pImpl->format = SDL_PIXELFORMAT_ARGB8888;
        this->pImpl->colorspace = SDL_COLORSPACE_RGB_DEFAULT;
        break;
    case SCE_CAMERA_FORMAT_ABGR: // ABGR format
        this->pImpl->format = SDL_PIXELFORMAT_ABGR8888;
        this->pImpl->colorspace = SDL_COLORSPACE_RGB_DEFAULT;
        break;
    case SCE_CAMERA_FORMAT_RAW8: // unsupported, use default
    default:
        this->pImpl->format = SDL_PIXELFORMAT_ARGB8888; // ABGR format
        this->pImpl->colorspace = SDL_COLORSPACE_RGB_DEFAULT;
    }

    if (this->info.range == SCE_CAMERA_DATA_RANGE_BT601) {
        this->pImpl->colorspace = SDL_COLORSPACE_BT601_LIMITED;
    }

    return 0;
}

int Camera::start() {
    refresh_camera_image(this);
    this->is_started = true;
    this->frame_idx = 0;
    return 0;
}

int Camera::read(SceCameraRead *read, void *pIBase, void *pUBase, void *pVBase, SceSize sizeIBase, SceSize sizeUBase, SceSize sizeVBase) {
    const std::lock_guard lock(pImpl->frame_mutex);
    constexpr uint64_t minimal_delay_ns = SDL_NS_PER_US; // arbitrary value
    uint64_t sdl_current_time_ns = SDL_GetTicksNS();
    uint64_t sdl_current_time_us = sdl_current_time_ns / SDL_NS_PER_US;
    if (next_frame_timestamp_us == 0) {
        next_frame_timestamp_us = sdl_current_time_us;
    } else {
        uint64_t wait_time_ns = (sdl_current_time_ns < next_frame_timestamp_us * SDL_NS_PER_US) ? (next_frame_timestamp_us * SDL_NS_PER_US - sdl_current_time_ns) : 0;
        if (wait_time_ns > minimal_delay_ns) {
            if (read->mode == SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_ON) {
                SDL_DelayPrecise(wait_time_ns);
            } else {
                read->frame = frame_idx;
                read->timestamp = last_frame_timestamp_us;
                read->status = SCE_CAMERA_STATUS_IS_ALREADY_READ;
                return 0;
            }
        }
    }
    // next frame
    last_frame_timestamp_us = next_frame_timestamp_us;
    next_frame_timestamp_us = next_frame_timestamp_us + frame_interval_us;
    if (type == CameraType::WebCamera) {
        Uint64 timestampNS = 0;
        auto surface = SDL_AcquireCameraFrame(pImpl->sdl_camera.get(), &timestampNS);
        // drains all buffered frames to get the latest one
        Uint64 next_timestampNS;
        auto next_surface = SDL_AcquireCameraFrame(pImpl->sdl_camera.get(), &next_timestampNS);
        int i = 16; // Safety counter to avoid endless loop.
                    // sdl buffer is 8 surfaces, so if we read much more than 8 times, something go really wrong
        while (next_surface && i > 0) {
            SDL_ReleaseCameraFrame(pImpl->sdl_camera.get(), surface);
            surface = next_surface;
            timestampNS = next_timestampNS;
            next_surface = SDL_AcquireCameraFrame(pImpl->sdl_camera.get(), &next_timestampNS);
            i--;
        }
        if (next_surface)
            SDL_ReleaseCameraFrame(pImpl->sdl_camera.get(), next_surface);
        if (surface) {
            pImpl->frame.reset(SDL_DuplicateSurface(surface));
            SDL_ReleaseCameraFrame(pImpl->sdl_camera.get(), surface);
            LOG_ERROR_IF(!pImpl->frame, "Failed to duplicate camera surface: {}", SDL_GetError());
            last_frame_timestamp_us = timestampNS / SDL_NS_PER_US + tick_diff_us;
        }
    }
    read->frame = frame_idx++;
    read->status = SCE_CAMERA_STATUS_IS_ACTIVE;
    read->timestamp = last_frame_timestamp_us;
    if (pImpl->frame) {
        if (this->info.format == SCE_CAMERA_FORMAT_YUV420_PLANE) {
            sizeIBase = std::min(sizeIBase, (SceSize)(pImpl->frame->pitch * pImpl->frame->h));
            sizeUBase = std::min(sizeUBase, (SceSize)(pImpl->frame->pitch * pImpl->frame->h / 4));
            sizeVBase = std::min(sizeVBase, (SceSize)(pImpl->frame->pitch * pImpl->frame->h / 4));
            memcpy(pIBase, (uint8_t *)pImpl->frame->pixels, sizeIBase);
            memcpy(pVBase, (uint8_t *)pImpl->frame->pixels + (pImpl->frame->pitch * pImpl->frame->h), sizeUBase);
            memcpy(pUBase, (uint8_t *)pImpl->frame->pixels + (pImpl->frame->pitch * pImpl->frame->h * 5 / 4), sizeVBase);
        } else if (this->info.format == SCE_CAMERA_FORMAT_YUV422_PLANE) {
            // convert SDL_PIXELFORMAT_YUY2 Y0+U0+Y1+V0 to planar format
            const int width = pImpl->frame->w;
            const int height = pImpl->frame->h;
            if (sizeIBase < (SceSize)(width * height) || sizeUBase < (SceSize)((width / 2) * height) || sizeVBase < (SceSize)((width / 2) * height)) {
                LOG_ERROR_ONCE("Buffer sizes too small for YUV422 planar conversion: IBase {}, UBase {}, VBase {}, required I {}, U {}, V {}", sizeIBase, sizeUBase, sizeVBase, width * height, (width / 2) * height, (width / 2) * height);
                return SCE_CAMERA_ERROR_PARAM;
            }
            if (!SDL_LockSurface(pImpl->frame.get())) {
                LOG_ERROR("Failed to lock camera frame surface: {}", SDL_GetError());
                goto BAD_FRAME;
            };
            uint8_t const *packed = (uint8_t *)pImpl->frame->pixels;
            uint8_t *Y = (uint8_t *)pIBase;
            uint8_t *U = (uint8_t *)pUBase;
            uint8_t *V = (uint8_t *)pVBase;
            int pitch = pImpl->frame->pitch;
            for (ptrdiff_t y = 0; y < height; y++) {
                uint8_t const *row = packed + y * pitch;
                for (ptrdiff_t x = 0; x < width; x += 2) {
                    ptrdiff_t pair_idx = x / 2;
                    Y[y * width + x] = row[pair_idx * 4];
                    Y[y * width + x + 1] = row[pair_idx * 4 + 2];
                    U[y * (width / 2) + pair_idx] = row[pair_idx * 4 + 1];
                    V[y * (width / 2) + pair_idx] = row[pair_idx * 4 + 3];
                }
            }
            SDL_UnlockSurface(pImpl->frame.get());
        } else {
            sizeIBase = std::min(sizeIBase, (SceSize)(pImpl->frame->pitch * pImpl->frame->h)); // here pitch is width in bytes
            memcpy(pIBase, pImpl->frame->pixels, sizeIBase);
        }
    } else {
    BAD_FRAME:
        LOG_ERROR_ONCE("No camera frame available.");
        memset(pIBase, 0, sizeIBase);
        if (pUBase && sizeUBase)
            memset(pUBase, 127, sizeUBase);
        if (pVBase && sizeVBase)
            memset(pVBase, 127, sizeVBase);
    }
    return 0;
}

int Camera::close() {
    this->is_started = false;
    this->is_opened = false;
    return 0;
}

int Camera::stop() {
    this->is_started = false;
    pImpl->frame.reset();
    pImpl->sdl_camera.reset();
    return 0;
}

void Camera::update_config(int type, const std::string &id, const std::string &image, uint32_t color) {
    if (static_cast<int>(this->type) == type && this->id == id && this->image == image && this->color == color)
        return;
    this->type = static_cast<CameraType>(type);
    this->id = id;
    this->image = image;
    this->color = color;
    if (is_started)
        refresh_camera_image(this);
}

BOOST_DESCRIBE_ENUM(SDL_CameraPosition, SDL_CAMERA_POSITION_UNKNOWN, SDL_CAMERA_POSITION_FRONT_FACING, SDL_CAMERA_POSITION_BACK_FACING)

void init_default_cameras(Config &cfg) {
    if (!((cfg.front_camera_type == WebCamera && cfg.front_camera_id.empty()) || (cfg.back_camera_type == WebCamera && cfg.back_camera_id.empty())))
        return;
    LOG_DEBUG("Initialize default cameras...");
    int num_cameras;
    SDL_CameraListPtr cameras(SDL_GetCameras(&num_cameras));
    for (int i = 0; i < num_cameras; i++) {
        auto position = SDL_GetCameraPosition(cameras[i]);
        LOG_TRACE("Found camera: name='{}' position={}", SDL_GetCameraName(cameras[i]), position);
        if (position == SDL_CAMERA_POSITION_FRONT_FACING) {
            cfg.front_camera_id = SDL_GetCameraName(cameras[i]);
            cfg.front_camera_type = WebCamera;
        }
        if (position == SDL_CAMERA_POSITION_BACK_FACING) {
            cfg.back_camera_id = SDL_GetCameraName(cameras[i]);
            cfg.back_camera_type = WebCamera;
        }
    }
    if (num_cameras > 0) {
        if (cfg.front_camera_id.empty()) {
            cfg.front_camera_id = SDL_GetCameraName(cameras[0]);
            cfg.front_camera_type = WebCamera;
        }
        if (cfg.back_camera_id.empty()) {
            cfg.back_camera_id = SDL_GetCameraName(cameras[0]);
            cfg.back_camera_type = WebCamera;
        }
    } else {
        cfg.front_camera_type = SolidColor;
        cfg.back_camera_type = SolidColor;
    }
}
