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

#include <cstdint>
#include <string>
#include <string_view>

namespace lang {

// Translatable string keys defined in lang/strings.def.
enum class str : uint16_t {
#define LANG_STRING(name, key) name,
#include <lang/strings.def>
#undef LANG_STRING

    _count
};

// Get a translated string. Returns English default if no translation set.
const std::string &get(str key);

// Select a catalog by locale tag or Vita sys_lang enum value.
bool set_locale(std::string_view locale_tag);
bool set_locale(int sys_lang);

// Reset all strings to English defaults.
void reset_defaults();

} // namespace lang
