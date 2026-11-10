/*
 * Copyright 2013 Dolphin Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

package org.vita3k.emulator.overlay;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.view.MotionEvent;

/**
 * Custom {@link BitmapDrawable} that is capable
 * of storing it's own ID.
 */
public final class InputOverlayDrawableButton
{
  private boolean mVisible = true;
  // The legacy ID identifying what type of button this Drawable represents.
  private int mLegacyId;
  private int mControl;
  private int mTrackId;
  private int mPreviousTouchX, mPreviousTouchY;
  private int mControlPositionX, mControlPositionY;
  private int mWidth;
  private int mHeight;
  private float mScaleFactor = 1.0f;
  private BitmapDrawable mDefaultStateBitmap;
  private BitmapDrawable mPressedStateBitmap;
  private boolean mPressedState = false;
  private int mRole = 0;

  /**
   * Constructor
   *
   * @param res                {@link Resources} instance.
   * @param defaultStateBitmap {@link Bitmap} to use with the default state Drawable.
   * @param pressedStateBitmap {@link Bitmap} to use with the pressed state Drawable.
   * @param legacyId           Legacy identifier (ButtonType) for this type of button.
   * @param control            Control ID for this type of button.
   */
  public InputOverlayDrawableButton(Resources res, Bitmap defaultStateBitmap,
          Bitmap pressedStateBitmap, int legacyId, int control, int role)
  {
    mTrackId = -1;
    mDefaultStateBitmap = new BitmapDrawable(res, defaultStateBitmap);
    mPressedStateBitmap = new BitmapDrawable(res, pressedStateBitmap);
    mLegacyId = legacyId;
    mControl = control;

    mWidth = mDefaultStateBitmap.getIntrinsicWidth();
    mHeight = mDefaultStateBitmap.getIntrinsicHeight();

    mRole = role;
  }

  /**
   * Gets this InputOverlayDrawableButton's legacy button ID.
   */
  public int getLegacyId()
  {
    return mLegacyId;
  }

  public int getControl()
  {
    return mControl;
  }

  public void setTrackId(int trackId)
  {
    mTrackId = trackId;
  }

  public int getTrackId()
  {
    return mTrackId;
  }

  public int getRole() { return mRole; }

  public boolean getPressed() { return mPressedState; }

  public void onConfigureTouch(MotionEvent event)
  {
    onConfigureTouch(event, event.getActionIndex());
  }

  public void onConfigureTouch(MotionEvent event, int pointerIndex)
  {
    if (pointerIndex < 0 || pointerIndex >= event.getPointerCount()) return;
    switch (event.getActionMasked())
    {
      case MotionEvent.ACTION_DOWN:
      case MotionEvent.ACTION_POINTER_DOWN:
        mPreviousTouchX = (int) event.getX(pointerIndex);
        mPreviousTouchY = (int) event.getY(pointerIndex);
        break;
      case MotionEvent.ACTION_MOVE:
        mControlPositionX += (int) event.getX(pointerIndex) - mPreviousTouchX;
        mControlPositionY += (int) event.getY(pointerIndex) - mPreviousTouchY;
        setBounds(mControlPositionX, mControlPositionY, getWidth() + mControlPositionX,
                getHeight() + mControlPositionY);
        mPreviousTouchX = (int) event.getX(pointerIndex);
        mPreviousTouchY = (int) event.getY(pointerIndex);
        break;
    }
  }

  public void setPosition(int x, int y)
  {
    mControlPositionX = x;
    mControlPositionY = y;
  }

  public void setVisible(boolean visible) { mVisible = visible; }
  public boolean isVisible() { return mVisible; }
  public void setScaleFactor(float scale) { mScaleFactor = Math.max(0.35f, Math.min(2.5f, scale)); }
  public void draw(Canvas canvas)
  {
    draw(canvas, false);
  }

  public void draw(Canvas canvas, boolean showHiddenDimmed)
  {
    if (!mVisible && !showHiddenDimmed) return;
    Rect bounds = getBounds();
    int dimLayer = -1;
    if (!mVisible) {
      Rect dimBounds = getTouchBounds();
      dimLayer = canvas.saveLayerAlpha(dimBounds.left, dimBounds.top, dimBounds.right, dimBounds.bottom, 105);
    }
    int save = canvas.save();
    canvas.scale(mScaleFactor, mScaleFactor, bounds.exactCenterX(), bounds.exactCenterY());
    getCurrentStateBitmapDrawable().draw(canvas);
    canvas.restoreToCount(save);
    if (dimLayer != -1) canvas.restoreToCount(dimLayer);
  }

  private BitmapDrawable getCurrentStateBitmapDrawable()
  {
    return mPressedState ? mPressedStateBitmap : mDefaultStateBitmap;
  }

  public void setBounds(int left, int top, int right, int bottom)
  {
    mDefaultStateBitmap.setBounds(left, top, right, bottom);
    mPressedStateBitmap.setBounds(left, top, right, bottom);
  }

  public void setOpacity(int value)
  {
    mDefaultStateBitmap.setAlpha(value);
    mPressedStateBitmap.setAlpha(value);
  }

  public Rect getBounds()
  {
    return mDefaultStateBitmap.getBounds();
  }

  public Rect getTouchBounds()
  {
    Rect bounds = getBounds();
    int width = Math.round(bounds.width() * mScaleFactor);
    int height = Math.round(bounds.height() * mScaleFactor);
    int cx = bounds.centerX();
    int cy = bounds.centerY();
    return new Rect(cx - width / 2, cy - height / 2, cx + (width + 1) / 2, cy + (height + 1) / 2);
  }

  public int getWidth()
  {
    return mWidth;
  }

  public int getHeight()
  {
    return mHeight;
  }

  public void setPressedState(boolean isPressed)
  {
    if(mRole == InputOverlay.OVERLAY_MASK_TOUCH_SCREEN_SWITCH){
      if(isPressed)
        mPressedState = !mPressedState;
    } else {
      mPressedState = isPressed;
    }
  }
}
