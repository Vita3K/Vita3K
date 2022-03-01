// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <gxm/types.h>
#include <shader/usse_translator_types.h>
#include <shader/usse_types.h>

namespace shader {

usse::GenericType translate_generic_type(const gxp::GenericParameterType &type);
std::tuple<usse::DataType, std::string> get_parameter_type_store_and_name(const SceGxmParameterType &type);
usse::ProgramInput get_program_input(const SceGxmProgram &program);
} // namespace shader