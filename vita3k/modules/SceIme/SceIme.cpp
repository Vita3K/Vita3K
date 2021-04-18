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

#include <ime/types.h>

#include <kernel/thread/thread_functions.h>
#include <util/lock_and_find.h>
#include <util/string_utils.h>

EXPORT(void, SceImeEventHandler, Ptr<void> arg, const SceImeEvent *e) {
    Ptr<SceImeEvent> e1 = Ptr<SceImeEvent>(alloc(host.mem, sizeof(SceImeEvent), "ime2"));
    memcpy(e1.get(host.mem), e, sizeof(SceImeEvent));
    auto thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    run_callback(host.kernel, *thread, thread_id, host.ime.param.handler.address(), { arg.address(), e1.address() });
    free(host.mem, e1.address());
}

EXPORT(SceInt32, sceImeClose) {
    host.ime.state = false;
    memset(&host.ime, 0, sizeof(Ime));

    return 0;
}

EXPORT(SceInt32, sceImeOpen, SceImeParam *param) {
    host.ime = {};
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
    host.ime.edit_text.str = Ptr<SceWChar16>(alloc(host.mem, SCE_IME_MAX_TEXT_LENGTH + host.ime.param.maxTextLength + 1, "ime_str"));
    const std::u16string initial16 = reinterpret_cast<char16_t *>(host.ime.param.initialText.get(host.mem));
    if (!initial16.empty()) {
        host.ime.str = string_utils::utf16_to_utf8(initial16);
        host.ime.caretIndex = host.ime.edit_text.caretIndex = host.ime.edit_text.preeditIndex = uint32_t(initial16.size());
    } else
        host.ime.caps_level = 1;

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
    std::u16string str16 = string_utils::utf8_to_utf16(host.ime.str);
    memcpy(host.ime.edit_text.str.get(host.mem), str16.c_str(), SCE_IME_MAX_TEXT_LENGTH + host.ime.param.maxTextLength + 1);
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
