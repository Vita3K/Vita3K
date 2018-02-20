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

#include <cpu/functions.h>
#include <mem/ptr.h>

// Given some types on the stack...
template <typename... Args>
struct StackLayout {
    // Compute the offset of a particular argument on the stack.
    template <size_t index>
    static constexpr size_t offset() {
        constexpr size_t sizes[] = { sizeof(Args)... };
        size_t size = 0;

        for (size_t i = 0; i < index; ++i) {
            size += sizes[i];
        }

        return size;
    }
};

// Simple case - argument can be cast from integer.
template <typename T>
struct RegArg {
    static T bridge(const MemState &, u32 value) {
        return static_cast<T>(value);
    }
};

// Emulated pointer constructed from address in register.
template <typename Pointee>
struct RegArg<Ptr<Pointee>> {
    static Ptr<Pointee> bridge(const MemState &, u32 value) {
        return Ptr<Pointee>(value);
    }
};

// Real pointer bridged from address in register.
template <typename Pointee>
struct RegArg<Pointee *> {
    static Pointee *bridge(const MemState &mem, u32 value) {
        const Ptr<Pointee> ptr(value);
        return ptr.get(mem);
    }
};

// Simple case - argument is on stack.
template <typename T>
struct StackArg {
    using Type = T;

    static T bridge(const MemState &, const Type &value) {
        return value;
    }
};

// Real pointer bridged from emulated pointer on stack.
template <typename Pointee>
struct StackArg<Pointee *> {
    using Type = Ptr<Pointee>;

    static Pointee *bridge(const MemState &mem, Ptr<Pointee> ptr) {
        return ptr.get(mem);
    }
};

// Syntactic sugar.
template <typename T>
using StackType = const typename StackArg<T>::Type;

// Simple case - all arguments in registers.
template <typename... Args>
struct ArgLayout {
    template <typename T, size_t index>
    static T read(CPUState &cpu, const MemState &mem) {
        const u32 value = read_reg(cpu, index);
        return RegArg<T>::bridge(mem, value);
    }
};

// Complex case - at least one argument has spilled onto stack.
template <typename R0, typename R1, typename R2, typename R3, typename StackHead, typename... StackTail>
struct ArgLayout<R0, R1, R2, R3, StackHead, StackTail...> {
    using StackLayout2 = StackLayout<StackType<StackHead>, StackType<StackTail>...>;

    template <typename T, size_t index>
        static typename std::enable_if < index<4, T>::type read(CPUState &cpu, const MemState &mem) {
        const u32 value = read_reg(cpu, index);
        return RegArg<T>::bridge(mem, value);
    }

    template <typename T, size_t index>
    static typename std::enable_if<index >= 4, T>::type read(CPUState &cpu, const MemState &mem) {
        using StackType = StackType<T>;

        constexpr size_t offset_on_stack = StackLayout2::template offset<index - 4>();
        const Address sp = read_sp(cpu);
        const Address address = static_cast<Address>(sp + offset_on_stack);
        const Ptr<StackType> ptr(address);
        const StackType &value_on_stack = *ptr.get(mem);
        const T bridged_value = StackArg<T>::bridge(mem, std::forward<StackType>(value_on_stack));
        return bridged_value;
    }
};
