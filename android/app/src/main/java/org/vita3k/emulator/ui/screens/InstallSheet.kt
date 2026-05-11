package org.vita3k.emulator.ui.screens

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.FolderZip
import androidx.compose.material.icons.filled.Inventory2
import androidx.compose.material.icons.filled.Key
import androidx.compose.material.icons.filled.SystemUpdate
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.vita3k.emulator.R
import org.vita3k.emulator.ui.viewmodel.DeleteSourceOption
import org.vita3k.emulator.ui.viewmodel.InstallResult
import org.vita3k.emulator.ui.viewmodel.InstallResultStatus
import org.vita3k.emulator.ui.viewmodel.InstallType

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun InstallBottomSheet(
    onDismiss: () -> Unit,
    onSelectType: (InstallType) -> Unit
) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    val itemColors = ListItemDefaults.colors(
        containerColor = MaterialTheme.colorScheme.surface,
        headlineColor = MaterialTheme.colorScheme.onSurface,
        supportingColor = MaterialTheme.colorScheme.onSurfaceVariant,
        leadingIconColor = MaterialTheme.colorScheme.onSurfaceVariant
    )
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        containerColor = MaterialTheme.colorScheme.surface,
        contentColor = MaterialTheme.colorScheme.onSurface
    ) {
        Text(
            stringResource(R.string.install_sheet_title),
            style = MaterialTheme.typography.titleLarge,
            modifier = Modifier.padding(horizontal = 24.dp, vertical = 8.dp)
        )
        ListItem(
            headlineContent = { Text(stringResource(R.string.install_firmware_title)) },
            supportingContent = { Text(stringResource(R.string.install_firmware_desc)) },
            leadingContent = { Icon(Icons.Default.SystemUpdate, contentDescription = null) },
            colors = itemColors,
            modifier = Modifier.clickable { onSelectType(InstallType.FIRMWARE) }
        )
        ListItem(
            headlineContent = { Text(stringResource(R.string.install_pkg_title)) },
            supportingContent = { Text(stringResource(R.string.install_pkg_desc)) },
            leadingContent = { Icon(Icons.Default.Inventory2, contentDescription = null) },
            colors = itemColors,
            modifier = Modifier.clickable { onSelectType(InstallType.PKG) }
        )
        ListItem(
            headlineContent = { Text(stringResource(R.string.install_archive_title)) },
            supportingContent = { Text(stringResource(R.string.install_archive_desc)) },
            leadingContent = { Icon(Icons.Default.FolderZip, contentDescription = null) },
            colors = itemColors,
            modifier = Modifier.clickable { onSelectType(InstallType.ARCHIVE) }
        )
        ListItem(
            headlineContent = { Text(stringResource(R.string.install_license_title)) },
            supportingContent = { Text(stringResource(R.string.install_license_desc)) },
            leadingContent = { Icon(Icons.Default.Key, contentDescription = null) },
            colors = itemColors,
            modifier = Modifier.clickable { onSelectType(InstallType.LICENSE) }
        )
        Spacer(modifier = Modifier.height(24.dp))
    }
}

@Composable
fun InstallProgressDialog(
    progress: Int,
    statusMessage: String
) {
    AlertDialog(
        onDismissRequest = {},
        title = { Text(stringResource(R.string.install_progress_title)) },
        text = {
            Column {
                Text(statusMessage)
                Spacer(modifier = Modifier.height(16.dp))
                if (progress > 0) {
                    LinearProgressIndicator(
                        progress = { progress / 100f },
                        modifier = Modifier.fillMaxWidth(),
                        drawStopIndicator = {}
                    )
                    Text(
                        "$progress%",
                        style = MaterialTheme.typography.bodySmall,
                        modifier = Modifier.padding(top = 4.dp)
                    )
                } else {
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                }
            }
        },
        confirmButton = {}
    )
}

@Composable
fun InstallResultDialog(
    result: InstallResult,
    onConfirm: (List<DeleteSourceOption>) -> Unit
) {
    var selectedDeleteOptions by remember(result) {
        mutableStateOf(emptySet<Int>())
    }

    AlertDialog(
        onDismissRequest = { onConfirm(emptyList()) },
        title = {
            Text(
                when (result.status) {
                    InstallResultStatus.SUCCESS -> stringResource(R.string.install_result_success_title)
                    InstallResultStatus.PARTIAL -> stringResource(R.string.install_result_partial_title)
                    InstallResultStatus.ERROR -> stringResource(R.string.install_result_failed_title)
                }
            )
        },
        text = {
            Column {
                Text(result.message)
                if (result.deleteOptions.isNotEmpty()) {
                    Spacer(modifier = Modifier.height(16.dp))
                    result.deleteOptions.forEachIndexed { index, option ->
                        Row(
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Checkbox(
                                checked = index in selectedDeleteOptions,
                                onCheckedChange = { checked ->
                                    selectedDeleteOptions = if (checked) {
                                        selectedDeleteOptions + index
                                    } else {
                                        selectedDeleteOptions - index
                                    }
                                }
                            )
                            Text(
                                text = option.label,
                                modifier = Modifier.padding(top = 12.dp)
                            )
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    onConfirm(
                        selectedDeleteOptions
                            .sorted()
                            .map(result.deleteOptions::get)
                    )
                }
            ) {
                Text(stringResource(R.string.action_ok))
            }
        }
    )
}

@Composable
fun LicenseSourceDialog(
    title: String,
    message: String,
    onSelectLicenseFile: () -> Unit,
    onEnterZrif: (String) -> Unit,
    onDismiss: () -> Unit
) {
    var showManualEntry by remember { mutableStateOf(false) }
    var zrif by remember { mutableStateOf("") }

    if (showManualEntry) {
        AlertDialog(
            onDismissRequest = { showManualEntry = false },
            title = { Text(stringResource(R.string.zrif_dialog_title)) },
            text = {
                OutlinedTextField(
                    value = zrif,
                    onValueChange = { zrif = it.trim() },
                    label = { Text(stringResource(R.string.zrif_dialog_hint)) },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )
            },
            confirmButton = {
                TextButton(
                    onClick = { onEnterZrif(zrif) },
                    enabled = zrif.isNotEmpty()
                ) { Text(stringResource(R.string.action_install)) }
            },
            dismissButton = {
                TextButton(onClick = { showManualEntry = false }) { Text(stringResource(R.string.action_back)) }
            }
        )
    } else {
        AlertDialog(
            onDismissRequest = onDismiss,
            title = { Text(title) },
            text = {
                Column {
                    Text(
                        message,
                        style = MaterialTheme.typography.bodyMedium
                    )
                    Spacer(modifier = Modifier.height(16.dp))
                    FilledTonalButton(
                        onClick = onSelectLicenseFile,
                        modifier = Modifier.fillMaxWidth()
                    ) { Text(stringResource(R.string.license_select_file)) }
                    Spacer(modifier = Modifier.height(8.dp))
                    FilledTonalButton(
                        onClick = { showManualEntry = true },
                        modifier = Modifier.fillMaxWidth()
                    ) { Text(stringResource(R.string.license_enter_zrif)) }
                }
            },
            confirmButton = {},
            dismissButton = {
                TextButton(onClick = onDismiss) { Text(stringResource(R.string.action_cancel)) }
            }
        )
    }
}

@Composable
fun ArchiveInstallSourceDialog(
    onSelectFile: () -> Unit,
    onSelectFolder: () -> Unit,
    onDismiss: () -> Unit
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.install_archive_source_title)) },
        text = {
            Column {
                Text(
                    stringResource(R.string.install_archive_source_message),
                    style = MaterialTheme.typography.bodyMedium
                )
                Spacer(modifier = Modifier.height(16.dp))
                FilledTonalButton(
                    onClick = onSelectFile,
                    modifier = Modifier.fillMaxWidth()
                ) { Text(stringResource(R.string.install_archive_source_file)) }
                Spacer(modifier = Modifier.height(8.dp))
                FilledTonalButton(
                    onClick = onSelectFolder,
                    modifier = Modifier.fillMaxWidth()
                ) { Text(stringResource(R.string.install_archive_source_folder)) }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss) { Text(stringResource(R.string.action_cancel)) }
        }
    )
}
