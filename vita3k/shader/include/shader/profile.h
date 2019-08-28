#pragma once

#include <microprofile.h>

#define SHADER_PROFILE(name) MICROPROFILE_SCOPEI("SHADER", name, MP_BLUE)
