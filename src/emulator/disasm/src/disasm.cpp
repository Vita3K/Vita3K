// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <disasm/functions.h>

#include <disasm/state.h>

#include <capstone.h>

#include <assert.h>
#include <sstream>

static void delete_insn(cs_insn *insn) {
    if (insn != nullptr) {
        cs_free(insn, 1);
    }
}

bool init(DisasmState &state) {
    cs_err err = cs_open(CS_ARCH_ARM, CS_MODE_THUMB, &state.csh);
    if (err != CS_ERR_OK) {
        return false;
    }

    cs_option(state.csh, CS_OPT_SKIPDATA, CS_OPT_ON);

    state.insn = InsnPtr(cs_malloc(state.csh), delete_insn);
    if (!state.insn) {
        return false;
    }

    return true;
}

std::string disassemble(DisasmState &state, const uint8_t *code, size_t size, uint64_t address, bool thumb) {
    const cs_err err = cs_option(state.csh, CS_OPT_MODE, thumb ? CS_MODE_THUMB : CS_MODE_ARM);
    assert(err == CS_ERR_OK);

    const bool success = cs_disasm_iter(state.csh, &code, &size, &address, state.insn.get());

    std::ostringstream out;
    out << state.insn->mnemonic << " " << state.insn->op_str;
    if (!success) {
        const cs_err err = cs_errno(state.csh);
        out << " (" << cs_strerror(err) << ")";
    }

    return out.str();
}
