package org.vita3k.emulator;

import android.content.Context;
import android.content.Intent;

public final class ImGuiActivity extends Emulator {
    @Override
    protected boolean isImGuiFrontend() {
        return true;
    }

    public static Intent createLaunchIntent(Context context) {
        return new Intent(context, ImGuiActivity.class).putExtra("imgui_frontend", true);
    }
}
