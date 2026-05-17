package org.vita3k.emulator.overlay

import android.app.Activity
import android.content.Context
import android.content.SharedPreferences
import android.os.Build
import android.util.DisplayMetrics
import org.vita3k.emulator.R

data class OverlayPosition(
    val normalizedX: Float,
    val normalizedY: Float
)

object OverlayStore {
    private const val PREFS_NAME = "overlay_settings"
    private const val LEGACY_PREFS_SUFFIX = "_preferences"
    private const val GLOBAL_SCOPE = "global"
    private const val GAME_SCOPE_PREFIX = "game:"
    private const val KEY_OVERRIDE_ENABLED = "override_enabled"
    private const val KEY_OVERLAY_MASK = "overlay_mask"
    private const val KEY_OVERLAY_SCALE = "overlay_scale"
    private const val KEY_OVERLAY_OPACITY = "overlay_opacity"
    private const val KEY_HIDE_ON_CONTROLLER = "hide_overlay_on_controller"
    private const val KEY_CONFIG_PREFIX = "config"
    private const val KEY_LAYOUT_PREFIX = "layout"
    private const val KEY_POSITION_X_SUFFIX = ":x"
    private const val KEY_POSITION_Y_SUFFIX = ":y"

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
    fun normalizeScopeId(scopeId: String?): String = scopeId.orEmpty().trim()

    @JvmStatic
    fun hasGameOverride(context: Context, scopeId: String?): Boolean {
        val normalizedScopeId = normalizeScopeId(scopeId)
        if (normalizedScopeId.isEmpty()) {
            return false
        }
        return preferences(context).getBoolean(overrideEnabledKey(normalizedScopeId), false)
    }

    @JvmStatic
    fun loadGlobalState(context: Context): OverlayState {
        val prefs = preferences(context)
        val fallback = legacyGlobalState(context)
        return loadScopeState(prefs, scopeId = null, fallback = fallback)
    }

    @JvmStatic
    fun loadGameOverride(context: Context, scopeId: String?): OverlayState? {
        val normalizedScopeId = normalizeScopeId(scopeId)
        if (normalizedScopeId.isEmpty() || !hasGameOverride(context, normalizedScopeId)) {
            return null
        }
        return loadScopeState(preferences(context), normalizedScopeId, loadGlobalState(context))
    }

    @JvmStatic
    fun resolveState(context: Context, scopeId: String?, allowGameOverride: Boolean): OverlayState {
        val normalizedScopeId = normalizeScopeId(scopeId)
        val globalState = loadGlobalState(context)
        if (!allowGameOverride || normalizedScopeId.isEmpty() || !hasGameOverride(context, normalizedScopeId)) {
            return globalState
        }
        return loadScopeState(preferences(context), normalizedScopeId, globalState)
    }

    @JvmStatic
    fun saveGlobalState(context: Context, state: OverlayState) {
        saveScopeState(context, scopeId = null, state = state, markOverride = false)
    }

    @JvmStatic
    fun saveGameOverride(context: Context, scopeId: String?, state: OverlayState) {
        val normalizedScopeId = normalizeScopeId(scopeId)
        if (normalizedScopeId.isEmpty()) {
            saveGlobalState(context, state)
            return
        }
        saveScopeState(context, normalizedScopeId, state, markOverride = true)
    }

    @JvmStatic
    fun deleteGameOverride(context: Context, scopeId: String?) {
        val normalizedScopeId = normalizeScopeId(scopeId)
        if (normalizedScopeId.isEmpty()) {
            return
        }

        preferences(context).edit().apply {
            remove(overrideEnabledKey(normalizedScopeId))
            removeScopePayload(this, normalizedScopeId)
            apply()
        }
    }

    @JvmStatic
    fun clearAllGameOverrides(context: Context) {
        val prefs = preferences(context)
        prefs.edit().apply {
            prefs.all.keys
                .mapNotNull { key ->
                    if (!key.startsWith("$GAME_SCOPE_PREFIX")) {
                        null
                    } else {
                        key.removePrefix(GAME_SCOPE_PREFIX).substringBefore(':').takeIf { it.isNotBlank() }
                    }
                }
                .toSet()
                .forEach { scopeId ->
                    remove(overrideEnabledKey(scopeId))
                    removeScopePayload(this, scopeId)
                }
            apply()
        }
    }

    @JvmStatic
    fun defaultLayout(context: Context): OverlayLayout {
        val resources = context.resources
        val positions = defaultEntries.associate { entry ->
            entry.buttonType to OverlayPosition(
                normalizedX = (resources.getInteger(entry.xResId) / 1000f).coerceNormalizedCoordinate(),
                normalizedY = (resources.getInteger(entry.yResId) / 1000f).coerceNormalizedCoordinate()
            )
        }
        return OverlayLayout(positions).normalized()
    }

    @JvmStatic
    fun defaultState(context: Context): OverlayState =
        OverlayState(
            config = OverlayConfig().normalized(),
            layout = defaultLayout(context)
        )

    @JvmStatic
    fun loadConfig(context: Context, scopeId: String? = null, allowGameOverride: Boolean = false): OverlayConfig =
        resolveState(context, scopeId, allowGameOverride).config

    @JvmStatic
    fun saveConfig(context: Context, config: OverlayConfig, scopeId: String? = null, useGameOverride: Boolean = false) {
        val existing = resolveState(context, scopeId, useGameOverride)
        val updated = existing.copy(config = config.normalized())
        if (useGameOverride && !normalizeScopeId(scopeId).isEmpty()) {
            saveGameOverride(context, scopeId, updated)
        } else {
            saveGlobalState(context, updated)
        }
    }

    @JvmStatic
    fun loadLayout(context: Context, scopeId: String? = null, allowGameOverride: Boolean = false): OverlayLayout =
        resolveState(context, scopeId, allowGameOverride).layout

    private fun loadScopeState(
        prefs: SharedPreferences,
        scopeId: String?,
        fallback: OverlayState
    ): OverlayState {
        val normalizedScopeId = normalizeScopeId(scopeId)
        val layout = loadStoredLayout(prefs, normalizedScopeId, fallback.layout)
        return OverlayState(
            config = loadStoredConfig(prefs, normalizedScopeId, fallback.config),
            layout = layout
        )
    }

    private fun saveScopeState(context: Context, scopeId: String?, state: OverlayState, markOverride: Boolean) {
        val normalizedScopeId = normalizeScopeId(scopeId)
        val normalizedState = OverlayState(
            config = state.config.normalized(),
            layout = state.layout.normalized()
        )

        preferences(context).edit().apply {
            putInt(configKey(normalizedScopeId, KEY_OVERLAY_MASK), normalizedState.config.overlayMask)
            putInt(configKey(normalizedScopeId, KEY_OVERLAY_SCALE), normalizedState.config.overlayScale)
            putInt(configKey(normalizedScopeId, KEY_OVERLAY_OPACITY), normalizedState.config.overlayOpacity)
            putBoolean(
                configKey(normalizedScopeId, KEY_HIDE_ON_CONTROLLER),
                normalizedState.config.hideOverlayWhenControllerConnected
            )
            saveLayoutEntries(this, normalizedScopeId, normalizedState.layout)
            if (markOverride && normalizedScopeId.isNotEmpty()) {
                putBoolean(overrideEnabledKey(normalizedScopeId), true)
            }
            apply()
        }
    }

    private fun loadStoredConfig(
        prefs: SharedPreferences,
        scopeId: String,
        fallback: OverlayConfig
    ): OverlayConfig {
        val keys = listOf(
            configKey(scopeId, KEY_OVERLAY_MASK),
            configKey(scopeId, KEY_OVERLAY_SCALE),
            configKey(scopeId, KEY_OVERLAY_OPACITY),
            configKey(scopeId, KEY_HIDE_ON_CONTROLLER)
        )
        if (!keys.all(prefs::contains)) {
            return fallback
        }
        return OverlayConfig(
            overlayMask = prefs.getInt(keys[0], fallback.overlayMask),
            overlayScale = prefs.getInt(keys[1], fallback.overlayScale),
            overlayOpacity = prefs.getInt(keys[2], fallback.overlayOpacity),
            hideOverlayWhenControllerConnected = prefs.getBoolean(keys[3], fallback.hideOverlayWhenControllerConnected)
        ).normalized()
    }

    private fun loadStoredLayout(
        prefs: SharedPreferences,
        scopeId: String,
        fallback: OverlayLayout
    ): OverlayLayout {
        val anyEntryPresent = defaultEntries.any { entry ->
            prefs.contains(positionKey(scopeId, entry.buttonType, KEY_POSITION_X_SUFFIX)) &&
                prefs.contains(positionKey(scopeId, entry.buttonType, KEY_POSITION_Y_SUFFIX))
        }
        if (!anyEntryPresent) {
            return fallback
        }

        val positions = defaultEntries.associate { entry ->
            val fallbackPosition = fallback.positionFor(entry.buttonType) ?: OverlayPosition(0f, 0f)
            entry.buttonType to OverlayPosition(
                normalizedX = prefs.getFloat(
                    positionKey(scopeId, entry.buttonType, KEY_POSITION_X_SUFFIX),
                    fallbackPosition.normalizedX
                ).coerceNormalizedCoordinate(),
                normalizedY = prefs.getFloat(
                    positionKey(scopeId, entry.buttonType, KEY_POSITION_Y_SUFFIX),
                    fallbackPosition.normalizedY
                ).coerceNormalizedCoordinate()
            )
        }
        return OverlayLayout(positions).normalized()
    }

    private fun legacyGlobalState(context: Context): OverlayState {
        val defaultState = defaultState(context)
        val legacyConfigPrefs = context.applicationContext.getSharedPreferences("emulation_session", Context.MODE_PRIVATE)
        val config = if (legacyConfigPrefs.contains(KEY_OVERLAY_MASK)) {
            OverlayConfig(
                overlayMask = legacyConfigPrefs.getInt(KEY_OVERLAY_MASK, defaultState.config.overlayMask),
                overlayScale = legacyConfigPrefs.getInt(KEY_OVERLAY_SCALE, defaultState.config.overlayScale),
                overlayOpacity = legacyConfigPrefs.getInt(KEY_OVERLAY_OPACITY, defaultState.config.overlayOpacity),
                hideOverlayWhenControllerConnected = legacyConfigPrefs.getBoolean(
                    KEY_HIDE_ON_CONTROLLER,
                    defaultState.config.hideOverlayWhenControllerConnected
                )
            ).normalized()
        } else {
            defaultState.config
        }

        val legacyLayout = loadLegacyGlobalLayout(context, defaultState.layout)
        return OverlayState(
            config = config,
            layout = legacyLayout
        )
    }

    private fun loadLegacyGlobalLayout(context: Context, fallback: OverlayLayout): OverlayLayout {
        val legacyPrefs = context.applicationContext.getSharedPreferences("${context.packageName}$LEGACY_PREFS_SUFFIX", Context.MODE_PRIVATE)
        val anyEntryPresent = defaultEntries.any { entry ->
            legacyPrefs.contains(legacyPositionKey(entry.buttonType, "-X")) &&
                legacyPrefs.contains(legacyPositionKey(entry.buttonType, "-Y"))
        }
        if (!anyEntryPresent) {
            return fallback
        }

        val (width, height) = resolveLegacyOverlayBounds(context)
        val positions = defaultEntries.associate { entry ->
            val fallbackPosition = fallback.positionFor(entry.buttonType) ?: OverlayPosition(0f, 0f)
            val rawX = legacyPrefs.getFloat(legacyPositionKey(entry.buttonType, "-X"), fallbackPosition.normalizedX * width)
            val rawY = legacyPrefs.getFloat(legacyPositionKey(entry.buttonType, "-Y"), fallbackPosition.normalizedY * height)
            entry.buttonType to OverlayPosition(
                normalizedX = if (width > 0f) (rawX / width).coerceNormalizedCoordinate() else fallbackPosition.normalizedX,
                normalizedY = if (height > 0f) (rawY / height).coerceNormalizedCoordinate() else fallbackPosition.normalizedY
            )
        }
        return OverlayLayout(positions).normalized()
    }

    private fun saveLayoutEntries(
        editor: SharedPreferences.Editor,
        scopeId: String,
        layout: OverlayLayout
    ) {
        defaultEntries.forEach { entry ->
            val position = layout.positionFor(entry.buttonType) ?: OverlayPosition(0f, 0f)
            editor.putFloat(positionKey(scopeId, entry.buttonType, KEY_POSITION_X_SUFFIX), position.normalizedX)
            editor.putFloat(positionKey(scopeId, entry.buttonType, KEY_POSITION_Y_SUFFIX), position.normalizedY)
            editor.remove(scopePrefix(scopeId) + ":" + KEY_LAYOUT_PREFIX + ":portrait:" + entry.buttonType + KEY_POSITION_X_SUFFIX)
            editor.remove(scopePrefix(scopeId) + ":" + KEY_LAYOUT_PREFIX + ":portrait:" + entry.buttonType + KEY_POSITION_Y_SUFFIX)
        }
    }

    private fun removeScopePayload(editor: SharedPreferences.Editor, scopeId: String) {
        editor.remove(configKey(scopeId, KEY_OVERLAY_MASK))
        editor.remove(configKey(scopeId, KEY_OVERLAY_SCALE))
        editor.remove(configKey(scopeId, KEY_OVERLAY_OPACITY))
        editor.remove(configKey(scopeId, KEY_HIDE_ON_CONTROLLER))
        defaultEntries.forEach { entry ->
            editor.remove(positionKey(scopeId, entry.buttonType, KEY_POSITION_X_SUFFIX))
            editor.remove(positionKey(scopeId, entry.buttonType, KEY_POSITION_Y_SUFFIX))
            editor.remove(scopePrefix(scopeId) + ":" + KEY_LAYOUT_PREFIX + ":portrait:" + entry.buttonType + KEY_POSITION_X_SUFFIX)
            editor.remove(scopePrefix(scopeId) + ":" + KEY_LAYOUT_PREFIX + ":portrait:" + entry.buttonType + KEY_POSITION_Y_SUFFIX)
        }
    }

    private fun preferences(context: Context): SharedPreferences =
        context.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    private fun overrideEnabledKey(scopeId: String): String = "$GAME_SCOPE_PREFIX$scopeId:$KEY_OVERRIDE_ENABLED"

    private fun configKey(scopeId: String?, key: String): String =
        scopePrefix(scopeId) + ":" + KEY_CONFIG_PREFIX + ":" + key

    private fun positionKey(scopeId: String?, buttonType: Int, suffix: String): String =
        scopePrefix(scopeId) + ":" + KEY_LAYOUT_PREFIX + ":landscape:" + buttonType + suffix

    private fun scopePrefix(scopeId: String?): String {
        val normalizedScopeId = normalizeScopeId(scopeId)
        return if (normalizedScopeId.isEmpty()) {
            GLOBAL_SCOPE
        } else {
            GAME_SCOPE_PREFIX + normalizedScopeId
        }
    }

    private fun legacyPositionKey(buttonType: Int, axisSuffix: String): String = "$buttonType$axisSuffix"

    private fun resolveLegacyOverlayBounds(context: Context): Pair<Float, Float> {
        val activity = context as? Activity
        if (activity != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val bounds = activity.windowManager.currentWindowMetrics.bounds
            return maxOf(bounds.width(), bounds.height()).toFloat() to minOf(bounds.width(), bounds.height()).toFloat()
        }

        val metrics = DisplayMetrics()
        if (activity != null) {
            @Suppress("DEPRECATION")
            activity.windowManager.defaultDisplay.getMetrics(metrics)
        } else {
            metrics.setTo(context.resources.displayMetrics)
        }
        return maxOf(metrics.widthPixels, metrics.heightPixels).toFloat() to
            minOf(metrics.widthPixels, metrics.heightPixels).toFloat()
    }
}
