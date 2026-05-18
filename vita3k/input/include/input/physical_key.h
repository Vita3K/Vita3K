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
//
// Canonical physical key identifiers in this file follow the W3C UI Events
// KeyboardEvent.code specification:
// https://w3c.github.io/uievents-code/

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace input {

enum class NativeScanCodeSet : uint8_t {
    Windows,
    Evdev,
    Xkb,
    MacOS,
};

enum class PhysicalKeyCode : uint32_t {
    Unbound = 0,
    Fn = 0x000012,
#define PHYSICAL_KEY_ENTRY(usb_usage, evdev, win, mac, code_name, enum_name) enum_name = usb_usage,
#include "input/physical_key_data.inc"
#undef PHYSICAL_KEY_ENTRY
};

PhysicalKeyCode physical_key_from_native_scan_code(NativeScanCodeSet scan_code_set, uint32_t native_scan_code);
std::string_view physical_key_code_name(PhysicalKeyCode code);
PhysicalKeyCode physical_key_from_code_name(std::string_view name);
std::string physical_key_display_name(PhysicalKeyCode code);

} // namespace input

namespace std {

template <>
struct hash<input::PhysicalKeyCode> {
    size_t operator()(const input::PhysicalKeyCode code) const noexcept {
        return hash<uint32_t>{}(static_cast<uint32_t>(code));
    }
};

} // namespace std
