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

void ime_commit_text(Ime &ime, const std::u16string &text);
void ime_set_preedit(Ime &ime, const std::u16string &preedit);

void ime_cursor_left(Ime &ime);
void ime_cursor_right(Ime &ime);
void ime_backspace(Ime &ime);

std::vector<std::pair<SceImeLanguage, std::string>>::const_iterator
get_ime_lang_index(Ime &ime, SceImeLanguage lang);