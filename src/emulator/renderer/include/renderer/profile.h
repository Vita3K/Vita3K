#pragma once

#include <microprofile.h>

#define R_PROFILE(name) MICROPROFILE_SCOPEI("renderer", name, MP_BLUE)
