/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * This software may be used and distributed according to the terms of the GNU
 * General Public License version 2 or any later version.
 */

// Stripped-down version of dynarmic's src/common/mp.h that only supports providing arg count

#pragma once

namespace util {

/// Used to provide information about an arbitrary function.
template <typename Function>
struct FunctionInfo : public FunctionInfo<decltype(&Function::operator())> {};

/**
* Partial specialization for function types.
*
* This is used as the supporting base for all other specializations.
*/
template <typename R, typename... Args>
struct FunctionInfo<R(Args...)> {
    static constexpr size_t args_count = sizeof...(Args);
};

/// Partial specialization for function pointers
template <typename R, typename... Args>
struct FunctionInfo<R (*)(Args...)> : public FunctionInfo<R(Args...)> {};

/// Partial specialization for member function pointers.
template <typename C, typename R, typename... Args>
struct FunctionInfo<R (C::*)(Args...)> : public FunctionInfo<R(Args...)> {};

/// Partial specialization for const member function pointers.
template <typename C, typename R, typename... Args>
struct FunctionInfo<R (C::*)(Args...) const> : public FunctionInfo<R(Args...)> {};

} // namespace util

// namespace util
