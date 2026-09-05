// In SDL's own package: SDL 3.4 made SDLDummyEdit's constructor and
// setInputType package-private. Being alongside it also makes `ic` and
// `input_type` reachable directly, which is why the reflection this class used
// to need is gone.
package org.libsdl.app;

import android.content.Context;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.vita3k.emulator.Emulator;
import org.vita3k.emulator.NativeLib;

public class VitaTextEdit extends SDLDummyEdit {
    public VitaTextEdit(Context context) {
        super(context);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        InputConnection connection = new VitaInputConnection(this, true);
        ic = connection;

        outAttrs.inputType = getInputTypeValue(this);
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI |
                EditorInfo.IME_FLAG_NO_FULLSCREEN;

        return connection;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_UP) {
            try {
                if (NativeLib.INSTANCE.isImeActive()) {
                    Context context = getContext();
                    if (context instanceof Emulator) {
                        ((Emulator) context).dismissImeFromKeyboard(this);
                        return true;
                    }
                }
            } catch (Throwable ignored) {
            }
        }

        return super.onKeyPreIme(keyCode, event);
    }

    public void clearConnectionState() {
        if (ic instanceof VitaInputConnection) {
            ((VitaInputConnection) ic).clearConnectionState();
        }
    }

    public static int getInputTypeValue(SDLDummyEdit edit) {
        return edit == null ? 0 : edit.input_type;
    }

    /**
     * The way in from outside this package.
     *
     * SDLDummyEdit.setInputType is package-private in SDL 3.4, and Emulator
     * lives in org.vita3k.emulator -- so it asks through here rather than
     * reaching for a method it cannot see.
     */
    public void applyInputType(int inputType) {
        setInputType(inputType);
    }
}
