package org.vita3k.emulator.ui.screens

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.unit.dp
import org.vita3k.emulator.R
import org.vita3k.emulator.data.AppInfo
import org.vita3k.emulator.ui.theme.SCRIM_ALPHA
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AppInfoSheet(
    app: AppInfo,
    installSizeBytes: Long,
    onDismiss: () -> Unit
) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        scrimColor = Color.Black.copy(alpha = SCRIM_ALPHA)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(stringResource(R.string.app_info_title), style = MaterialTheme.typography.titleLarge)
            IconButton(onClick = onDismiss) {
                Icon(Icons.Default.Close, contentDescription = stringResource(R.string.app_info_cd_close))
            }
        }

        Row(
            modifier = Modifier.padding(horizontal = 24.dp, vertical = 16.dp),
            horizontalArrangement = Arrangement.spacedBy(16.dp),
            verticalAlignment = Alignment.Top
        ) {
            AppIcon(app = app, size = 80)
            Column {
                Text(app.title, style = MaterialTheme.typography.titleMedium)
                Text(
                    app.titleId,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Spacer(modifier = Modifier.height(8.dp))
                CompatBadge(app)
            }
        }

        HorizontalDivider(modifier = Modifier.padding(horizontal = 24.dp))

        InfoRow(label = stringResource(R.string.app_info_field_title), value = app.title)
        InfoRow(label = stringResource(R.string.app_info_field_serial), value = app.titleId)
        InfoRow(label = stringResource(R.string.app_info_field_app_version), value = app.appVer.ifEmpty { "\u2014" })
        InfoRow(label = stringResource(R.string.app_info_field_category), value = app.category.ifEmpty { "\u2014" })
        InfoRow(
            label = stringResource(R.string.app_info_field_install_size),
            value = when {
                installSizeBytes == -1L -> stringResource(R.string.app_info_size_calculating)
                installSizeBytes == 0L -> stringResource(R.string.app_info_size_unknown)
                installSizeBytes < 1024L * 1024L ->
                    stringResource(R.string.app_info_size_kb, (installSizeBytes / 1024L).toInt())
                installSizeBytes < 1024L * 1024L * 1024L ->
                    stringResource(R.string.app_info_size_mb, (installSizeBytes / (1024.0 * 1024.0)).toFloat())
                else ->
                    stringResource(R.string.app_info_size_gb, (installSizeBytes / (1024.0 * 1024.0 * 1024.0)).toFloat())
            }
        )
        InfoRow(label = stringResource(R.string.app_info_field_play_time), value = formatPlaytime(app.playtime))
        InfoRow(label = stringResource(R.string.app_info_field_last_played), value = formatLastPlayed(app.lastPlayed))

        Spacer(modifier = Modifier.height(32.dp))
    }
}

@Composable
private fun formatPlaytime(seconds: Long): String {
    if (seconds <= 0) return stringResource(R.string.app_info_playtime_never)
    val h = seconds / 3600
    val m = (seconds % 3600) / 60
    val s = seconds % 60
    return when {
        h > 0 -> stringResource(R.string.app_info_playtime_hm, h.toInt(), m.toInt())
        m > 0 -> stringResource(R.string.app_info_playtime_ms, m.toInt(), s.toInt())
        else -> stringResource(R.string.app_info_playtime_s, s.toInt())
    }
}

@Composable
private fun formatLastPlayed(timestampSecs: Long): String {
    if (timestampSecs <= 0) return stringResource(R.string.app_info_last_played_never)
    val formatter = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm")
        .withZone(ZoneId.systemDefault())
    return formatter.format(Instant.ofEpochSecond(timestampSecs))
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun InfoRow(label: String, value: String) {
    val clipboardManager = LocalClipboardManager.current
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .combinedClickable(
                onClick = {},
                onLongClick = { clipboardManager.setText(AnnotatedString(value)) }
            )
            .padding(horizontal = 24.dp, vertical = 10.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            label,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        Text(value, style = MaterialTheme.typography.bodyMedium)
    }
}
