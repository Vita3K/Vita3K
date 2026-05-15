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
    fun load(
        context: Context,
        scopeId: String? = null,
        allowGameOverride: Boolean = false
    ): OverlayConfig = OverlayStore.loadConfig(context, scopeId, allowGameOverride)

    fun save(
        context: Context,
        config: OverlayConfig,
        scopeId: String? = null,
        useGameOverride: Boolean = false
    ) {
        OverlayStore.saveConfig(context, config, scopeId, useGameOverride)
    }
}
