package org.vita3k.emulator.ui.screens.settings

import android.content.Context
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import androidx.annotation.StringRes
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.Dialog
import org.vita3k.emulator.ConnectedGamepad
import org.vita3k.emulator.ControllerFamily
import org.vita3k.emulator.R
import org.vita3k.emulator.data.EmulatorConfig
import org.vita3k.emulator.ui.theme.ApplyDialogDim
import kotlin.math.abs
import kotlin.math.roundToInt

private const val SDL_GAMEPAD_BUTTON_SOUTH = 0
private const val SDL_GAMEPAD_BUTTON_EAST = 1
private const val SDL_GAMEPAD_BUTTON_WEST = 2
private const val SDL_GAMEPAD_BUTTON_NORTH = 3
private const val SDL_GAMEPAD_BUTTON_BACK = 4
private const val SDL_GAMEPAD_BUTTON_GUIDE = 5
private const val SDL_GAMEPAD_BUTTON_START = 6
private const val SDL_GAMEPAD_BUTTON_LEFT_STICK = 7
private const val SDL_GAMEPAD_BUTTON_RIGHT_STICK = 8
private const val SDL_GAMEPAD_BUTTON_LEFT_SHOULDER = 9
private const val SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER = 10
private const val SDL_GAMEPAD_BUTTON_DPAD_UP = 11
private const val SDL_GAMEPAD_BUTTON_DPAD_DOWN = 12
private const val SDL_GAMEPAD_BUTTON_DPAD_LEFT = 13
private const val SDL_GAMEPAD_BUTTON_DPAD_RIGHT = 14

private const val SDL_GAMEPAD_AXIS_LEFTX = 0
private const val SDL_GAMEPAD_AXIS_LEFTY = 1
private const val SDL_GAMEPAD_AXIS_RIGHTX = 2
private const val SDL_GAMEPAD_AXIS_RIGHTY = 3
private const val SDL_GAMEPAD_AXIS_LEFT_TRIGGER = 4
private const val SDL_GAMEPAD_AXIS_RIGHT_TRIGGER = 5

private data class ButtonBindingRow(
    @StringRes val labelRes: Int,
    val bindIndex: Int
)

private data class AxisBindingRow(
    @StringRes val labelRes: Int,
    val bindIndex: Int
)

private data class ManualBindingOption(
    val label: String,
    val value: Int
)

private enum class CaptureMode {
    Button,
    Axis
}

private val buttonBindingRows = listOf(
    ButtonBindingRow(R.string.settings_controls_bind_dpad_up, SDL_GAMEPAD_BUTTON_DPAD_UP),
    ButtonBindingRow(R.string.settings_controls_bind_dpad_down, SDL_GAMEPAD_BUTTON_DPAD_DOWN),
    ButtonBindingRow(R.string.settings_controls_bind_dpad_left, SDL_GAMEPAD_BUTTON_DPAD_LEFT),
    ButtonBindingRow(R.string.settings_controls_bind_dpad_right, SDL_GAMEPAD_BUTTON_DPAD_RIGHT),
    ButtonBindingRow(R.string.settings_controls_bind_triangle, SDL_GAMEPAD_BUTTON_NORTH),
    ButtonBindingRow(R.string.settings_controls_bind_circle, SDL_GAMEPAD_BUTTON_EAST),
    ButtonBindingRow(R.string.settings_controls_bind_cross, SDL_GAMEPAD_BUTTON_SOUTH),
    ButtonBindingRow(R.string.settings_controls_bind_square, SDL_GAMEPAD_BUTTON_WEST),
    ButtonBindingRow(R.string.settings_controls_bind_l1, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER),
    ButtonBindingRow(R.string.settings_controls_bind_r1, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER),
    ButtonBindingRow(R.string.settings_controls_bind_l3, SDL_GAMEPAD_BUTTON_LEFT_STICK),
    ButtonBindingRow(R.string.settings_controls_bind_r3, SDL_GAMEPAD_BUTTON_RIGHT_STICK),
    ButtonBindingRow(R.string.settings_controls_bind_select, SDL_GAMEPAD_BUTTON_BACK),
    ButtonBindingRow(R.string.settings_controls_bind_start, SDL_GAMEPAD_BUTTON_START),
    ButtonBindingRow(R.string.settings_controls_bind_ps_button, SDL_GAMEPAD_BUTTON_GUIDE)
)

private val axisBindingRows = listOf(
    AxisBindingRow(R.string.settings_controls_bind_left_stick_up, SDL_GAMEPAD_AXIS_LEFTY),
    AxisBindingRow(R.string.settings_controls_bind_left_stick_down, SDL_GAMEPAD_AXIS_LEFTY),
    AxisBindingRow(R.string.settings_controls_bind_left_stick_left, SDL_GAMEPAD_AXIS_LEFTX),
    AxisBindingRow(R.string.settings_controls_bind_left_stick_right, SDL_GAMEPAD_AXIS_LEFTX),
    AxisBindingRow(R.string.settings_controls_bind_right_stick_up, SDL_GAMEPAD_AXIS_RIGHTY),
    AxisBindingRow(R.string.settings_controls_bind_right_stick_down, SDL_GAMEPAD_AXIS_RIGHTY),
    AxisBindingRow(R.string.settings_controls_bind_right_stick_left, SDL_GAMEPAD_AXIS_RIGHTX),
    AxisBindingRow(R.string.settings_controls_bind_right_stick_right, SDL_GAMEPAD_AXIS_RIGHTX),
    AxisBindingRow(R.string.settings_controls_bind_l2, SDL_GAMEPAD_AXIS_LEFT_TRIGGER),
    AxisBindingRow(R.string.settings_controls_bind_r2, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
)

@Composable
internal fun ControllerMappingSections(
    cfg: EmulatorConfig,
    connectedGamepads: List<ConnectedGamepad>,
    onUpdate: (EmulatorConfig.() -> Unit) -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    val controllerFamily = connectedGamepads.firstOrNull()?.family ?: ControllerFamily.Standard
    var pendingButtonBinding by remember { mutableStateOf<ButtonBindingRow?>(null) }
    var pendingAxisBinding by remember { mutableStateOf<AxisBindingRow?>(null) }

    SettingsSectionCard(
        title = stringResource(R.string.settings_controls_connected_title),
        summary = if (connectedGamepads.isEmpty()) {
            stringResource(R.string.settings_controls_connected_none)
        } else {
            connectedGamepads.joinToString(separator = "\n") { it.name }
        },
        help = SettingsHelpEntry(
            title = stringResource(R.string.settings_controls_connected_title),
            body = stringResource(R.string.settings_controls_connected_desc),
            scope = SettingsScope.Global
        ),
        onShowHelp = onShowHelp
    ) {
        if (connectedGamepads.isEmpty()) {
            SettingsNote(text = stringResource(R.string.settings_controls_manual_available))
        }
    }

    SettingsSectionCard(
        title = stringResource(R.string.settings_controls_mapping_buttons_title),
        summary = stringResource(R.string.settings_controls_mapping_buttons_desc),
        help = SettingsHelpEntry(
            title = stringResource(R.string.settings_controls_mapping_buttons_title),
            body = stringResource(R.string.settings_controls_mapping_buttons_help),
            scope = SettingsScope.Global
        ),
        onShowHelp = onShowHelp
    ) {
        buttonBindingRows.forEach { row ->
            SettingsActionRow(
                title = stringResource(row.labelRes),
                value = buttonLabel(controllerFamily, currentButtonBinding(cfg, row.bindIndex)),
                onClick = { pendingButtonBinding = row },
                help = null,
                onShowHelp = onShowHelp
            )
        }
    }

    SettingsSectionCard(
        title = stringResource(R.string.settings_controls_mapping_axes_title),
        summary = stringResource(R.string.settings_controls_mapping_axes_desc),
        help = SettingsHelpEntry(
            title = stringResource(R.string.settings_controls_mapping_axes_title),
            body = stringResource(R.string.settings_controls_mapping_axes_help),
            scope = SettingsScope.Global
        ),
        onShowHelp = onShowHelp
    ) {
        SettingsNote(text = stringResource(R.string.settings_controls_mapping_axes_note))
        axisBindingRows.forEach { row ->
            SettingsActionRow(
                title = stringResource(row.labelRes),
                value = axisLabel(controllerFamily, currentAxisBinding(cfg, row.bindIndex)),
                onClick = { pendingAxisBinding = row },
                help = null,
                onShowHelp = onShowHelp
            )
        }
    }

    SettingsSectionCard(
        title = stringResource(R.string.settings_controls_behavior_title),
        summary = null,
        help = null,
        onShowHelp = onShowHelp
    ) {
        SettingsSliderRow(
            title = stringResource(R.string.settings_controls_analog_multiplier),
            valueLabel = stringResource(
                R.string.settings_controls_multiplier_value,
                cfg.controllerAnalogMultiplier.coerceIn(0.1f, 2.0f)
            ),
            value = (cfg.controllerAnalogMultiplier * 100f).coerceIn(10f, 200f),
            onValueChange = {
                onUpdate {
                    controllerAnalogMultiplier = (it.roundToInt() / 100f).coerceIn(0.1f, 2.0f)
                }
            },
            valueRange = 10f..200f,
            steps = 189,
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_controls_analog_multiplier),
                body = stringResource(R.string.settings_controls_analog_multiplier_desc),
                scope = SettingsScope.Global
            ),
            onShowHelp = onShowHelp
        )
        SettingsToggleRow(
            title = stringResource(R.string.settings_controls_disable_motion),
            checked = cfg.disableMotion,
            onCheckedChange = { checked ->
                onUpdate { disableMotion = checked }
            },
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_controls_disable_motion),
                body = stringResource(R.string.settings_controls_disable_motion_desc),
                scope = SettingsScope.Global
            ),
            onShowHelp = onShowHelp
        )
    }

    pendingButtonBinding?.let { row ->
        BindingCaptureDialog(
            title = stringResource(row.labelRes),
            prompt = stringResource(R.string.settings_controls_capture_button_prompt),
            options = buttonManualOptions(controllerFamily),
            selectedValue = currentButtonBinding(cfg, row.bindIndex),
            captureMode = CaptureMode.Button,
            connectedGamepads = connectedGamepads,
            onDismiss = { pendingButtonBinding = null },
            onSelected = { mappedButton ->
                onUpdate {
                    controllerBinds = controllerBinds.copyOf().also { binds ->
                        binds[row.bindIndex] = mappedButton
                    }
                }
                pendingButtonBinding = null
            }
        )
    }

    pendingAxisBinding?.let { row ->
        BindingCaptureDialog(
            title = stringResource(row.labelRes),
            prompt = stringResource(R.string.settings_controls_capture_axis_prompt),
            options = axisManualOptions(controllerFamily),
            selectedValue = currentAxisBinding(cfg, row.bindIndex),
            captureMode = CaptureMode.Axis,
            connectedGamepads = connectedGamepads,
            onDismiss = { pendingAxisBinding = null },
            onSelected = { mappedAxis ->
                onUpdate {
                    controllerAxisBinds = controllerAxisBinds.copyOf().also { binds ->
                        binds[row.bindIndex] = mappedAxis
                    }
                }
                pendingAxisBinding = null
            }
        )
    }
}

@Composable
private fun BindingCaptureDialog(
    title: String,
    prompt: String,
    options: List<ManualBindingOption>,
    selectedValue: Int,
    captureMode: CaptureMode,
    connectedGamepads: List<ConnectedGamepad>,
    onDismiss: () -> Unit,
    onSelected: (Int) -> Unit
) {
    Dialog(onDismissRequest = onDismiss) {
        ApplyDialogDim()
        Surface(
            modifier = Modifier.fillMaxWidth(),
            shape = MaterialTheme.shapes.extraLarge,
            color = MaterialTheme.colorScheme.surfaceContainerHigh
        ) {
            Column(
                modifier = Modifier.padding(24.dp),
                verticalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.headlineSmall,
                    color = MaterialTheme.colorScheme.onSurface
                )
                Text(
                    text = prompt,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                if (connectedGamepads.isNotEmpty()) {
                    Text(
                        text = connectedGamepads.joinToString(separator = "\n") { it.name },
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                CaptureInputSurface(
                    captureMode = captureMode,
                    enabled = connectedGamepads.isNotEmpty(),
                    onCaptured = onSelected
                )
                SettingsScrollableChoiceSelector(
                    title = stringResource(R.string.settings_controls_manual_selection),
                    options = options.map { it.label },
                    selectedIndex = options.indexOfFirst { it.value == selectedValue }.coerceAtLeast(0),
                    onSelected = { index -> onSelected(options[index].value) },
                    summary = options.firstOrNull { it.value == selectedValue }?.label,
                    help = null,
                    onShowHelp = {}
                )
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.End
                ) {
                    TextButton(onClick = onDismiss) {
                        Text(stringResource(R.string.action_cancel))
                    }
                }
            }
        }
    }
}

@Composable
private fun CaptureInputSurface(
    captureMode: CaptureMode,
    enabled: Boolean,
    onCaptured: (Int) -> Unit
) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f),
        shape = MaterialTheme.shapes.large
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(
                text = if (enabled) {
                    stringResource(R.string.settings_controls_capture_listening)
                } else {
                    stringResource(R.string.settings_controls_capture_waiting_for_controller)
                },
                style = MaterialTheme.typography.bodyLarge,
                color = MaterialTheme.colorScheme.onSurface
            )
            AndroidView(
                factory = { context ->
                    GamepadCaptureView(context).apply {
                        this.captureMode = captureMode
                        this.onCaptured = onCaptured
                    }
                },
                update = { view ->
                    view.captureMode = captureMode
                    view.onCaptured = onCaptured
                    if (enabled) {
                        view.post {
                            view.requestFocusFromTouch()
                            view.requestFocus()
                        }
                    }
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .height(56.dp)
            )
        }
    }
}

private class GamepadCaptureView(context: Context) : View(context) {
    var captureMode: CaptureMode = CaptureMode.Button
    var onCaptured: (Int) -> Unit = {}

    init {
        isFocusable = true
        isFocusableInTouchMode = true
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        post {
            requestFocusFromTouch()
            requestFocus()
        }
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (captureMode != CaptureMode.Button) {
            return super.dispatchKeyEvent(event)
        }
        if (event.action != KeyEvent.ACTION_DOWN || event.repeatCount != 0 || !isGamepadDevice(event.device)) {
            return super.dispatchKeyEvent(event)
        }

        val mapped = keyCodeToSdlButton(event.keyCode) ?: return super.dispatchKeyEvent(event)
        onCaptured(mapped)
        return true
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        if (captureMode != CaptureMode.Axis || !event.isFromSource(InputDevice.SOURCE_JOYSTICK)) {
            return super.onGenericMotionEvent(event)
        }

        val mapped = dominantAxisMapping(event) ?: return super.onGenericMotionEvent(event)
        onCaptured(mapped)
        return true
    }
}

private fun isGamepadDevice(device: InputDevice?): Boolean {
    device ?: return false
    val sources = device.sources
    return (sources and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
        (sources and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
}

private fun dominantAxisMapping(event: MotionEvent): Int? {
    val candidates = listOf(
        MotionEvent.AXIS_X to SDL_GAMEPAD_AXIS_LEFTX,
        MotionEvent.AXIS_Y to SDL_GAMEPAD_AXIS_LEFTY,
        MotionEvent.AXIS_Z to SDL_GAMEPAD_AXIS_RIGHTX,
        MotionEvent.AXIS_RZ to SDL_GAMEPAD_AXIS_RIGHTY,
        MotionEvent.AXIS_RX to SDL_GAMEPAD_AXIS_RIGHTX,
        MotionEvent.AXIS_RY to SDL_GAMEPAD_AXIS_RIGHTY,
        MotionEvent.AXIS_LTRIGGER to SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
        MotionEvent.AXIS_BRAKE to SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
        MotionEvent.AXIS_RTRIGGER to SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
        MotionEvent.AXIS_GAS to SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
    )

    var bestValue = 0f
    var bestAxis: Int? = null
    for ((androidAxis, sdlAxis) in candidates) {
        val value = abs(event.getAxisValue(androidAxis))
        if (value > 0.6f && value > bestValue) {
            bestValue = value
            bestAxis = sdlAxis
        }
    }
    return bestAxis
}

private fun keyCodeToSdlButton(keyCode: Int): Int? {
    return when (keyCode) {
        KeyEvent.KEYCODE_BUTTON_A -> SDL_GAMEPAD_BUTTON_SOUTH
        KeyEvent.KEYCODE_BUTTON_B -> SDL_GAMEPAD_BUTTON_EAST
        KeyEvent.KEYCODE_BUTTON_X -> SDL_GAMEPAD_BUTTON_WEST
        KeyEvent.KEYCODE_BUTTON_Y -> SDL_GAMEPAD_BUTTON_NORTH
        KeyEvent.KEYCODE_BUTTON_SELECT -> SDL_GAMEPAD_BUTTON_BACK
        KeyEvent.KEYCODE_BUTTON_MODE -> SDL_GAMEPAD_BUTTON_GUIDE
        KeyEvent.KEYCODE_BUTTON_START -> SDL_GAMEPAD_BUTTON_START
        KeyEvent.KEYCODE_BUTTON_THUMBL -> SDL_GAMEPAD_BUTTON_LEFT_STICK
        KeyEvent.KEYCODE_BUTTON_THUMBR -> SDL_GAMEPAD_BUTTON_RIGHT_STICK
        KeyEvent.KEYCODE_BUTTON_L1 -> SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
        KeyEvent.KEYCODE_BUTTON_R1 -> SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
        KeyEvent.KEYCODE_DPAD_UP -> SDL_GAMEPAD_BUTTON_DPAD_UP
        KeyEvent.KEYCODE_DPAD_DOWN -> SDL_GAMEPAD_BUTTON_DPAD_DOWN
        KeyEvent.KEYCODE_DPAD_LEFT -> SDL_GAMEPAD_BUTTON_DPAD_LEFT
        KeyEvent.KEYCODE_DPAD_RIGHT -> SDL_GAMEPAD_BUTTON_DPAD_RIGHT
        else -> null
    }
}

private fun currentButtonBinding(cfg: EmulatorConfig, index: Int): Int {
    return if (index in cfg.controllerBinds.indices) {
        cfg.controllerBinds[index]
    } else {
        index
    }
}

private fun currentAxisBinding(cfg: EmulatorConfig, index: Int): Int {
    return if (index in cfg.controllerAxisBinds.indices) {
        cfg.controllerAxisBinds[index]
    } else {
        index
    }
}

private fun buttonManualOptions(family: ControllerFamily): List<ManualBindingOption> = listOf(
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_DPAD_UP), SDL_GAMEPAD_BUTTON_DPAD_UP),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_DPAD_DOWN), SDL_GAMEPAD_BUTTON_DPAD_DOWN),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_DPAD_LEFT), SDL_GAMEPAD_BUTTON_DPAD_LEFT),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_DPAD_RIGHT), SDL_GAMEPAD_BUTTON_DPAD_RIGHT),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER), SDL_GAMEPAD_BUTTON_LEFT_SHOULDER),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER), SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_LEFT_STICK), SDL_GAMEPAD_BUTTON_LEFT_STICK),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_RIGHT_STICK), SDL_GAMEPAD_BUTTON_RIGHT_STICK),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_BACK), SDL_GAMEPAD_BUTTON_BACK),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_GUIDE), SDL_GAMEPAD_BUTTON_GUIDE),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_START), SDL_GAMEPAD_BUTTON_START),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_NORTH), SDL_GAMEPAD_BUTTON_NORTH),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_EAST), SDL_GAMEPAD_BUTTON_EAST),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_SOUTH), SDL_GAMEPAD_BUTTON_SOUTH),
    ManualBindingOption(buttonLabel(family, SDL_GAMEPAD_BUTTON_WEST), SDL_GAMEPAD_BUTTON_WEST)
)

private fun axisManualOptions(family: ControllerFamily): List<ManualBindingOption> = listOf(
    ManualBindingOption(axisLabel(family, SDL_GAMEPAD_AXIS_LEFTX), SDL_GAMEPAD_AXIS_LEFTX),
    ManualBindingOption(axisLabel(family, SDL_GAMEPAD_AXIS_LEFTY), SDL_GAMEPAD_AXIS_LEFTY),
    ManualBindingOption(axisLabel(family, SDL_GAMEPAD_AXIS_RIGHTX), SDL_GAMEPAD_AXIS_RIGHTX),
    ManualBindingOption(axisLabel(family, SDL_GAMEPAD_AXIS_RIGHTY), SDL_GAMEPAD_AXIS_RIGHTY),
    ManualBindingOption(axisLabel(family, SDL_GAMEPAD_AXIS_LEFT_TRIGGER), SDL_GAMEPAD_AXIS_LEFT_TRIGGER),
    ManualBindingOption(axisLabel(family, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER), SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
)

private fun buttonLabel(family: ControllerFamily, button: Int): String {
    return when (button) {
        SDL_GAMEPAD_BUTTON_DPAD_UP -> "D-Pad Up"
        SDL_GAMEPAD_BUTTON_DPAD_DOWN -> "D-Pad Down"
        SDL_GAMEPAD_BUTTON_DPAD_LEFT -> "D-Pad Left"
        SDL_GAMEPAD_BUTTON_DPAD_RIGHT -> "D-Pad Right"
        SDL_GAMEPAD_BUTTON_BACK -> when (family) {
            ControllerFamily.Xbox -> "Back"
            ControllerFamily.Nintendo -> "-"
            ControllerFamily.Standard -> "Select"
        }
        SDL_GAMEPAD_BUTTON_GUIDE -> when (family) {
            ControllerFamily.Xbox -> "Guide"
            ControllerFamily.Nintendo -> "Home"
            ControllerFamily.Standard -> "PS"
        }
        SDL_GAMEPAD_BUTTON_START -> when (family) {
            ControllerFamily.Nintendo -> "+"
            else -> "Start"
        }
        SDL_GAMEPAD_BUTTON_LEFT_STICK -> if (family == ControllerFamily.Standard) "L3" else "LS"
        SDL_GAMEPAD_BUTTON_RIGHT_STICK -> if (family == ControllerFamily.Standard) "R3" else "RS"
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER -> when (family) {
            ControllerFamily.Xbox -> "LB"
            ControllerFamily.Nintendo -> "L"
            ControllerFamily.Standard -> "L1"
        }
        SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER -> when (family) {
            ControllerFamily.Xbox -> "RB"
            ControllerFamily.Nintendo -> "R"
            ControllerFamily.Standard -> "R1"
        }
        SDL_GAMEPAD_BUTTON_SOUTH -> when (family) {
            ControllerFamily.Xbox -> "A"
            ControllerFamily.Nintendo -> "B"
            ControllerFamily.Standard -> "Cross"
        }
        SDL_GAMEPAD_BUTTON_EAST -> when (family) {
            ControllerFamily.Xbox -> "B"
            ControllerFamily.Nintendo -> "A"
            ControllerFamily.Standard -> "Circle"
        }
        SDL_GAMEPAD_BUTTON_WEST -> when (family) {
            ControllerFamily.Xbox -> "X"
            ControllerFamily.Nintendo -> "Y"
            ControllerFamily.Standard -> "Square"
        }
        SDL_GAMEPAD_BUTTON_NORTH -> when (family) {
            ControllerFamily.Xbox -> "Y"
            ControllerFamily.Nintendo -> "X"
            ControllerFamily.Standard -> "Triangle"
        }
        else -> "Button $button"
    }
}

private fun axisLabel(family: ControllerFamily, axis: Int): String {
    return when (axis) {
        SDL_GAMEPAD_AXIS_LEFTX -> "Left X"
        SDL_GAMEPAD_AXIS_LEFTY -> "Left Y"
        SDL_GAMEPAD_AXIS_RIGHTX -> "Right X"
        SDL_GAMEPAD_AXIS_RIGHTY -> "Right Y"
        SDL_GAMEPAD_AXIS_LEFT_TRIGGER -> when (family) {
            ControllerFamily.Xbox -> "LT"
            ControllerFamily.Nintendo -> "ZL"
            ControllerFamily.Standard -> "L2"
        }
        SDL_GAMEPAD_AXIS_RIGHT_TRIGGER -> when (family) {
            ControllerFamily.Xbox -> "RT"
            ControllerFamily.Nintendo -> "ZR"
            ControllerFamily.Standard -> "R2"
        }
        else -> "Axis $axis"
    }
}
