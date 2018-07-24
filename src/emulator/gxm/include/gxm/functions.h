#pragma once

#include <psp2/gxm.h>

#include <string>

namespace texture {
    unsigned int get_width(const SceGxmTexture *texture);
    unsigned int get_height(const SceGxmTexture *texture);
    SceGxmTextureFormat get_format(const SceGxmTexture *texture);
    SceGxmTextureBaseFormat get_base_format(SceGxmTextureFormat src);
    bool is_paletted_format(SceGxmTextureFormat src);
}

const SceGxmProgramParameter *program_parameters(const SceGxmProgram &program);
std::string parameter_name_raw(const SceGxmProgramParameter &parameter);
std::string parameter_name(const SceGxmProgramParameter &parameter);
