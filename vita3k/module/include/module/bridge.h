// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include "bridge_types.h"
#include "call_result.h"
#include "exported_variable.h"
#include "lay_out_args.h"
#include "vargs.h"

#include <config/state.h>
#include <emuenv/state.h>

#include <type_traits>
#include <utility>

using ImportFn = CallResult (*)(EmuEnvState &emuenv, CPUState &cpu, SceUID thread_id);
using ImportVarFactory = std::function<std::optional<ExportedVariable>(EmuEnvState &emuenv, MemState &mem)>;
using LibraryInitFn = std::function<void(EmuEnvState &emuenv)>;

// Reads a scalar or vargs argument from the CPU state, applying the bridge types conversion.
template <ArgLayout Layout, LayoutArgsState VargsState, typename Arg>
Arg read_bridged_arg(CPUState &cpu) {
    if constexpr (std::is_same_v<Arg, module::vargs>) {
        return module::vargs{ VargsState };
    } else {
        using Arm = typename BridgeTypes<Arg>::ArmType;
        return BridgeTypes<Arg>::arm_to_host(read<Arm>(cpu, Layout, cpu.mem), cpu.mem);
    }
}

template <typename... Args, typename Fn>
CallResult invoke_bridged(CPUState &cpu, Fn &&function) {
    static constexpr auto layout = lay_out<typename BridgeTypes<Args>::ArmType...>();
    constexpr auto arr = std::get<0>(layout);
    constexpr auto vargs_state = std::get<1>(layout);
    return [&]<size_t... Is>(std::index_sequence<Is...>) -> CallResult {
        if constexpr (std::is_void_v<std::invoke_result_t<Fn, Args...>>) {
            function(read_bridged_arg<arr[Is], vargs_state, Args>(cpu)...);
            return {};
        } else {
            return call_result(function(read_bridged_arg<arr[Is], vargs_state, Args>(cpu)...));
        }
    }(std::index_sequence_for<Args...>{});
}

// Creates a bridge function that reads arguments from the CPU state and calls the export function.
template <auto ExportFn, typename Ret, typename... Args>
ImportFn make_bridge(const char *name, Ret (*)(EmuEnvState &, SceUID, const char *, Args...)) {
    static const char *export_name = name;

    return [](EmuEnvState &emuenv, CPUState &cpu, SceUID thread_id) {
#ifdef TRACY_ENABLE
        ZoneNamedC(___tracy_scoped_zone, 0xFFF34C, emuenv.cfg.tracy_primitive_impl);
        ZoneNameV(___tracy_scoped_zone, export_name, strlen(export_name));
#endif
        return invoke_bridged<Args...>(cpu, [&](Args... args) -> Ret {
            return ExportFn(emuenv, thread_id, export_name, args...);
        });
    };
}
