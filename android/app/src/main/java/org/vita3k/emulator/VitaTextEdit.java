package org.vita3k.emulator;

import android.content.Context;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.libsdl.app.SDLDummyEdit;

import java.lang.reflect.Field;

public class VitaTextEdit extends SDLDummyEdit {
    private static final Field IC_FIELD = resolveField("ic");
    private static final Field INPUT_TYPE_FIELD = resolveField("input_type");

    public VitaTextEdit(Context context) {
        super(context);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        InputConnection connection = new VitaInputConnection(this, true);
        setFieldValue(IC_FIELD, this, connection);

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
        Object connection = getFieldValue(IC_FIELD, this);
        if (connection instanceof VitaInputConnection) {
            ((VitaInputConnection) connection).clearConnectionState();
        }
    }

    public static int getInputTypeValue(SDLDummyEdit edit) {
        Object value = getFieldValue(INPUT_TYPE_FIELD, edit);
        return value instanceof Integer ? (Integer) value : 0;
    }

    private static Field resolveField(String name) {
        try {
            Field field = SDLDummyEdit.class.getDeclaredField(name);
            field.setAccessible(true);
            return field;
        } catch (Exception e) {
            return null;
        }
    }

    private static Object getFieldValue(Field field, Object target) {
        if (field == null || target == null) {
            return null;
        }

        try {
            return field.get(target);
        } catch (Exception e) {
            return null;
        }
    }

    private static void setFieldValue(Field field, Object target, Object value) {
        if (field == null || target == null) {
            return;
        }

        try {
            field.set(target, value);
        } catch (Exception ignored) {
        }
    }
}
