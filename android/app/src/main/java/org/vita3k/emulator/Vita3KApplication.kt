package org.vita3k.emulator

import android.app.Application
import org.libsdl.app.SDLActivity
import org.vita3k.emulator.data.UiLanguages

class Vita3KApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        System.loadLibrary("Vita3K")
        SDLActivity.nativeSetenv("SDL_ANDROID_ALLOW_RECREATE_ACTIVITY", "1")
        UiLanguages.applyStored(this)
    }
}
