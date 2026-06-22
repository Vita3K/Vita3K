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

#include <lang/state.h>

#include "generated_catalog.h"

#include <array>

namespace lang {

static std::array<std::string, static_cast<size_t>(str::_count)> g_strings;

namespace {

constexpr size_t k_string_count = static_cast<size_t>(str::_count);
static_assert(generated::k_string_count == k_string_count, "Generated lang catalog is out of sync with lang::str.");

void apply_locale(const generated::LocaleData &locale) {
    for (size_t index = 0; index < k_string_count; ++index)
        g_strings[index] = locale.strings[index];
}

std::string_view locale_tag_for_sys_lang(int sys_lang) {
    using namespace std::literals;

    static constexpr std::array locale_tags = {
        "ja"sv,
        "en"sv,
        "fr"sv,
        "es"sv,
        "de"sv,
        "it"sv,
        "nl"sv,
        "pt-PT"sv,
        "ru"sv,
        "ko"sv,
        "zh-Hant"sv,
        "zh-Hans"sv,
        "fi"sv,
        "sv"sv,
        "da"sv,
        "no"sv,
        "pl"sv,
        "pt-BR"sv,
        "en-GB"sv,
        "tr"sv,
    };

    const auto index = static_cast<size_t>(sys_lang);
    return index < locale_tags.size() ? locale_tags[index] : "en"sv;
}

struct DefaultInit {
    DefaultInit() { reset_defaults(); }
};
static DefaultInit s_init;

} // namespace

const std::string &get(str key) {
    return g_strings[static_cast<size_t>(key)];
}

bool set_locale(std::string_view locale_tag) {
    if (locale_tag.empty()) {
        reset_defaults();
        return true;
    }

    const auto *locale = generated::find_locale(locale_tag);
    if (!locale) {
        reset_defaults();
        return false;
    }

    apply_locale(*locale);
    return true;
}

bool set_locale(int sys_lang) {
    return set_locale(locale_tag_for_sys_lang(sys_lang));
}

void reset_defaults() {
    apply_locale(generated::english_locale());
}

} // namespace lang
