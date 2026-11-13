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

#include <ime/functions.h>

#include <dialog/state.h>
#include <emuenv/state.h>
#include <util/string_utils.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

bool is_ime_dialog_active(const EmuEnvState &emuenv) {
    return emuenv.common_dialog.type == IME_DIALOG
        && emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING;
}

bool is_any_ime_active(const EmuEnvState &emuenv) {
    return emuenv.ime.state || is_ime_dialog_active(emuenv);
}

void finish_ime_dialog(EmuEnvState &emuenv) {
    auto &dialog = emuenv.common_dialog;
    auto &ime = emuenv.ime;

    std::lock_guard<std::recursive_mutex> dialog_lock(dialog.mutex);
    std::lock_guard<std::mutex> ime_lock(ime.mutex);

    const size_t copy_len = std::min(static_cast<size_t>(ime.str.length()),
        static_cast<size_t>(dialog.ime.max_length));
    if (dialog.ime.result) {
        std::memcpy(dialog.ime.result, ime.str.c_str(), copy_len * sizeof(uint16_t));
        dialog.ime.result[copy_len] = 0;
    }

    const std::string utf8 = string_utils::utf16_to_utf8(ime.str);
    std::snprintf(dialog.ime.text, sizeof(dialog.ime.text), "%s", utf8.c_str());
    dialog.ime.status = SCE_IME_DIALOG_BUTTON_ENTER;
    dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
}

void cancel_ime_dialog(EmuEnvState &emuenv) {
    auto &dialog = emuenv.common_dialog;
    if (!dialog.ime.cancelable)
        return;

    std::lock_guard<std::recursive_mutex> dialog_lock(dialog.mutex);
    dialog.ime.status = SCE_IME_DIALOG_BUTTON_CLOSE;
    dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
}

bool submit_current_ime(EmuEnvState &emuenv) {
    if (!is_any_ime_active(emuenv))
        return false;

    if (is_ime_dialog_active(emuenv)) {
        finish_ime_dialog(emuenv);
    } else {
        std::lock_guard<std::mutex> lock(emuenv.ime.mutex);
        emuenv.ime.queue_submit_events();
    }

    return true;
}

bool dismiss_current_ime(EmuEnvState &emuenv) {
    if (!is_any_ime_active(emuenv))
        return false;

    if (is_ime_dialog_active(emuenv)) {
        cancel_ime_dialog(emuenv);
        if (emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
            return false;
    } else {
        std::lock_guard<std::mutex> lock(emuenv.ime.mutex);
        emuenv.ime.queue_event(SCE_IME_EVENT_PRESS_CLOSE);
    }

    return true;
}
