package org.vita3k.emulator;

import android.content.Context;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.View;

import org.libsdl.app.SDLSurface;
import org.vita3k.emulator.overlay.InputOverlay;

public class EmuSurface extends SDLSurface {

    private InputOverlay mOverlay;

    public InputOverlay getmOverlay() {
        return mOverlay;
    }

    public EmuSurface(Context context){
        super(context);
        mOverlay = new InputOverlay(context);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        setSurfaceStatus(true);
        super.surfaceCreated(holder);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        setSurfaceStatus(false);
        super.surfaceDestroyed(holder);
    }

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        if(mOverlay.onTouch(v, event)){
            // priority is given to the overlay
            return true;
        }

        return super.onTouch(v, event);
    }

    public native void setSurfaceStatus(boolean surface_present);

}
