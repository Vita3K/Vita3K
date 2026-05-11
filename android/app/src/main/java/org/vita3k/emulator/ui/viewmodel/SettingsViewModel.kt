package org.vita3k.emulator.ui.viewmodel

import android.app.Application
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.vita3k.emulator.NativeLib
import org.vita3k.emulator.R
import org.vita3k.emulator.data.AppStorage
import org.vita3k.emulator.data.CustomDriverLoadStatus
import org.vita3k.emulator.data.CustomDriverSupportInfo
import org.vita3k.emulator.data.EmulatorConfig
import org.vita3k.emulator.data.RestartRequiredSetting
import org.vita3k.emulator.data.SettingsRepository
import org.vita3k.emulator.data.SettingsSnapshot
import org.vita3k.emulator.data.UiLanguages

data class SettingsOperationResult(
    val message: String,
    val isError: Boolean
)

private const val GLOBAL_SETTINGS_ROUTE_KEY = "__global__"

class SettingsViewModel(application: Application) : AndroidViewModel(application) {

    private fun initialConfig(): EmulatorConfig =
        if (NativeLib.isInitialized()) NativeLib.getDefaultConfig() else EmulatorConfig()

    private fun str(resId: Int, vararg args: Any): String =
        getApplication<Application>().getString(resId, *args)

    private var loadJob: Job? = null
    private var memoryMappingRefreshJob: Job? = null
    private var loadRequestId = 0
    private var memoryMappingRefreshRequestId = 0
    private var activeLoadRouteKey: String? = null
    private var loadedRouteKey by mutableStateOf<String?>(null)

    var config by mutableStateOf(initialConfig())
        private set

    var currentStoragePath by mutableStateOf(
        if (NativeLib.isInitialized()) NativeLib.getCurrentEmulatorPath() else AppStorage.defaultStoragePath(getApplication())
    )
        private set

    val defaultStoragePath: String = AppStorage.defaultStoragePath(getApplication())

    private var originalConfig by mutableStateOf(config.copy())
    private var originalModulesList: List<Pair<String, Boolean>> = emptyList()

    var titleId: String? = null
        private set

    var loading by mutableStateOf(false)
        private set

    var saving by mutableStateOf(false)
        private set

    var hasCustomConfig by mutableStateOf(false)
        private set

    var installedCustomDrivers by mutableStateOf<List<String>>(emptyList())
        private set

    var showCustomDriverOptions by mutableStateOf(true)
        private set

    var customDriverBusy by mutableStateOf(false)
        private set

    var operationResult by mutableStateOf<SettingsOperationResult?>(null)
        private set

    var supportedMemoryMappingMask by mutableStateOf(1)
        private set

    var customDriverLoadStatus by mutableStateOf(CustomDriverLoadStatus.Default)
        private set

    var availableCameras by mutableStateOf<List<String>>(emptyList())
        private set

    var availableAdhocAddresses by mutableStateOf<List<Pair<String, String>>>(emptyList())
        private set

    val isDirty: Boolean
        get() = config != originalConfig

    var modulesList by mutableStateOf<List<Pair<String, Boolean>>>(emptyList())
        private set

    var modulesSearch by mutableStateOf("")
        private set

    fun isLoaded(titleId: String?): Boolean = loadedRouteKey == routeKey(titleId)

    fun preloadGlobalSettings() {
        load(titleId = null)
    }

    fun load(titleId: String?, force: Boolean = false) {
        val routeKey = routeKey(titleId)
        this.titleId = titleId
        if (!force && loading && activeLoadRouteKey == routeKey) {
            return
        }

        loadJob?.cancel()
        val requestId = ++loadRequestId
        activeLoadRouteKey = routeKey
        loading = true
        loadJob = viewModelScope.launch {
            try {
                val snapshot = withContext(Dispatchers.IO) {
                    SettingsRepository.load(titleId)
                }
                if (loadRequestId != requestId) return@launch
                applySnapshot(titleId, routeKey, snapshot)
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                if (loadRequestId == requestId) {
                    operationResult = SettingsOperationResult(
                        str(R.string.install_error_generic, e.message ?: ""),
                        true
                    )
                }
            } finally {
                if (loadRequestId == requestId) {
                    loading = false
                    activeLoadRouteKey = null
                    loadJob = null
                }
            }
        }
    }

    fun save(forceCustomConfig: Boolean = false, onSaved: ((List<RestartRequiredSetting>) -> Unit)? = null) {
        if (saving) return
        val configSnapshot = config.copy()
        val routeTitleId = titleId
        val saveTitleId = routeTitleId?.takeIf { forceCustomConfig || hasCustomConfig }
        saving = true
        viewModelScope.launch {
            var saveSucceeded = false
            var restartRequiredSettings: List<RestartRequiredSetting> = emptyList()
            try {
                val saveResult = withContext(Dispatchers.IO) {
                    SettingsRepository.save(routeTitleId, saveTitleId, configSnapshot)
                }
                hasCustomConfig = saveResult.hasCustomConfig
                restartRequiredSettings = saveResult.restartRequiredSettings
                originalConfig = configSnapshot.copy()
                originalModulesList = modulesList
                loadedRouteKey = routeKey(routeTitleId)
                saveSucceeded = true
                if (saveTitleId == null) {
                    UiLanguages.applyAndPersist(getApplication(), configSnapshot.userLang)
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = SettingsOperationResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            } finally {
                saving = false
            }
            if (saveSucceeded) {
                onSaved?.invoke(restartRequiredSettings)
            }
        }
    }

    fun changeStorageFolder(storagePath: String, onStorageChanged: () -> Unit) {
        viewModelScope.launch {
            try {
                if (storagePath.isNullOrBlank()) {
                    operationResult = SettingsOperationResult(
                        str(R.string.settings_emulator_storage_folder_resolve_failed),
                        true
                    )
                    return@launch
                }

                val success = withContext(Dispatchers.IO) {
                    SettingsRepository.setCurrentEmulatorPath(storagePath)
                }
                if (!success) {
                    operationResult = SettingsOperationResult(
                        str(R.string.settings_emulator_storage_folder_change_failed),
                        true
                    )
                    return@launch
                }
                val routeKey = routeKey(titleId)
                val snapshot = withContext(Dispatchers.IO) {
                    SettingsRepository.load(titleId)
                }
                applySnapshot(titleId, routeKey, snapshot)
                onStorageChanged()
                operationResult = SettingsOperationResult(
                    str(R.string.settings_emulator_storage_folder_change_success, storagePath),
                    false
                )
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = SettingsOperationResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            }
        }
    }

    fun resetStorageFolder(onStorageChanged: () -> Unit) {
        changeStorageFolder(defaultStoragePath, onStorageChanged)
    }

    fun resetToDefaults() {
        viewModelScope.launch {
            try {
                val defaults = withContext(Dispatchers.IO) {
                    SettingsRepository.loadDefaults()
                }
                config = defaults.config.copy()
                modulesList = defaults.modulesList
                installedCustomDrivers = defaults.installedCustomDrivers
                supportedMemoryMappingMask = defaults.supportedMemoryMappingMask
                customDriverLoadStatus = defaults.customDriverLoadStatus
                availableCameras = defaults.availableCameras
                availableAdhocAddresses = defaults.availableAdhocAddresses
                operationResult = SettingsOperationResult(
                    str(R.string.settings_reset_defaults_done),
                    false
                )
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = SettingsOperationResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            }
        }
    }

    fun deleteCustomConfig(onDeleted: ((List<RestartRequiredSetting>) -> Unit)? = null) {
        val id = titleId ?: return
        viewModelScope.launch {
            val result = withContext(Dispatchers.IO) {
                SettingsRepository.deleteCustomConfig(id)
            }
            applySnapshot(id, routeKey(id), result.snapshot)
            onDeleted?.invoke(result.restartRequiredSettings)
        }
    }

    fun discardChanges() {
        config = originalConfig.copy()
        modulesList = originalModulesList
        modulesSearch = ""
    }

    fun clearAllCustomConfigs() {
        viewModelScope.launch {
            val deletedCount = withContext(Dispatchers.IO) {
                SettingsRepository.clearAllCustomConfigs()
            }
            operationResult = SettingsOperationResult(
                str(R.string.settings_clear_all_custom_configs_success, deletedCount),
                false
            )
        }
    }

    fun installCustomDriver(path: String) {
        if (customDriverBusy) return

        customDriverBusy = true
        viewModelScope.launch {
            try {
                if (path.isBlank()) {
                    operationResult = SettingsOperationResult(
                        str(R.string.settings_gpu_custom_driver_resolve_failed),
                        true
                    )
                    return@launch
                }

                val installedName = withContext(Dispatchers.IO) {
                    SettingsRepository.installCustomDriver(path)
                }
                if (installedName.isEmpty()) {
                    operationResult = SettingsOperationResult(
                        str(R.string.settings_gpu_custom_driver_install_failed),
                        true
                    )
                    return@launch
                }

                installedCustomDrivers = withContext(Dispatchers.IO) {
                    SettingsRepository.getInstalledCustomDrivers()
                }
                val supportInfo = withContext(Dispatchers.IO) {
                    SettingsRepository.getCustomDriverSupportInfo(installedName)
                }
                applyCustomDriverSupportInfo(
                    requestedDriverName = installedName,
                    supportInfo = supportInfo,
                    coerceSelectionOnFallback = false
                )
                operationResult = SettingsOperationResult(
                    if (supportInfo.loadStatus == CustomDriverLoadStatus.Loaded) {
                        str(R.string.settings_gpu_custom_driver_install_success, installedName)
                    } else {
                        str(R.string.settings_gpu_custom_driver_install_fallback, installedName)
                    },
                    supportInfo.loadStatus != CustomDriverLoadStatus.Loaded
                )
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = SettingsOperationResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            } finally {
                customDriverBusy = false
            }
        }
    }

    fun removeCustomDriver(driverName: String) {
        if (customDriverBusy || driverName.isEmpty()) return

        customDriverBusy = true
        viewModelScope.launch {
            try {
                val removed = withContext(Dispatchers.IO) {
                    SettingsRepository.removeCustomDriver(driverName)
                }
                if (!removed) {
                    operationResult = SettingsOperationResult(
                        str(R.string.settings_gpu_custom_driver_remove_failed, driverName),
                        true
                    )
                    return@launch
                }

                installedCustomDrivers = withContext(Dispatchers.IO) {
                    SettingsRepository.getInstalledCustomDrivers()
                }
                if (config.customDriverName == driverName) {
                    val supportInfo = withContext(Dispatchers.IO) {
                        SettingsRepository.getCustomDriverSupportInfo("")
                    }
                    applyCustomDriverSupportInfo(
                        requestedDriverName = "",
                        supportInfo = supportInfo,
                        coerceSelectionOnFallback = false
                    )
                }
                operationResult = SettingsOperationResult(
                    str(R.string.settings_gpu_custom_driver_remove_success, driverName),
                    false
                )
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = SettingsOperationResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            } finally {
                customDriverBusy = false
            }
        }
    }

    fun setCameraImage(isFront: Boolean, path: String) {
        viewModelScope.launch {
            try {
                if (path.isNullOrBlank()) {
                    operationResult = SettingsOperationResult(
                        str(R.string.settings_camera_image_resolve_failed),
                        true
                    )
                    return@launch
                }

                update {
                    if (isFront) {
                        frontCameraType = 1
                        frontCameraImage = path
                    } else {
                        backCameraType = 1
                        backCameraImage = path
                    }
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = SettingsOperationResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            }
        }
    }

    fun dismissOperationResult() {
        operationResult = null
    }

    fun selectCustomDriver(driverName: String) {
        if (customDriverBusy) return

        val normalizedDriverName = driverName.trim()
        if (config.customDriverName == normalizedDriverName) {
            refreshSupportedMemoryMappingMask(normalizedDriverName)
            return
        }

        customDriverBusy = true
        config = config.copy().apply {
            customDriverName = normalizedDriverName
        }
        viewModelScope.launch {
            try {
                val supportInfo = withContext(Dispatchers.IO) {
                    SettingsRepository.getCustomDriverSupportInfo(normalizedDriverName)
                }
                applyCustomDriverSupportInfo(
                    requestedDriverName = normalizedDriverName,
                    supportInfo = supportInfo,
                    coerceSelectionOnFallback = false
                )
                operationResult = when {
                    normalizedDriverName.isEmpty() -> null
                    supportInfo.loadStatus == CustomDriverLoadStatus.Loaded -> SettingsOperationResult(
                        str(R.string.settings_gpu_custom_driver_selected_success, normalizedDriverName),
                        false
                    )
                    else -> SettingsOperationResult(
                        str(R.string.settings_gpu_custom_driver_selected_fallback, normalizedDriverName),
                        true
                    )
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = SettingsOperationResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
                val fallbackSupportInfo = CustomDriverSupportInfo(
                    supportedMemoryMappingMask = 1,
                    loadStatus = if (normalizedDriverName.isEmpty()) {
                        CustomDriverLoadStatus.Default
                    } else {
                        CustomDriverLoadStatus.Fallback
                    }
                )
                applyCustomDriverSupportInfo(
                    requestedDriverName = normalizedDriverName,
                    supportInfo = fallbackSupportInfo,
                    coerceSelectionOnFallback = false
                )
            } finally {
                customDriverBusy = false
            }
        }
    }

    fun update(block: EmulatorConfig.() -> Unit) {
        val previousDriverName = config.customDriverName
        val updatedConfig = config.copy().apply(block)
        config = updatedConfig

        if (previousDriverName != updatedConfig.customDriverName) {
            refreshSupportedMemoryMappingMask(updatedConfig.customDriverName)
        }
    }

    fun onModulesSearchChange(query: String) {
        modulesSearch = query
    }

    fun toggleModule(name: String) {
        val newList = modulesList.map { (n, e) -> if (n == name) n to !e else n to e }
        modulesList = newList
        update { lleModules = newList.filter { it.second }.map { it.first }.toTypedArray() }
    }

    private fun refreshSupportedMemoryMappingMask(
        customDriverName: String,
        coerceSelectionOnFallback: Boolean = false,
        onApplied: ((CustomDriverSupportInfo) -> Unit)? = null
    ) {
        memoryMappingRefreshJob?.cancel()
        val requestId = ++memoryMappingRefreshRequestId
        memoryMappingRefreshJob = viewModelScope.launch {
            try {
                val supportInfo = withContext(Dispatchers.IO) {
                    SettingsRepository.getCustomDriverSupportInfo(customDriverName)
                }
                if (memoryMappingRefreshRequestId != requestId) return@launch
                if (!coerceSelectionOnFallback && config.customDriverName != customDriverName) return@launch

                applyCustomDriverSupportInfo(customDriverName, supportInfo, coerceSelectionOnFallback)
                onApplied?.invoke(supportInfo)
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (_: Exception) {
                if (memoryMappingRefreshRequestId != requestId) return@launch
                if (!coerceSelectionOnFallback && config.customDriverName != customDriverName) return@launch

                val fallbackSupportInfo = CustomDriverSupportInfo(
                    supportedMemoryMappingMask = 1,
                    loadStatus = if (customDriverName.isEmpty()) {
                        CustomDriverLoadStatus.Default
                    } else {
                        CustomDriverLoadStatus.Fallback
                    }
                )
                applyCustomDriverSupportInfo(customDriverName, fallbackSupportInfo, coerceSelectionOnFallback)
                onApplied?.invoke(fallbackSupportInfo)
            }
        }
    }

    private fun applyCustomDriverSupportInfo(
        requestedDriverName: String,
        supportInfo: CustomDriverSupportInfo,
        coerceSelectionOnFallback: Boolean
    ) {
        val shouldFallbackToDefault = requestedDriverName.isNotEmpty() &&
            supportInfo.loadStatus == CustomDriverLoadStatus.Fallback &&
            coerceSelectionOnFallback
        val effectiveDriverName = if (shouldFallbackToDefault) "" else requestedDriverName
        val effectiveLoadStatus = when {
            effectiveDriverName.isEmpty() -> CustomDriverLoadStatus.Default
            else -> supportInfo.loadStatus
        }

        supportedMemoryMappingMask = supportInfo.supportedMemoryMappingMask
        customDriverLoadStatus = effectiveLoadStatus

        val coercedMapping = coerceMemoryMapping(config.memoryMapping, supportInfo.supportedMemoryMappingMask)
        val currentConfig = config
        if (currentConfig.customDriverName != effectiveDriverName || currentConfig.memoryMapping != coercedMapping) {
            config = currentConfig.copy().apply {
                customDriverName = effectiveDriverName
                memoryMapping = coercedMapping
            }
        }
    }

    private fun coerceMemoryMapping(memoryMapping: String, supportedMask: Int): String {
        if (isMemoryMappingSupported(memoryMapping, supportedMask)) {
            return memoryMapping
        }

        return when {
            (supportedMask and (1 shl 1)) != 0 -> "double-buffer"
            (supportedMask and (1 shl 2)) != 0 -> "external-host"
            (supportedMask and (1 shl 3)) != 0 -> "page-table"
            (supportedMask and (1 shl 4)) != 0 -> "native-buffer"
            else -> "disabled"
        }
    }

    private fun isMemoryMappingSupported(memoryMapping: String, supportedMask: Int): Boolean {
        val bit = when (memoryMapping) {
            "disabled" -> 0
            "double-buffer" -> 1
            "external-host" -> 2
            "page-table" -> 3
            "native-buffer" -> 4
            else -> return false
        }

        return (supportedMask and (1 shl bit)) != 0
    }

    private fun routeKey(titleId: String?): String = titleId ?: GLOBAL_SETTINGS_ROUTE_KEY

    private fun applySnapshot(titleId: String?, routeKey: String, snapshot: SettingsSnapshot) {
        this.titleId = titleId
        currentStoragePath = snapshot.emulatorStoragePath
        hasCustomConfig = snapshot.hasCustomConfig
        originalConfig = snapshot.config.copy()
        originalModulesList = snapshot.modulesList
        config = snapshot.config.copy()
        modulesList = snapshot.modulesList
        installedCustomDrivers = snapshot.installedCustomDrivers
        showCustomDriverOptions = snapshot.showCustomDriverOptions
        supportedMemoryMappingMask = snapshot.supportedMemoryMappingMask
        customDriverLoadStatus = snapshot.customDriverLoadStatus
        availableCameras = snapshot.availableCameras
        availableAdhocAddresses = snapshot.availableAdhocAddresses
        loadedRouteKey = routeKey
    }
}
