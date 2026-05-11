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
//
// Native scan-code mappings in this file are adapted from Chromium's keycode
// converter tables:
// - ui/events/keycodes/dom/keycode_converter.cc
// - ui/events/keycodes/dom/keycode_converter_data.inc
//
// Copyright 2013 The Chromium Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// 3. Neither the name of Google Inc. nor the names of its contributors may be
// used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <input/physical_key.h>

#include <array>
#include <cctype>

namespace input {
namespace {

struct PhysicalKeyEntry {
    PhysicalKeyCode code;
    uint16_t evdev_scan_code;
    uint32_t windows_scan_code;
    uint16_t mac_scan_code;
    std::string_view code_name;
};

constexpr std::array kPhysicalKeyEntries = {
    PhysicalKeyEntry{ PhysicalKeyCode::Fn, 0, 0, 0x003F, "Fn" },
#define PHYSICAL_KEY_ENTRY(usb_usage, evdev, win, mac, code_name, enum_name) \
    PhysicalKeyEntry{ PhysicalKeyCode::enum_name, evdev, win, mac, code_name },
#include "input/physical_key_data.inc"
#undef PHYSICAL_KEY_ENTRY
};

const PhysicalKeyEntry *find_entry(const PhysicalKeyCode code) {
    for (const auto &entry : kPhysicalKeyEntries) {
        if (entry.code == code)
            return &entry;
    }
    return nullptr;
}

const PhysicalKeyEntry *find_entry(std::string_view name) {
    for (const auto &entry : kPhysicalKeyEntries) {
        if (entry.code_name == name)
            return &entry;
    }
    return nullptr;
}

std::string prettify_code_name(std::string_view name) {
    std::string pretty;
    pretty.reserve(name.size() + 4);

    for (size_t i = 0; i < name.size(); ++i) {
        const char current = name[i];
        const char previous = i > 0 ? name[i - 1] : '\0';
        const bool previous_is_lower = std::islower(static_cast<unsigned char>(previous)) != 0;
        const bool previous_is_alpha = std::isalpha(static_cast<unsigned char>(previous)) != 0;
        const bool current_is_upper = std::isupper(static_cast<unsigned char>(current)) != 0;
        const bool current_is_digit = std::isdigit(static_cast<unsigned char>(current)) != 0;
        if (i > 0 && ((current_is_upper && previous_is_lower) || (current_is_digit && previous_is_alpha)))
            pretty.push_back(' ');
        pretty.push_back(current);
    }

    return pretty;
}

} // namespace

PhysicalKeyCode physical_key_from_native_scan_code(const NativeScanCodeSet scan_code_set, const uint32_t native_scan_code) {
    if (native_scan_code == 0)
        return PhysicalKeyCode::Unbound;

    for (const auto &entry : kPhysicalKeyEntries) {
        switch (scan_code_set) {
        case NativeScanCodeSet::Windows:
            if (entry.windows_scan_code == native_scan_code)
                return entry.code;
            break;
        case NativeScanCodeSet::Evdev:
            if (entry.evdev_scan_code == native_scan_code)
                return entry.code;
            break;
        case NativeScanCodeSet::Xkb:
            if (entry.evdev_scan_code != 0 && native_scan_code == entry.evdev_scan_code + 8)
                return entry.code;
            break;
        case NativeScanCodeSet::MacOS:
            if (entry.mac_scan_code == native_scan_code)
                return entry.code;
            break;
        }
    }

    return PhysicalKeyCode::Unbound;
}

std::string_view physical_key_code_name(const PhysicalKeyCode code) {
    if (code == PhysicalKeyCode::Unbound)
        return {};

    if (const auto *entry = find_entry(code))
        return entry->code_name;

    return {};
}

PhysicalKeyCode physical_key_from_code_name(const std::string_view name) {
    if (name.empty() || name == "Unbound")
        return PhysicalKeyCode::Unbound;

    if (const auto *entry = find_entry(name))
        return entry->code;

    return PhysicalKeyCode::Unbound;
}

std::string physical_key_display_name(const PhysicalKeyCode code) {
    if (code == PhysicalKeyCode::Unbound)
        return "Unbound";

    const auto name = physical_key_code_name(code);
    if (name.empty())
        return "Unknown";

    if (name.size() == 4 && name.rfind("Key", 0) == 0)
        return std::string(1, name[3]);

    if (name.size() == 6 && name.rfind("Digit", 0) == 0)
        return std::string(1, name[5]);

    if (name.size() >= 2 && name[0] == 'F') {
        bool function_key = true;
        for (size_t i = 1; i < name.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(name[i]))) {
                function_key = false;
                break;
            }
        }
        if (function_key)
            return std::string(name);
    }

    return prettify_code_name(name);
}

} // namespace input
