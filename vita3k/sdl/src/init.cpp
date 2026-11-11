#include <sdl/functions.h>

#include <util/log.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_hints.h>

namespace sdl {

bool initialize(bool has_video) {
    // Joystick events on background thread
    SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");

    // Enable HIDAPI rumble for DS4/DS
    SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");

    // Enable Switch controller
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS, "1");

    Uint32 init_flags = SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC | SDL_INIT_SENSOR;
    if (has_video)
        init_flags |= SDL_INIT_VIDEO;

    if (!SDL_Init(init_flags))
        return false;

    if (!(SDL_WasInit(SDL_INIT_CAMERA) & SDL_INIT_CAMERA)
        && !SDL_InitSubSystem(SDL_INIT_CAMERA))
        LOG_WARN("SDL camera initialisation failed, continuing without it: {}", SDL_GetError());

    return true;
}

} // namespace sdl