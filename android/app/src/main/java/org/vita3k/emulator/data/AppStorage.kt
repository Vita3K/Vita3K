package org.vita3k.emulator.data

import android.content.Context
import java.io.File

internal object AppStorage {
    private const val PREFS_NAME = "vita3k_app"
    private const val KEY_INITIAL_SETUP_COMPLETED = "initial_setup_completed"
    private const val DEFAULT_STORAGE_DIR_NAME = "vita"

    fun storageRootPath(context: Context): String {
        return (context.getExternalFilesDir(null) ?: context.filesDir ?: File(context.cacheDir, "files")).absolutePath
    }

    fun defaultStoragePath(context: Context): String {
        val baseDir = File(storageRootPath(context))
        return File(baseDir, DEFAULT_STORAGE_DIR_NAME).absolutePath
    }

    fun isInitialSetupCompleted(context: Context): Boolean =
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .getBoolean(KEY_INITIAL_SETUP_COMPLETED, false)

    fun setInitialSetupCompleted(context: Context, completed: Boolean) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putBoolean(KEY_INITIAL_SETUP_COMPLETED, completed)
            .apply()
    }
}
