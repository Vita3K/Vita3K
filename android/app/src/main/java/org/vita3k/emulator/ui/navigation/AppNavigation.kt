package org.vita3k.emulator.ui.navigation

import android.net.Uri
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.material3.CircularProgressIndicator
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import org.vita3k.emulator.MainActivity
import org.vita3k.emulator.data.AppStorage
import org.vita3k.emulator.data.AppInfo
import org.vita3k.emulator.ui.screens.AppInfoSheet
import org.vita3k.emulator.ui.screens.AppsListScreen
import org.vita3k.emulator.ui.screens.InitialSetupScreen
import org.vita3k.emulator.ui.screens.ArchiveInstallSourceDialog
import org.vita3k.emulator.ui.screens.InstallBottomSheet
import org.vita3k.emulator.ui.screens.InstallProgressDialog
import org.vita3k.emulator.ui.screens.InstallResultDialog
import org.vita3k.emulator.ui.screens.LicenseSourceDialog
import org.vita3k.emulator.ui.screens.UserManagementScreen
import org.vita3k.emulator.ui.screens.settings.SettingsRoute
import org.vita3k.emulator.trophies.ui.screen.TrophyBrowserRoute
import org.vita3k.emulator.trophies.ui.viewmodel.TrophyViewerViewModel
import org.vita3k.emulator.ui.common.ImmersiveLandscapeEffect
import org.vita3k.emulator.ui.viewmodel.AppsListViewModel
import org.vita3k.emulator.ui.viewmodel.InstallViewModel
import org.vita3k.emulator.ui.viewmodel.InstallType
import org.vita3k.emulator.ui.viewmodel.SettingsViewModel
import org.vita3k.emulator.ui.viewmodel.UserManagementViewModel

private const val ROUTE_INITIAL_SETUP = "initial_setup"
private const val ROUTE_INITIAL_SETUP_MANUAL = "initial_setup/manual"
private const val ROUTE_APPS_LIST = "apps_list"
private const val ROUTE_SETTINGS = "settings"
private const val ROUTE_USER_MANAGEMENT = "users"
private const val ROUTE_TROPHIES = "trophies"
private const val ROUTE_CUSTOM_CONFIG = "settings/custom/{titleId}?appName={appName}"
private const val ARG_TITLE_ID = "titleId"
private const val ARG_APP_NAME = "appName"
private val FIRMWARE_EXTENSIONS = setOf(".pup")
private val PKG_EXTENSIONS = setOf(".pkg")
private val ARCHIVE_EXTENSIONS = setOf(".zip", ".vpk")
private val LICENSE_EXTENSIONS = setOf(".bin", ".rif")

private fun customConfigRoute(titleId: String, appName: String): String {
    return "settings/custom/${Uri.encode(titleId)}?appName=${Uri.encode(appName)}"
}

private fun isTrophyRoute(route: String?): Boolean = route == ROUTE_TROPHIES

@Composable
fun AppNavigation(
    appsListViewModel: AppsListViewModel,
    installViewModel: InstallViewModel,
    settingsViewModel: SettingsViewModel,
    userManagementViewModel: UserManagementViewModel,
    onAppLaunch: (AppInfo) -> Unit
) {
    val navController = rememberNavController()
    val currentBackStackEntry by navController.currentBackStackEntryAsState()
    val context = LocalContext.current
    val activity = context as? MainActivity
    val trophyRouteActive = isTrophyRoute(currentBackStackEntry?.destination?.route)
    var showArchiveSourceDialog by remember { mutableStateOf(false) }
    var showStandaloneLicenseDialog by remember { mutableStateOf(false) }
    val startDestination by produceState<String?>(initialValue = null, key1 = context) {
        value = if (AppStorage.isInitialSetupCompleted(context)) ROUTE_APPS_LIST else ROUTE_INITIAL_SETUP
    }

    ImmersiveLandscapeEffect(activity = activity, enabled = trophyRouteActive)

    LaunchedEffect(appsListViewModel.initialized) {
        if (appsListViewModel.initialized) {
            settingsViewModel.preloadGlobalSettings()
        }
    }

    LaunchedEffect(installViewModel.completedInstallToken) {
        if (installViewModel.completedInstallToken != 0L) {
            appsListViewModel.reloadAppsList()
            settingsViewModel.load(titleId = null, force = true)
        }
    }

    fun requestInstallPicker(type: InstallType) {
        val hostActivity = activity ?: return
        when (type) {
            InstallType.FIRMWARE -> hostActivity.requestInstallFilePath(FIRMWARE_EXTENSIONS) { path ->
                path?.let { installViewModel.installFirmware(it) }
            }
            InstallType.PKG -> hostActivity.requestInstallFilePath(PKG_EXTENSIONS) { path ->
                path?.let { installViewModel.onPkgPicked(it) }
            }
            InstallType.ARCHIVE -> hostActivity.requestInstallFilePath(ARCHIVE_EXTENSIONS) { path ->
                path?.let { installViewModel.installArchive(it) }
            }
            InstallType.LICENSE -> hostActivity.requestInstallFilePath(LICENSE_EXTENSIONS) { path ->
                path?.let { installViewModel.installLicense(it) }
            }
        }
    }

    if (installViewModel.showPkgLicenseDialog) {
        LicenseSourceDialog(
            title = stringResource(org.vita3k.emulator.R.string.license_required_title),
            message = stringResource(org.vita3k.emulator.R.string.license_required_message),
            onSelectLicenseFile = {
                activity?.requestInstallFilePath(LICENSE_EXTENSIONS) { path ->
                    if (path != null) {
                        installViewModel.onPkgLicenseFilePicked(path)
                    } else {
                        installViewModel.cancelPkgInstall()
                    }
                }
            },
            onEnterZrif = { zrif -> installViewModel.confirmPkgInstall(zrif) },
            onDismiss = { installViewModel.cancelPkgInstall() }
        )
    }

    if (showArchiveSourceDialog) {
        ArchiveInstallSourceDialog(
            onSelectFile = {
                showArchiveSourceDialog = false
                requestInstallPicker(InstallType.ARCHIVE)
            },
            onSelectFolder = {
                showArchiveSourceDialog = false
                activity?.requestArchiveFolderPaths(ARCHIVE_EXTENSIONS) { paths ->
                    if (paths.isNotEmpty()) {
                        installViewModel.installArchiveFolder(paths)
                    }
                }
            },
            onDismiss = { showArchiveSourceDialog = false }
        )
    }

    if (showStandaloneLicenseDialog) {
        LicenseSourceDialog(
            title = stringResource(org.vita3k.emulator.R.string.install_license_source_title),
            message = stringResource(org.vita3k.emulator.R.string.install_license_source_message),
            onSelectLicenseFile = {
                showStandaloneLicenseDialog = false
                activity?.requestInstallFilePath(LICENSE_EXTENSIONS) { path ->
                    path?.let { installViewModel.installLicense(it) }
                }
            },
            onEnterZrif = { zrif ->
                showStandaloneLicenseDialog = false
                installViewModel.installLicenseFromZrif(zrif)
            },
            onDismiss = { showStandaloneLicenseDialog = false }
        )
    }

    if (installViewModel.showInstallSheet) {
        InstallBottomSheet(
            onDismiss = { installViewModel.hideSheet() },
            onSelectType = {
                installViewModel.hideSheet()
                when (it) {
                    InstallType.ARCHIVE -> showArchiveSourceDialog = true
                    InstallType.LICENSE -> showStandaloneLicenseDialog = true
                    else -> requestInstallPicker(it)
                }
            }
        )
    }

    if (installViewModel.installing) {
        InstallProgressDialog(
            progress = installViewModel.progress,
            statusMessage = installViewModel.statusMessage
        )
    }

    installViewModel.installResult?.let { result ->
        InstallResultDialog(
            result = result,
            onConfirm = { selectedOptions -> installViewModel.confirmInstallResult(selectedOptions) }
        )
    }

    appsListViewModel.infoDialogApp?.let { app ->
        AppInfoSheet(
            app = app,
            installSizeBytes = appsListViewModel.infoAppInstallSizeBytes,
            onDismiss = { appsListViewModel.dismissAppInfo() }
        )
    }

    if (startDestination == null) {
        Box(
            modifier = Modifier.fillMaxSize(),
            contentAlignment = Alignment.Center
        ) {
            CircularProgressIndicator()
        }
        return
    }

    NavHost(
        navController = navController,
        startDestination = startDestination!!,
        enterTransition = { EnterTransition.None },
        exitTransition = { ExitTransition.None },
        popEnterTransition = { EnterTransition.None },
        popExitTransition = { ExitTransition.None }
    ) {
        composable(ROUTE_INITIAL_SETUP) {
            val completeSetup: () -> Unit = {
                AppStorage.setInitialSetupCompleted(context, true)
                navController.navigate(ROUTE_APPS_LIST) {
                    launchSingleTop = true
                    popUpTo(ROUTE_INITIAL_SETUP) { inclusive = true }
                }
            }

            InitialSetupScreen(
                firmwareInstallState = appsListViewModel.firmwareInstallState,
                preferredLanguageIndex = settingsViewModel.config.sysLang,
                onInstallFirmware = { requestInstallPicker(InstallType.FIRMWARE) },
                dismissLabel = stringResource(org.vita3k.emulator.R.string.initial_setup_skip),
                onSkip = completeSetup,
                onFinish = completeSetup
            )
        }

        composable(ROUTE_INITIAL_SETUP_MANUAL) {
            InitialSetupScreen(
                firmwareInstallState = appsListViewModel.firmwareInstallState,
                preferredLanguageIndex = settingsViewModel.config.sysLang,
                onInstallFirmware = { requestInstallPicker(InstallType.FIRMWARE) },
                dismissLabel = stringResource(org.vita3k.emulator.R.string.emulation_exit),
                onSkip = { navController.popBackStack() },
                onFinish = { navController.popBackStack() }
            )
        }

        composable(ROUTE_APPS_LIST) {
            AppsListScreen(
                apps = appsListViewModel.apps,
                initialized = appsListViewModel.initialized,
                loading = appsListViewModel.loading,
                appVersion = appsListViewModel.appVersion,
                searchQuery = appsListViewModel.searchQuery,
                sortOption = appsListViewModel.sortOption,
                viewMode = appsListViewModel.viewMode,
                updateCheckInProgress = appsListViewModel.updateCheckInProgress,
                updateCheckResult = appsListViewModel.updateCheckResult,
                firmwareInstallState = appsListViewModel.firmwareInstallState,
                actionInProgress = appsListViewModel.actionInProgress,
                actionResultMessage = appsListViewModel.actionResultMessage,
                selectionMode = appsListViewModel.selectionMode,
                selectedAppIds = appsListViewModel.selectedAppIds,
                resolveAvailableActions = { app -> appsListViewModel.getAvailableActions(app.titleId) },
                onAppSelected = { app -> onAppLaunch(app) },
                onRunAppAction = { app, action -> appsListViewModel.runAppAction(app, action) },
                onPrepareAppActions = { app -> appsListViewModel.prepareAppActions(app) },
                onShowAppInfo = { app -> appsListViewModel.showAppInfo(app) },
                onToggleAppSelection = { app -> appsListViewModel.toggleAppSelection(app) },
                onSetSelectionMode = { enabled -> appsListViewModel.updateSelectionMode(enabled) },
                onSelectAllVisible = { appsListViewModel.selectAllVisibleApps() },
                onRunBatchDeleteSelected = { appsListViewModel.runBatchDeleteSelected() },
                onDismissActionResult = { appsListViewModel.dismissActionResult() },
                onSearchChanged = { appsListViewModel.setSearch(it) },
                onSortChanged = { appsListViewModel.setSort(it) },
                onViewModeToggle = { appsListViewModel.toggleViewMode() },
                onCheckForUpdates = { appsListViewModel.checkForUpdates() },
                onDismissUpdateCheckResult = { appsListViewModel.dismissUpdateCheckResult() },
                onRefresh = { appsListViewModel.refreshAppsList(syncCompatibility = true) },
                onInstallClick = { installViewModel.showSheet() },
                onOpenSettings = {
                    navController.navigate(ROUTE_SETTINGS) {
                        launchSingleTop = true
                    }
                },
                onOpenTrophyManager = {
                    navController.navigate(ROUTE_TROPHIES) {
                        launchSingleTop = true
                    }
                },
                onOpenUserManagement = {
                    navController.navigate(ROUTE_USER_MANAGEMENT) {
                        launchSingleTop = true
                    }
                },
                onOpenWelcomeScreen = {
                    navController.navigate(ROUTE_INITIAL_SETUP_MANUAL) {
                        launchSingleTop = true
                    }
                },
                onOpenCustomConfig = { app ->
                    navController.navigate(customConfigRoute(app.titleId, app.title)) {
                        launchSingleTop = true
                    }
                }
            )
        }

        composable(route = ROUTE_TROPHIES) {
            val trophyViewerViewModel: TrophyViewerViewModel = viewModel(
                factory = TrophyViewerViewModel.factory()
            )

            TrophyBrowserRoute(
                viewModel = trophyViewerViewModel,
                onExit = { navController.popBackStack() }
            )
        }

        composable(ROUTE_SETTINGS) {
            SettingsRoute(
                titleId = null,
                appName = null,
                viewModel = settingsViewModel,
                onStorageChanged = { appsListViewModel.refreshAppsList() },
                onBack = { navController.popBackStack() }
            )
        }

        composable(ROUTE_USER_MANAGEMENT) {
            UserManagementScreen(
                viewModel = userManagementViewModel,
                onBack = { navController.popBackStack() },
                onUsersChanged = { appsListViewModel.refreshAppsList() }
            )
        }

        composable(
            route = ROUTE_CUSTOM_CONFIG,
            arguments = listOf(
                navArgument(ARG_TITLE_ID) { type = NavType.StringType },
                navArgument(ARG_APP_NAME) {
                    type = NavType.StringType
                    defaultValue = ""
                }
            )
        ) { backStackEntry ->
            val titleId = backStackEntry.arguments?.getString(ARG_TITLE_ID).orEmpty()
            val appName = backStackEntry.arguments?.getString(ARG_APP_NAME)
                ?.takeIf { it.isNotBlank() }
                ?: appsListViewModel.apps.firstOrNull { it.titleId == titleId }?.title
                ?: titleId

            SettingsRoute(
                titleId = titleId,
                appName = appName,
                viewModel = settingsViewModel,
                onStorageChanged = { appsListViewModel.refreshAppsList() },
                onBack = {
                    appsListViewModel.refreshAppsList()
                    navController.popBackStack()
                }
            )
        }
    }
}
