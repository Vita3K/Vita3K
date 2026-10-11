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

#include <deque>
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
    std::deque<uint32_t> pending_events;
    bool terminal_event_pending = false;

    static bool is_terminal_event(uint32_t id) {
        return id == SCE_IME_EVENT_PRESS_ENTER || id == SCE_IME_EVENT_PRESS_CLOSE;
    }

    bool queue_event(uint32_t id) {
        if (terminal_event_pending)
            return false;
        if (!is_terminal_event(id) && !pending_events.empty() && pending_events.back() == id)
            return true;
        pending_events.push_back(id);
        terminal_event_pending = is_terminal_event(id);
        return true;
    }

    void queue_submit_events() {
        if (terminal_event_pending)
            return;
        pending_events.push_back(SCE_IME_EVENT_PRESS_ENTER);
        pending_events.push_back(SCE_IME_EVENT_PRESS_CLOSE);
        terminal_event_pending = true;
    }

    void clear_pending_event() {
        pending_events.clear();
        terminal_event_pending = false;
    }

    void deinit() {
        state = false;
        edit_text = {};
        param = {};
        enter_label.clear();
        str.clear();
        caps_level = 0;
        caretIndex = 0;
        clear_pending_event();
    }
};