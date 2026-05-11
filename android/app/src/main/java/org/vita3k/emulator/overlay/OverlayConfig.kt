package org.vita3k.emulator.overlay

import android.content.Context

val DEFAULT_OVERLAY_MASK: Int =
    InputOverlay.OVERLAY_MASK_BASIC or InputOverlay.OVERLAY_MASK_TOUCH_SCREEN_SWITCH

data class OverlayConfig(
    val overlayMask: Int = DEFAULT_OVERLAY_MASK,
    val overlayScale: Int = 100,
    val overlayOpacity: Int = 100,
    val hideOverlayWhenControllerConnected: Boolean = true
) {
    val controlsVisible: Boolean
        get() = overlayMask != 0

    val l2r2Visible: Boolean
        get() = overlayMask and InputOverlay.OVERLAY_MASK_L2R2 != 0

    val l3r3Visible: Boolean
        get() = overlayMask and InputOverlay.OVERLAY_MASK_L3R3 != 0

    val touchSwitchVisible: Boolean
        get() = overlayMask and InputOverlay.OVERLAY_MASK_TOUCH_SCREEN_SWITCH != 0

    val hideToggleVisible: Boolean
        get() = overlayMask and InputOverlay.OVERLAY_MASK_HIDE_TOGGLE != 0

    fun activeMask(): Int = if (overlayMask == 0) DEFAULT_OVERLAY_MASK else overlayMask

    fun withControlsVisible(visible: Boolean): OverlayConfig {
        return copy(overlayMask = if (visible) activeMask() else 0).normalized()
    }

    fun withL2R2Visible(visible: Boolean): OverlayConfig {
        val baseMask = activeMask()
        val updatedMask = if (visible) {
            baseMask or InputOverlay.OVERLAY_MASK_L2R2
        } else {
            baseMask and InputOverlay.OVERLAY_MASK_L2R2.inv()
        }
        return copy(overlayMask = updatedMask).normalized()
    }

    fun withL3R3Visible(visible: Boolean): OverlayConfig {
        val baseMask = activeMask()
        val updatedMask = if (visible) {
            baseMask or InputOverlay.OVERLAY_MASK_L3R3
        } else {
            baseMask and InputOverlay.OVERLAY_MASK_L3R3.inv()
        }
        return copy(overlayMask = updatedMask).normalized()
    }

    fun withTouchSwitchVisible(visible: Boolean): OverlayConfig {
        val baseMask = activeMask()
        val updatedMask = if (visible) {
            baseMask or InputOverlay.OVERLAY_MASK_TOUCH_SCREEN_SWITCH
        } else {
            baseMask and InputOverlay.OVERLAY_MASK_TOUCH_SCREEN_SWITCH.inv()
        }
        return copy(overlayMask = updatedMask).normalized()
    }

    fun withHideToggleVisible(visible: Boolean): OverlayConfig {
        val baseMask = activeMask()
        val updatedMask = if (visible) {
            baseMask or InputOverlay.OVERLAY_MASK_HIDE_TOGGLE
        } else {
            baseMask and InputOverlay.OVERLAY_MASK_HIDE_TOGGLE.inv()
        }
        return copy(overlayMask = updatedMask).normalized()
    }

    fun normalized(): OverlayConfig {
        val knownMask = InputOverlay.OVERLAY_MASK_BASIC or
            InputOverlay.OVERLAY_MASK_L2R2 or
            InputOverlay.OVERLAY_MASK_L3R3 or
            InputOverlay.OVERLAY_MASK_TOUCH_SCREEN_SWITCH or
            InputOverlay.OVERLAY_MASK_HIDE_TOGGLE
        return copy(
            overlayMask = overlayMask.coerceAtLeast(0) and knownMask,
            overlayScale = overlayScale.coerceIn(50, 150),
            overlayOpacity = overlayOpacity.coerceIn(20, 100)
        )
    }
}

object OverlayConfigStore {
    private const val PREFS_NAME = "emulation_session"
    private const val KEY_OVERLAY_MASK = "overlay_mask"
    private const val KEY_OVERLAY_SCALE = "overlay_scale"
    private const val KEY_OVERLAY_OPACITY = "overlay_opacity"
    private const val KEY_HIDE_ON_CONTROLLER = "hide_overlay_on_controller"

    fun load(context: Context): OverlayConfig {
        val prefs = context.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return OverlayConfig(
            overlayMask = prefs.getInt(KEY_OVERLAY_MASK, DEFAULT_OVERLAY_MASK),
            overlayScale = prefs.getInt(KEY_OVERLAY_SCALE, 100),
            overlayOpacity = prefs.getInt(KEY_OVERLAY_OPACITY, 100),
            hideOverlayWhenControllerConnected = prefs.getBoolean(KEY_HIDE_ON_CONTROLLER, true)
        ).normalized()
    }

    fun save(context: Context, config: OverlayConfig) {
        val normalized = config.normalized()
        context.applicationContext
            .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putInt(KEY_OVERLAY_MASK, normalized.overlayMask)
            .putInt(KEY_OVERLAY_SCALE, normalized.overlayScale)
            .putInt(KEY_OVERLAY_OPACITY, normalized.overlayOpacity)
            .putBoolean(KEY_HIDE_ON_CONTROLLER, normalized.hideOverlayWhenControllerConnected)
            .apply()
    }
}
