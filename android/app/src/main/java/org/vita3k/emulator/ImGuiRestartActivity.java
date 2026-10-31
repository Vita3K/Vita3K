package org.vita3k.emulator;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;

public final class ImGuiRestartActivity extends Activity {
    static final String EXTRA_PROCESS_ID = "imgui_process_id";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        int previousProcessId = getIntent().getIntExtra(EXTRA_PROCESS_ID, -1);
        String[] args = getIntent().getStringArrayExtra(Emulator.APP_RESTART_PARAMETERS);
        if (previousProcessId <= 0 || previousProcessId == Process.myPid()
                || args == null || args.length == 0) {
            Log.e("Vita3K", "Invalid ImGui cold restart request");
            finish();
            return;
        }

        // This activity runs in a separate process, without loading the emulator runtime.
        Process.killProcess(previousProcessId);
        Log.i("Vita3K", "Launching ImGui in a new process");
        startActivity(ImGuiActivity.createLaunchIntent(this)
                .putExtra(Emulator.APP_RESTART_PARAMETERS, args)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK));
        finish();
        Process.killProcess(Process.myPid());
    }
}