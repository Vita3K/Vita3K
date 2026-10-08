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

#include <ime/state.h>

#include <string>
#include <utility>
#include <vector>

struct EmuEnvState;

// Whether the guest shows an IME dialog
bool is_ime_dialog_active(const EmuEnvState &emuenv);
// Whether the guest shows an IME, through sceIme or an IME dialog
bool is_any_ime_active(const EmuEnvState &emuenv);
// Closes the IME dialog with its current text
void finish_ime_dialog(EmuEnvState &emuenv);
// Closes the IME dialog without its text, if the guest made it cancelable
void cancel_ime_dialog(EmuEnvState &emuenv);
// Presses enter on the active IME. Returns false if there is none.
bool submit_current_ime(EmuEnvState &emuenv);
// Closes the active IME. Returns false if there is none, or it can't be closed.
bool dismiss_current_ime(EmuEnvState &emuenv);

void ime_commit_text(Ime &ime, const std::u16string &text);
void ime_set_preedit(Ime &ime, const std::u16string &preedit);

void ime_cursor_left(Ime &ime);
void ime_cursor_right(Ime &ime);
void ime_backspace(Ime &ime);

std::vector<std::pair<SceImeLanguage, std::string>>::const_iterator
get_ime_lang_index(Ime &ime, SceImeLanguage lang);