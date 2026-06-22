package org.vita3k.emulator;

import android.content.Context;
import android.view.KeyEvent;
import android.view.View;

import org.libsdl.app.SDLInputConnection;

public class VitaInputConnection extends SDLInputConnection {
    public VitaInputConnection(View targetView, boolean fullEditor) {
        super(targetView, fullEditor);
    }

    private boolean isNativeImeActive() {
        try {
            return NativeLib.INSTANCE.isImeActive();
        } catch (Throwable ignored) {
            return false;
        }
    }

    private Emulator getEmulator() {
        Context context = mEditText.getContext();
        return context instanceof Emulator ? (Emulator) context : null;
    }

    public void clearConnectionState() {
        android.text.Editable content = getEditable();
        if (content != null) {
            content.clear();
            removeComposingSpans(content);
        }
        mCommittedText = "";
    }

    private void submitFromKeyboard() {
        Emulator emulator = getEmulator();
        if (emulator != null) {
            emulator.completeImeFromKeyboard(mEditText);
        } else {
            try {
                NativeLib.INSTANCE.submitIme();
            } catch (Throwable ignored) {
            }
            clearConnectionState();
        }
    }

    @Override
    public boolean performEditorAction(int actionCode) {
        if (isNativeImeActive()) {
            submitFromKeyboard();
            return true;
        }

        return super.performEditorAction(actionCode);
    }

    @Override
    public boolean sendKeyEvent(KeyEvent event) {
        if (isNativeImeActive()
                && event.getAction() == KeyEvent.ACTION_UP
                && (event.getKeyCode() == KeyEvent.KEYCODE_ENTER
                || event.getKeyCode() == KeyEvent.KEYCODE_NUMPAD_ENTER)) {
            submitFromKeyboard();
            return true;
        }

        return super.sendKeyEvent(event);
    }
}
