#pragma once

#include <touch/touch.h>

struct TouchState {
    SceTouchSamplingState touch_mode[2] = { SCE_TOUCH_SAMPLING_STATE_STOP, SCE_TOUCH_SAMPLING_STATE_STOP };
};
