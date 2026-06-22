package org.vita3k.emulator.ui.screens.settings

import android.app.Activity
import android.content.Context
import android.content.ContextWrapper
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.viewinterop.AndroidView
import org.vita3k.emulator.overlay.InputOverlay
import org.vita3k.emulator.overlay.OverlayConfig
import org.vita3k.emulator.overlay.OverlayLayout
import org.vita3k.emulator.overlay.OverlayStore
import org.vita3k.emulator.ui.common.ImmersiveLandscapeEffect
import org.vita3k.emulator.ui.components.OverlayEditorPalette

@Composable
internal fun OverlayLayoutEditorDialog(
    overlayConfig: OverlayConfig,
    overlayLayout: OverlayLayout,
    onDismiss: (OverlayLayout) -> Unit
) {
    val context = LocalContext.current
    val activity = context.findActivity()
    var overlayView by remember { mutableStateOf<InputOverlay?>(null) }

    fun finishEditor() {
        onDismiss(overlayView?.captureLayout() ?: overlayLayout)
    }

    ImmersiveLandscapeEffect(activity = activity)
    BackHandler(onBack = ::finishEditor)

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black)
    ) {
        AndroidView(
            factory = { viewContext ->
                InputOverlay(viewContext).apply {
                    overlayView = this
                    setLayout(overlayLayout)
                    setAllowVirtualController(false)
                    setAutoHideEnabled(false)
                    setIsInEditMode(true)
                    setScale(overlayConfig.overlayScale / 100f)
                    setOpacity(overlayConfig.overlayOpacity)
                    setState(overlayConfig.activeMask())
                }
            },
            update = { view ->
                overlayView = view
                view.setLayout(overlayLayout)
                view.setAllowVirtualController(false)
                view.setAutoHideEnabled(false)
                view.setIsInEditMode(true)
                view.setScale(overlayConfig.overlayScale / 100f)
                view.setOpacity(overlayConfig.overlayOpacity)
                view.setState(overlayConfig.activeMask())
            },
            modifier = Modifier.fillMaxSize()
        )

        OverlayEditorPalette(
            onDone = ::finishEditor,
            onReset = {
                overlayView?.setLayout(OverlayStore.defaultLayout(context))
            }
        )
    }
}

private tailrec fun Context.findActivity(): Activity? = when (this) {
    is Activity -> this
    is ContextWrapper -> baseContext.findActivity()
    else -> null
}
