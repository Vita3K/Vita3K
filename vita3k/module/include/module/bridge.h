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

#include "lay_out_args.h"
#include "read_arg.h"
#include "vargs.h"
#include "write_return_value.h"

#include <host/state.h>

#include <microprofile.h>

using ImportFn = std::function<void(HostState &host, CPUState &cpu, SceUID thread_id)>;
using ImportVarFactory = std::function<Address(HostState &host)>;

// Function returns a value that is written to CPU registers.
template <typename Ret, typename... Args, size_t... indices>
std::enable_if_t<!std::is_same_v<Ret, void>> call(Ret (*export_fn)(HostState &, SceUID, const char *, Args...), const char *export_name, const ArgsLayout<Args...> &args_layout, const LayoutArgsState &state, std::index_sequence<indices...>, SceUID thread_id, CPUState &cpu, HostState &host) {
    const Ret ret = (*export_fn)(host, thread_id, export_name, read<Args, indices, Args...>(cpu, args_layout, state, host.mem)...);
    write_return_value(cpu, ret);
}

// Function does not return a value.
template <typename... Args, size_t... indices>
void call(void (*export_fn)(HostState &, SceUID, const char *, Args...), const char *export_name, const ArgsLayout<Args...> &args_layout, const LayoutArgsState &state, std::index_sequence<indices...>, SceUID thread_id, CPUState &cpu, HostState &host) {
    (*export_fn)(host, thread_id, export_name, read<Args, indices, Args...>(cpu, args_layout, state, host.mem)...);
}

template <typename Ret, typename... Args>
ImportFn bridge(Ret (*export_fn)(HostState &, SceUID, const char *, Args...), const char *export_name) {
    constexpr std::tuple<ArgsLayout<Args...>, LayoutArgsState> args_layout = lay_out<typename BridgeTypes<Args>::ArmType...>();

    return [export_fn, export_name, args_layout](HostState &host, CPUState &cpu, SceUID thread_id) {
        MICROPROFILE_SCOPEI("HLE", export_name, MP_YELLOW);

        using Indices = std::index_sequence_for<Args...>;
        call(export_fn, export_name, std::get<0>(args_layout), std::get<1>(args_layout), Indices(), thread_id, cpu, host);
    };
}
