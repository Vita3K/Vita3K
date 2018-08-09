#pragma once

#include <microprofile.h>

#define SHADERGEN_PROFILE(name) MICROPROFILE_SCOPEI("SHADERGEN", name, MP_BLUE)
