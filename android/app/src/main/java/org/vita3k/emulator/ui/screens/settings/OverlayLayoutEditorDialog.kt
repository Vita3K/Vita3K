package org.vita3k.emulator.ui.screens.settings

import android.app.Activity
import android.content.Context
import android.content.ContextWrapper
import android.content.pm.ActivityInfo
import android.os.Build
import android.view.ViewTreeObserver
import android.view.WindowManager
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.vita3k.emulator.overlay.InputOverlay
import org.vita3k.emulator.overlay.OverlayConfig
import org.vita3k.emulator.ui.components.OverlayEditorPalette

@Composable
internal fun OverlayLayoutEditorDialog(
    overlayConfig: OverlayConfig,
    onDismiss: () -> Unit,
    layoutProfileId: String = ""
) {
    BackHandler(onBack = onDismiss)

    val activity = LocalContext.current.findActivity()
    var overlayView by remember { mutableStateOf<InputOverlay?>(null) }
    OverlayEditorWindowEffect(activity = activity)

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black)
    ) {
        AndroidView(
            factory = { context ->
                InputOverlay(context).apply {
                    overlayView = this
                    setLayoutProfileId(layoutProfileId)
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
                view.setLayoutProfileId(layoutProfileId)
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
            onDone = onDismiss,
            onReset = { overlayView?.resetButtonPlacement() }
        )
    }
}

@Composable
private fun OverlayEditorWindowEffect(activity: Activity?) {
    DisposableEffect(activity) {
        if (activity == null) {
            return@DisposableEffect onDispose { }
        }

        val window = activity.window
        val decorView = window.decorView
        val previousOrientation = activity.requestedOrientation
        val previousAttributes = WindowManager.LayoutParams().also { it.copyFrom(window.attributes) }
        val systemBars = WindowInsetsCompat.Type.systemBars()
        val barsWereVisible = ViewCompat.getRootWindowInsets(decorView)?.isVisible(systemBars) ?: true
        val controller = WindowCompat.getInsetsController(window, decorView)
        val focusListener = ViewTreeObserver.OnWindowFocusChangeListener { hasFocus ->
            if (hasFocus) {
                WindowCompat.getInsetsController(window, decorView)?.hide(systemBars)
            }
        }

        activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        WindowCompat.setDecorFitsSystemWindows(window, false)
        controller?.setSystemBarsBehavior(
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        )
        controller?.hide(systemBars)
        ViewCompat.setOnApplyWindowInsetsListener(decorView) { view, insets ->
            if (insets.isVisible(systemBars)) {
                WindowCompat.getInsetsController(window, view)?.hide(systemBars)
            }
            insets
        }
        if (decorView.viewTreeObserver.isAlive) {
            decorView.viewTreeObserver.addOnWindowFocusChangeListener(focusListener)
        }
        decorView.post {
            WindowCompat.getInsetsController(window, decorView)?.hide(systemBars)
            ViewCompat.requestApplyInsets(decorView)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            val updatedAttributes = window.attributes
            updatedAttributes.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            window.attributes = updatedAttributes
        }

        onDispose {
            ViewCompat.setOnApplyWindowInsetsListener(decorView, null)
            if (decorView.viewTreeObserver.isAlive) {
                decorView.viewTreeObserver.removeOnWindowFocusChangeListener(focusListener)
            }
            WindowCompat.setDecorFitsSystemWindows(window, true)
            if (barsWereVisible) {
                controller?.show(systemBars)
            } else {
                controller?.hide(systemBars)
            }
            window.attributes = previousAttributes
            activity.requestedOrientation = previousOrientation
        }
    }
}

private tailrec fun Context.findActivity(): Activity? = when (this) {
    is Activity -> this
    is ContextWrapper -> baseContext.findActivity()
    else -> null
}
