package org.vita3k.emulator;

import android.content.Context;
import android.content.Intent;
import android.os.Process;
import android.util.Log;
import android.view.KeyEvent;

import androidx.annotation.Keep;

import org.libsdl.app.SDLSurface;

import java.util.ArrayList;

public final class ImGuiActivity extends Emulator {
    @Override
    protected boolean isImGuiFrontend() {
        return true;
    }

    @Override
    public void onBackPressed() {
        onNativeKeyDown(KeyEvent.KEYCODE_BACK);
        onNativeKeyUp(KeyEvent.KEYCODE_BACK);
    }

    @Override
    protected SDLSurface createSDLSurface(Context context) {
        SDLSurface surface = super.createSDLSurface(context);
        getmOverlay().setZOrderMediaOverlay(true);
        return surface;
    }

    @Override
    protected String[] getArguments() {
        String[] launchArgs = super.getArguments();
        String[] args = new String[launchArgs.length + 1];
        args[0] = "--imgui";
        System.arraycopy(launchArgs, 0, args, 1, launchArgs.length);
        return args;
    }

    @Keep
    @Override
    public void restartApp(String app_path, String exec_path, String exec_args) {
        ArrayList<String> args = new ArrayList<>();
        args.add("-r");
        args.add(app_path);
        if (exec_path != null && !exec_path.isEmpty()) {
            args.add("--self");
            args.add(exec_path);
        }
        if (exec_args != null && !exec_args.isEmpty()) {
            args.add("--app-args");
            args.add(exec_args);
        }

        Intent intent = new Intent(this, ImGuiRestartActivity.class)
                .putExtra(APP_RESTART_PARAMETERS, args.toArray(new String[0]))
                .putExtra(ImGuiRestartActivity.EXTRA_PROCESS_ID, Process.myPid());
        startActivity(intent);
        Log.i("Vita3K", "Requested cold restart of ImGui for " + app_path);
    }

    @Keep
    public String[] getCustomDriversForGui() {
        return NativeLib.INSTANCE.getInstalledCustomDrivers();
    }

    @Keep
    public String installCustomDriverForGui(String path) {
        return NativeLib.INSTANCE.installCustomDriver(path);
    }

    @Keep
    public boolean removeCustomDriverForGui(String driverName) {
        return NativeLib.INSTANCE.removeCustomDriver(driverName);
    }

    public static Intent createLaunchIntent(Context context) {
        return new Intent(context, ImGuiActivity.class);
    }
}
