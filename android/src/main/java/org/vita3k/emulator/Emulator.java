package org.vita3k.emulator;

import android.content.Context;
import android.content.Intent;
import android.content.res.AssetFileDescriptor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.Settings;
import android.system.ErrnoException;
import android.system.Os;
import android.view.Surface;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.Keep;
import androidx.core.content.FileProvider;
import androidx.core.content.pm.ShortcutInfoCompat;
import androidx.core.content.pm.ShortcutManagerCompat;
import androidx.core.graphics.drawable.IconCompat;
import androidx.documentfile.provider.DocumentFile;

import com.jakewharton.processphoenix.ProcessPhoenix;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;
import org.vita3k.emulator.overlay.InputOverlay;

public class Emulator extends SDLActivity
{
    private String currentGameId = "";
    private EmuSurface mSurface;

    public InputOverlay getmOverlay() {
        return mSurface.getmOverlay();
    }

    @Keep
    public void setCurrentGameId(String gameId){
        currentGameId = gameId;
    }

    /**
     * This method is called by SDL before loading the native shared libraries.
     * It can be overridden to provide names of shared libraries to be loaded.
     * The default implementation returns the defaults. It never returns null.
     * An array returned by a new implementation must at least contain "SDL2".
     * Also keep in mind that the order the libraries are loaded may matter.
     *
     * @return names of shared libraries to be loaded (e.g. "SDL3", "main").
     */
    @Override
    protected String[] getLibraries() {
        return new String[] { "Vita3K" };
    }

    @Override
    protected SDLSurface createSDLSurface(Context context) {
        mSurface = new EmuSurface(context);
        mSurface.post(() -> {
            if (mSurface.getParent() instanceof ViewGroup) {
                ((ViewGroup) mSurface.getParent()).addView(getmOverlay());
            }
        });
    return mSurface;
    }

    static private final String APP_RESTART_PARAMETERS = "AppStartParameters";

    @Override
    protected String[] getArguments() {
        Intent intent = getIntent();

        String[] args = intent.getStringArrayExtra(APP_RESTART_PARAMETERS);
        if(args == null)
            args = new String[]{};

        return args;
    }

    @Override
    protected void onNewIntent(Intent intent){
        super.onNewIntent(intent);

        // if we start the app from a shortcut and are in the main menu
        // or in a different game, start the new game
        if(intent.getAction().startsWith("LAUNCH_")){
            String game_id = intent.getAction().substring(7);
            if(!game_id.equals(currentGameId))
                ProcessPhoenix.triggerRebirth(getContext(), intent);
        }
    }

    @Keep
    public void restartApp(String app_path, String exec_path, String exec_args){
        ArrayList<String> args = new ArrayList<>();

        // first build the args given to Vita3K when it restarts
        // this is similar to run_execv in main.cpp
        args.add("-a");
        args.add("true");
        if(!app_path.isEmpty()){
            args.add("-r");
            args.add(app_path);

            if(!exec_path.isEmpty()){
                args.add("--self");
                args.add(exec_path);

                if(!exec_args.isEmpty()){
                    args.add("--app-args");
                    args.add(exec_args);
                }
            }
        }

        Intent restart_intent = new Intent(getContext(), Emulator.class);
        restart_intent.putExtra(APP_RESTART_PARAMETERS, args.toArray(new String[]{}));
        ProcessPhoenix.triggerRebirth(getContext(), restart_intent);
    }

    static final int PICKER_DIALOG_CODE = 545;
    static final int STORAGE_MANAGER_FILE_DIALOG_CODE = 546;
    static final int STORAGE_MANAGER_FOLDER_DIALOG_CODE = 547;

    private boolean ensureStoragePermission(int requestCode) {
        // If running Android 10-, SDL should have already asked for read and write permissions
        if (!isStorageManagerEnabled()) {
            Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION)
                    .setData(Uri.parse("package:" + BuildConfig.APPLICATION_ID));
            startActivityForResult(intent, requestCode);
            return false;
        }
        return true;
    }

    @Keep
    public void showFileDialog() {
        if (!ensureStoragePermission(STORAGE_MANAGER_FILE_DIALOG_CODE))
            return;

        Intent intent = new Intent()
                .setType("*/*")
                .setAction(Intent.ACTION_GET_CONTENT)
                .putExtra(Intent.EXTRA_LOCAL_ONLY, true);

        intent = Intent.createChooser(intent, "Choose a file");
        startActivityForResult(intent, PICKER_DIALOG_CODE);
    }

    private boolean isStorageManagerEnabled(){
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R)
            return true;

        return Environment.isExternalStorageManager();
    }

    @Keep
    public void showFolderDialog() {
        if (!ensureStoragePermission(STORAGE_MANAGER_FOLDER_DIALOG_CODE))
            return;

        Intent intent = new Intent()
                .setAction(Intent.ACTION_OPEN_DOCUMENT_TREE)
                .putExtra(Intent.EXTRA_LOCAL_ONLY, true);

        intent = Intent.createChooser(intent, "Choose a folder");
        startActivityForResult(intent, PICKER_DIALOG_CODE);
    }

    private String resolveUriToPath(Intent data) {
        String result_path = "";
        int result_fd = -1;
        Uri result_uri = data.getData();

        try (ParcelFileDescriptor file_descr = getContentResolver().openFileDescriptor(result_uri, "r")) {
            result_fd = file_descr.detachFd();
            result_path = Os.readlink("/proc/self/fd/" + result_fd);

            // replace /mnt/user/{id} with /storage
            if (result_path.startsWith("/mnt/user/")) {
                result_path = result_path.substring("/mnt/user/".length());
                result_path = "/storage" + result_path.substring(result_path.indexOf('/'));
            }

        } catch (Exception e) {
        }

        return result_path;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        switch (requestCode) {
            // --- STORAGE MANAGER ---
            case STORAGE_MANAGER_FILE_DIALOG_CODE:
            case STORAGE_MANAGER_FOLDER_DIALOG_CODE:
                if (isStorageManagerEnabled()) {
                    if (requestCode == STORAGE_MANAGER_FILE_DIALOG_CODE)
                        showFileDialog();
                    else
                        showFolderDialog();
                } else
                    filedialogReturn("");
                break;
            // --- PICKER ---
            case PICKER_DIALOG_CODE: {
                if (resultCode == RESULT_OK) {
                    String res = resolveUriToPath(data);
                    if (res != null)
                        filedialogReturn(res);
                }
                break;
            }
        }
    }

    @Keep
    public void setControllerOverlayState(int overlay_mask, boolean edit, boolean reset){
        getmOverlay().setState(overlay_mask);
        getmOverlay().setIsInEditMode(edit);

        if(reset)
            getmOverlay().resetButtonPlacement();
    }

    @Keep
    public void setControllerOverlayScale(float scale){
        getmOverlay().setScale(scale);
    }

    @Keep
    public void setControllerOverlayOpacity(int opacity){
        getmOverlay().setOpacity(opacity);
    }

    @Keep
    public boolean createShortcut(String game_id, String game_name){
        if(!ShortcutManagerCompat.isRequestPinShortcutSupported(getContext()))
            return false;

        // first look at the icon, its location should always be the same
        File src_icon = new File(getExternalFilesDir(null), "cache/icons/" + game_id + ".png");
        Bitmap icon;
        if(src_icon.exists())
            icon = BitmapFactory.decodeFile(src_icon.getPath());
        else
            icon = BitmapFactory.decodeResource(getResources(), R.mipmap.ic_launcher);

        // intent to directly start the game
        Intent game_intent = new Intent(getContext(), Emulator.class);
        ArrayList<String> args = new ArrayList<String>();
        args.add("-r");
        args.add(game_id);
        game_intent.putExtra(APP_RESTART_PARAMETERS, args.toArray(new String[]{}));
        game_intent.setAction("LAUNCH_" + game_id);

        // now create the pinned shortcut
        ShortcutInfoCompat shortcut = new ShortcutInfoCompat.Builder(getContext(), game_id)
                .setShortLabel(game_name)
                .setLongLabel(game_name)
                .setIcon(IconCompat.createWithBitmap(icon))
                .setIntent(game_intent)
                .build();
        ShortcutManagerCompat.requestPinShortcut(getContext(), shortcut, null);

        return true;
    }

    @Keep
    public void requestInstallUpdate() {
        runOnUiThread(() -> {
            File apkFile = new File(getExternalFilesDir(null), "vita3k-latest.apk");

            if (!apkFile.exists()) {
                Toast.makeText(this, "APK file not found.", Toast.LENGTH_LONG).show();
                return;
            }

            try {
                Uri apkUri = FileProvider.getUriForFile(
                    this,
                    BuildConfig.APPLICATION_ID + ".fileprovider",
                    apkFile
                    );

                Intent intent = new Intent(Intent.ACTION_INSTALL_PACKAGE);
                intent.setData(apkUri);
                intent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_ACTIVITY_NEW_TASK);

                startActivity(intent);
             } catch (Exception e) {
                e.printStackTrace();
                Toast.makeText(this, "Failed to launch installer.", Toast.LENGTH_LONG).show();
             }
        });
    }

    @Keep
    public int getNativeDisplayRotation() {
        // Returns the device's default display rotation (0, 90, 180, or 270 degrees)
        return getWindowManager().getDefaultDisplay().getRotation();
    }

    public native void filedialogReturn(String result_path);
}
