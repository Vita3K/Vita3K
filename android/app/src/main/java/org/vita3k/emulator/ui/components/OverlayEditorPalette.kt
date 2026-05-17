package org.vita3k.emulator.ui.components

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
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
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import org.vita3k.emulator.R
import kotlin.math.roundToInt

@Composable
internal fun OverlayEditorPalette(
    onDone: () -> Unit,
    onReset: () -> Unit,
    modifier: Modifier = Modifier
) {
    BoxWithConstraints(
        modifier = modifier.fillMaxSize()
    ) {
        var offsetX by rememberSaveable { mutableIntStateOf(0) }
        var offsetY by rememberSaveable { mutableIntStateOf(0) }
        var panelSize by remember { mutableStateOf(IntSize.Zero) }

        val maxOffsetX = ((constraints.maxWidth - panelSize.width) / 2).coerceAtLeast(0)
        val maxOffsetY = ((constraints.maxHeight - panelSize.height) / 2).coerceAtLeast(0)

        Surface(
            modifier = Modifier
                .align(Alignment.Center)
                .offset { IntOffset(offsetX, offsetY) }
                .widthIn(min = 220.dp, max = 280.dp)
                .onSizeChanged { panelSize = it }
                .pointerInput(maxOffsetX, maxOffsetY) {
                    detectDragGestures { change, dragAmount ->
                        change.consume()
                        offsetX = (offsetX + dragAmount.x.roundToInt())
                            .coerceIn(-maxOffsetX, maxOffsetX)
                        offsetY = (offsetY + dragAmount.y.roundToInt())
                            .coerceIn(-maxOffsetY, maxOffsetY)
                    }
                },
            shape = RoundedCornerShape(24.dp),
            color = MaterialTheme.colorScheme.surface.copy(alpha = 0.95f),
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurface.copy(alpha = 0.08f))
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(12.dp),
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                OutlinedButton(
                    onClick = onReset,
                    modifier = Modifier.weight(1f)
                ) {
                    Icon(Icons.Default.Restore, contentDescription = null)
                    Text(
                        text = stringResource(R.string.action_reset),
                        modifier = Modifier.padding(start = 6.dp)
                    )
                }
                FilledTonalButton(
                    onClick = onDone,
                    modifier = Modifier.weight(1f)
                ) {
                    Icon(Icons.Default.Done, contentDescription = null)
                    Text(
                        text = stringResource(R.string.action_done),
                        modifier = Modifier.padding(start = 6.dp)
                    )
                }
            }
        }
    }
}
