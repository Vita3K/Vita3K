// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include "SceIme.h"

#include <ime/functions.h>
#include <ime/types.h>

#include <util/lock_and_find.h>

EXPORT(void, SceImeEventHandler, Ptr<void> arg, const SceImeEvent *e) {
    Ptr<SceImeEvent> e1 = Ptr<SceImeEvent>(alloc(host.mem, sizeof(SceImeEvent), "ime2"));
    memcpy(e1.get(host.mem), e, sizeof(SceImeEvent));
    auto thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    thread->request_callback(host.ime.param.handler.address(), { arg.address(), e1.address() }, [&host, e1](int res) {
        free(host.mem, e1.address());
    });
}

EXPORT(SceInt32, sceImeClose) {
    host.ime.state = false;

    return 0;
}

EXPORT(SceInt32, sceImeOpen, SceImeParam *param) {
    host.ime.caps_level = 0;
    host.ime.caretIndex = 0;
    host.ime.edit_text = {};
    host.ime.enter_label.clear();
    host.ime.str.clear();
    host.ime.param = {};
    host.ime.param = *param;

    switch (host.ime.param.enterLabel) {
    case SCE_IME_ENTER_LABEL_DEFAULT:
        host.ime.enter_label = "Enter";
        break;
    case SCE_IME_ENTER_LABEL_SEND:
        host.ime.enter_label = "Send";
        break;
    case SCE_IME_ENTER_LABEL_SEARCH:
        host.ime.enter_label = "Search";
        break;
    case SCE_IME_ENTER_LABEL_GO:
        host.ime.enter_label = "Go";
        break;
    default: break;
    }

    gui::init_ime_lang(host.ime, SceImeLanguage(host.cfg.current_ime_lang));

    host.ime.edit_text.str = host.ime.param.inputTextBuffer;
    host.ime.param.inputTextBuffer = Ptr<SceWChar16>(alloc(host.mem, SCE_IME_MAX_PREEDIT_LENGTH + host.ime.param.maxTextLength + 1, "ime_str"));
    host.ime.str = reinterpret_cast<char16_t *>(host.ime.param.initialText.get(host.mem));
    if (!host.ime.str.empty())
        host.ime.caretIndex = host.ime.edit_text.caretIndex = host.ime.edit_text.preeditIndex = SceUInt32(host.ime.str.length());
    else
        host.ime.caps_level = 1;

    host.ime.event_id = SCE_IME_EVENT_OPEN;
    host.ime.state = true;

    SceImeEvent e{};
    memset(&e, 0, sizeof(e));

    return 0;
}

EXPORT(SceInt32, sceImeSetCaret, const SceImeCaret *caret) {
    Ptr<SceImeEvent> event = Ptr<SceImeEvent>(alloc(host.mem, sizeof(SceImeEvent), "ime_event"));
    SceImeEvent *e = event.get(host.mem);
    e->param.caretIndex = caret->index;
    CALL_EXPORT(SceImeEventHandler, host.ime.param.arg, e);

    return 0;
}

EXPORT(SceInt32, sceImeSetPreeditGeometry, const SceImePreeditGeometry *preedit) {
    Ptr<SceImeEvent> event = Ptr<SceImeEvent>(alloc(host.mem, sizeof(SceImeEvent), "ime_event"));
    SceImeEvent *e = event.get(host.mem);
    e->param.rect.height = preedit->height;
    e->param.rect.x = preedit->x;
    e->param.rect.y = preedit->y;
    CALL_EXPORT(SceImeEventHandler, host.ime.param.arg, e);

    return 0;
}

EXPORT(int, sceImeSetText) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceImeUpdate) {
    Ptr<SceImeEvent> event = Ptr<SceImeEvent>(alloc(host.mem, sizeof(SceImeEvent), "ime_event"));
    SceImeEvent *e = event.get(host.mem);
    e->id = host.ime.event_id;
    memcpy(host.ime.edit_text.str.get(host.mem), host.ime.str.c_str(), host.ime.str.length() * sizeof(SceWChar16) + 1);
    e->param.text = host.ime.edit_text;
    e->param.caretIndex = host.ime.caretIndex;
    CALL_EXPORT(SceImeEventHandler, host.ime.param.arg, e);
    host.ime.event_id = SCE_IME_EVENT_OPEN;

    return 0;
}

BRIDGE_IMPL(sceImeClose)
BRIDGE_IMPL(sceImeOpen)
BRIDGE_IMPL(sceImeSetCaret)
BRIDGE_IMPL(sceImeSetPreeditGeometry)
BRIDGE_IMPL(sceImeSetText)
BRIDGE_IMPL(sceImeUpdate)
