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

#include <gui-qt/ime_keyboard_filter.h>

#include <dialog/state.h>
#include <emuenv/state.h>
#include <ime/functions.h>
#include <util/string_utils.h>

#include <QEvent>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QWindow>

#include <algorithm>
#include <cstring>
#include <mutex>

ImeKeyboardFilter::ImeKeyboardFilter(EmuEnvState &emuenv, QObject *parent)
    : QObject(parent)
    , m_emuenv(emuenv) {
}

bool ImeKeyboardFilter::eventFilter(QObject *watched, QEvent *event) {
    auto &ime = m_emuenv.ime;
    auto &dialog = m_emuenv.common_dialog;

    const bool dialog_ime_active = (dialog.type == IME_DIALOG
        && dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING);

    if (!ime.state && !dialog_ime_active)
        return QObject::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::InputMethod: {
        auto *im_event = static_cast<QInputMethodEvent *>(event);
        const QString commit = im_event->commitString();
        const QString preedit = im_event->preeditString();

        std::lock_guard lock(ime.mutex);

        if (!commit.isEmpty()) {
            const std::u16string text(reinterpret_cast<const char16_t *>(commit.utf16()),
                static_cast<size_t>(commit.size()));
            ime_commit_text(ime, text);
        }

        if (!preedit.isEmpty()) {
            const std::u16string pre(reinterpret_cast<const char16_t *>(preedit.utf16()),
                static_cast<size_t>(preedit.size()));
            ime_set_preedit(ime, pre);
        } else if (commit.isEmpty()) {
            ime_set_preedit(ime, u"");
        }

        return true;
    }

    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent *>(event);

        if (QGuiApplication::inputMethod()->isVisible()
            && !ke->text().isEmpty()
            && ke->key() != Qt::Key_Backspace
            && ke->key() != Qt::Key_Return
            && ke->key() != Qt::Key_Enter
            && ke->key() != Qt::Key_Escape
            && ke->key() != Qt::Key_Left
            && ke->key() != Qt::Key_Right) {
            return QObject::eventFilter(watched, event);
        }

        switch (ke->key()) {
        case Qt::Key_Backspace: {
            std::lock_guard lock(ime.mutex);
            ime_backspace(ime);
            return true;
        }

        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (dialog_ime_active) {
                std::lock_guard<std::recursive_mutex> dlock(dialog.mutex);
                std::lock_guard lock(ime.mutex);
                const size_t copy_len = std::min(static_cast<size_t>(ime.str.length()), static_cast<size_t>(dialog.ime.max_length));
                memcpy(dialog.ime.result, ime.str.c_str(), copy_len * sizeof(uint16_t));
                dialog.ime.result[copy_len] = 0;

                const std::string utf8 = string_utils::utf16_to_utf8(ime.str);
                snprintf(dialog.ime.text, sizeof(dialog.ime.text), "%s", utf8.c_str());
                ime.event_id = SCE_IME_EVENT_PRESS_ENTER;
                dialog.ime.status = SCE_IME_DIALOG_BUTTON_ENTER;
                dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
                dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
            } else {
                ime.event_id = SCE_IME_EVENT_PRESS_ENTER;
            }
            return true;

        case Qt::Key_Escape:
            if (dialog_ime_active) {
                if (dialog.ime.cancelable) {
                    std::lock_guard<std::recursive_mutex> dlock(dialog.mutex);
                    std::lock_guard lock(ime.mutex);
                    ime.event_id = SCE_IME_EVENT_PRESS_CLOSE;
                    dialog.ime.status = SCE_IME_DIALOG_BUTTON_CLOSE;
                    dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
                    dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
                }
            } else {
                ime.event_id = SCE_IME_EVENT_PRESS_CLOSE;
            }
            return true;

        case Qt::Key_Left: {
            std::lock_guard lock(ime.mutex);
            ime_cursor_left(ime);
            return true;
        }

        case Qt::Key_Right: {
            std::lock_guard lock(ime.mutex);
            ime_cursor_right(ime);
            return true;
        }

        default: {
            const QString text = ke->text();
            if (!text.isEmpty()) {
                std::lock_guard lock(ime.mutex);
                const std::u16string u16(reinterpret_cast<const char16_t *>(text.utf16()),
                    static_cast<size_t>(text.size()));
                ime_commit_text(ime, u16);
                return true;
            }
            break;
        }
        }

        return true;
    }

    case QEvent::KeyRelease:
        // block key releases when ime active
        return true;

    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}