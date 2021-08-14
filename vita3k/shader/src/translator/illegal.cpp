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

#include <shader/usse_translator.h>

#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/SpvBuilder.h>

#include <shader/usse_decoder_helpers.h>
#include <shader/usse_disasm.h>
#include <shader/usse_types.h>
#include <util/log.h>

bool shader::usse::USSETranslatorVisitor::illegal22() {
    LOG_ERROR("Illegal shader opcode: 22");
    return false;
}

bool shader::usse::USSETranslatorVisitor::illegal23() {
    LOG_ERROR("Illegal shader opcode: 23");
    return false;
}

bool shader::usse::USSETranslatorVisitor::illegal24() {
    LOG_ERROR("Illegal shader opcode: 24");
    return false;
}

bool shader::usse::USSETranslatorVisitor::illegal27() {
    LOG_ERROR("Illegal shader opcode: 27");
    return false;
}