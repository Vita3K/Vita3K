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

#pragma once

#include <cstdint>

#include "args_layout.h"
#include "lay_out_args.h"
#include "read_arg.h"

namespace module {
class vargs {
    Address currentVaList;
    LayoutArgsState layoutState;
    ArgLayout currentLayout;

public:
    vargs() {}

    explicit vargs(LayoutArgsState layoutState)
        : layoutState(layoutState)
        , currentVaList(0) {
    }

    explicit vargs(Address addr)
        : layoutState()
        , currentVaList(addr) {
    }

    template <typename T>
    T next(CPUState &cpu, MemState &mem) {
        if (!currentVaList) {
            const auto state_tuple = add_arg_to_layout<T>(layoutState);

            layoutState = std::move(std::get<1>(state_tuple));
            currentLayout = std::move(std::get<0>(state_tuple));

            return read<T>(cpu, currentLayout, mem);
        } else {
            const auto out = *Ptr<T>(currentVaList).get(mem);
            currentVaList += sizeof(T);
            return out;
        }
    }
};

} // namespace module
