package org.vita3k.emulator.ui.components

import android.text.method.LinkMovementMethod
import android.view.Gravity
import android.view.View
import android.widget.TextView
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.text.HtmlCompat

@Composable
fun HtmlText(
    html: String,
    modifier: Modifier = Modifier,
    textStyle: TextStyle = MaterialTheme.typography.bodyMedium,
    textColor: Color = MaterialTheme.colorScheme.onSurface,
    linkColor: Color = MaterialTheme.colorScheme.primary,
    gravity: Int = Gravity.START
) {
    AndroidView(
        modifier = modifier,
        factory = { context ->
            TextView(context).apply {
                movementMethod = LinkMovementMethod.getInstance()
                highlightColor = android.graphics.Color.TRANSPARENT
                setBackgroundColor(android.graphics.Color.TRANSPARENT)
                linksClickable = true
            }
        },
        update = { view ->
            view.text = HtmlCompat.fromHtml(html, HtmlCompat.FROM_HTML_MODE_COMPACT)
            view.setTextColor(textColor.toArgb())
            view.setLinkTextColor(linkColor.toArgb())
            view.textSize = textStyle.fontSize.value
            view.gravity = gravity
            view.textAlignment = if (gravity == Gravity.CENTER) {
                View.TEXT_ALIGNMENT_CENTER
            } else {
                View.TEXT_ALIGNMENT_VIEW_START
            }
        }
    )
}
