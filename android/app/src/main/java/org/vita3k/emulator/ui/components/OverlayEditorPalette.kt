package org.vita3k.emulator.ui.components

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Done
import androidx.compose.material.icons.filled.Restore
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import org.vita3k.emulator.R
import org.vita3k.emulator.overlay.InputOverlay
import org.vita3k.emulator.overlay.OverlayCustomization
import kotlin.math.roundToInt

private val editableControls = listOf(
    InputOverlay.ButtonType.BUTTON_CROSS to "Cross", InputOverlay.ButtonType.BUTTON_CIRCLE to "Circle",
    InputOverlay.ButtonType.BUTTON_SQUARE to "Square", InputOverlay.ButtonType.BUTTON_TRIANGLE to "Triangle",
    InputOverlay.ButtonType.BUTTON_START to "Start", InputOverlay.ButtonType.BUTTON_SELECT to "Select",
    InputOverlay.ButtonType.BUTTON_PS to "PS", InputOverlay.ButtonType.DPAD_UP to "D-pad",
    InputOverlay.ButtonType.STICK_LEFT to "Left stick", InputOverlay.ButtonType.STICK_RIGHT to "Right stick",
    InputOverlay.ButtonType.TRIGGER_L to "L", InputOverlay.ButtonType.TRIGGER_R to "R",
    InputOverlay.ButtonType.TRIGGER_L2 to "L2", InputOverlay.ButtonType.TRIGGER_R2 to "R2",
    InputOverlay.ButtonType.TRIGGER_L3 to "L3", InputOverlay.ButtonType.TRIGGER_R3 to "R3",
    InputOverlay.ButtonType.BUTTON_TOUCH_SWITCH to "Touch switch", InputOverlay.ButtonType.BUTTON_TOUCH_HIDE to "Hide toggle"
)

@Composable
internal fun OverlayEditorPalette(overlayView: InputOverlay?, onDone: () -> Unit, onReset: () -> Unit, modifier: Modifier = Modifier) {
    BoxWithConstraints(modifier = modifier.fillMaxSize()) {
        var offsetX by rememberSaveable { mutableIntStateOf(0) }
        var offsetY by rememberSaveable { mutableIntStateOf(0) }
        val initialSelectedIndex = editableControls.indexOfFirst {
            it.first == overlayView?.selectedControlId
        }.takeIf { it >= 0 } ?: 0
        var selectedIndex by remember(overlayView) { mutableIntStateOf(initialSelectedIndex) }
        var panelSize by remember { mutableStateOf(IntSize.Zero) }
        var revision by remember { mutableIntStateOf(0) }
        DisposableEffect(overlayView) {
            val listener = InputOverlay.ControlSelectionListener { controlId ->
                val index = editableControls.indexOfFirst { it.first == controlId }
                if (index >= 0) selectedIndex = index
            }
            overlayView?.setControlSelectionListener(listener)
            onDispose { overlayView?.setControlSelectionListener(null) }
        }
        val maxOffsetX = ((constraints.maxWidth - panelSize.width) / 2).coerceAtLeast(0)
        val maxOffsetY = ((constraints.maxHeight - panelSize.height) / 2).coerceAtLeast(0)
        val selected = editableControls[selectedIndex.coerceIn(editableControls.indices)]
        val customization = overlayView?.getControlCustomization(selected.first) ?: OverlayCustomization()
        val selectedIsThumbStick = selected.first == InputOverlay.ButtonType.STICK_LEFT ||
            selected.first == InputOverlay.ButtonType.STICK_RIGHT
        revision
        fun selectControl(index: Int) {
            selectedIndex = index
            overlayView?.selectControlForEditing(editableControls[index].first)
        }
        fun update(
            visible: Boolean = customization.visible,
            scale: Int = customization.scalePercent,
            opacity: Int = customization.opacityPercent,
            sensitivity: Int = customization.sensitivityPercent
        ) {
            overlayView?.setControlCustomization(selected.first, visible, scale, opacity, sensitivity)
            revision++
        }
        Surface(
            modifier = Modifier.align(Alignment.Center).offset { IntOffset(offsetX, offsetY) }
                .widthIn(min = 250.dp, max = 360.dp).onSizeChanged { panelSize = it }
                .pointerInput(maxOffsetX, maxOffsetY) {
                    detectDragGestures { change, dragAmount ->
                        change.consume()
                        offsetX = (offsetX + dragAmount.x.roundToInt()).coerceIn(-maxOffsetX, maxOffsetX)
                        offsetY = (offsetY + dragAmount.y.roundToInt()).coerceIn(-maxOffsetY, maxOffsetY)
                    }
                },
            shape = RoundedCornerShape(24.dp),
            color = MaterialTheme.colorScheme.surface.copy(alpha = 0.95f),
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurface.copy(alpha = 0.08f))
        ) {
            Column(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
                    TextButton(onClick = { selectControl((selectedIndex + editableControls.size - 1) % editableControls.size) }) { Text("‹") }
                    Text(
                        "${selected.second} (${selectedIndex + 1}/${editableControls.size})",
                        color = Color.White,
                        style = MaterialTheme.typography.titleSmall,
                        fontWeight = FontWeight.SemiBold
                    )
                    TextButton(onClick = { selectControl((selectedIndex + 1) % editableControls.size) }) { Text("›") }
                }
                Text(
                    "Tap a control on the overlay to select it",
                    color = Color(0xFFD1D5DB),
                    style = MaterialTheme.typography.labelMedium
                )
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text("Visibility", modifier = Modifier.weight(1f), color = Color.White, fontWeight = FontWeight.Medium)
                    OutlinedButton(onClick = { update(visible = !customization.visible) }) {
                        Text(if (customization.visible) "Visible" else "Hidden", color = Color(0xFFFFC857), fontWeight = FontWeight.SemiBold)
                    }
                }
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(4.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text("Size", modifier = Modifier.weight(1f), color = Color.White, fontWeight = FontWeight.Medium)
                    TextButton(onClick = { update(scale = (customization.scalePercent - 10).coerceAtLeast(35)) }) { Text("−", color = Color(0xFFFFC857)) }
                    Text(
                        "${customization.scalePercent}%",
                        modifier = Modifier.widthIn(min = 54.dp),
                        color = Color.White,
                        fontWeight = FontWeight.Bold,
                        textAlign = TextAlign.Center
                    )
                    TextButton(onClick = { update(scale = (customization.scalePercent + 10).coerceAtMost(250)) }) { Text("+", color = Color(0xFFFFC857)) }
                }
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(4.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text("Opacity", modifier = Modifier.weight(1f), color = Color.White, fontWeight = FontWeight.Medium)
                    TextButton(onClick = { update(opacity = (customization.opacityPercent - 10).coerceAtLeast(10)) }) { Text("−", color = Color(0xFFFFC857)) }
                    Text(
                        "${customization.opacityPercent}%",
                        modifier = Modifier.widthIn(min = 54.dp),
                        color = Color.White,
                        fontWeight = FontWeight.Bold,
                        textAlign = TextAlign.Center
                    )
                    TextButton(onClick = { update(opacity = (customization.opacityPercent + 10).coerceAtMost(100)) }) { Text("+", color = Color(0xFFFFC857)) }
                }
                if (selectedIsThumbStick) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(4.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("Sensitivity", modifier = Modifier.weight(1f), color = Color.White, fontWeight = FontWeight.Medium)
                        TextButton(onClick = { update(sensitivity = (customization.sensitivityPercent - 10).coerceAtLeast(10)) }) {
                            Text("−", color = Color(0xFFFFC857))
                        }
                        Text(
                            "${customization.sensitivityPercent}%",
                            modifier = Modifier.widthIn(min = 54.dp),
                            color = Color.White,
                            fontWeight = FontWeight.Bold,
                            textAlign = TextAlign.Center
                        )
                        TextButton(onClick = { update(sensitivity = (customization.sensitivityPercent + 10).coerceAtMost(200)) }) {
                            Text("+", color = Color(0xFFFFC857))
                        }
                    }
                }
                Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    OutlinedButton(onClick = { onReset(); revision++ }, modifier = Modifier.weight(1f)) {
                        Icon(Icons.Default.Restore, contentDescription = null)
                        Text(text = stringResource(R.string.action_reset), modifier = Modifier.padding(start = 6.dp))
                    }
                    FilledTonalButton(onClick = onDone, modifier = Modifier.weight(1f)) {
                        Icon(Icons.Default.Done, contentDescription = null)
                        Text(text = stringResource(R.string.action_done), modifier = Modifier.padding(start = 6.dp))
                    }
                }
            }
        }
    }
}
