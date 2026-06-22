package org.vita3k.emulator.ui.screens

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.view.Gravity
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.automirrored.filled.List
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.EmojiEvents
import androidx.compose.material.icons.filled.FilterList
import androidx.compose.material.icons.filled.GridView
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.SystemUpdate
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import coil.compose.AsyncImage
import org.vita3k.emulator.R
import org.vita3k.emulator.data.FirmwareInstallState
import org.vita3k.emulator.data.AppInfo
import org.vita3k.emulator.data.FirmwareComponent
import org.vita3k.emulator.data.SortOption
import org.vita3k.emulator.data.UpdateCheckResult
import org.vita3k.emulator.data.UpdateCheckStatus
import org.vita3k.emulator.data.ViewMode
import org.vita3k.emulator.ui.components.HtmlText
import org.vita3k.emulator.ui.theme.ApplyDialogDim
import org.vita3k.emulator.ui.theme.SCRIM_ALPHA
import org.vita3k.emulator.ui.viewmodel.AppAction
import org.vita3k.emulator.ui.viewmodel.AppActionGroup
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AppsListScreen(
    apps: List<AppInfo>,
    initialized: Boolean,
    loading: Boolean,
    appVersion: String,
    searchQuery: String,
    sortOption: SortOption,
    viewMode: ViewMode,
    updateCheckInProgress: Boolean,
    updateCheckResult: UpdateCheckResult?,
    firmwareInstallState: FirmwareInstallState,
    actionInProgress: Boolean,
    actionResultMessage: String?,
    selectionMode: Boolean,
    selectedAppIds: Set<String>,
    resolveAvailableActions: (AppInfo) -> Set<AppAction>,
    onAppSelected: (AppInfo) -> Unit,
    onRunAppAction: (AppInfo, AppAction) -> Unit,
    onPrepareAppActions: (AppInfo) -> Unit,
    onShowAppInfo: (AppInfo) -> Unit,
    onToggleAppSelection: (AppInfo) -> Unit,
    onSetSelectionMode: (Boolean) -> Unit,
    onSelectAllVisible: () -> Unit,
    onRunBatchDeleteSelected: () -> Unit,
    onDismissActionResult: () -> Unit,
    onSearchChanged: (String) -> Unit,
    onSortChanged: (SortOption) -> Unit,
    onViewModeToggle: () -> Unit,
    onCheckForUpdates: () -> Unit,
    onDismissUpdateCheckResult: () -> Unit,
    onRefresh: () -> Unit,
    onInstallClick: () -> Unit,
    onOpenSettings: () -> Unit = {},
    onOpenTrophyManager: () -> Unit = {},
    onOpenUserManagement: () -> Unit = {},
    onOpenWelcomeScreen: () -> Unit = {},
    onOpenCustomConfig: (AppInfo) -> Unit = {}
) {
    var showSearchBar by remember { mutableStateOf(false) }
    var showFilterSheet by remember { mutableStateOf(false) }
    var showOverflowMenu by remember { mutableStateOf(false) }
    var showAboutSheet by remember { mutableStateOf(false) }
    var selectedAppForActions by remember { mutableStateOf<AppInfo?>(null) }
    var actionTargetApp by remember { mutableStateOf<AppInfo?>(null) }
    var pendingAction by remember { mutableStateOf<AppAction?>(null) }
    var showBatchDeleteConfirm by remember { mutableStateOf(false) }

    Scaffold(
        containerColor = MaterialTheme.colorScheme.background,
        floatingActionButton = {
            FloatingActionButton(onClick = onInstallClick) {
                Icon(Icons.Default.Add, contentDescription = stringResource(R.string.apps_list_cd_install))
            }
        },
        topBar = {
            if (showSearchBar) {
                SearchBar(
                    query = searchQuery,
                    onQueryChange = onSearchChanged,
                    onClose = {
                        showSearchBar = false
                        onSearchChanged("")
                    }
                )
            } else {
                TopAppBar(
                    colors = TopAppBarDefaults.topAppBarColors(
                        containerColor = MaterialTheme.colorScheme.surface,
                        titleContentColor = MaterialTheme.colorScheme.onSurface,
                        actionIconContentColor = MaterialTheme.colorScheme.onSurfaceVariant,
                        navigationIconContentColor = MaterialTheme.colorScheme.onSurfaceVariant
                    ),
                    title = {
                        if (selectionMode) {
                            Text(pluralStringResource(R.plurals.apps_list_n_selected, selectedAppIds.size, selectedAppIds.size))
                        } else {
                            AppsListTitle(appVersion = appVersion)
                        }
                    },
                    navigationIcon = {
                        if (selectionMode) {
                            IconButton(onClick = { onSetSelectionMode(false) }) {
                                Icon(Icons.Default.Close, contentDescription = stringResource(R.string.apps_list_cd_exit_selection))
                            }
                        }
                    },
                    actions = {
                        if (selectionMode) {
                            IconButton(onClick = onSelectAllVisible) {
                                Icon(Icons.Default.Check, contentDescription = stringResource(R.string.apps_list_cd_select_all))
                            }
                            IconButton(
                                enabled = selectedAppIds.isNotEmpty(),
                                onClick = { showBatchDeleteConfirm = true }
                            ) {
                                Icon(Icons.Default.Delete, contentDescription = stringResource(R.string.apps_list_cd_delete_selected))
                            }
                        } else {
                            IconButton(onClick = { showSearchBar = true }) {
                                Icon(Icons.Default.Search, contentDescription = stringResource(R.string.apps_list_cd_search))
                            }
                            IconButton(onClick = onOpenSettings) {
                                Icon(Icons.Default.Settings, contentDescription = stringResource(R.string.settings_cd_open))
                            }
                            IconButton(onClick = { showFilterSheet = true }) {
                                Icon(Icons.Default.FilterList, contentDescription = stringResource(R.string.filter_cd_open))
                            }
                            AppsListOverflowMenu(
                                expanded = showOverflowMenu,
                                onExpandedChange = { showOverflowMenu = it },
                                onRefresh = {
                                    showOverflowMenu = false
                                    onRefresh()
                                },
                                onTrophyManager = {
                                    showOverflowMenu = false
                                    onOpenTrophyManager()
                                },
                                onUserManagement = {
                                    showOverflowMenu = false
                                    onOpenUserManagement()
                                },
                                onWelcomeScreen = {
                                    showOverflowMenu = false
                                    onOpenWelcomeScreen()
                                },
                                onCheckForUpdates = {
                                    showOverflowMenu = false
                                    onCheckForUpdates()
                                },
                                onAbout = {
                                    showOverflowMenu = false
                                    showAboutSheet = true
                                }
                            )
                        }
                    }
                )
            }
        }
    ) { padding ->
        when {
            loading && apps.isEmpty() -> {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(padding),
                    contentAlignment = Alignment.Center
                ) {
                    CircularProgressIndicator()
                }
            }
            !initialized -> {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(padding),
                    contentAlignment = Alignment.Center
                ) {
                    Text(stringResource(R.string.apps_list_init_failed))
                }
            }
            apps.isEmpty() -> {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(padding),
                    contentAlignment = Alignment.Center
                ) {
                    when {
                        searchQuery.isNotEmpty() -> {
                            AppsEmptyState(
                                title = stringResource(R.string.apps_list_empty_search, searchQuery)
                            )
                        }
                        !firmwareInstallState.hasRequiredComponents -> {
                            FirmwareMissingEmptyState(
                                firmwareInstallState = firmwareInstallState,
                                onInstallClick = onInstallClick
                            )
                        }
                        else -> {
                            AppsEmptyState(
                                title = stringResource(R.string.apps_list_empty_no_apps),
                                actionLabel = stringResource(R.string.apps_list_install_app),
                                onAction = onInstallClick
                            )
                        }
                    }
                }
            }
            else -> {
                when (viewMode) {
                    ViewMode.LIST -> AppsListView(
                        apps = apps,
                        onAppSelected = onAppSelected,
                        selectedAppIds = selectedAppIds,
                        selectionMode = selectionMode,
                        onSelectionClick = { onToggleAppSelection(it) },
                        onActionLongPress = {
                            selectedAppForActions = it
                            onPrepareAppActions(it)
                        },
                        modifier = Modifier.padding(padding)
                    )
                    ViewMode.GRID -> AppsGridView(
                        apps = apps,
                        onAppSelected = onAppSelected,
                        selectedAppIds = selectedAppIds,
                        selectionMode = selectionMode,
                        onSelectionClick = { onToggleAppSelection(it) },
                        onActionLongPress = {
                            selectedAppForActions = it
                            onPrepareAppActions(it)
                        },
                        modifier = Modifier.padding(padding)
                    )
                }
            }
        }
    }

    if (showFilterSheet) {
        FilterSheet(
            sortOption = sortOption,
            viewMode = viewMode,
            onSortChanged = onSortChanged,
            onViewModeToggle = onViewModeToggle,
            onDismiss = { showFilterSheet = false }
        )
    }

    if (showAboutSheet) {
        AboutSheet(
            appVersion = appVersion,
            onDismiss = { showAboutSheet = false }
        )
    }

    selectedAppForActions?.let { app ->
        AppActionsDialog(
            app = app,
            availableActions = resolveAvailableActions(app),
            onDismiss = { selectedAppForActions = null },
            onActionSelected = { action ->
                actionTargetApp = app
                selectedAppForActions = null
                pendingAction = action
            },
            onShowInfo = {
                selectedAppForActions = null
                onShowAppInfo(app)
            },
            onCustomConfig = {
                selectedAppForActions = null
                onOpenCustomConfig(app)
            }
        )
    }

    if (showBatchDeleteConfirm) {
        AlertDialog(
            onDismissRequest = { showBatchDeleteConfirm = false },
            title = { Text(pluralStringResource(R.plurals.batch_delete_title, selectedAppIds.size, selectedAppIds.size)) },
            text = {
                ApplyDialogDim()
                Text(pluralStringResource(R.plurals.batch_delete_message, selectedAppIds.size, selectedAppIds.size))
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        onRunBatchDeleteSelected()
                        showBatchDeleteConfirm = false
                    }
                ) {
                    Text(stringResource(R.string.action_delete), color = MaterialTheme.colorScheme.error)
                }
            },
            dismissButton = {
                TextButton(onClick = { showBatchDeleteConfirm = false }) {
                    Text(stringResource(R.string.action_cancel))
                }
            }
        )
    }

    val action = pendingAction
    if (action != null) {
        val actionLabel = stringResource(action.labelResId)
        AlertDialog(
            onDismissRequest = { pendingAction = null },
            title = {
                Text(
                    if (action.destructive) stringResource(R.string.confirm_delete_title, actionLabel)
                    else stringResource(R.string.confirm_reset_playtime_title)
                )
            },
            text = {
                ApplyDialogDim()
                Text(
                    if (action.destructive)
                        stringResource(R.string.confirm_delete_message, actionLabel)
                    else
                        stringResource(
                            R.string.confirm_reset_playtime_message,
                            actionTargetApp?.title ?: stringResource(R.string.confirm_reset_playtime_this_app)
                        )
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        val app = actionTargetApp
                        if (app != null) onRunAppAction(app, action)
                        pendingAction = null
                        actionTargetApp = null
                    }
                ) {
                    Text(
                        if (action.destructive) stringResource(R.string.action_delete) else stringResource(R.string.action_yes),
                        color = if (action.destructive) MaterialTheme.colorScheme.error
                                else MaterialTheme.colorScheme.primary
                    )
                }
            },
            dismissButton = {
                TextButton(onClick = {
                    pendingAction = null
                    actionTargetApp = null
                }) {
                    Text(if (action.destructive) stringResource(R.string.action_cancel) else stringResource(R.string.action_no))
                }
            }
        )
    }

    if (actionInProgress) {
        AlertDialog(
            onDismissRequest = {},
            title = {
                Text(
                    if (pendingAction != null)
                        stringResource(R.string.action_deleting, stringResource(pendingAction!!.labelResId))
                    else
                        stringResource(R.string.action_applying)
                )
            },
            text = {
                ApplyDialogDim()
                Column {
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                }
            },
            confirmButton = {}
        )
    }

    actionResultMessage?.let { message ->
        AlertDialog(
            onDismissRequest = onDismissActionResult,
            title = { Text(stringResource(R.string.action_done)) },
            text = {
                ApplyDialogDim()
                Text(message)
            },
            confirmButton = {
                TextButton(onClick = onDismissActionResult) {
                    Text(stringResource(R.string.action_ok))
                }
            }
        )
    }

    updateCheckResult?.let { result ->
        UpdateCheckDialog(
            result = result,
            onDismiss = onDismissUpdateCheckResult
        )
    }

    if (updateCheckInProgress) {
        AlertDialog(
            onDismissRequest = {},
            title = { Text(stringResource(R.string.updates_check_title)) },
            text = {
                ApplyDialogDim()
                Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text(stringResource(R.string.updates_check_progress))
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                }
            },
            confirmButton = {}
        )
    }
}

private data class ParsedAppVersion(
    val versionLabel: String,
    val buildNumber: String? = null,
    val revision: String? = null
)

private const val ABOUT_DESCRIPTION_HTML =
    """Vita3K is the world's first functional PS Vita and PS TV emulator, open source and written in C++ for Windows, Linux, macOS, and Android. Visit <a href="https://vita3k.org/quickstart.html">vita3k.org</a> for more info, browse the project on <a href="https://github.com/Vita3K/Vita3K">GitHub</a> if you want to contribute, or support us on <a href="https://ko-fi.com/vita3k">Ko-fi</a>."""

private const val ABOUT_CREDIT_HTML =
    """Icon by <a href="https://gordonmackayillustration.blogspot.com">Gordon Mackay</a>."""

private const val ABOUT_FOOTER_HTML =
    """<a href="https://vita3k.org">Website</a> | <a href="https://github.com/Vita3K/Vita3K">GitHub</a> | <a href="https://ko-fi.com/vita3k">Ko-fi</a> | <a href="https://discord.com/invite/6aGwQzh">Discord</a>"""

@Composable
private fun AppsEmptyState(
    title: String,
    body: String? = null,
    actionLabel: String? = null,
    onAction: (() -> Unit)? = null
) {
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 24.dp)
            .widthIn(max = 440.dp),
        shape = RoundedCornerShape(28.dp),
        color = MaterialTheme.colorScheme.surfaceContainerHigh.copy(alpha = 0.92f)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp, vertical = 28.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(
                text = title,
                style = MaterialTheme.typography.headlineSmall,
                textAlign = TextAlign.Center
            )
            body?.takeIf { it.isNotBlank() }?.let {
                Text(
                    text = it,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    textAlign = TextAlign.Center
                )
            }
            if (actionLabel != null && onAction != null) {
                FilledTonalButton(
                    onClick = onAction,
                    colors = ButtonDefaults.filledTonalButtonColors()
                ) {
                    Text(actionLabel)
                }
            }
        }
    }
}

@Composable
private fun FirmwareMissingEmptyState(
    firmwareInstallState: FirmwareInstallState,
    onInstallClick: () -> Unit
) {
    val missingLabels = remember(firmwareInstallState) {
        firmwareInstallState.components.missingRequiredComponentsInOrder.map(::firmwareComponentLabelRes)
    }

    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 24.dp)
            .widthIn(max = 440.dp),
        shape = RoundedCornerShape(28.dp),
        color = MaterialTheme.colorScheme.surfaceContainerHigh.copy(alpha = 0.94f)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp, vertical = 28.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Text(
                text = stringResource(R.string.apps_list_empty_firmware_title),
                style = MaterialTheme.typography.headlineSmall,
                textAlign = TextAlign.Center
            )
            Text(
                text = stringResource(R.string.apps_list_empty_firmware_body),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                textAlign = TextAlign.Center
            )
            Surface(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(20.dp),
                color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.72f)
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 18.dp, vertical = 16.dp),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Text(
                        text = stringResource(R.string.apps_list_empty_firmware_missing_list_title),
                        style = MaterialTheme.typography.labelLarge,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    missingLabels.forEach { labelResId ->
                        Text(
                            text = "\u2022 ${stringResource(labelResId)}",
                            style = MaterialTheme.typography.bodyLarge,
                            textAlign = TextAlign.Center
                        )
                    }
                }
            }
            FilledTonalButton(onClick = onInstallClick) {
                Text(stringResource(R.string.apps_list_install_firmware))
            }
        }
    }
}

private fun firmwareComponentLabelRes(component: FirmwareComponent): Int {
    return when (component) {
        FirmwareComponent.Preinstalled -> R.string.initial_setup_preinstall_title
        FirmwareComponent.Main -> R.string.initial_setup_main_title
        FirmwareComponent.FontPackage -> R.string.initial_setup_font_title
    }
}

private fun parseAppVersion(appVersion: String): ParsedAppVersion {
    if (appVersion.isBlank()) {
        return ParsedAppVersion(versionLabel = "")
    }

    val parts = appVersion.split("-", limit = 3)
    return ParsedAppVersion(
        versionLabel = parts.getOrNull(0).orEmpty().ifBlank { appVersion },
        buildNumber = parts.getOrNull(1)?.takeIf { it.isNotBlank() },
        revision = parts.getOrNull(2)?.takeIf { it.isNotBlank() }
    )
}

@Composable
private fun AppsListTitle(appVersion: String) {
    val parsedVersion = remember(appVersion) { parseAppVersion(appVersion) }
    val subtitle = remember(parsedVersion) {
        when {
            parsedVersion.versionLabel.isBlank() -> ""
            parsedVersion.buildNumber.isNullOrBlank() -> parsedVersion.versionLabel
            else -> "${parsedVersion.versionLabel} (${parsedVersion.buildNumber})"
        }
    }

    Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
        Text(
            text = stringResource(R.string.apps_list_app_title),
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
        if (subtitle.isNotBlank()) {
            Text(
                text = subtitle,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }
    }
}

@Composable
private fun AppsListOverflowMenu(
    expanded: Boolean,
    onExpandedChange: (Boolean) -> Unit,
    onRefresh: () -> Unit,
    onTrophyManager: () -> Unit,
    onUserManagement: () -> Unit,
    onWelcomeScreen: () -> Unit,
    onCheckForUpdates: () -> Unit,
    onAbout: () -> Unit
) {
    Box {
        IconButton(onClick = { onExpandedChange(true) }) {
            Icon(
                imageVector = Icons.Default.MoreVert,
                contentDescription = stringResource(R.string.apps_list_cd_more_options)
            )
        }
        DropdownMenu(
            expanded = expanded,
            onDismissRequest = { onExpandedChange(false) }
        ) {
            DropdownMenuItem(
                text = { Text(stringResource(R.string.apps_list_cd_refresh)) },
                leadingIcon = {
                    Icon(
                        imageVector = Icons.Default.Refresh,
                        contentDescription = null
                    )
                },
                onClick = onRefresh
            )
            DropdownMenuItem(
                text = { Text(stringResource(R.string.apps_list_menu_trophies)) },
                leadingIcon = {
                    Icon(
                        imageVector = Icons.Default.EmojiEvents,
                        contentDescription = null
                    )
                },
                onClick = onTrophyManager
            )
            DropdownMenuItem(
                text = { Text(stringResource(R.string.apps_list_menu_users)) },
                leadingIcon = {
                    Icon(
                        imageVector = Icons.Default.Person,
                        contentDescription = null
                    )
                },
                onClick = onUserManagement
            )
            DropdownMenuItem(
                text = { Text(stringResource(R.string.apps_list_menu_welcome)) },
                leadingIcon = {
                    Icon(
                        imageVector = Icons.Default.Info,
                        contentDescription = null
                    )
                },
                onClick = onWelcomeScreen
            )
            DropdownMenuItem(
                text = { Text(stringResource(R.string.apps_list_menu_check_updates)) },
                leadingIcon = {
                    Icon(
                        imageVector = Icons.Default.SystemUpdate,
                        contentDescription = null
                    )
                },
                onClick = onCheckForUpdates
            )
            DropdownMenuItem(
                text = { Text(stringResource(R.string.apps_list_menu_about)) },
                leadingIcon = {
                    Icon(
                        imageVector = Icons.Default.Info,
                        contentDescription = null
                    )
                },
                onClick = onAbout
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SearchBar(
    query: String,
    onQueryChange: (String) -> Unit,
    onClose: () -> Unit
) {
    val focusRequester = remember { FocusRequester() }
    val keyboardController = LocalSoftwareKeyboardController.current

    LaunchedEffect(Unit) {
        focusRequester.requestFocus()
        keyboardController?.show()
    }

    TopAppBar(
        colors = TopAppBarDefaults.topAppBarColors(
            containerColor = MaterialTheme.colorScheme.surface,
            titleContentColor = MaterialTheme.colorScheme.onSurface,
            navigationIconContentColor = MaterialTheme.colorScheme.onSurfaceVariant
        ),
        title = {
            TextField(
                value = query,
                onValueChange = onQueryChange,
                placeholder = { Text(stringResource(R.string.apps_list_search_placeholder)) },
                singleLine = true,
                colors = TextFieldDefaults.colors(
                    focusedContainerColor = Color.Transparent,
                    unfocusedContainerColor = Color.Transparent,
                    focusedIndicatorColor = Color.Transparent,
                    unfocusedIndicatorColor = Color.Transparent
                ),
                modifier = Modifier
                    .fillMaxWidth()
                    .focusRequester(focusRequester)
            )
        },
        navigationIcon = {
            IconButton(onClick = {
                keyboardController?.hide()
                onClose()
            }) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = stringResource(R.string.apps_list_cd_close_search))
            }
        }
    )
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun AboutSheet(
    appVersion: String,
    onDismiss: () -> Unit
) {
    val parsedVersion = remember(appVersion) { parseAppVersion(appVersion) }
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        scrimColor = Color.Black.copy(alpha = SCRIM_ALPHA)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 28.dp, vertical = 12.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            Image(
                painter = painterResource(id = R.mipmap.ic_launcher),
                contentDescription = stringResource(R.string.apps_list_app_title),
                modifier = Modifier
                    .size(132.dp)
                    .alpha(0.94f)
            )

            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Text(
                    text = stringResource(R.string.apps_list_app_title),
                    style = MaterialTheme.typography.headlineLarge,
                    fontWeight = FontWeight.SemiBold,
                    textAlign = TextAlign.Center
                )
                if (parsedVersion.versionLabel.isNotBlank()) {
                    Text(
                        text = parsedVersion.versionLabel,
                        style = MaterialTheme.typography.headlineSmall,
                        fontWeight = FontWeight.SemiBold,
                        textAlign = TextAlign.Center
                    )
                }
                parsedVersion.buildNumber?.let { build ->
                    Text(
                        text = stringResource(R.string.about_build, build),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                parsedVersion.revision?.let { revision ->
                    Text(
                        text = stringResource(R.string.about_revision, revision),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        textAlign = TextAlign.Center
                    )
                }
            }

            Spacer(modifier = Modifier.height(8.dp))

            HtmlText(
                html = ABOUT_DESCRIPTION_HTML,
                modifier = Modifier.fillMaxWidth(),
                textStyle = MaterialTheme.typography.bodyLarge,
                gravity = Gravity.CENTER
            )
            HtmlText(
                html = ABOUT_CREDIT_HTML,
                modifier = Modifier.fillMaxWidth(),
                textStyle = MaterialTheme.typography.bodySmall,
                textColor = MaterialTheme.colorScheme.onSurfaceVariant,
                gravity = Gravity.CENTER
            )

            Spacer(modifier = Modifier.height(4.dp))

            Text(
                text = stringResource(R.string.about_legal_notice),
                style = MaterialTheme.typography.bodyLarge,
                color = MaterialTheme.colorScheme.error,
                textAlign = TextAlign.Center
            )

            Spacer(modifier = Modifier.height(8.dp))

            HtmlText(
                html = ABOUT_FOOTER_HTML,
                modifier = Modifier.fillMaxWidth(),
                textStyle = MaterialTheme.typography.titleMedium,
                gravity = Gravity.CENTER
            )

            Spacer(modifier = Modifier.height(18.dp))
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun AppsListView(
    apps: List<AppInfo>,
    onAppSelected: (AppInfo) -> Unit,
    selectedAppIds: Set<String>,
    selectionMode: Boolean,
    onSelectionClick: (AppInfo) -> Unit,
    onActionLongPress: (AppInfo) -> Unit,
    modifier: Modifier = Modifier
) {
    LazyColumn(modifier = modifier.fillMaxSize()) {
        items(apps, key = { it.titleId }) { app ->
            val haptic = LocalHapticFeedback.current
            ListItem(
                headlineContent = {
                    Text(app.title, maxLines = 1, overflow = TextOverflow.Ellipsis)
                },
                supportingContent = {
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(app.titleId)
                        AppStatusBadges(app)
                    }
                },
                leadingContent = {
                    AppIcon(app, size = 48)
                },
                trailingContent = {
                    if (selectionMode && selectedAppIds.contains(app.titleId)) {
                        Icon(
                            imageVector = Icons.Default.Check,
                            contentDescription = stringResource(R.string.apps_list_cd_selected),
                            tint = MaterialTheme.colorScheme.primary
                        )
                    }
                },
                colors = ListItemDefaults.colors(
                    containerColor = MaterialTheme.colorScheme.background,
                    headlineColor = MaterialTheme.colorScheme.onSurface,
                    supportingColor = MaterialTheme.colorScheme.onSurfaceVariant
                ),
                modifier = Modifier.combinedClickable(
                    onClick = {
                        if (selectionMode) onSelectionClick(app) else onAppSelected(app)
                    },
                    onLongClick = {
                        haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                        if (selectionMode) onSelectionClick(app) else onActionLongPress(app)
                    }
                )
            )
        }
    }
}

@Composable
private fun AppsGridView(
    apps: List<AppInfo>,
    onAppSelected: (AppInfo) -> Unit,
    selectedAppIds: Set<String>,
    selectionMode: Boolean,
    onSelectionClick: (AppInfo) -> Unit,
    onActionLongPress: (AppInfo) -> Unit,
    modifier: Modifier = Modifier
) {
    LazyVerticalGrid(
        columns = GridCells.Adaptive(minSize = 120.dp),
        contentPadding = PaddingValues(8.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
        modifier = modifier.fillMaxSize()
    ) {
        items(apps, key = { it.titleId }) { app ->
            AppGridItem(
                app = app,
                selected = selectedAppIds.contains(app.titleId),
                selectionMode = selectionMode,
                onClick = {
                    if (selectionMode) onSelectionClick(app) else onAppSelected(app)
                },
                onLongClick = {
                    if (selectionMode) onSelectionClick(app) else onActionLongPress(app)
                }
            )
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun AppGridItem(
    app: AppInfo,
    selected: Boolean,
    selectionMode: Boolean,
    onClick: () -> Unit,
    onLongClick: () -> Unit
) {
    val haptic = LocalHapticFeedback.current
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .combinedClickable(
                onClick = onClick,
                onLongClick = {
                    haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                    onLongClick()
                }
            ),
        shape = RoundedCornerShape(8.dp)
    ) {
        Column {
            AppIcon(app, size = 120, modifier = Modifier.fillMaxWidth())
            Column(modifier = Modifier.padding(8.dp)) {
                Text(
                    app.title,
                    style = MaterialTheme.typography.bodySmall,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis
                )
                Row(
                    horizontalArrangement = Arrangement.spacedBy(4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.padding(top = 4.dp)
                ) {
                    AppStatusBadges(app)
                    if (selectionMode && selected) {
                        Icon(
                            imageVector = Icons.Default.Check,
                            contentDescription = stringResource(R.string.apps_list_cd_selected),
                            tint = MaterialTheme.colorScheme.primary,
                            modifier = Modifier.size(16.dp)
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun UpdateCheckDialog(
    result: UpdateCheckResult,
    onDismiss: () -> Unit
) {
    val context = LocalContext.current
    val defaultDownloadUrl = "https://github.com/Vita3K/Vita3K/releases/tag/continuous"
    val latestVersion = result.info.version.ifBlank {
        if (result.info.buildNumber > 0L) "Build ${result.info.buildNumber}" else stringResource(R.string.updates_latest_build)
    }
    val publishedAt = remember(result.info.publishedAt) { formatPublishedAt(result.info.publishedAt) }
    val downloadUrl = result.info.releaseUrl.ifBlank { defaultDownloadUrl }

    val titleRes = when (result.status) {
        UpdateCheckStatus.UpdateAvailable -> R.string.updates_available_title
        UpdateCheckStatus.CustomBuildCanUpdate -> R.string.updates_official_available_title
        else -> R.string.updates_check_title
    }

    val summary = remember(result, latestVersion, publishedAt) {
        when (result.status) {
            UpdateCheckStatus.UpdateAvailable ->
                if (publishedAt.isBlank()) {
                    context.getString(
                        R.string.updates_summary_available,
                        result.currentDisplayVersion.ifBlank { "Unknown" },
                        latestVersion
                    )
                } else {
                    context.getString(
                        R.string.updates_summary_available_with_date,
                        result.currentDisplayVersion.ifBlank { "Unknown" },
                        latestVersion,
                        publishedAt
                    )
                }
            UpdateCheckStatus.CustomBuildCanUpdate ->
                if (publishedAt.isBlank()) {
                    context.getString(R.string.updates_summary_custom_build, latestVersion)
                } else {
                    context.getString(R.string.updates_summary_custom_build_with_date, latestVersion, publishedAt)
                }
            else -> result.message
        }
    }

    val showDownloadAction = result.status == UpdateCheckStatus.UpdateAvailable ||
        result.status == UpdateCheckStatus.CustomBuildCanUpdate

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(titleRes)) },
        text = {
            ApplyDialogDim()
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(max = 320.dp)
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                Text(summary)
                if (result.info.notes.isNotBlank()) {
                    Text(
                        text = result.info.notes,
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        },
        confirmButton = {
            if (showDownloadAction) {
                TextButton(onClick = {
                    context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(downloadUrl)))
                    onDismiss()
                }) {
                    Text(stringResource(R.string.updates_open_download_page))
                }
            } else {
                TextButton(onClick = onDismiss) {
                    Text(stringResource(R.string.action_ok))
                }
            }
        },
        dismissButton = {
            if (showDownloadAction) {
                TextButton(onClick = onDismiss) {
                    Text(stringResource(R.string.action_cancel))
                }
            }
        }
    )
}

private fun formatPublishedAt(publishedAt: String): String {
    if (publishedAt.isBlank()) {
        return ""
    }

    return runCatching {
        val instant = java.time.Instant.parse(publishedAt)
        java.time.format.DateTimeFormatter
            .ofLocalizedDateTime(java.time.format.FormatStyle.SHORT)
            .withZone(java.time.ZoneId.systemDefault())
            .format(instant)
    }.getOrElse { publishedAt }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun AppActionsDialog(
    app: AppInfo,
    availableActions: Set<AppAction>,
    onDismiss: () -> Unit,
    onActionSelected: (AppAction) -> Unit,
    onShowInfo: () -> Unit,
    onCustomConfig: () -> Unit = {}
) {
    var showDeleteSubmenu by remember { mutableStateOf(false) }

    if (showDeleteSubmenu) {
        val availableDeletes = AppAction.entries
            .filter { it.group == AppActionGroup.DELETE && availableActions.contains(it) }

        AppMenuDialog(
            title = app.title,
            onDismiss = { showDeleteSubmenu = false }
        ) {
            availableDeletes.forEach { action ->
                AppMenuRow(
                    label = stringResource(action.labelResId),
                    labelColor = MaterialTheme.colorScheme.error,
                    onClick = {
                        showDeleteSubmenu = false
                        onActionSelected(action)
                    }
                )
            }
            AppMenuRow(
                label = stringResource(R.string.action_back),
                labelColor = MaterialTheme.colorScheme.onSurfaceVariant,
                icon = {
                    Icon(
                        Icons.AutoMirrored.Filled.ArrowBack,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.size(18.dp)
                    )
                },
                onClick = { showDeleteSubmenu = false }
            )
        }
        return
    }

    val hasAnyDelete = AppAction.entries
        .any { it.group == AppActionGroup.DELETE && availableActions.contains(it) }
    val otherActions = AppAction.entries.filter { it.group == AppActionGroup.OTHER }

    AppMenuDialog(title = app.title, onDismiss = onDismiss) {
        // Information
        AppMenuRow(
            label = stringResource(R.string.app_menu_information),
            icon = {
                Icon(
                    Icons.Default.Info,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.size(18.dp)
                )
            },
            onClick = { onDismiss(); onShowInfo() }
        )
        // Custom Config
        AppMenuRow(
            label = stringResource(R.string.app_menu_custom_config),
            icon = {
                Icon(
                    Icons.Default.Settings,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.size(18.dp)
                )
            },
            onClick = { onCustomConfig() }
        )

        // Other actions (reset last played, etc.)
        otherActions.forEach { action ->
            val enabled = availableActions.contains(action)
            val actionIcon: (@Composable () -> Unit)? = when (action) {
                AppAction.RESET_LAST_PLAYED -> ({
                    Icon(
                        Icons.Default.History,
                        contentDescription = null,
                        tint = if (enabled) MaterialTheme.colorScheme.onSurfaceVariant
                               else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.38f),
                        modifier = Modifier.size(18.dp)
                    )
                })
                else -> null
            }
            AppMenuRow(
                label = stringResource(action.labelResId),
                labelColor = if (enabled) MaterialTheme.colorScheme.onSurface
                             else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.38f),
                icon = actionIcon,
                onClick = { if (enabled) { onActionSelected(action) } }
            )
        }

        
        
        // Delete — drills into submenu
        if (hasAnyDelete) {
            AppMenuRow(
                label = stringResource(R.string.app_menu_delete),
                labelColor = MaterialTheme.colorScheme.error,
                icon = {
                    Icon(
                        Icons.Default.Delete,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.error,
                        modifier = Modifier.size(18.dp)
                    )
                },
                onClick = { showDeleteSubmenu = true }
            )
        }
    }
}

@Composable
private fun AppMenuDialog(
    title: String,
    onDismiss: () -> Unit,
    content: @Composable ColumnScope.() -> Unit
) {
    Dialog(onDismissRequest = onDismiss) {
        ApplyDialogDim()
        Surface(
            shape = RoundedCornerShape(16.dp),
            color = MaterialTheme.colorScheme.surfaceContainerHigh,
            tonalElevation = 0.dp,
            modifier = Modifier
                .fillMaxWidth()
                .wrapContentHeight()
        ) {
            Column(modifier = Modifier.padding(vertical = 8.dp)) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleLarge,
                    color = MaterialTheme.colorScheme.onSurface,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.padding(horizontal = 20.dp, vertical = 14.dp)
                )
                content()
                Spacer(modifier = Modifier.height(4.dp))
            }
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun AppMenuRow(
    label: String,
    labelColor: Color = MaterialTheme.colorScheme.onSurface,
    icon: (@Composable () -> Unit)? = null,
    onClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .combinedClickable(onClick = onClick)
            .padding(horizontal = 20.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        if (icon != null) {
            Box(modifier = Modifier.size(18.dp)) { icon() }
        } else {
            Spacer(modifier = Modifier.size(18.dp))
        }
        Text(label, style = MaterialTheme.typography.bodyLarge, color = labelColor)
    }
}

@Composable
internal fun AppIcon(app: AppInfo, size: Int, modifier: Modifier = Modifier) {
    val iconFile = app.iconFile
    if (iconFile != null) {
        AsyncImage(
            model = iconFile,
            contentDescription = app.title,
            contentScale = ContentScale.Crop,
            modifier = modifier
                .size(size.dp)
                .clip(RoundedCornerShape(4.dp))
        )
    } else {
        Box(
            modifier = modifier
                .size(size.dp)
                .clip(RoundedCornerShape(4.dp))
                .background(MaterialTheme.colorScheme.surfaceVariant),
            contentAlignment = Alignment.Center
        ) {
            Text(
                app.title.take(2).uppercase(),
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
internal fun CompatBadge(app: AppInfo) {
    val compat = app.compatibility
    Surface(
        color = Color(compat.colorHex),
        shape = RoundedCornerShape(4.dp)
    ) {
        Text(
            stringResource(compat.labelResId),
            style = MaterialTheme.typography.labelSmall,
            color = Color(compat.onColorHex),
            modifier = Modifier.padding(horizontal = 6.dp, vertical = 2.dp)
        )
    }
}

@Composable
private fun AppStatusBadges(app: AppInfo) {
    Row(
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        CompatBadge(app)
        if (app.hasCustomConfig) {
            CustomConfigBadge()
        }
    }
}

@Composable
private fun CustomConfigBadge() {
    Surface(
        color = Color(0xFF2458A6),
        contentColor = Color(0xFFF6FAFF),
        shape = RoundedCornerShape(999.dp)
    ) {
        Text(
            text = stringResource(R.string.apps_list_custom_config_badge),
            style = MaterialTheme.typography.labelSmall,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.padding(horizontal = 8.dp, vertical = 3.dp)
        )
    }
}
