package org.vita3k.emulator.overlay

import android.app.Activity
import android.content.Context
import android.content.SharedPreferences
import android.os.Build
import android.util.DisplayMetrics
import org.vita3k.emulator.R

object OverlayLayoutStore {
    private const val KEY_OVERLAY_INIT = "OverlayInit"

    private data class OverlayPositionDefaults(
        val buttonType: Int,
        val xResId: Int,
        val yResId: Int
    )

    private val defaultEntries = listOf(
        OverlayPositionDefaults(InputOverlay.ButtonType.BUTTON_CROSS, R.integer.BUTTON_CROSS_X, R.integer.BUTTON_CROSS_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.BUTTON_CIRCLE, R.integer.BUTTON_CIRCLE_X, R.integer.BUTTON_CIRCLE_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.BUTTON_SQUARE, R.integer.BUTTON_SQUARE_X, R.integer.BUTTON_SQUARE_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.BUTTON_TRIANGLE, R.integer.BUTTON_TRIANGLE_X, R.integer.BUTTON_TRIANGLE_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.BUTTON_SELECT, R.integer.BUTTON_SELECT_X, R.integer.BUTTON_SELECT_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.BUTTON_START, R.integer.BUTTON_START_X, R.integer.BUTTON_START_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.BUTTON_PS, R.integer.BUTTON_PS_X, R.integer.BUTTON_PS_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.DPAD_UP, R.integer.DPAD_UP_X, R.integer.DPAD_UP_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.STICK_LEFT, R.integer.STICK_LEFT_X, R.integer.STICK_LEFT_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.STICK_RIGHT, R.integer.STICK_RIGHT_X, R.integer.STICK_RIGHT_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.TRIGGER_L, R.integer.TRIGGER_L_X, R.integer.TRIGGER_L_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.TRIGGER_R, R.integer.TRIGGER_R_X, R.integer.TRIGGER_R_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.TRIGGER_L2, R.integer.TRIGGER_L2_X, R.integer.TRIGGER_L2_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.TRIGGER_R2, R.integer.TRIGGER_R2_X, R.integer.TRIGGER_R2_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.TRIGGER_L3, R.integer.TRIGGER_L3_X, R.integer.TRIGGER_L3_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.TRIGGER_R3, R.integer.TRIGGER_R3_X, R.integer.TRIGGER_R3_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.BUTTON_TOUCH_HIDE, R.integer.BUTTON_TOUCH_HIDE_X, R.integer.BUTTON_TOUCH_HIDE_Y),
        OverlayPositionDefaults(InputOverlay.ButtonType.BUTTON_TOUCH_SWITCH, R.integer.BUTTON_TOUCH_SWITCH_X, R.integer.BUTTON_TOUCH_SWITCH_Y)
    )

    @JvmStatic
    fun ensureInitialized(context: Context, profileId: String? = null) {
        val prefs = preferences(context)
        val initKey = initKey(profileId)
        if (!prefs.getBoolean(initKey, false)) {
            resetToDefaults(context, profileId)
        }
    }

    @JvmStatic
    fun resetToDefaults(context: Context, profileId: String? = null) {
        val prefs = preferences(context)
        val editor = prefs.edit()
        val resources = context.resources
        val (maxX, maxY) = resolveOverlayBounds(context)

        defaultEntries.forEach { entry ->
            editor.putFloat(
                positionXKey(entry.buttonType, "", profileId),
                (resources.getInteger(entry.xResId) / 1000f) * maxX
            )
            editor.putFloat(
                positionYKey(entry.buttonType, "", profileId),
                (resources.getInteger(entry.yResId) / 1000f) * maxY
            )
        }

        editor.putBoolean(initKey(profileId), true)
        editor.apply()
    }

    @JvmStatic
    fun positionXKey(buttonType: Int, orientation: String, profileId: String? = null): String =
        scopedKey("$buttonType$orientation-X", profileId)

    @JvmStatic
    fun positionYKey(buttonType: Int, orientation: String, profileId: String? = null): String =
        scopedKey("$buttonType$orientation-Y", profileId)

    @JvmStatic
    fun initKey(profileId: String? = null): String = scopedKey(KEY_OVERLAY_INIT, profileId)

    @JvmStatic
    fun normalizeProfileId(profileId: String?): String = profileId.orEmpty().trim()

    @JvmStatic
    fun preferences(context: Context): SharedPreferences =
        context.getSharedPreferences("${context.packageName}_preferences", Context.MODE_PRIVATE)

    private fun scopedKey(rawKey: String, profileId: String?): String {
        val normalizedProfile = normalizeProfileId(profileId)
        return if (normalizedProfile.isEmpty()) rawKey else "overlay:$normalizedProfile:$rawKey"
    }

    private fun resolveOverlayBounds(context: Context): Pair<Float, Float> {
        val activity = context as? Activity
        if (activity != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val bounds = activity.windowManager.currentWindowMetrics.bounds
            val width = bounds.width().toFloat()
            val height = bounds.height().toFloat()
            return if (width >= height) {
                width to height
            } else {
                height to width
            }
        }

        val metrics = DisplayMetrics()
        if (activity != null) {
            @Suppress("DEPRECATION")
            activity.windowManager.defaultDisplay.getMetrics(metrics)
        } else {
            metrics.setTo(context.resources.displayMetrics)
        }

        var maxX = metrics.heightPixels.toFloat()
        var maxY = metrics.widthPixels.toFloat()
        if (maxY > maxX) {
            val tmp = maxX
            maxX = maxY
            maxY = tmp
        }
        return maxX to maxY
    }
}
