package org.vita3k.emulator.ui.viewmodel

import android.app.Application
import androidx.annotation.PluralsRes
import androidx.annotation.StringRes
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.launch
import org.vita3k.emulator.NativeLib
import org.vita3k.emulator.R
import org.vita3k.emulator.data.FirmwareInstallState
import org.vita3k.emulator.data.AppInfo
import org.vita3k.emulator.data.AppRepository
import org.vita3k.emulator.data.SortOption
import org.vita3k.emulator.data.UpdateCheckResult
import org.vita3k.emulator.data.UpdateCheckStatus
import org.vita3k.emulator.data.ViewMode

class AppsListViewModel(application: Application) : AndroidViewModel(application) {

    private fun str(@StringRes id: Int, vararg args: Any): String =
        getApplication<Application>().getString(id, *args)

    private fun qty(@PluralsRes id: Int, quantity: Int, vararg args: Any): String =
        getApplication<Application>().resources.getQuantityString(id, quantity, *args)

    private val _allApps = mutableListOf<AppInfo>()
    val apps = mutableStateListOf<AppInfo>()
    private val _selectedAppIds = mutableStateListOf<String>()
    // Stable snapshot of selected IDs avoids .toSet() allocation on every recomposition.
    val selectedAppIds: Set<String> by derivedStateOf { _selectedAppIds.toHashSet() }

    private val availableActionsByTitleId = mutableStateMapOf<String, Set<AppAction>>()
    private var compatSyncStarted = false
    private var compatSyncInProgress = false
    private var startupUpdateCheckStarted = false

    var initialized by mutableStateOf(false)
        private set
    var loading by mutableStateOf(false)
        private set
    var firmwareInstallState by mutableStateOf<FirmwareInstallState>(FirmwareInstallState.Missing)
        private set
    var actionInProgress by mutableStateOf(false)
        private set
    var actionResultMessage by mutableStateOf<String?>(null)
        private set
    var selectionMode by mutableStateOf(false)
        private set

    var searchQuery by mutableStateOf("")
        private set
    var sortOption by mutableStateOf(SortOption.TITLE)
        private set
    var viewMode by mutableStateOf(ViewMode.LIST)
        private set
    var appVersion by mutableStateOf("")
        private set
    var updateCheckInProgress by mutableStateOf(false)
        private set
    var updateCheckResult by mutableStateOf<UpdateCheckResult?>(null)
        private set

    // --- App info dialog ---
    var infoDialogApp by mutableStateOf<AppInfo?>(null)
        private set
    var infoAppInstallSizeBytes by mutableStateOf(-1L)
        private set

    fun initialize(storagePath: String) {
        if (initialized || loading) return

        loading = true
        viewModelScope.launch {
            appVersion = AppRepository.getAppVersion()
            val success = AppRepository.initialize(storagePath)
            initialized = success
            if (success) {
                firmwareInstallState = AppRepository.getFirmwareInstallState()
                loadApps()
                startCompatibilitySync()
                startUpdateCheckIfEnabled()
            }
            loading = false
        }
    }

    fun refreshAppsList(syncCompatibility: Boolean = false) {
        if (!initialized) return
        viewModelScope.launch {
            loading = true
            AppRepository.refreshAppsList()
            if (syncCompatibility) {
                syncCompatibilityDatabase()
            }
            firmwareInstallState = AppRepository.getFirmwareInstallState()
            loadApps()
            loading = false
        }
    }

    fun reloadAppsList() {
        if (!initialized) return
        viewModelScope.launch {
            loading = true
            firmwareInstallState = AppRepository.getFirmwareInstallState()
            loadApps()
            loading = false
        }
    }

    fun setSearch(query: String) {
        searchQuery = query
        applyFilterAndSort()
    }

    fun setSort(option: SortOption) {
        sortOption = option
        applyFilterAndSort()
    }

    fun toggleViewMode() {
        viewMode = if (viewMode == ViewMode.LIST) ViewMode.GRID else ViewMode.LIST
    }

    fun dismissActionResult() {
        actionResultMessage = null
    }

    fun dismissUpdateCheckResult() {
        updateCheckResult = null
    }

    // Returns the actions available for a title, defaulting to emptySet() until the
    // async fetch completes (prevents showing all actions as enabled before data arrives).
    fun getAvailableActions(titleId: String): Set<AppAction> {
        return availableActionsByTitleId[titleId] ?: emptySet()
    }

    fun prepareAppActions(app: AppInfo) {
        if (!initialized) return
        viewModelScope.launch {
            availableActionsByTitleId[app.titleId] = AppRepository.getAvailableAppActions(app.titleId)
        }
    }

    fun updateSelectionMode(enabled: Boolean) {
        selectionMode = enabled
        if (!enabled) {
            _selectedAppIds.clear()
        }
    }

    fun toggleAppSelection(app: AppInfo) {
        if (!selectionMode) selectionMode = true
        if (_selectedAppIds.contains(app.titleId)) {
            _selectedAppIds.remove(app.titleId)
        } else {
            _selectedAppIds.add(app.titleId)
        }
        if (_selectedAppIds.isEmpty()) selectionMode = false
    }

    fun selectAllVisibleApps() {
        if (apps.isEmpty()) {
            selectionMode = false
            _selectedAppIds.clear()
            return
        }
        selectionMode = true
        _selectedAppIds.clear()
        _selectedAppIds.addAll(apps.map { it.titleId })
    }

    fun runBatchDeleteSelected() {
        if (!initialized || actionInProgress || _selectedAppIds.isEmpty()) return

        runActionWithProgress {
            var successCount = 0
            val selected = _selectedAppIds.toList()
            for (titleId in selected) {
                if (AppRepository.runAppAction(titleId, AppAction.DELETE_APPLICATION)) {
                    successCount++
                }
            }
            refreshAppsList()
            _selectedAppIds.clear()
            selectionMode = false
            if (successCount == selected.size)
                qty(R.plurals.batch_delete_success, successCount, successCount)
            else
                str(R.string.batch_delete_partial, successCount, selected.size)
        }
    }

    fun runAppAction(app: AppInfo, action: AppAction) {
        if (!initialized || actionInProgress) return

        runActionWithProgress {
            val success = AppRepository.runAppAction(app.titleId, action)
            if (success) {
                if (action.group == AppActionGroup.DELETE) refreshAppsList()
                availableActionsByTitleId[app.titleId] = AppRepository.getAvailableAppActions(app.titleId)
                str(R.string.action_deleted_success, str(action.labelResId), app.title)
            } else {
                str(R.string.action_deleted_failed, str(action.labelResId), app.title)
            }
        }
    }

    fun showAppInfo(app: AppInfo) {
        infoDialogApp = app
        infoAppInstallSizeBytes = -1L
        viewModelScope.launch {
            infoAppInstallSizeBytes = AppRepository.getAppInstallSize(app.titleId)
        }
    }

    fun dismissAppInfo() {
        infoDialogApp = null
    }

    fun checkForUpdates(manual: Boolean = true) {
        if (updateCheckInProgress) {
            if (manual) {
                updateCheckResult = UpdateCheckResult(
                    status = UpdateCheckStatus.Failed,
                    message = str(R.string.updates_check_in_progress),
                    currentDisplayVersion = appVersion
                )
            }
            return
        }

        updateCheckInProgress = true
        viewModelScope.launch {
            try {
                val result = AppRepository.checkForUpdates(
                    appVersion = appVersion,
                    officialBuild = NativeLib.isOfficialBuild()
                )
                if (manual || result.status == UpdateCheckStatus.UpdateAvailable || result.status == UpdateCheckStatus.CustomBuildCanUpdate) {
                    updateCheckResult = result
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                if (manual) {
                    updateCheckResult = UpdateCheckResult(
                        status = UpdateCheckStatus.Failed,
                        message = str(R.string.install_error_generic, e.message ?: ""),
                        currentDisplayVersion = appVersion
                    )
                }
            } finally {
                updateCheckInProgress = false
            }
        }
    }

    private fun runActionWithProgress(block: suspend () -> String) {
        actionInProgress = true
        viewModelScope.launch {
            try {
                actionResultMessage = block()
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                actionResultMessage = str(R.string.install_error_generic, e.message ?: "")
            } finally {
                actionInProgress = false
            }
        }
    }

    private suspend fun loadApps() {
        val list = AppRepository.getAppList()
        _allApps.clear()
        _allApps.addAll(list)
        applyFilterAndSort()
    }

    private fun startCompatibilitySync() {
        if (compatSyncStarted) return

        compatSyncStarted = true
        viewModelScope.launch {
            if (syncCompatibilityDatabase()) {
                loadApps()
            }
        }
    }

    private fun startUpdateCheckIfEnabled() {
        if (startupUpdateCheckStarted) return

        startupUpdateCheckStarted = true
        viewModelScope.launch {
            val enabled = runCatching { NativeLib.getGlobalConfig().checkForUpdatesMode != 0 }.getOrDefault(true)
            if (enabled) {
                checkForUpdates(manual = false)
            }
        }
    }

    private suspend fun syncCompatibilityDatabase(): Boolean {
        if (compatSyncInProgress) return false

        compatSyncInProgress = true
        return try {
            AppRepository.syncCompatibilityDatabase()
        } finally {
            compatSyncInProgress = false
        }
    }

    private fun applyFilterAndSort() {
        val filtered = if (searchQuery.isBlank()) {
            _allApps
        } else {
            _allApps.filter {
                it.title.contains(searchQuery, ignoreCase = true) ||
                    it.titleId.contains(searchQuery, ignoreCase = true)
            }
        }

        val sorted = when (sortOption) {
            SortOption.TITLE -> filtered.sortedBy { it.title.lowercase() }
            SortOption.LAST_PLAYED -> filtered.sortedByDescending { it.lastPlayed }
            SortOption.COMPATIBILITY -> filtered.sortedByDescending { it.compatibility.value }
        }

        apps.clear()
        apps.addAll(sorted)
    }

}
