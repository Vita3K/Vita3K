package org.vita3k.emulator.data

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.vita3k.emulator.NativeLib

internal data class SettingsSnapshot(
    val config: EmulatorConfig,
    val emulatorStoragePath: String,
    val hasCustomConfig: Boolean,
    val modulesList: List<Pair<String, Boolean>>,
    val installedCustomDrivers: List<String>,
    val showCustomDriverOptions: Boolean,
    val supportedMemoryMappingMask: Int,
    val customDriverLoadStatus: CustomDriverLoadStatus,
    val availableCameras: List<String>,
    val availableAdhocAddresses: List<Pair<String, String>>
)

enum class CustomDriverLoadStatus {
    Default,
    Loaded,
    Fallback;

    companion object {
        fun fromNative(value: Int): CustomDriverLoadStatus = when (value) {
            1 -> Loaded
            2 -> Fallback
            else -> Default
        }
    }
}

data class CustomDriverSupportInfo(
    val supportedMemoryMappingMask: Int,
    val loadStatus: CustomDriverLoadStatus
)

internal data class SettingsSaveResult(
    val hasCustomConfig: Boolean,
    val restartRequiredSettings: List<RestartRequiredSetting>
)

internal data class DeleteCustomConfigResult(
    val snapshot: SettingsSnapshot,
    val restartRequiredSettings: List<RestartRequiredSetting>
)

internal object SettingsRepository {
    private val customDriverSupportInfoCache = mutableMapOf<String, CustomDriverSupportInfo>()


    suspend fun load(titleId: String?): SettingsSnapshot = withContext(Dispatchers.IO) {
        val hasCustomConfig = titleId != null && NativeLib.hasCustomConfig(titleId)
        val loadedConfig = when {
            titleId == null -> NativeLib.getGlobalConfig()
            hasCustomConfig -> requireNotNull(NativeLib.getCustomConfig(titleId))
            else -> NativeLib.getGlobalConfig()
        }
        buildSnapshot(loadedConfig, hasCustomConfig)
    }

    suspend fun save(routeTitleId: String?, effectiveTitleId: String?, config: EmulatorConfig): SettingsSaveResult {
        val restartRequiredSettings = RestartRequiredSetting.fromNative(NativeLib.saveSettings(effectiveTitleId, config))
        return SettingsSaveResult(
            hasCustomConfig = routeTitleId != null && NativeLib.hasCustomConfig(routeTitleId),
            restartRequiredSettings = restartRequiredSettings
        )
    }

    suspend fun loadDefaults(): SettingsSnapshot = withContext(Dispatchers.IO) {
        buildSnapshot(NativeLib.getDefaultConfig(), hasCustomConfig = false)
    }

    suspend fun setCurrentEmulatorPath(path: String): Boolean = withContext(Dispatchers.IO) {
        NativeLib.setCurrentEmulatorPath(path)
    }

    suspend fun deleteCustomConfig(titleId: String): DeleteCustomConfigResult {
        val restartRequiredSettings = RestartRequiredSetting.fromNative(NativeLib.deleteCustomConfig(titleId))
        return DeleteCustomConfigResult(
            snapshot = buildSnapshot(NativeLib.getGlobalConfig(), hasCustomConfig = false),
            restartRequiredSettings = restartRequiredSettings
        )
    }

    suspend fun clearAllCustomConfigs(): Int = withContext(Dispatchers.IO) {
        NativeLib.clearAllCustomConfigs()
    }

    suspend fun getInstalledCustomDrivers(): List<String> = withContext(Dispatchers.IO) {
        NativeLib.getInstalledCustomDrivers().toList()
    }

    suspend fun installCustomDriver(path: String): String = withContext(Dispatchers.IO) {
        NativeLib.installCustomDriver(path).also { installedName ->
            if (installedName.isNotEmpty()) {
                synchronized(customDriverSupportInfoCache) {
                    customDriverSupportInfoCache.remove(installedName)
                }
            }
        }
    }

    suspend fun removeCustomDriver(driverName: String): Boolean = withContext(Dispatchers.IO) {
        NativeLib.removeCustomDriver(driverName).also { removed ->
            if (removed) {
                synchronized(customDriverSupportInfoCache) {
                    customDriverSupportInfoCache.remove(driverName)
                }
            }
        }
    }

    suspend fun getSupportedMemoryMappingMask(customDriverName: String): Int = withContext(Dispatchers.IO) {
        NativeLib.getSupportedMemoryMappingMask(customDriverName)
    }

    suspend fun getCustomDriverSupportInfo(customDriverName: String): CustomDriverSupportInfo = withContext(Dispatchers.IO) {
        getCustomDriverSupportInfoInternal(customDriverName)
    }

    private fun getCustomDriverSupportInfoInternal(customDriverName: String): CustomDriverSupportInfo {
        if (customDriverName.isNotEmpty()) {
            synchronized(customDriverSupportInfoCache) {
                customDriverSupportInfoCache[customDriverName]?.let { return it }
            }
        }

        val raw = NativeLib.getCustomDriverSupportInfo(customDriverName)
        val supportedMask = raw.getOrNull(0) ?: NativeLib.getSupportedMemoryMappingMask(customDriverName)
        val loadStatus = CustomDriverLoadStatus.fromNative(raw.getOrNull(1) ?: 0)
        val supportInfo = CustomDriverSupportInfo(
            supportedMemoryMappingMask = supportedMask,
            loadStatus = loadStatus
        )

        if (customDriverName.isNotEmpty()) {
            synchronized(customDriverSupportInfoCache) {
                customDriverSupportInfoCache.putIfAbsent(customDriverName, supportInfo)
            }
        }

        return supportInfo
    }

    private fun buildSnapshot(config: EmulatorConfig, hasCustomConfig: Boolean): SettingsSnapshot {
        val driverSupportInfo = getCustomDriverSupportInfoInternal(config.customDriverName)
        return SettingsSnapshot(
            config = config,
            emulatorStoragePath = NativeLib.getCurrentEmulatorPath(),
            hasCustomConfig = hasCustomConfig,
            modulesList = parseModules(NativeLib.getModulesList(config.lleModules)),
            installedCustomDrivers = NativeLib.getInstalledCustomDrivers().toList(),
            showCustomDriverOptions = NativeLib.shouldShowCustomDriverOptions(),
            supportedMemoryMappingMask = driverSupportInfo.supportedMemoryMappingMask,
            customDriverLoadStatus = driverSupportInfo.loadStatus,
            availableCameras = NativeLib.getAvailableCameras().toList(),
            availableAdhocAddresses = parsePairs(NativeLib.getAvailableAdhocAddresses())
        )
    }

    private fun parseModules(rawModules: Array<String>): List<Pair<String, Boolean>> = buildList {
        var index = 0
        while (index + 1 < rawModules.size) {
            add(rawModules[index] to (rawModules[index + 1] == "true"))
            index += 2
        }
    }

    private fun parsePairs(rawPairs: Array<String>): List<Pair<String, String>> = buildList {
        var index = 0
        while (index + 1 < rawPairs.size) {
            add(rawPairs[index] to rawPairs[index + 1])
            index += 2
        }
    }
}
