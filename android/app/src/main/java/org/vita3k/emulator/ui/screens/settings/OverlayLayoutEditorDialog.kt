package org.vita3k.emulator.ui.screens.settings

import android.app.Activity
import android.content.Context
import android.content.ContextWrapper
import android.content.pm.ActivityInfo
import android.os.Build
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.vita3k.emulator.overlay.InputOverlay
import org.vita3k.emulator.overlay.OverlayConfig
import org.vita3k.emulator.overlay.OverlayLayout
import org.vita3k.emulator.overlay.OverlayStore
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

    OverlayEditorOrientationEffect(activity)
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

@Composable
private fun OverlayEditorOrientationEffect(activity: Activity?) {
    DisposableEffect(activity) {
        if (activity == null) {
            return@DisposableEffect onDispose { }
        }

        val previousOrientation = activity.requestedOrientation
        val window = activity.window
        val previousCutoutMode = window.attributes.layoutInDisplayCutoutMode
        val decorView = window.decorView

        activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        WindowCompat.setDecorFitsSystemWindows(window, false)
        ViewCompat.getWindowInsetsController(decorView)?.apply {
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(WindowInsetsCompat.Type.systemBars())
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P &&
            previousCutoutMode != android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        ) {
            val attributes = window.attributes
            attributes.layoutInDisplayCutoutMode =
                android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            window.attributes = attributes
        }
        decorView.post {
            ViewCompat.getWindowInsetsController(decorView)?.hide(WindowInsetsCompat.Type.systemBars())
        }

        onDispose {
            ViewCompat.getWindowInsetsController(decorView)?.show(WindowInsetsCompat.Type.systemBars())
            WindowCompat.setDecorFitsSystemWindows(window, true)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                val attributes = window.attributes
                attributes.layoutInDisplayCutoutMode = previousCutoutMode
                window.attributes = attributes
            }
            activity.requestedOrientation = previousOrientation
        }
    }
}

private tailrec fun Context.findActivity(): Activity? = when (this) {
    is Activity -> this
    is ContextWrapper -> baseContext.findActivity()
    else -> null
}
