#pragma once

#include <microprofile.h>

#define GXM_PROFILE(name) MICROPROFILE_SCOPEI("GXM", name, MP_BLUE)
