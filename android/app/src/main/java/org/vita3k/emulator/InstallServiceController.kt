package org.vita3k.emulator

import android.content.Context
import android.content.Intent
import androidx.core.content.ContextCompat
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import org.vita3k.emulator.ui.viewmodel.InstallResult
import java.util.concurrent.atomic.AtomicLong

enum class InstallOperationType {
    FIRMWARE,
    PKG,
    ARCHIVE,
    ARCHIVE_FOLDER,
    LICENSE_FILE,
    LICENSE_ZRIF
}

data class InstallServiceState(
    val operationId: Long = 0L,
    val operationType: InstallOperationType? = null,
    val installing: Boolean = false,
    val progress: Int = 0,
    val statusMessage: String = "",
    val installResult: InstallResult? = null
)

object InstallServiceController {
    const val EXTRA_OPERATION_ID = "operation_id"
    const val EXTRA_OPERATION_TYPE = "operation_type"
    const val EXTRA_PATH = "path"
    const val EXTRA_PATHS = "paths"
    const val EXTRA_ZRIF = "zrif"
    const val EXTRA_LICENSE_PATH = "license_path"

    private val nextOperationId = AtomicLong(1L)
    private val mutableState = MutableStateFlow(InstallServiceState())

    val state: StateFlow<InstallServiceState> = mutableState

    fun startFirmware(context: Context, path: String): Long =
        start(context, InstallOperationType.FIRMWARE, context.getString(org.vita3k.emulator.R.string.install_status_firmware)) {
            putExtra(EXTRA_PATH, path)
        }

    fun startPkg(context: Context, path: String, zrif: String, licensePath: String?): Long =
        start(context, InstallOperationType.PKG, context.getString(org.vita3k.emulator.R.string.install_status_package)) {
            putExtra(EXTRA_PATH, path)
            putExtra(EXTRA_ZRIF, zrif)
            putExtra(EXTRA_LICENSE_PATH, licensePath)
        }

    fun startArchive(context: Context, path: String): Long =
        start(context, InstallOperationType.ARCHIVE, context.getString(org.vita3k.emulator.R.string.install_status_archive)) {
            putExtra(EXTRA_PATH, path)
        }

    fun startArchiveFolder(context: Context, paths: List<String>): Long =
        start(context, InstallOperationType.ARCHIVE_FOLDER, context.getString(org.vita3k.emulator.R.string.install_status_archive)) {
            putStringArrayListExtra(EXTRA_PATHS, ArrayList(paths))
        }

    fun startLicenseFile(context: Context, path: String): Long =
        start(context, InstallOperationType.LICENSE_FILE, context.getString(org.vita3k.emulator.R.string.install_status_license)) {
            putExtra(EXTRA_PATH, path)
        }

    fun startLicenseZrif(context: Context, zrif: String): Long =
        start(context, InstallOperationType.LICENSE_ZRIF, context.getString(org.vita3k.emulator.R.string.install_status_license)) {
            putExtra(EXTRA_ZRIF, zrif)
        }

    fun updateProgress(operationId: Long, percent: Int, status: String) {
        mutableState.update { state ->
            if (state.operationId != operationId) {
                state
            } else {
                state.copy(
                    installing = true,
                    progress = percent.coerceIn(0, 100),
                    statusMessage = status
                )
            }
        }
    }

    fun finish(operationId: Long, result: InstallResult, progress: Int = 100, status: String = "") {
        mutableState.update { state ->
            if (state.operationId != operationId) {
                state
            } else {
                state.copy(
                    installing = false,
                    progress = progress.coerceIn(0, 100),
                    statusMessage = status.ifBlank { state.statusMessage },
                    installResult = result
                )
            }
        }
    }

    fun clearResult() {
        mutableState.update { state ->
            state.copy(installResult = null)
        }
    }

    private inline fun start(
        context: Context,
        type: InstallOperationType,
        initialStatus: String,
        extras: Intent.() -> Unit
    ): Long {
        val operationId = nextOperationId.getAndIncrement()
        mutableState.value = InstallServiceState(
            operationId = operationId,
            operationType = type,
            installing = true,
            statusMessage = initialStatus
        )

        val intent = Intent(context, InstallForegroundService::class.java).apply {
            putExtra(EXTRA_OPERATION_ID, operationId)
            putExtra(EXTRA_OPERATION_TYPE, type.name)
            extras()
        }
        ContextCompat.startForegroundService(context, intent)
        return operationId
    }
}
