// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include "private.h"

#include <config/functions.h>
#include <config/state.h>
#include <ime/state.h>
#include <util/string_utils.h>

namespace gui {

std::vector<std::pair<SceImeLanguage, std::string>>::const_iterator get_ime_lang_index(Ime &ime, const SceImeLanguage &lang) {
    const auto &ime_lang_index = std::find_if(ime.lang.ime_keyboards.begin(), ime.lang.ime_keyboards.end(), [&](const auto &l) {
        return l.first == lang;
    });

    return ime_lang_index;
}

static const std::map<int, std::vector<std::u16string>> special_key = {
    { 1, { u"!", u"?", u"\"", u"'", u"#", u"%" } },
    { 2, { u"(", u")", u"()", u"_", u",", u";" } },
    { 3, { u"*", u"+", u"=", u"&", u"<", u">" } },
    { 4, { u"[", u"]", u"[]", u"{", u"}", u"{}" } },
    { 5, { u"\\", u"|", u"^", u"`", u"$", u"¥" } },
    { 6, { u"´", u"‘", u"’", u"‚", u"“", u"”" } },
    { 7, { u"„", u"~", u"¡", u"¡!", u"¿", u"¿?" } },
    { 8, { u"‹", u"›", u"«", u"»", u"°", u"ª" } },
    { 9, { u"º", u"×", u"÷", u"¤", u"¢", u"€" } },
    { 10, { u"£", u"₩", u"§", u"¦", u"µ", u"¬" } },
    { 11, { u"¹", u"²", u"³", u"¼", u"½", u"¾" } },
    { 12, { u"№", u"·" } }
};

static const std::map<int, std::vector<std::u16string>> pad_numeric_key = {
    { 1, { u"7", u"8", u"9", u"-", u"/" } },
    { 2, { u"4", u"5", u"6", u":", u"@" } },
    { 3, { u"1", u"2", u"3" } }
};

enum CapsLevel {
    NO = 0,
    YES = 1,
    LOCK = 2
};

enum Row {
    FIRST = 1,
    SECOND = 2,
    THIRD = 3
};

static void update_str(Ime &ime, const std::u16string &key) {
    if (ime.str.empty())
        ime.edit_text.preeditIndex = 0;
    ime.edit_text.editIndex = ime.edit_text.caretIndex;
    if (ime.str.length() < ime.param.maxTextLength) {
        ime.str.insert(ime.edit_text.caretIndex, key);
        ime.edit_text.caretIndex++;
    }
    if (ime.caps_level == YES)
        ime.caps_level = NO;
    ime.event_id = SCE_IME_EVENT_UPDATE_TEXT;
}

static void update_key(Ime &ime, const std::u16string &key) {
    update_str(ime, key);
    ime.edit_text.editLengthChange = ime.edit_text.preeditLength += (int32_t)key.length();
}

static void reset_preedit(Ime &ime) {
    ime.caretIndex = ime.edit_text.caretIndex;
    ime.edit_text.preeditIndex = ime.edit_text.caretIndex;
    ime.edit_text.preeditLength = ime.edit_text.editLengthChange = 0;
    ime.event_id = SCE_IME_EVENT_UPDATE_TEXT;
}

static void update_ponct(Ime &ime, const std::u16string &key) {
    update_str(ime, key);
    reset_preedit(ime);
}

static std::map<int, float> lang_keyboard_pos;
static std::map<int, std::vector<std::u16string>> lang_key, shift_lang_key, punct;
static std::map<int, std::string> second_keyboard;
static std::string space_str;
static float size_key, size_button;
static Row current_keyboard = FIRST;

static void set_second_keyboard(Ime &ime) {
    lang_keyboard_pos = { { FIRST, 13.f }, { SECOND, 60.f }, { THIRD, 154.f } };
    lang_key = {
        { FIRST, { u"q", u"w", u"e", u"r", u"t", u"y", u"u", u"i", u"o", u"p" } },
        { SECOND, { u"a", u"s", u"d", u"f", u"g", u"h", u"j", u"k", u"l" } },
        { THIRD, { u"z", u"x", u"c", u"v", u"b", u"n", u"m" } }
    };
    shift_lang_key = {
        { FIRST, { u"Q", u"W", u"E", u"R", u"T", u"Y", u"U", u"I", u"O", u"P" } },
        { SECOND, { u"A", u"S", u"D", u"F", u"G", u"H", u"J", u"K", u"L" } },
        { THIRD, { u"Z", u"X", u"C", u"V", u"B", u"N", u"M" } }
    };
    size_button = 135.f;
    size_key = 88.f;
}

void init_ime_lang(Ime &ime, const SceImeLanguage &lang) {
    second_keyboard.clear();
    // Set Punctuation
    switch (lang) {
    case SCE_IME_LANGUAGE_SPANISH:
        punct = { { FIRST, { u",", u"¿?" } }, { SECOND, { u".", u"¡!" } } };
        break;
    default:
        punct = { { FIRST, { u",", u"?" } }, { SECOND, { u".", u"!" } } };
        break;
    }

    // Set Space String
    switch (lang) {
    case SCE_IME_LANGUAGE_DANISH:
        space_str = "Mellemrum";
        break;
    case SCE_IME_LANGUAGE_GERMAN:
        space_str = "Leerstelle";
        break;
    case SCE_IME_LANGUAGE_SPANISH:
        space_str = "Espacio";
        break;
    case SCE_IME_LANGUAGE_FRENCH:
        space_str = "Espace";
        break;
    case SCE_IME_LANGUAGE_ITALIAN:
        space_str = "Spazio";
        break;
    case SCE_IME_LANGUAGE_DUTCH:
        space_str = "Spatie";
        break;
    case SCE_IME_LANGUAGE_NORWEGIAN:
        space_str = "Mellomrom";
        break;
    case SCE_IME_LANGUAGE_POLISH:
        space_str = "Spacja";
        break;
    case SCE_IME_LANGUAGE_PORTUGUESE_BR:
    case SCE_IME_LANGUAGE_PORTUGUESE_PT:
        space_str = reinterpret_cast<const char *>(u8"Espaço");
        break;
    case SCE_IME_LANGUAGE_RUSSIAN:
        space_str = reinterpret_cast<const char *>(u8"П р о б е л");
        break;
    case SCE_IME_LANGUAGE_FINNISH:
        space_str = reinterpret_cast<const char *>(u8"Välilyönti");
        break;
    case SCE_IME_LANGUAGE_SWEDISH:
        space_str = "Blanksteg";
        break;
    case SCE_IME_LANGUAGE_TURKISH:
        space_str = reinterpret_cast<const char *>(u8"Boşluk");
        break;
    default:
        space_str = "Space";
        break;
    }

    // Set Keyboard Pos
    switch (lang) {
    case SCE_IME_LANGUAGE_FRENCH:
        lang_keyboard_pos = { { FIRST, 13.f }, { SECOND, 13.f }, { THIRD, 201.f } };
        break;
    case SCE_IME_LANGUAGE_RUSSIAN:
        lang_keyboard_pos = { { FIRST, 13.f }, { SECOND, 13.f }, { THIRD, 130.5f } };
        break;
    default:
        lang_keyboard_pos = { { FIRST, 13.f }, { SECOND, 60.f }, { THIRD, 154.f } };
        break;
    }

    // Set Keyboard Key
    switch (lang) {
    case SCE_IME_LANGUAGE_GERMAN:
        lang_key = {
            { FIRST, { u"q", u"w", u"e", u"r", u"t", u"z", u"u", u"i", u"o", u"p" } },
            { SECOND, { u"a", u"s", u"d", u"f", u"g", u"h", u"j", u"k", u"l" } },
            { THIRD, { u"y", u"x", u"c", u"v", u"b", u"n", u"m" } }
        };
        shift_lang_key = {
            { FIRST, { u"Q", u"W", u"E", u"R", u"T", u"Z", u"U", u"I", u"O", u"P" } },
            { SECOND, { u"A", u"S", u"D", u"F", u"G", u"H", u"J", u"K", u"L" } },
            { THIRD, { u"Y", u"X", u"C", u"V", u"B", u"N", u"M" } }
        };
        break;
    case SCE_IME_LANGUAGE_FRENCH:
        lang_key = {
            { FIRST, { u"a", u"z", u"e", u"r", u"t", u"y", u"u", u"i", u"o", u"p" } },
            { SECOND, { u"q", u"s", u"d", u"f", u"g", u"h", u"j", u"k", u"l", u"m" } },
            { THIRD, { u"w", u"x", u"c", u"v", u"b", u"n" } }
        };
        shift_lang_key = {
            { FIRST, { u"A", u"Z", u"E", u"R", u"T", u"Y", u"U", u"I", u"O", u"P" } },
            { SECOND, { u"Q", u"S", u"D", u"F", u"G", u"H", u"J", u"K", u"L", u"M" } },
            { THIRD, { u"W", u"X", u"C", u"V", u"B", u"N" } }
        };
        break;
    case SCE_IME_LANGUAGE_RUSSIAN:
        second_keyboard = { { FIRST, "ABC" }, { SECOND, reinterpret_cast<const char *>(u8"РУ") } };
        lang_key = {
            { FIRST, { u"й", u"ц", u"у", u"к", u"е", u"н", u"г", u"ш", u"щ", u"з", u"х", u"ъ" } },
            { SECOND, { u"ё", u"ф", u"ы", u"в", u"а", u"п", u"р", u"о", u"л", u"д", u"ж", u"э" } },
            { THIRD, { u"я", u"ч", u"с", u"м", u"и", u"т", u"ь", u"б", u"ю" } }
        };
        shift_lang_key = {
            { FIRST, { u"Й", u"Ц", u"У", u"К", u"Е", u"Н", u"Г", u"Ш", u"Щ", u"З", u"Х", u"Ъ" } },
            { SECOND, { u"Ё", u"Ф", u"Ы", u"В", u"А", u"П", u"Р", u"О", u"Л", u"Д", u"Ж", u"Э" } },
            { THIRD, { u"Я", u"Ч", u"С", u"М", u"И", u"Т", u"Ь", u"Б", u"Ю" } }
        };
        break;
    default:
        lang_key = {
            { FIRST, { u"q", u"w", u"e", u"r", u"t", u"y", u"u", u"i", u"o", u"p" } },
            { SECOND, { u"a", u"s", u"d", u"f", u"g", u"h", u"j", u"k", u"l" } },
            { THIRD, { u"z", u"x", u"c", u"v", u"b", u"n", u"m" } }
        };
        shift_lang_key = {
            { FIRST, { u"Q", u"W", u"E", u"R", u"T", u"Y", u"U", u"I", u"O", u"P" } },
            { SECOND, { u"A", u"S", u"D", u"F", u"G", u"H", u"J", u"K", u"L" } },
            { THIRD, { u"Z", u"X", u"C", u"V", u"B", u"N", u"M" } }
        };
        break;
    }

    // Set Key Size
    switch (lang) {
    case SCE_IME_LANGUAGE_RUSSIAN:
        size_button = 111.5f;
        size_key = 868.f / 12.f;
        break;
    default:
        size_button = 135.f;
        size_key = 88.f;
        break;
    }
}

static std::map<int, float> key_row_pos = { { FIRST, 11.f }, { SECOND, 69.f }, { THIRD, 127.f } };

void draw_ime(Ime &ime, EmuEnvState &emuenv) {
    static bool numeric_pad = false;
    static float scroll_special_current;
    static float scroll_special_max;

    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    const auto BUTTON_HEIGHT_SIZE = 52.f * SCALE.y;
    const auto PUNCT_BUTTON_SIZE = ImVec2(56.f * SCALE.x, BUTTON_HEIGHT_SIZE);
    const auto KEY_BUTTON_SIZE = ImVec2(size_key * SCALE.x, BUTTON_HEIGHT_SIZE);
    const auto ENTER_BUTTON_SIZE = ImVec2(135.f * SCALE.x, BUTTON_HEIGHT_SIZE);
    const auto BUTTON_SIZE = numeric_pad ? ENTER_BUTTON_SIZE : ImVec2(size_button * SCALE.x, BUTTON_HEIGHT_SIZE);
    const auto SPACE = 6.f * SCALE.x;
    const auto MARGE_BORDER = 13.f * SCALE.x;
    const auto LAST_ROW_KEY_POS = 185.f * SCALE.y;
    const auto BUTTON_POS_X = VIEWPORT_SIZE.x - MARGE_BORDER - BUTTON_SIZE.x;
    const auto ENTER_BUTTON_POS_X = VIEWPORT_SIZE.x - MARGE_BORDER - ENTER_BUTTON_SIZE.x;
    const auto NUM_BUTTON_SIZE = ImVec2(74.f * SCALE.x, BUTTON_HEIGHT_SIZE);
    const auto NUM_BUTTON_POS_X = BUTTON_POS_X - (3 * NUM_BUTTON_SIZE.x) - (SPACE * 3);
    const auto SPACE_BUTTON_SIZE = ImVec2(numeric_pad ? 216.f * SCALE.x : 276.f * SCALE.x, KEY_BUTTON_SIZE.y);
    const auto SPACE_BUTTON_POS = ImVec2(numeric_pad ? NUM_BUTTON_POS_X - SPACE - SPACE_BUTTON_SIZE.x : ENTER_BUTTON_POS_X - (PUNCT_BUTTON_SIZE.x * 3.f) - SPACE_BUTTON_SIZE.x - (SPACE * 4.f), LAST_ROW_KEY_POS);
    const auto is_shift = ime.caps_level != NO;

    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + VIEWPORT_SIZE.y - (248.f * SCALE.y));
    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, 248.f * SCALE.y);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, numeric_pad ? IME_NUMERIC_BG : GUI_SMOOTH_GRAY);
    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##ime", &ime.state, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
    ImGui::PushStyleColor(ImGuiCol_Button, GUI_COLOR_TEXT);
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_BLACK);
    ImGui::SetWindowFontScale(RES_SCALE.x);
    if (numeric_pad) {
        ImGui::SetCursorPosX(MARGE_BORDER);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 62.f * SCALE.y);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 10.f * SCALE.y);
        ImGui::VSliderFloat("##scroll_special", ImVec2(42.f * SCALE.x, 140.f * SCALE.y), &scroll_special_current, scroll_special_max, 0, "");
        ImGui::PopStyleVar(2);
        ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + (74.f * SCALE.x), WINDOW_POS.y));
        ImGui::BeginChild("##special_key", ImVec2(488.f * SCALE.x, 178.f * SCALE.y), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);
        const auto scroll_value = ImGui::GetIO().MouseWheel * 20.f;
        if (ImGui::GetIO().MouseWheel == 1)
            scroll_special_current -= std::min(scroll_value, scroll_special_current);
        else
            scroll_special_current += std::min(-scroll_value, scroll_special_max - scroll_special_current);
        ImGui::SetScrollY(scroll_special_current);
        scroll_special_max = ImGui::GetScrollMaxY();
        ImGui::PushStyleColor(ImGuiCol_Button, IME_NUMERIC_BG);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
        for (const auto &special : special_key) {
            for (auto i = 0; i < special.second.size(); i++) {
                if (!i) {
                    ImGui::SetCursorPosX(0.f);
                    if (special.first)
                        ImGui::Separator();
                }
                if (ImGui::Button(string_utils::utf16_to_utf8(special.second[i]).c_str(), NUM_BUTTON_SIZE) && (ime.str.length() < ime.param.maxTextLength))
                    update_key(ime, special.second[i]);
                if (i != (special.second.size() - 1))
                    ImGui::SameLine(0, SPACE);
            }
        }
        ImGui::PopStyleColor(2);
        ImGui::EndChild();
        ImGui::SetCursorPos(ImVec2(NUM_BUTTON_POS_X, key_row_pos[1] * SCALE.y));
        for (const auto &numeric : pad_numeric_key) {
            for (uint32_t i = 0; i < numeric.second.size(); i++) {
                if (!i)
                    ImGui::SetCursorPos(ImVec2(NUM_BUTTON_POS_X, key_row_pos[numeric.first] * SCALE.y));
                if (ImGui::Button(string_utils::utf16_to_utf8(numeric.second[i]).c_str(), ImVec2(i < 3 ? NUM_BUTTON_SIZE.x : 65.f * SCALE.x, NUM_BUTTON_SIZE.y)) && (ime.str.length() < ime.param.maxTextLength))
                    update_key(ime, numeric.second[i]);
                if (i != (numeric.second.size() - 1))
                    ImGui::SameLine(0, SPACE);
            }
        }
        ImGui::SetCursorPos(ImVec2(NUM_BUTTON_POS_X, LAST_ROW_KEY_POS));
        if (ImGui::Button("0", ImVec2((NUM_BUTTON_SIZE.x * 2.f) + SPACE, NUM_BUTTON_SIZE.y)) && (ime.str.length() < ime.param.maxTextLength))
            update_key(ime, u"0");
        ImGui::SameLine(0, SPACE);
        if (ImGui::Button(".", NUM_BUTTON_SIZE) && (ime.str.length() < ime.param.maxTextLength))
            update_ponct(ime, u".");
    } else {
        for (const auto &keyboard : is_shift ? shift_lang_key : lang_key) {
            for (uint32_t i = 0; i < keyboard.second.size(); i++) {
                if (!i)
                    ImGui::SetCursorPos(ImVec2(lang_keyboard_pos[keyboard.first] * SCALE.x, key_row_pos[keyboard.first] * SCALE.y));
                if (ImGui::Button(string_utils::utf16_to_utf8(keyboard.second[i]).c_str(), KEY_BUTTON_SIZE) && (ime.str.length() < ime.param.maxTextLength))
                    update_key(ime, keyboard.second[i]);
                if (i != (keyboard.second.size() - 1))
                    ImGui::SameLine(0, SPACE);
            }
        }
        ImGui::SetCursorPos(ImVec2(MARGE_BORDER, key_row_pos[3] * SCALE.y));
        ImGui::PushStyleColor(ImGuiCol_Button, IME_BUTTON_BG);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
        if (ImGui::Button("Shift", BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_l2))) {
            if (ime.edit_text.caretIndex == 0)
                ime.caps_level = ime.caps_level == YES ? NO : ++ime.caps_level;
            else
                ime.caps_level = ime.caps_level == LOCK ? NO : ++ime.caps_level;
        }
        ImGui::PopStyleColor(2);
        ImGui::SetCursorPos(ImVec2(SPACE_BUTTON_POS.x - (PUNCT_BUTTON_SIZE.x * 2.f) - (SPACE * 2.f), SPACE_BUTTON_POS.y));
        ImGui::PushStyleColor(ImGuiCol_Button, GUI_COLOR_TEXT);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_BLACK);
        const auto &ponct_1 = is_shift ? punct[FIRST][YES] : punct[FIRST][NO];
        if (ImGui::Button(string_utils::utf16_to_utf8(ponct_1).c_str(), PUNCT_BUTTON_SIZE))
            update_ponct(ime, ponct_1);
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, SPACE);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
        ImGui::PushStyleColor(ImGuiCol_Button, IME_BUTTON_BG);
        if (ImGui::Button("<", PUNCT_BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_l1))) {
            ime.edit_text.editIndex = ime.edit_text.caretIndex;
            if (ime.edit_text.caretIndex)
                --ime.edit_text.caretIndex;
            if (ime.edit_text.editIndex == ime.edit_text.preeditIndex)
                reset_preedit(ime);
            else if (ime.caretIndex) {
                --ime.caretIndex;
                ime.event_id = SCE_IME_EVENT_UPDATE_CARET;
            }
            if ((ime.edit_text.caretIndex == 0) && (ime.caps_level == NO))
                ime.caps_level = YES;
        }
        ImGui::SameLine(0, SPACE_BUTTON_SIZE.x + (SPACE * 2.f));
        if (ImGui::Button(">", PUNCT_BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_r1))) {
            ime.edit_text.editIndex = ime.edit_text.caretIndex;
            if (ime.edit_text.caretIndex < ime.str.length())
                ++ime.edit_text.caretIndex;
            if (ime.edit_text.editIndex == (ime.edit_text.preeditIndex + ime.edit_text.preeditLength))
                reset_preedit(ime);
            else if (ime.caretIndex < ime.str.length()) {
                ime.caretIndex++;
                ime.event_id = SCE_IME_EVENT_UPDATE_CARET;
            }
            if (ime.edit_text.caretIndex && ime.caps_level != LOCK)
                ime.caps_level = NO;
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, SPACE);
        ImGui::PushStyleColor(ImGuiCol_Button, GUI_COLOR_TEXT);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_BLACK);
        const auto &ponct_2 = is_shift ? punct[SECOND][YES] : punct[SECOND][NO];
        if (ImGui::Button(string_utils::utf16_to_utf8(ponct_2).c_str(), PUNCT_BUTTON_SIZE))
            update_ponct(ime, ponct_2);
        ImGui::PopStyleColor(2);
        if (!second_keyboard.empty()) {
            ImGui::SameLine(0, SPACE);
            ImGui::PushStyleColor(ImGuiCol_Button, IME_BUTTON_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
            if (ImGui::Button(second_keyboard[current_keyboard].c_str(), PUNCT_BUTTON_SIZE)) {
                const auto is_first = current_keyboard == FIRST;
                if (is_first)
                    set_second_keyboard(ime);
                else
                    init_ime_lang(ime, SceImeLanguage(emuenv.cfg.current_ime_lang));
                current_keyboard = is_first ? SECOND : FIRST;
            }
            ImGui::PopStyleColor(2);
        }
    }
    ImGui::PopStyleColor(2);
    ImGui::SetCursorPos(ImVec2(BUTTON_POS_X, key_row_pos[3] * SCALE.y));
    ImGui::PushStyleColor(ImGuiCol_Button, IME_BUTTON_BG);
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
    if ((ImGui::Button("Backspace", BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_square))) && ime.edit_text.caretIndex) {
        ime.str.erase(ime.edit_text.caretIndex - 1, 1);
        ime.edit_text.editIndex = ime.edit_text.caretIndex;
        --ime.edit_text.caretIndex;
        if (ime.edit_text.preeditIndex > ime.edit_text.caretIndex)
            ime.edit_text.preeditIndex = ime.caretIndex = ime.edit_text.caretIndex;
        if (ime.edit_text.preeditLength)
            --ime.edit_text.editLengthChange;
        if (ime.edit_text.preeditLength)
            --ime.edit_text.preeditLength;
        ime.event_id = SCE_IME_EVENT_UPDATE_TEXT;
        if (ime.str.empty() && (ime.caps_level == NO))
            ime.caps_level = YES;
    }
    ImGui::PopStyleColor(2);
    ImGui::PushStyleColor(ImGuiCol_Button, GUI_COLOR_TEXT_BLACK);
    ImGui::SetCursorPos(ImVec2(MARGE_BORDER, LAST_ROW_KEY_POS));
    if (ImGui::Button("V", ImVec2(44.f * SCALE.x, KEY_BUTTON_SIZE.y)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle)))
        ime.event_id = SCE_IME_EVENT_PRESS_CLOSE;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 18.f);
    ImGui::PushStyleColor(ImGuiCol_Button, IME_BUTTON_BG);
    if (ImGui::Button(numeric_pad ? "ABC" : "@123", emuenv.cfg.ime_langs.size() > 1 ? NUM_BUTTON_SIZE : BUTTON_SIZE))
        numeric_pad = !numeric_pad;
    if (!numeric_pad && emuenv.cfg.ime_langs.size() > 1) {
        ImGui::SameLine(0, SPACE);
        if (ImGui::Button("S/K", PUNCT_BUTTON_SIZE))
            ImGui::OpenPopup("S/K");
        if (ImGui::BeginPopup("S/K", ImGuiWindowFlags_NoMove)) {
            for (const auto &lang : emuenv.cfg.ime_langs) {
                if (ImGui::MenuItem(get_ime_lang_index(ime, SceImeLanguage(lang))->second.c_str(), nullptr, emuenv.cfg.current_ime_lang == lang)) {
                    init_ime_lang(ime, SceImeLanguage(lang));
                    emuenv.cfg.current_ime_lang = lang;
                    config::serialize_config(emuenv.cfg, emuenv.config_path);
                }
            }
            ImGui::EndPopup();
        }
    }
    ImGui::SetCursorPos(SPACE_BUTTON_POS);
    if (ImGui::Button(space_str.c_str(), SPACE_BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_triangle)))
        update_ponct(ime, u" ");
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(ImVec2(ENTER_BUTTON_POS_X, LAST_ROW_KEY_POS));
    ImGui::PushStyleColor(ImGuiCol_Button, GUI_PROGRESS_BAR);
    if (ImGui::Button(ime.enter_label.c_str(), ENTER_BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_r2)))
        ime.event_id = SCE_IME_EVENT_PRESS_ENTER;
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleColor();
}

} // namespace gui
