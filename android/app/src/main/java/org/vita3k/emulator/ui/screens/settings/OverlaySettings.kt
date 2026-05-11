package org.vita3k.emulator.ui.screens.settings

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Restore
import androidx.compose.material.icons.filled.TouchApp
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.vita3k.emulator.ConnectedGamepad
import org.vita3k.emulator.R
import org.vita3k.emulator.data.EmulatorConfig
import org.vita3k.emulator.overlay.OverlayConfig

@Composable
internal fun OverlaySettingsSection(
    cfg: EmulatorConfig,
    overlayConfig: OverlayConfig,
    isPerApp: Boolean,
    connectedGamepads: List<ConnectedGamepad>,
    onUpdate: (EmulatorConfig.() -> Unit) -> Unit,
    onOverlayConfigChange: (OverlayConfig) -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit,
    controllerConnected: Boolean = false,
    onStartControlsEditor: (() -> Unit)? = null,
    onResetControlsLayout: (() -> Unit)? = null
) {
    Column(verticalArrangement = Arrangement.spacedBy(16.dp)) {
        SettingsSectionCard(
            title = stringResource(R.string.settings_tab_controls),
            summary = null,
            help = null,
            onShowHelp = onShowHelp
        ) {
            OverlaySettingsRows(
                overlayConfig = overlayConfig,
                onOverlayConfigChange = onOverlayConfigChange,
                onShowHelp = onShowHelp,
                controllerConnected = controllerConnected,
                onStartControlsEditor = onStartControlsEditor,
                onResetControlsLayout = onResetControlsLayout
            )
        }

        if (!isPerApp) {
            ControllerMappingSections(
                cfg = cfg,
                connectedGamepads = connectedGamepads,
                onUpdate = onUpdate,
                onShowHelp = onShowHelp
            )
        }
    }
}

@Composable
internal fun OverlaySettingsRows(
    overlayConfig: OverlayConfig,
    onOverlayConfigChange: (OverlayConfig) -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit,
    controllerConnected: Boolean = false,
    onStartControlsEditor: (() -> Unit)? = null,
    onResetControlsLayout: (() -> Unit)? = null
) {
    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        if (onStartControlsEditor != null || onResetControlsLayout != null) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                if (onStartControlsEditor != null) {
                    FilledTonalButton(
                        onClick = onStartControlsEditor,
                        enabled = overlayConfig.controlsVisible,
                        modifier = Modifier.weight(1f)
                    ) {
                        Icon(Icons.Default.TouchApp, contentDescription = null)
                        Spacer(modifier = Modifier.padding(horizontal = 4.dp))
                        androidx.compose.material3.Text(stringResource(R.string.emulation_controls_edit))
                    }
                }
                if (onResetControlsLayout != null) {
                    OutlinedButton(
                        onClick = onResetControlsLayout,
                        enabled = overlayConfig.controlsVisible,
                        modifier = Modifier.weight(1f)
                    ) {
                        Icon(Icons.Default.Restore, contentDescription = null)
                        Spacer(modifier = Modifier.padding(horizontal = 4.dp))
                        androidx.compose.material3.Text(stringResource(R.string.action_reset))
                    }
                }
            }
        }

        SettingsToggleRow(
            title = stringResource(R.string.settings_controls_show),
            checked = overlayConfig.controlsVisible,
            onCheckedChange = { onOverlayConfigChange(overlayConfig.withControlsVisible(it)) },
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_controls_show),
                body = stringResource(R.string.settings_controls_show_desc),
                scope = SettingsScope.Global
            ),
            onShowHelp = onShowHelp
        )
        SettingsToggleRow(
            title = stringResource(R.string.settings_controls_hide_on_controller),
            checked = overlayConfig.hideOverlayWhenControllerConnected,
            onCheckedChange = {
                onOverlayConfigChange(overlayConfig.copy(hideOverlayWhenControllerConnected = it))
            },
            summary = if (controllerConnected) {
                stringResource(R.string.settings_controls_controller_connected)
            } else {
                stringResource(R.string.settings_controls_controller_disconnected)
            },
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_controls_hide_on_controller),
                body = stringResource(R.string.settings_controls_hide_on_controller_desc),
                scope = SettingsScope.Global
            ),
            onShowHelp = onShowHelp
        )
        SettingsToggleRow(
            title = stringResource(R.string.settings_controls_l2r2),
            checked = overlayConfig.l2r2Visible,
            onCheckedChange = { onOverlayConfigChange(overlayConfig.withL2R2Visible(it)) },
            enabled = overlayConfig.controlsVisible,
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_controls_l2r2),
                body = stringResource(R.string.settings_controls_l2r2_desc),
                scope = SettingsScope.Global
            ),
            onShowHelp = onShowHelp
        )
        SettingsToggleRow(
            title = stringResource(R.string.settings_controls_l3r3),
            checked = overlayConfig.l3r3Visible,
            onCheckedChange = { onOverlayConfigChange(overlayConfig.withL3R3Visible(it)) },
            enabled = overlayConfig.controlsVisible,
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_controls_l3r3),
                body = stringResource(R.string.settings_controls_l3r3_desc),
                scope = SettingsScope.Global
            ),
            onShowHelp = onShowHelp
        )
        SettingsToggleRow(
            title = stringResource(R.string.settings_controls_touch_switch),
            checked = overlayConfig.touchSwitchVisible,
            onCheckedChange = { onOverlayConfigChange(overlayConfig.withTouchSwitchVisible(it)) },
            enabled = overlayConfig.controlsVisible,
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_controls_touch_switch),
                body = stringResource(R.string.settings_controls_touch_switch_desc),
                scope = SettingsScope.Global
            ),
            onShowHelp = onShowHelp
        )
        SettingsToggleRow(
            title = stringResource(R.string.settings_controls_hide_toggle),
            checked = overlayConfig.hideToggleVisible,
            onCheckedChange = { onOverlayConfigChange(overlayConfig.withHideToggleVisible(it)) },
            enabled = overlayConfig.controlsVisible,
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_controls_hide_toggle),
                body = stringResource(R.string.settings_controls_hide_toggle_desc),
                scope = SettingsScope.Global
            ),
            onShowHelp = onShowHelp
        )
        SettingsSliderRow(
            title = stringResource(R.string.settings_controls_scale),
            valueLabel = stringResource(R.string.settings_controls_percent_value, overlayConfig.overlayScale),
            value = overlayConfig.overlayScale.toFloat(),
            onValueChange = {
                onOverlayConfigChange(overlayConfig.copy(overlayScale = it.toInt()))
            },
            valueRange = 50f..150f,
            steps = 99,
            enabled = overlayConfig.controlsVisible,
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_controls_scale),
                body = stringResource(R.string.settings_controls_scale_desc),
                scope = SettingsScope.Global
            ),
            onShowHelp = onShowHelp
        )
        SettingsSliderRow(
            title = stringResource(R.string.settings_controls_opacity),
            valueLabel = stringResource(R.string.settings_controls_percent_value, overlayConfig.overlayOpacity),
            value = overlayConfig.overlayOpacity.toFloat(),
            onValueChange = {
                onOverlayConfigChange(overlayConfig.copy(overlayOpacity = it.toInt()))
            },
            valueRange = 20f..100f,
            steps = 79,
            enabled = overlayConfig.controlsVisible,
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_controls_opacity),
                body = stringResource(R.string.settings_controls_opacity_desc),
                scope = SettingsScope.Global
            ),
            onShowHelp = onShowHelp
        )
    }
}
