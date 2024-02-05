// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <module/module.h>

#include <ime/functions.h>
#include <ime/types.h>
#include <kernel/state.h>

#include <util/lock_and_find.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceIme);

EXPORT(void, SceImeEventHandler, Ptr<void> arg, const SceImeEvent *e) {
    TRACY_FUNC(SceImeEventHandler, arg, e);
    Ptr<SceImeEvent> e1 = Ptr<SceImeEvent>(alloc(emuenv.mem, sizeof(SceImeEvent), "ime2"));
    memcpy(e1.get(emuenv.mem), e, sizeof(SceImeEvent));
    auto thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    thread->run_callback(emuenv.ime.param.handler.address(), { arg.address(), e1.address() });
    free(emuenv.mem, e1.address());
}

EXPORT(SceInt32, sceImeClose) {
    TRACY_FUNC(sceImeClose);
    emuenv.ime.state = false;

    return 0;
}

EXPORT(SceInt32, sceImeOpen, SceImeParam *param) {
    TRACY_FUNC(sceImeOpen, param);
    emuenv.ime.caps_level = 0;
    emuenv.ime.caretIndex = 0;
    emuenv.ime.edit_text = {};
    emuenv.ime.enter_label.clear();
    emuenv.ime.str.clear();
    emuenv.ime.param = *param;

    switch (emuenv.ime.param.enterLabel) {
    case SCE_IME_ENTER_LABEL_DEFAULT:
        emuenv.ime.enter_label = "Enter";
        break;
    case SCE_IME_ENTER_LABEL_SEND:
        emuenv.ime.enter_label = "Send";
        break;
    case SCE_IME_ENTER_LABEL_SEARCH:
        emuenv.ime.enter_label = "Search";
        break;
    case SCE_IME_ENTER_LABEL_GO:
        emuenv.ime.enter_label = "Go";
        break;
    default: break;
    }

    gui::init_ime_lang(emuenv.ime, static_cast<SceImeLanguage>(emuenv.cfg.current_ime_lang));

    emuenv.ime.edit_text.str = emuenv.ime.param.inputTextBuffer;
    emuenv.ime.param.inputTextBuffer = Ptr<SceWChar16>(alloc(emuenv.mem, SCE_IME_MAX_PREEDIT_LENGTH + emuenv.ime.param.maxTextLength + 1, "ime_str"));
    emuenv.ime.str = reinterpret_cast<char16_t *>(emuenv.ime.param.initialText.get(emuenv.mem));
    if (!emuenv.ime.str.empty())
        emuenv.ime.caretIndex = emuenv.ime.edit_text.caretIndex = emuenv.ime.edit_text.preeditIndex = SceUInt32(emuenv.ime.str.length());
    else
        emuenv.ime.caps_level = 1;

    emuenv.ime.event_id = SCE_IME_EVENT_OPEN;
    emuenv.ime.state = true;

    SceImeEvent e{};
    memset(&e, 0, sizeof(e));

    return 0;
}

EXPORT(SceInt32, sceImeSetCaret, const SceImeCaret *caret) {
    TRACY_FUNC(sceImeSetCaret, caret);
    Ptr<SceImeEvent> event = Ptr<SceImeEvent>(alloc(emuenv.mem, sizeof(SceImeEvent), "ime_event"));
    SceImeEvent *e = event.get(emuenv.mem);
    e->param.caretIndex = caret->index;
    CALL_EXPORT(SceImeEventHandler, emuenv.ime.param.arg, e);

    return 0;
}

EXPORT(SceInt32, sceImeSetPreeditGeometry, const SceImePreeditGeometry *preedit) {
    TRACY_FUNC(sceImeSetPreeditGeometry, preedit);
    Ptr<SceImeEvent> event = Ptr<SceImeEvent>(alloc(emuenv.mem, sizeof(SceImeEvent), "ime_event"));
    SceImeEvent *e = event.get(emuenv.mem);
    e->param.rect.height = preedit->height;
    e->param.rect.x = preedit->x;
    e->param.rect.y = preedit->y;
    CALL_EXPORT(SceImeEventHandler, emuenv.ime.param.arg, e);

    return 0;
}

EXPORT(int, sceImeSetText, const SceWChar16 *text, SceUInt32 length) {
    TRACY_FUNC(sceImeSetText, text, length);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceImeUpdate) {
    TRACY_FUNC(sceImeUpdate);
    if (!emuenv.ime.state)
        return RET_ERROR(SCE_IME_ERROR_NOT_OPENED);

    Ptr<SceImeEvent> event = Ptr<SceImeEvent>(alloc(emuenv.mem, sizeof(SceImeEvent), "ime_event"));
    SceImeEvent *e = event.get(emuenv.mem);
    e->id = emuenv.ime.event_id;
    memcpy(emuenv.ime.edit_text.str.get(emuenv.mem), emuenv.ime.str.c_str(), emuenv.ime.str.length() * sizeof(SceWChar16) + 1);
    e->param.text = emuenv.ime.edit_text;
    e->param.caretIndex = emuenv.ime.caretIndex;
    CALL_EXPORT(SceImeEventHandler, emuenv.ime.param.arg, e);
    emuenv.ime.event_id = SCE_IME_EVENT_OPEN;

    return 0;
}
