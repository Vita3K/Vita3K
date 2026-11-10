/*
 * Copyright 2016 Dolphin Emulator Project
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
public final class InputOverlayDrawableDpad
{
  private boolean mVisible = true;
  // The legacy ID identifying what type of button this Drawable represents.
  private int mLegacyId;
  private int[] mControls = new int[4];
  private int mTrackId;
  private int mPreviousTouchX, mPreviousTouchY;
  private int mControlPositionX, mControlPositionY;
  private int mWidth;
  private int mHeight;
  private float mScaleFactor = 1.0f;
  private BitmapDrawable mDefaultStateBitmap;
  private BitmapDrawable mPressedOneDirectionStateBitmap;
  private BitmapDrawable mPressedTwoDirectionsStateBitmap;
  private int mPressState = STATE_DEFAULT;

  public static final int STATE_DEFAULT = 0;
  public static final int STATE_PRESSED_UP = 1;
  public static final int STATE_PRESSED_DOWN = 2;
  public static final int STATE_PRESSED_LEFT = 3;
  public static final int STATE_PRESSED_RIGHT = 4;
  public static final int STATE_PRESSED_UP_LEFT = 5;
  public static final int STATE_PRESSED_UP_RIGHT = 6;
  public static final int STATE_PRESSED_DOWN_LEFT = 7;
  public static final int STATE_PRESSED_DOWN_RIGHT = 8;

  /**
   * Constructor
   *
   * @param res                             {@link Resources} instance.
   * @param defaultStateBitmap              {@link Bitmap} of the default state.
   * @param pressedOneDirectionStateBitmap  {@link Bitmap} of the pressed state in one direction.
   * @param pressedTwoDirectionsStateBitmap {@link Bitmap} of the pressed state in two direction.
   * @param legacyId                        Legacy identifier (ButtonType) for the up button.
   * @param upControl                       Control identifier for the up button.
   * @param downControl                     Control identifier for the down button.
   * @param leftControl                     Control identifier for the left button.
   * @param rightControl                    Control identifier for the right button.
   */
  public InputOverlayDrawableDpad(Resources res, Bitmap defaultStateBitmap,
          Bitmap pressedOneDirectionStateBitmap, Bitmap pressedTwoDirectionsStateBitmap,
          int legacyId, int upControl, int downControl, int leftControl, int rightControl)
  {
    mTrackId = -1;
    mDefaultStateBitmap = new BitmapDrawable(res, defaultStateBitmap);
    mPressedOneDirectionStateBitmap = new BitmapDrawable(res, pressedOneDirectionStateBitmap);
    mPressedTwoDirectionsStateBitmap = new BitmapDrawable(res, pressedTwoDirectionsStateBitmap);

    mWidth = mDefaultStateBitmap.getIntrinsicWidth();
    mHeight = mDefaultStateBitmap.getIntrinsicHeight();

    mLegacyId = legacyId;
    mControls[0] = upControl;
    mControls[1] = downControl;
    mControls[2] = leftControl;
    mControls[3] = rightControl;
  }

  public void setVisible(boolean visible) { mVisible = visible; }
  public boolean isVisible() { return mVisible; }
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
    int px = mControlPositionX + (getWidth() / 2);
    int py = mControlPositionY + (getHeight() / 2);
    switch (mPressState)
    {
      case STATE_DEFAULT:
        mDefaultStateBitmap.draw(canvas);
        break;
      case STATE_PRESSED_UP:
        mPressedOneDirectionStateBitmap.draw(canvas);
        break;
      case STATE_PRESSED_RIGHT:
        canvas.save();
        canvas.rotate(90, px, py);
        mPressedOneDirectionStateBitmap.draw(canvas);
        canvas.restore();
        break;
      case STATE_PRESSED_DOWN:
        canvas.save();
        canvas.rotate(180, px, py);
        mPressedOneDirectionStateBitmap.draw(canvas);
        canvas.restore();
        break;
      case STATE_PRESSED_LEFT:
        canvas.save();
        canvas.rotate(270, px, py);
        mPressedOneDirectionStateBitmap.draw(canvas);
        canvas.restore();
        break;
      case STATE_PRESSED_UP_LEFT:
        mPressedTwoDirectionsStateBitmap.draw(canvas);
        break;
      case STATE_PRESSED_UP_RIGHT:
        canvas.save();
        canvas.rotate(90, px, py);
        mPressedTwoDirectionsStateBitmap.draw(canvas);
        canvas.restore();
        break;
      case STATE_PRESSED_DOWN_RIGHT:
        canvas.save();
        canvas.rotate(180, px, py);
        mPressedTwoDirectionsStateBitmap.draw(canvas);
        canvas.restore();
        break;
      case STATE_PRESSED_DOWN_LEFT:
        canvas.save();
        canvas.rotate(270, px, py);
        mPressedTwoDirectionsStateBitmap.draw(canvas);
        canvas.restore();
        break;
    }
    canvas.restoreToCount(save);
    if (dimLayer != -1) canvas.restoreToCount(dimLayer);
  }

  public int getLegacyId()
  {
    return mLegacyId;
  }

  /**
   * Gets one of the InputOverlayDrawableDpad's control IDs.
   */
  public int getControl(int direction)
  {
    return mControls[direction];
  }

  public void setTrackId(int trackId)
  {
    mTrackId = trackId;
  }

  public int getTrackId()
  {
    return mTrackId;
  }

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

  public void setBounds(int left, int top, int right, int bottom)
  {
    mDefaultStateBitmap.setBounds(left, top, right, bottom);
    mPressedOneDirectionStateBitmap.setBounds(left, top, right, bottom);
    mPressedTwoDirectionsStateBitmap.setBounds(left, top, right, bottom);
  }

  public void setOpacity(int value)
  {
    mDefaultStateBitmap.setAlpha(value);
    mPressedOneDirectionStateBitmap.setAlpha(value);
    mPressedTwoDirectionsStateBitmap.setAlpha(value);
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

  public void setScaleFactor(float scale) { mScaleFactor = Math.max(0.35f, Math.min(2.5f, scale)); }

  public int getWidth()
  {
    return mWidth;
  }

  public int getHeight()
  {
    return mHeight;
  }

  public void setState(int pressState)
  {
    mPressState = pressState;
  }
}
