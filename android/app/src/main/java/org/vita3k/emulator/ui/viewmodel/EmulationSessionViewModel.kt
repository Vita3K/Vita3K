package org.vita3k.emulator.ui.viewmodel

import android.app.Application
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import org.vita3k.emulator.Emulator
import org.vita3k.emulator.NativeLib
import org.vita3k.emulator.data.NativeImeState
import org.vita3k.emulator.overlay.DEFAULT_OVERLAY_MASK
import org.vita3k.emulator.overlay.OverlayConfig
import org.vita3k.emulator.overlay.OverlayLayout
import org.vita3k.emulator.overlay.OverlayState
import org.vita3k.emulator.overlay.OverlayStore

data class EmulationSessionUiState(
    val titleId: String = "",
    val gameTitle: String = "",
    val hasCustomConfig: Boolean = false,
    val showMenu: Boolean = false,
    val isPaused: Boolean = false,
    val isEditingControls: Boolean = false,
    val wasPausedBeforeControlsEditor: Boolean = false,
    val showExitConfirmation: Boolean = false,
    val controlsVisible: Boolean = true,
    val overlayMask: Int = DEFAULT_OVERLAY_MASK,
    val l2r2Visible: Boolean = false,
    val l3r3Visible: Boolean = false,
    val touchSwitchVisible: Boolean = true,
    val hideToggleVisible: Boolean = false,
    val overlayScale: Int = 100,
    val overlayOpacity: Int = 100,
    val overlayLayout: OverlayLayout = OverlayLayout(),
    val hideOverlayWhenControllerConnected: Boolean = true,
    val controllerConnected: Boolean = false,
    val statusMessage: String? = null
)

class EmulationSessionViewModel(application: Application) : AndroidViewModel(application) {

    private var initialized = false

    var uiState by mutableStateOf(EmulationSessionUiState())
        private set

    var imeState by mutableStateOf<NativeImeState?>(null)
        private set

    fun initialize(titleId: String?, gameTitle: String?) {
        if (initialized && uiState.titleId == titleId.orEmpty()) {
            return
        }

        val resolvedTitleId = titleId.orEmpty()
        val resolved = resolveOverlayState(resolvedTitleId)
        uiState = uiState
            .withOverlayState(resolved.second)
            .copy(
                titleId = resolvedTitleId,
                gameTitle = gameTitle.orEmpty(),
                hasCustomConfig = resolved.first
            )
        initialized = true
    }

    fun updateRunningApp(titleId: String?, gameTitle: String?) {
        val resolvedTitleId = titleId.orEmpty()
        val resolved = resolveOverlayState(resolvedTitleId)
        uiState = uiState
            .withOverlayState(resolved.second)
            .copy(
                titleId = resolvedTitleId,
                gameTitle = gameTitle.orEmpty(),
                hasCustomConfig = resolved.first
            )
        initialized = true
    }

    fun applyOverlayState(emulator: Emulator) {
        emulator.setControllerOverlayLayout(uiState.overlayLayout)
        emulator.setControllerOverlayScale(uiState.overlayScale / 100f)
        emulator.setControllerOverlayOpacity(uiState.overlayOpacity)
        val visibleMask = when {
            uiState.showMenu -> 0
            uiState.isEditingControls -> activeOverlayMask()
            uiState.hideOverlayWhenControllerConnected && uiState.controllerConnected -> 0
            else -> uiState.overlayMask
        }
        emulator.setControllerOverlayState(visibleMask, uiState.isEditingControls, false)
    }

    fun handleBackPressed(emulator: Emulator): Boolean {
        return when {
            uiState.isEditingControls -> {
                finishControlsEditor(emulator)
                true
            }
            uiState.showMenu -> {
                closeMenu(emulator)
                true
            }
            else -> {
                openMenu(emulator, pauseGame = false)
                true
            }
        }
    }

    @JvmOverloads
    fun openMenu(emulator: Emulator, pauseGame: Boolean = true) {
        if (uiState.showMenu) {
            return
        }

        emulator.prepareImeForPauseMenu()
        emulator.ensurePauseMenuOnTop()
        releaseInputs(emulator)
        val paused = if (pauseGame) {
            if (!setNativePauseReasonEnabled(NativeLib.PAUSE_REASON_MENU, true)) {
                return
            }
            true
        } else {
            isNativeAppPaused()
        }
        if (!setNativeInputIntercepted(true)) {
            if (pauseGame) {
                setNativePauseReasonEnabled(NativeLib.PAUSE_REASON_MENU, false)
            }
            return
        }
        uiState = uiState.copy(
            showMenu = true,
            isPaused = paused,
            isEditingControls = false,
            showExitConfirmation = false,
            statusMessage = null
        )
        emulator.setControllerOverlayState(0, false, false)
    }

    fun closeMenu(emulator: Emulator) {
        releaseInputs(emulator)
        if (uiState.isPaused) {
            setNativePauseReasonEnabled(NativeLib.PAUSE_REASON_MENU, false)
        }
        setNativeInputIntercepted(false)
        uiState = uiState.copy(
            showMenu = false,
            isPaused = false,
            isEditingControls = false,
            showExitConfirmation = false,
            statusMessage = null
        )
        applyOverlayState(emulator)
        emulator.restoreImeAfterPauseMenu()
    }

    fun togglePause(emulator: Emulator) {
        if (uiState.isPaused) {
            closeMenu(emulator)
        } else {
            releaseInputs(emulator)
            if (!setNativePauseReasonEnabled(NativeLib.PAUSE_REASON_MENU, true)) {
                return
            }
            uiState = uiState.copy(isPaused = true, statusMessage = null)
            emulator.setControllerOverlayState(0, false, false)
        }
    }

    fun requestExit() {
        uiState = uiState.copy(showExitConfirmation = true)
    }

    fun dismissExitConfirmation() {
        uiState = uiState.copy(showExitConfirmation = false)
    }

    fun confirmExit(emulator: Emulator) {
        releaseInputs(emulator)
        uiState = uiState.copy(showExitConfirmation = false, statusMessage = null)
        emulator.requestNativeQuit()
    }

    fun setControlsVisible(emulator: Emulator, visible: Boolean) {
        updateOverlayConfig(emulator) { it.withControlsVisible(visible) }
    }

    fun setL2R2Visible(emulator: Emulator, visible: Boolean) {
        updateOverlayConfig(emulator) { it.withL2R2Visible(visible) }
    }

    fun setL3R3Visible(emulator: Emulator, visible: Boolean) {
        updateOverlayConfig(emulator) { it.withL3R3Visible(visible) }
    }

    fun setTouchSwitchVisible(emulator: Emulator, visible: Boolean) {
        updateOverlayConfig(emulator) { it.withTouchSwitchVisible(visible) }
    }

    fun setHideToggleVisible(emulator: Emulator, visible: Boolean) {
        updateOverlayConfig(emulator) { it.withHideToggleVisible(visible) }
    }

    fun setOverlayScale(emulator: Emulator, value: Int) {
        updateOverlayConfig(emulator) { it.copy(overlayScale = value) }
    }

    fun setOverlayOpacity(emulator: Emulator, value: Int) {
        updateOverlayConfig(emulator) { it.copy(overlayOpacity = value) }
    }

    fun setControllerConnected(emulator: Emulator, connected: Boolean) {
        if (uiState.controllerConnected == connected) {
            return
        }

        uiState = uiState.copy(controllerConnected = connected)
        applyOverlayState(emulator)
    }

    fun startControlsEditor(emulator: Emulator) {
        val mask = activeOverlayMask()
        releaseInputs(emulator)
        uiState = uiState.copy(
            showMenu = false,
            isPaused = uiState.isPaused,
            isEditingControls = true,
            wasPausedBeforeControlsEditor = uiState.isPaused,
            controlsVisible = true,
            overlayMask = mask,
            showExitConfirmation = false,
            statusMessage = null
        )
        emulator.setControllerOverlayState(mask, true, false)
    }

    fun finishControlsEditor(emulator: Emulator) {
        releaseInputs(emulator)
        val updatedOverlayState = currentOverlayState().copy(
            layout = emulator.captureControllerOverlayLayout()
        )
        val hasCustomConfig = persistOverlayState(updatedOverlayState)
        uiState = uiState.withOverlayState(updatedOverlayState).copy(
            hasCustomConfig = hasCustomConfig,
            showMenu = true,
            isPaused = uiState.wasPausedBeforeControlsEditor,
            isEditingControls = false,
            wasPausedBeforeControlsEditor = false,
            statusMessage = null
        )
        emulator.ensurePauseMenuOnTop()
        emulator.setControllerOverlayState(0, false, false)
    }

    fun resetControlsLayout(emulator: Emulator) {
        val updatedOverlayState = currentOverlayState().copy(
            layout = OverlayStore.defaultLayout(getApplication())
        )
        val hasCustomConfig = persistOverlayState(updatedOverlayState)
        uiState = uiState.withOverlayState(updatedOverlayState).copy(
            hasCustomConfig = hasCustomConfig,
            statusMessage = null
        )
        applyOverlayState(emulator)
    }

    fun clearStatusMessage() {
        uiState = uiState.copy(statusMessage = null)
    }

    fun showStatusMessage(message: String) {
        uiState = uiState.copy(statusMessage = message)
    }

    fun updateImeState(state: NativeImeState?) {
        if (imeState == state) {
            return
        }

        imeState = state
    }

    fun currentOverlayConfig(): OverlayConfig {
        return currentOverlayState().config
    }

    fun updateOverlayConfig(emulator: Emulator, transform: (OverlayConfig) -> OverlayConfig) {
        val updatedOverlayState = currentOverlayState().copy(
            config = transform(currentOverlayConfig()).normalized()
        )
        val hasCustomConfig = persistOverlayState(updatedOverlayState)
        uiState = uiState.withOverlayState(updatedOverlayState).copy(hasCustomConfig = hasCustomConfig)
        applyOverlayState(emulator)
    }

    private fun activeOverlayMask(): Int {
        return if (uiState.overlayMask == 0) DEFAULT_OVERLAY_MASK else uiState.overlayMask
    }

    private fun currentOverlayState(): OverlayState {
        return OverlayState(
            config = OverlayConfig(
                overlayMask = uiState.overlayMask,
                overlayScale = uiState.overlayScale,
                overlayOpacity = uiState.overlayOpacity,
                hideOverlayWhenControllerConnected = uiState.hideOverlayWhenControllerConnected
            ).normalized(),
            layout = uiState.overlayLayout.normalized()
        )
    }

    private fun persistOverlayState(state: OverlayState): Boolean {
        val hasCustomConfig = resolveHasCustomConfig(uiState.titleId)
        if (hasCustomConfig && uiState.titleId.isNotBlank()) {
            OverlayStore.saveGameOverride(getApplication(), uiState.titleId, state)
        } else {
            OverlayStore.saveGlobalState(getApplication(), state)
        }
        return hasCustomConfig
    }

    private fun resolveOverlayState(titleId: String): Pair<Boolean, OverlayState> {
        val hasCustomConfig = resolveHasCustomConfig(titleId)
        val overlayState = OverlayStore.resolveState(
            context = getApplication(),
            scopeId = titleId,
            allowGameOverride = hasCustomConfig
        )
        return hasCustomConfig to overlayState
    }

    private fun resolveHasCustomConfig(titleId: String): Boolean {
        return titleId.isNotBlank() &&
            runCatching { NativeLib.hasCustomConfig(titleId) }.getOrDefault(false)
    }

    private fun releaseInputs(emulator: Emulator) {
        runCatching { emulator.releaseControllerOverlayInputs() }
    }

    private fun setNativePauseReasonEnabled(reasonMask: Int, enabled: Boolean): Boolean {
        return runCatching { NativeLib.setPauseReasonEnabled(reasonMask, enabled) }.getOrDefault(false)
    }

    private fun setNativeInputIntercepted(intercepted: Boolean): Boolean {
        return runCatching { NativeLib.setInputIntercepted(intercepted) }.getOrDefault(false)
    }

    private fun isNativeAppPaused(): Boolean {
        return runCatching { NativeLib.isAppPaused() }.getOrDefault(false)
    }

    private fun EmulationSessionUiState.withOverlayState(state: OverlayState): EmulationSessionUiState {
        val normalized = state.config.normalized()
        return copy(
            controlsVisible = normalized.controlsVisible,
            overlayMask = normalized.overlayMask,
            l2r2Visible = normalized.l2r2Visible,
            l3r3Visible = normalized.l3r3Visible,
            touchSwitchVisible = normalized.touchSwitchVisible,
            hideToggleVisible = normalized.hideToggleVisible,
            overlayScale = normalized.overlayScale,
            overlayOpacity = normalized.overlayOpacity,
            overlayLayout = state.layout.normalized(),
            hideOverlayWhenControllerConnected = normalized.hideOverlayWhenControllerConnected
        )
    }
}
