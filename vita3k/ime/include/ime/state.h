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

#include <ime/types.h>

#include <mutex>
#include <string>
#include <vector>

struct ImeLangState {
    std::vector<std::pair<SceImeLanguage, std::string>> ime_keyboards = {
        { SCE_IME_LANGUAGE_DANISH, "Danish" }, { SCE_IME_LANGUAGE_GERMAN, "German" },
        { SCE_IME_LANGUAGE_ENGLISH_GB, "English (United Kingdom)" }, { SCE_IME_LANGUAGE_ENGLISH_US, "English (United States)" },
        { SCE_IME_LANGUAGE_SPANISH, "Spanish" }, { SCE_IME_LANGUAGE_FRENCH, "French" },
        { SCE_IME_LANGUAGE_ITALIAN, "Italian" }, { SCE_IME_LANGUAGE_DUTCH, "Dutch" },
        { SCE_IME_LANGUAGE_NORWEGIAN, "Norwegian" }, { SCE_IME_LANGUAGE_POLISH, "Polish" },
        { SCE_IME_LANGUAGE_PORTUGUESE_BR, "Portuguese (Brazil)" }, { SCE_IME_LANGUAGE_PORTUGUESE_PT, "Portuguese (Portugal)" },
        { SCE_IME_LANGUAGE_RUSSIAN, "Russian" }, { SCE_IME_LANGUAGE_FINNISH, "Finnish" },
        { SCE_IME_LANGUAGE_SWEDISH, "Swedish" }, { SCE_IME_LANGUAGE_TURKISH, "Turkish" },
        { SCE_IME_LANGUAGE_JAPANESE, "Japanese" }, { SCE_IME_LANGUAGE_KOREAN, "Korean" },
        { SCE_IME_LANGUAGE_SIMPLIFIED_CHINESE, "Chinese (Simplified)" },
        { SCE_IME_LANGUAGE_TRADITIONAL_CHINESE, "Chinese (Traditional)" }
    };
};

struct Ime {
    ImeLangState lang;
    std::mutex mutex;

    bool state = false;
    SceImeEditText edit_text;
    SceImeParam param;
    std::string enter_label;
    std::u16string str;
    uint32_t caps_level = 0;
    uint32_t caretIndex = 0;
    uint32_t event_id = SCE_IME_EVENT_OPEN;

    void deinit() {
        state = false;
        edit_text = {};
        param = {};
        enter_label.clear();
        str.clear();
        caps_level = 0;
        caretIndex = 0;
        event_id = SCE_IME_EVENT_OPEN;
    }
};