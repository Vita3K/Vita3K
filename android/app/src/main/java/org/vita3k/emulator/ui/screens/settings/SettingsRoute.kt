package org.vita3k.emulator.ui.screens.settings

import android.content.Intent
import android.net.Uri
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Save
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import org.vita3k.emulator.MainActivity
import org.vita3k.emulator.R
import org.vita3k.emulator.data.CustomDriverLoadStatus
import org.vita3k.emulator.data.RestartRequiredSetting
import org.vita3k.emulator.overlay.OverlayConfig
import org.vita3k.emulator.overlay.OverlayStore
import org.vita3k.emulator.ui.CUSTOM_DRIVER_DOWNLOAD_URL
import org.vita3k.emulator.ui.rememberConnectedGamepads
import org.vita3k.emulator.ui.theme.ApplyDialogDim
import org.vita3k.emulator.ui.viewmodel.SettingsViewModel

private val CUSTOM_DRIVER_MIME_TYPES = arrayOf(
    "application/zip",
    "application/x-zip-compressed",
    "application/octet-stream"
)
private val IMAGE_MIME_TYPES = arrayOf("image/*")

private fun formatRestartRequiredSettings(
    context: android.content.Context,
    settings: List<RestartRequiredSetting>
): String = settings.joinToString(separator = "\n- ", prefix = "- ") { setting ->
    context.getString(setting.labelResId)
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsRoute(
    titleId: String?,
    appName: String? = null,
    viewModel: SettingsViewModel,
    onStorageChanged: () -> Unit = {},
    onBack: () -> Unit
) {
    LaunchedEffect(titleId) {
        if (!viewModel.isLoaded(titleId)) {
            viewModel.load(titleId)
        }
    }

    val context = LocalContext.current
    val isPerApp = titleId != null
    val settingsLoaded = viewModel.isLoaded(titleId)
    val categories = remember(isPerApp) { settingsCategories(isPerApp) }
    val scrollState = rememberScrollState()
    val overlayScopeId = titleId.orEmpty()
    var overlayState by remember(titleId) { mutableStateOf(OverlayStore.defaultState(context)) }
    var originalOverlayState by remember(titleId) { mutableStateOf(overlayState) }
    val connectedGamepads = rememberConnectedGamepads(context)
    val controllerConnected = connectedGamepads.isNotEmpty()
    val overlayDirty = overlayState != originalOverlayState
    val hasPendingChanges = viewModel.isDirty || overlayDirty

    var selectedCategory by rememberSaveable(titleId) { mutableStateOf(SettingsCategory.Core) }
    var searchQuery by rememberSaveable(titleId) { mutableStateOf("") }
    var showDiscardDialog by remember { mutableStateOf(false) }
    var showResetDefaultsDialog by remember { mutableStateOf(false) }
    var showDeleteCustomConfigDialog by remember { mutableStateOf(false) }
    var showClearAllCustomConfigsDialog by remember { mutableStateOf(false) }
    var pendingCustomDriverRemoval by remember { mutableStateOf<String?>(null) }
    var pendingCameraImageTarget by remember { mutableStateOf<Boolean?>(null) }
    var activeHelp by remember { mutableStateOf<SettingsHelpEntry?>(null) }
    var searchActive by rememberSaveable(titleId) { mutableStateOf(false) }
    var showOverflowMenu by remember { mutableStateOf(false) }
    var showOverlayEditor by rememberSaveable(titleId) { mutableStateOf(false) }
    var pendingRestartSettings by remember { mutableStateOf<List<RestartRequiredSetting>?>(null) }
    var closeAfterRestartNotice by remember { mutableStateOf(false) }

    fun requestStorageFolderChange() {
        val activity = context as? MainActivity ?: return
        activity.requestStorageFolderChange { path ->
            path?.let { viewModel.changeStorageFolder(it, onStorageChanged) }
        }
    }

    fun requestCustomDriverInstall() {
        val activity = context as? MainActivity ?: return
        activity.requestFilePath(CUSTOM_DRIVER_MIME_TYPES) { path ->
            path?.let(viewModel::installCustomDriver)
        }
    }

    fun closeSearch() {
        searchActive = false
        searchQuery = ""
    }

    fun pickCameraImage(isFront: Boolean) {
        val activity = context as? MainActivity ?: return
        pendingCameraImageTarget = isFront
        activity.requestFilePath(IMAGE_MIME_TYPES) { path ->
            val target = pendingCameraImageTarget
            pendingCameraImageTarget = null
            if (target != null && path != null) {
                viewModel.setCameraImage(target, path)
            }
        }
    }

    fun openUrl(url: String) {
        context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
    }

    LaunchedEffect(settingsLoaded, titleId, viewModel.hasCustomConfig) {
        if (settingsLoaded) {
            val loaded = OverlayStore.resolveState(
                context = context,
                scopeId = overlayScopeId,
                allowGameOverride = isPerApp && OverlayStore.hasGameOverride(context, overlayScopeId)
            )
            overlayState = loaded
            originalOverlayState = loaded
        }
    }

    fun reloadOverlayState() {
        val loaded = OverlayStore.resolveState(
            context = context,
            scopeId = overlayScopeId,
            allowGameOverride = isPerApp && OverlayStore.hasGameOverride(context, overlayScopeId)
        )
        overlayState = loaded
        originalOverlayState = loaded
    }

    fun persistOverlayStateChanges() {
        val normalized = overlayState.copy(
            config = overlayState.config.normalized(),
            layout = overlayState.layout.normalized()
        )
        overlayState = normalized
        if (isPerApp) {
            OverlayStore.saveGameOverride(context, overlayScopeId, normalized)
        } else {
            OverlayStore.saveGlobalState(context, normalized)
        }
        originalOverlayState = normalized
    }

    fun saveSettings(onSaved: (List<RestartRequiredSetting>) -> Unit) {
        if (overlayDirty) {
            persistOverlayStateChanges()
        }
        if (viewModel.isDirty) {
            viewModel.save(forceCustomConfig = isPerApp, onSaved = onSaved)
        } else {
            onSaved(emptyList())
        }
    }

    fun tryBack() {
        if (hasPendingChanges) showDiscardDialog = true else onBack()
    }

    BackHandler { tryBack() }

    LaunchedEffect(categories, selectedCategory) {
        if (selectedCategory !in categories) {
            selectedCategory = categories.firstOrNull() ?: SettingsCategory.Core
        }
    }

    LaunchedEffect(selectedCategory, searchQuery) {
        scrollState.scrollTo(0)
    }

    Box(modifier = Modifier.fillMaxSize()) {
        Scaffold(
            topBar = {
                TopAppBar(
                    colors = TopAppBarDefaults.topAppBarColors(
                        containerColor = MaterialTheme.colorScheme.surface,
                        titleContentColor = MaterialTheme.colorScheme.onSurface,
                        actionIconContentColor = MaterialTheme.colorScheme.onSurfaceVariant,
                        navigationIconContentColor = MaterialTheme.colorScheme.onSurfaceVariant
                    ),
                    title = {
                        if (searchActive) {
                            SettingsTopBarSearchField(
                                query = searchQuery,
                                onQueryChange = { searchQuery = it },
                                modifier = Modifier.fillMaxWidth()
                            )
                        } else {
                            Text(
                                if (isPerApp && appName != null) {
                                    stringResource(R.string.settings_custom_config_label, appName)
                                } else {
                                    stringResource(R.string.settings_title)
                                }
                            )
                        }
                    },
                    navigationIcon = {
                        IconButton(onClick = { tryBack() }) {
                            Icon(
                                imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                                contentDescription = stringResource(R.string.action_back)
                            )
                        }
                    },
                    actions = {
                        if (!searchActive && hasPendingChanges && !viewModel.saving && !viewModel.customDriverBusy) {
                            IconButton(onClick = {
                                saveSettings { restartRequired ->
                                    if (restartRequired.isNotEmpty()) {
                                        pendingRestartSettings = restartRequired
                                    }
                                }
                            }) {
                                Icon(
                                    imageVector = Icons.Default.Save,
                                    contentDescription = stringResource(R.string.settings_save_cd)
                                )
                            }
                        }
                        IconButton(onClick = {
                            if (searchActive) {
                                closeSearch()
                            } else {
                                searchActive = true
                            }
                        }) {
                            Icon(
                                imageVector = if (searchActive) Icons.Default.Close else Icons.Default.Search,
                                contentDescription = if (searchActive) {
                                    stringResource(R.string.apps_list_cd_close_search)
                                } else {
                                    stringResource(R.string.apps_list_cd_search)
                                }
                            )
                        }
                        SettingsOverflowMenu(
                            expanded = showOverflowMenu,
                            onExpandedChange = { showOverflowMenu = it },
                            showDeleteCustomConfig = isPerApp && viewModel.hasCustomConfig,
                            onResetDefaults = {
                                showOverflowMenu = false
                                showResetDefaultsDialog = true
                            },
                            onDeleteCustomConfig = {
                                showOverflowMenu = false
                                showDeleteCustomConfigDialog = true
                            }
                        )
                    }
                )
            }
        ) { padding ->
            if (!settingsLoaded) {
                SettingsLoadingState(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(padding)
                )
                return@Scaffold
            }

            val searchEntries = rememberSettingsSearchEntries(
                isPerApp = isPerApp,
                backendRenderer = viewModel.config.backendRenderer,
                showCustomDriverOptions = viewModel.showCustomDriverOptions,
                showTurboModeOption = viewModel.showCustomDriverOptions
            )

            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(padding)
                    .background(MaterialTheme.colorScheme.background)
            ) {
                SettingsContentPane(
                    modifier = Modifier.fillMaxSize(),
                    titleId = titleId,
                    isPerApp = isPerApp,
                    viewModel = viewModel,
                    overlayConfig = overlayState.config,
                    supportedMemoryMappingMask = viewModel.supportedMemoryMappingMask,
                    customDriverLoadStatus = viewModel.customDriverLoadStatus,
                    availableCameras = viewModel.availableCameras,
                    availableAdhocAddresses = viewModel.availableAdhocAddresses,
                    selectedCategory = selectedCategory,
                    searchQuery = searchQuery,
                    onCloseSearch = ::closeSearch,
                    onSelectedCategoryChange = { selectedCategory = it },
                    searchEntries = searchEntries,
                    currentStoragePath = viewModel.currentStoragePath,
                    scrollState = scrollState,
                    controllerConnected = controllerConnected,
                    hasOverlayChanges = overlayDirty,
                    connectedGamepads = connectedGamepads,
                    categories = categories,
                    onChangeStorageFolder = ::requestStorageFolderChange,
                    onResetStorageFolder = { viewModel.resetStorageFolder(onStorageChanged) },
                    onInstallCustomDriver = ::requestCustomDriverInstall,
                    onDownloadCustomDriver = { openUrl(CUSTOM_DRIVER_DOWNLOAD_URL) },
                    onPickCameraImage = ::pickCameraImage,
                    onRequestRemoveCustomDriver = { pendingCustomDriverRemoval = it },
                    onClearAllCustomConfigs = { showClearAllCustomConfigsDialog = true },
                    onOverlayConfigChange = { updated ->
                        overlayState = overlayState.copy(config = updated.normalized())
                    },
                    onStartControlsEditor = {
                        showOverlayEditor = true
                    },
                    onResetControlsLayout = {
                        overlayState = overlayState.copy(layout = OverlayStore.defaultLayout(context))
                    },
                    onShowHelp = { activeHelp = it },
                )
            }
        }

        if (showOverlayEditor) {
            OverlayLayoutEditorDialog(
                overlayConfig = overlayState.config,
                overlayLayout = overlayState.layout,
                onDismiss = { updatedLayout ->
                    overlayState = overlayState.copy(layout = updatedLayout)
                    showOverlayEditor = false
                }
            )
        }
    }

    if (showDiscardDialog) {
        SettingsUnsavedChangesDialog(
            saving = viewModel.saving,
            onCancel = { showDiscardDialog = false },
            onSaveAndExit = {
                showDiscardDialog = false
                saveSettings { restartRequired ->
                    if (restartRequired.isEmpty()) {
                        onBack()
                    } else {
                        closeAfterRestartNotice = true
                        pendingRestartSettings = restartRequired
                    }
                }
            },
            onDiscard = {
                showDiscardDialog = false
                viewModel.discardChanges()
                overlayState = originalOverlayState
                onBack()
            }
        )
    }

    if (showResetDefaultsDialog) {
        AlertDialog(
            onDismissRequest = { showResetDefaultsDialog = false },
            title = { Text(stringResource(R.string.settings_confirm_reset_defaults_title)) },
            text = {
                ApplyDialogDim()
                Text(stringResource(R.string.settings_confirm_reset_defaults_message))
            },
            confirmButton = {
                TextButton(onClick = {
                    showResetDefaultsDialog = false
                    viewModel.resetToDefaults()
                    overlayState = OverlayStore.defaultState(context)
                }) {
                    Text(stringResource(R.string.settings_reset_defaults))
                }
            },
            dismissButton = {
                TextButton(onClick = { showResetDefaultsDialog = false }) {
                    Text(stringResource(R.string.action_cancel))
                }
            }
        )
    }

    if (showDeleteCustomConfigDialog) {
        AlertDialog(
            onDismissRequest = { showDeleteCustomConfigDialog = false },
            title = { Text(stringResource(R.string.settings_confirm_delete_custom_config_title)) },
            text = {
                ApplyDialogDim()
                Text(
                    stringResource(
                        R.string.settings_confirm_delete_custom_config_message,
                        appName ?: titleId ?: ""
                    )
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    showDeleteCustomConfigDialog = false
                    OverlayStore.deleteGameOverride(context, overlayScopeId)
                    reloadOverlayState()
                    viewModel.deleteCustomConfig { restartRequired ->
                        if (restartRequired.isNotEmpty()) {
                            pendingRestartSettings = restartRequired
                        }
                    }
                }) {
                    Text(
                        text = stringResource(R.string.action_delete),
                        color = MaterialTheme.colorScheme.error
                    )
                }
            },
            dismissButton = {
                TextButton(onClick = { showDeleteCustomConfigDialog = false }) {
                    Text(stringResource(R.string.action_cancel))
                }
            }
        )
    }

    if (showClearAllCustomConfigsDialog) {
        AlertDialog(
            onDismissRequest = { showClearAllCustomConfigsDialog = false },
            title = { Text(stringResource(R.string.settings_confirm_clear_all_title)) },
            text = {
                ApplyDialogDim()
                Text(stringResource(R.string.settings_confirm_clear_all_message))
            },
            confirmButton = {
                TextButton(onClick = {
                    showClearAllCustomConfigsDialog = false
                    OverlayStore.clearAllGameOverrides(context)
                    viewModel.clearAllCustomConfigs()
                }) {
                    Text(
                        text = stringResource(R.string.action_delete),
                        color = MaterialTheme.colorScheme.error
                    )
                }
            },
            dismissButton = {
                TextButton(onClick = { showClearAllCustomConfigsDialog = false }) {
                    Text(stringResource(R.string.action_cancel))
                }
            }
        )
    }

    pendingCustomDriverRemoval?.let { driverName ->
        AlertDialog(
            onDismissRequest = { pendingCustomDriverRemoval = null },
            title = { Text(stringResource(R.string.settings_confirm_remove_custom_driver_title)) },
            text = {
                ApplyDialogDim()
                Text(stringResource(R.string.settings_confirm_remove_custom_driver_message, driverName))
            },
            confirmButton = {
                TextButton(onClick = {
                    pendingCustomDriverRemoval = null
                    viewModel.removeCustomDriver(driverName)
                }) {
                    Text(
                        text = stringResource(R.string.action_delete),
                        color = MaterialTheme.colorScheme.error
                    )
                }
            },
            dismissButton = {
                TextButton(onClick = { pendingCustomDriverRemoval = null }) {
                    Text(stringResource(R.string.action_cancel))
                }
            }
        )
    }

    viewModel.operationResult?.let { result ->
        AlertDialog(
            onDismissRequest = { viewModel.dismissOperationResult() },
            title = {
                Text(
                    if (result.isError) {
                        stringResource(R.string.settings_opt_error)
                    } else {
                        stringResource(R.string.settings_success_title)
                    }
                )
            },
            text = {
                ApplyDialogDim()
                Text(result.message)
            },
            confirmButton = {
                TextButton(onClick = { viewModel.dismissOperationResult() }) {
                    Text(stringResource(R.string.action_ok))
                }
            }
        )
    }

    pendingRestartSettings?.let { settings ->
        AlertDialog(
            onDismissRequest = { },
            title = { Text(stringResource(R.string.settings_restart_required_title)) },
            text = {
                ApplyDialogDim()
                Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text(stringResource(R.string.settings_restart_required_summary))
                    Text(
                        stringResource(
                            R.string.settings_restart_required_message,
                            formatRestartRequiredSettings(context, settings)
                        )
                    )
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    pendingRestartSettings = null
                    val shouldClose = closeAfterRestartNotice
                    closeAfterRestartNotice = false
                    if (shouldClose) {
                        onBack()
                    }
                }) {
                    Text(stringResource(R.string.action_ok))
                }
            }
        )
    }

    activeHelp?.let { help ->
        SettingsHelpSheet(help = help, onDismiss = { activeHelp = null })
    }
}

@Composable
private fun SettingsContentPane(
    modifier: Modifier,
    titleId: String?,
    isPerApp: Boolean,
    viewModel: SettingsViewModel,
    overlayConfig: OverlayConfig,
    supportedMemoryMappingMask: Int,
    customDriverLoadStatus: CustomDriverLoadStatus,
    availableCameras: List<String>,
    availableAdhocAddresses: List<Pair<String, String>>,
    selectedCategory: SettingsCategory,
    searchQuery: String,
    onCloseSearch: () -> Unit,
    onSelectedCategoryChange: (SettingsCategory) -> Unit,
    searchEntries: List<SettingsSearchEntry>,
    currentStoragePath: String,
    scrollState: androidx.compose.foundation.ScrollState,
    controllerConnected: Boolean,
    hasOverlayChanges: Boolean,
    connectedGamepads: List<org.vita3k.emulator.ConnectedGamepad>,
    categories: List<SettingsCategory>,
    onChangeStorageFolder: () -> Unit,
    onResetStorageFolder: () -> Unit,
    onInstallCustomDriver: () -> Unit,
    onDownloadCustomDriver: () -> Unit,
    onPickCameraImage: (Boolean) -> Unit,
    onRequestRemoveCustomDriver: (String) -> Unit,
    onClearAllCustomConfigs: () -> Unit,
    onOverlayConfigChange: (OverlayConfig) -> Unit,
    onStartControlsEditor: (() -> Unit)?,
    onResetControlsLayout: (() -> Unit)?,
    onShowHelp: (SettingsHelpEntry) -> Unit,
) {
    Column(
        modifier = modifier
            .verticalScroll(scrollState)
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
    ) {
        if (isPerApp && viewModel.hasCustomConfig) {
            Surface(
                modifier = Modifier.fillMaxWidth(),
                color = MaterialTheme.colorScheme.primaryContainer,
                shape = androidx.compose.foundation.shape.RoundedCornerShape(20.dp)
            ) {
                Text(
                    text = stringResource(R.string.settings_custom_config_banner),
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.onPrimaryContainer,
                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 10.dp)
                )
            }
        }

        SettingsCategoryStrip(
            categories = categories,
            selectedCategory = selectedCategory,
            onCategorySelected = onSelectedCategoryChange
        )

        if (searchQuery.isBlank()) {
            SettingsCategoryBody(
                category = selectedCategory,
                cfg = viewModel.config,
                overlayConfig = overlayConfig,
                modulesList = viewModel.modulesList,
                modulesSearch = viewModel.modulesSearch,
                onModulesSearchChange = viewModel::onModulesSearchChange,
                onToggleModule = viewModel::toggleModule,
                supportedMemoryMappingMask = supportedMemoryMappingMask,
                customDriverLoadStatus = customDriverLoadStatus,
                availableCameras = availableCameras,
                availableAdhocAddresses = availableAdhocAddresses,
                currentStoragePath = currentStoragePath,
                defaultStoragePath = viewModel.defaultStoragePath,
                onChangeStorageFolder = onChangeStorageFolder,
                onResetStorageFolder = onResetStorageFolder,
                installedCustomDrivers = viewModel.installedCustomDrivers,
                customDriverBusy = viewModel.customDriverBusy,
                onInstallCustomDriver = onInstallCustomDriver,
                onDownloadCustomDriver = onDownloadCustomDriver,
                onPickCameraImage = onPickCameraImage,
                onRequestRemoveCustomDriver = onRequestRemoveCustomDriver,
                isPerApp = isPerApp,
                showCustomDriverManagement = viewModel.showCustomDriverOptions,
                showTurboModeOption = viewModel.showCustomDriverOptions,
                onClearAllCustomConfigs = onClearAllCustomConfigs,
                onSelectCustomDriver = viewModel::selectCustomDriver,
                onUpdate = viewModel::update,
                onOverlayConfigChange = onOverlayConfigChange,
                onStartControlsEditor = onStartControlsEditor,
                onResetControlsLayout = onResetControlsLayout,
                controllerConnected = controllerConnected,
                connectedGamepads = connectedGamepads,
                onShowHelp = onShowHelp
            )
        } else {
            SettingsSearchResults(
                query = searchQuery,
                entries = searchEntries,
                onOpen = {
                    onSelectedCategoryChange(it)
                    onCloseSearch()
                },
                onShowHelp = onShowHelp
            )
        }

        if (titleId == null && (viewModel.isDirty || hasOverlayChanges)) {
            Spacer(modifier = Modifier.width(1.dp))
        }
    }
}

@Composable
private fun SettingsOverflowMenu(
    expanded: Boolean,
    onExpandedChange: (Boolean) -> Unit,
    showDeleteCustomConfig: Boolean,
    onResetDefaults: () -> Unit,
    onDeleteCustomConfig: () -> Unit
) {
    Box {
        IconButton(onClick = { onExpandedChange(true) }) {
            Icon(
                imageVector = Icons.Default.MoreVert,
                contentDescription = stringResource(R.string.settings_more_options)
            )
        }
        DropdownMenu(
            expanded = expanded,
            onDismissRequest = { onExpandedChange(false) }
        ) {
            DropdownMenuItem(
                text = { Text(stringResource(R.string.settings_reset_defaults)) },
                onClick = onResetDefaults
            )
            if (showDeleteCustomConfig) {
                DropdownMenuItem(
                    text = { Text(stringResource(R.string.settings_delete_custom_config)) },
                    onClick = onDeleteCustomConfig
                )
            }
        }
    }
}

@Composable
private fun SettingsTopBarSearchField(
    query: String,
    onQueryChange: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    val textStyle = MaterialTheme.typography.bodyLarge.merge(
        TextStyle(color = MaterialTheme.colorScheme.onSurface)
    )

    Surface(
        modifier = modifier,
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.65f),
        shape = RoundedCornerShape(22.dp)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .heightIn(min = 40.dp)
                .padding(start = 12.dp, end = 4.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(
                imageVector = Icons.Default.Search,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Box(
                modifier = Modifier
                    .weight(1f)
                    .padding(horizontal = 10.dp)
            ) {
                if (query.isBlank()) {
                    Text(
                        text = stringResource(R.string.settings_search_placeholder),
                        style = MaterialTheme.typography.bodyLarge,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                BasicTextField(
                    value = query,
                    onValueChange = onQueryChange,
                    singleLine = true,
                    textStyle = textStyle,
                    modifier = Modifier.fillMaxWidth()
                )
            }
        }
    }
}

@Composable
private fun SettingsUnsavedChangesDialog(
    saving: Boolean,
    onCancel: () -> Unit,
    onSaveAndExit: () -> Unit,
    onDiscard: () -> Unit
) {
    Dialog(onDismissRequest = onCancel) {
        ApplyDialogDim()
        Surface(
            shape = RoundedCornerShape(28.dp),
            color = MaterialTheme.colorScheme.surfaceContainerHigh,
            tonalElevation = 0.dp,
            modifier = Modifier.fillMaxWidth()
        ) {
            Column(
                modifier = Modifier.padding(24.dp),
                verticalArrangement = Arrangement.spacedBy(20.dp)
            ) {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(
                        text = stringResource(R.string.settings_unsaved_title),
                        style = MaterialTheme.typography.headlineSmall,
                        color = MaterialTheme.colorScheme.onSurface
                    )
                    Text(
                        text = stringResource(R.string.settings_unsaved_message),
                        style = MaterialTheme.typography.bodyLarge,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.End
                ) {
                    TextButton(onClick = onCancel, enabled = !saving) {
                        Text(stringResource(R.string.action_cancel))
                    }
                    TextButton(onClick = onDiscard, enabled = !saving) {
                        Text(
                            text = stringResource(R.string.settings_discard),
                            color = MaterialTheme.colorScheme.error
                        )
                    }
                    TextButton(onClick = onSaveAndExit, enabled = !saving) {
                        Text(stringResource(R.string.settings_save_and_exit))
                    }
                }
            }
        }
    }
}
