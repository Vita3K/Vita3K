package org.vita3k.emulator.ui.viewmodel

import android.app.Application
import androidx.annotation.StringRes
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import org.vita3k.emulator.InstallServiceController
import org.vita3k.emulator.R
import org.vita3k.emulator.data.InstallRepository
import java.io.File

enum class InstallType {
    FIRMWARE, PKG, ARCHIVE, LICENSE
}

enum class InstallResultStatus {
    SUCCESS, PARTIAL, ERROR
}

data class DeleteSourceOption(
    val label: String,
    val paths: List<String>
)

data class InstallResult(
    val status: InstallResultStatus,
    val message: String,
    val deleteOptions: List<DeleteSourceOption> = emptyList()
)

class InstallViewModel(application: Application) : AndroidViewModel(application) {

    private fun str(@StringRes id: Int, vararg args: Any): String =
        getApplication<Application>().getString(id, *args)

    var showInstallSheet by mutableStateOf(false)
        private set
    var installing by mutableStateOf(false)
        private set
    var progress by mutableStateOf(0)
        private set
    var statusMessage by mutableStateOf("")
        private set
    var installResult by mutableStateOf<InstallResult?>(null)
        private set
    var showPkgLicenseDialog by mutableStateOf(false)
        private set
    var completedInstallToken by mutableStateOf(0L)
        private set

    private var pendingPkgPath: String? = null
    private var pendingPkgLicensePath: String? = null
    private var lastObservedCompletionId = 0L

    init {
        viewModelScope.launch {
            InstallServiceController.state.collectLatest { state ->
                if (state.operationId == 0L) {
                    return@collectLatest
                }

                installing = state.installing
                progress = state.progress
                statusMessage = state.statusMessage
                installResult = state.installResult

                if (!state.installing && state.installResult != null && state.operationId != lastObservedCompletionId) {
                    lastObservedCompletionId = state.operationId
                    completedInstallToken = state.operationId
                }
            }
        }
    }

    fun showSheet() {
        showInstallSheet = true
    }

    fun hideSheet() {
        showInstallSheet = false
    }

    fun confirmInstallResult(selectedDeleteOptions: List<DeleteSourceOption>) {
        val paths = selectedDeleteOptions
            .flatMap(DeleteSourceOption::paths)
            .filter(String::isNotBlank)
            .distinct()

        installResult = null
        InstallServiceController.clearResult()
        if (paths.isEmpty()) {
            return
        }

        viewModelScope.launch {
            paths.forEach { path ->
                runCatching { File(path).delete() }
            }
        }
    }

    fun installFirmware(path: String) {
        InstallServiceController.clearResult()
        InstallServiceController.startFirmware(getApplication(), path)
    }

    fun onPkgPicked(path: String) {
        InstallServiceController.clearResult()
        pendingPkgLicensePath = null
        beginInstall(R.string.install_status_preparing)
        viewModelScope.launch {
            try {
                pendingPkgPath = path
                val autoZrif = InstallRepository.findPkgZrif(path)
                installing = false
                if (autoZrif.isNotEmpty()) {
                    confirmPkgInstall(autoZrif)
                } else {
                    showPkgLicenseDialog = true
                }
            } catch (e: Exception) {
                pendingPkgPath = null
                pendingPkgLicensePath = null
                installing = false
                installResult = InstallResult(
                    status = InstallResultStatus.ERROR,
                    message = str(R.string.install_error_generic, e.message ?: ""),
                    deleteOptions = sourceDeleteOptions(
                        deleteOption(R.string.install_delete_package_file, path)
                    )
                )
            }
        }
    }

    fun onPkgLicenseFilePicked(path: String) {
        pendingPkgLicensePath = path
        viewModelScope.launch {
            try {
                val zrif = InstallRepository.convertRifToZrif(path)
                if (zrif.isEmpty()) {
                    installResult = InstallResult(
                        status = InstallResultStatus.ERROR,
                        message = str(R.string.install_error_read_license),
                        deleteOptions = sourceDeleteOptions(
                            deleteOption(R.string.install_delete_package_file, pendingPkgPath),
                            deleteOption(R.string.install_delete_license_file, path)
                        )
                    )
                    cancelPkgInstall()
                    return@launch
                }

                confirmPkgInstall(zrif)
            } catch (e: Exception) {
                installResult = InstallResult(
                    status = InstallResultStatus.ERROR,
                    message = str(R.string.install_error_generic, e.message ?: ""),
                    deleteOptions = sourceDeleteOptions(
                        deleteOption(R.string.install_delete_package_file, pendingPkgPath),
                        deleteOption(R.string.install_delete_license_file, path)
                    )
                )
                cancelPkgInstall()
            }
        }
    }

    fun confirmPkgInstall(zrif: String) {
        showPkgLicenseDialog = false
        val path = pendingPkgPath ?: run {
            installResult = InstallResult(
                status = InstallResultStatus.ERROR,
                message = str(R.string.install_error_pkg_path_lost)
            )
            pendingPkgLicensePath = null
            return
        }
        val licensePath = pendingPkgLicensePath
        pendingPkgPath = null
        pendingPkgLicensePath = null
        InstallServiceController.clearResult()
        InstallServiceController.startPkg(getApplication(), path, zrif, licensePath)
    }

    fun cancelPkgInstall() {
        showPkgLicenseDialog = false
        pendingPkgPath = null
        pendingPkgLicensePath = null
    }

    fun installArchive(path: String) {
        InstallServiceController.clearResult()
        InstallServiceController.startArchive(getApplication(), path)
    }

    fun installArchiveFolder(paths: List<String>) {
        InstallServiceController.clearResult()
        InstallServiceController.startArchiveFolder(getApplication(), paths)
    }

    fun installLicense(path: String) {
        InstallServiceController.clearResult()
        InstallServiceController.startLicenseFile(getApplication(), path)
    }

    fun installLicenseFromZrif(zrif: String) {
        InstallServiceController.clearResult()
        InstallServiceController.startLicenseZrif(getApplication(), zrif)
    }

    private fun beginInstall(@StringRes statusResId: Int) {
        installResult = null
        installing = true
        progress = 0
        statusMessage = str(statusResId)
    }

    private fun deleteOption(@StringRes labelResId: Int, path: String?): DeleteSourceOption? =
        deleteOption(labelResId, listOfNotNull(path))

    private fun deleteOption(
        @StringRes labelResId: Int,
        paths: List<String>
    ): DeleteSourceOption? {
        val normalized = paths
            .filter(String::isNotBlank)
            .distinct()
        if (normalized.isEmpty()) {
            return null
        }

        return DeleteSourceOption(
            label = str(labelResId),
            paths = normalized
        )
    }

    private fun sourceDeleteOptions(vararg options: DeleteSourceOption?): List<DeleteSourceOption> =
        options.filterNotNull()

}
