// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#include "lay_out_args.h"
#include "read_arg.h"
#include "write_return_value.h"

#include <config/state.h>
#include <emuenv/state.h>

using ImportFn = std::function<void(EmuEnvState &emuenv, CPUState &cpu, SceUID thread_id)>;
using ImportVarFactory = std::function<Address(EmuEnvState &emuenv)>;
using LibraryInitFn = std::function<void(EmuEnvState &emuenv)>;

// Function returns a value that is written to CPU registers.
template <typename Ret, typename... Args, size_t... indices>
std::enable_if_t<!std::is_same_v<Ret, void>> call(Ret (*export_fn)(EmuEnvState &, SceUID, const char *, Args...), const char *export_name, const ArgsLayout<Args...> &args_layout, const LayoutArgsState &state, std::index_sequence<indices...>, SceUID thread_id, CPUState &cpu, EmuEnvState &emuenv) {
    const Ret ret = (*export_fn)(emuenv, thread_id, export_name, read<Args, indices, Args...>(cpu, args_layout, state, emuenv.mem)...);
    write_return_value(cpu, ret);
}

// Function does not return a value.
template <typename... Args, size_t... indices>
void call(void (*export_fn)(EmuEnvState &, SceUID, const char *, Args...), const char *export_name, const ArgsLayout<Args...> &args_layout, const LayoutArgsState &state, std::index_sequence<indices...>, SceUID thread_id, CPUState &cpu, EmuEnvState &emuenv) {
    (*export_fn)(emuenv, thread_id, export_name, read<Args, indices, Args...>(cpu, args_layout, state, emuenv.mem)...);
}

template <typename Ret, typename... Args>
ImportFn bridge(Ret (*export_fn)(EmuEnvState &, SceUID, const char *, Args...), const char *export_name) {
    constexpr std::tuple<ArgsLayout<Args...>, LayoutArgsState> args_layout = lay_out<typename BridgeTypes<Args>::ArmType...>();

    return [export_fn, export_name, args_layout](EmuEnvState &emuenv, CPUState &cpu, SceUID thread_id) {
#ifdef TRACY_ENABLE
        ZoneNamedC(___tracy_scoped_zone, 0xFFF34C, emuenv.cfg.tracy_primitive_impl); // Tracy - Track function scope and set color to yellow
        ZoneNameV(___tracy_scoped_zone, export_name, strlen(export_name)); // Tracy - Edit scope name based on export_name
#endif

        using Indices = std::index_sequence_for<Args...>;
        call(export_fn, export_name, std::get<0>(args_layout), std::get<1>(args_layout), Indices(), thread_id, cpu, emuenv);
    };
}
