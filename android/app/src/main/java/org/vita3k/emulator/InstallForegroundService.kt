package org.vita3k.emulator

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import org.vita3k.emulator.data.InstallRepository
import org.vita3k.emulator.ui.viewmodel.DeleteSourceOption
import org.vita3k.emulator.ui.viewmodel.InstallResult
import org.vita3k.emulator.ui.viewmodel.InstallResultStatus
import java.io.File

class InstallForegroundService : Service() {
    private val serviceScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var wakeLock: PowerManager.WakeLock? = null
    private var activeOperationId: Long = 0L

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val operationId = intent?.getLongExtra(InstallServiceController.EXTRA_OPERATION_ID, 0L) ?: 0L
        val operationTypeName = intent?.getStringExtra(InstallServiceController.EXTRA_OPERATION_TYPE)
        val operationType = operationTypeName?.let(InstallOperationType::valueOf)
        if (operationId == 0L || operationType == null) {
            stopSelfResult(startId)
            return START_NOT_STICKY
        }

        if (activeOperationId != 0L && activeOperationId != operationId) {
            stopSelfResult(startId)
            return START_NOT_STICKY
        }

        activeOperationId = operationId
        ensureNotificationChannel()
        startForeground(
            NOTIFICATION_ID,
            buildNotification(
                InstallServiceController.state.value.statusMessage.ifBlank {
                    getString(R.string.install_progress_title)
                }
            )
        )
        acquireWakeLock()
        NativeLib.prepareFrontend()

        serviceScope.launch {
            val result = runOperation(operationId, operationType, intent)
            InstallServiceController.finish(operationId, result)
            updateNotification(result.message)
            releaseWakeLock()
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelfResult(startId)
            activeOperationId = 0L
        }

        return START_NOT_STICKY
    }

    override fun onDestroy() {
        releaseWakeLock()
        serviceScope.cancel()
        super.onDestroy()
    }

    private suspend fun runOperation(
        operationId: Long,
        operationType: InstallOperationType,
        intent: Intent
    ): InstallResult = try {
        when (operationType) {
            InstallOperationType.FIRMWARE -> installFirmware(
                operationId = operationId,
                path = intent.getStringExtra(InstallServiceController.EXTRA_PATH).orEmpty()
            )
            InstallOperationType.PKG -> installPkg(
                operationId = operationId,
                path = intent.getStringExtra(InstallServiceController.EXTRA_PATH).orEmpty(),
                zrif = intent.getStringExtra(InstallServiceController.EXTRA_ZRIF).orEmpty(),
                licensePath = intent.getStringExtra(InstallServiceController.EXTRA_LICENSE_PATH)
            )
            InstallOperationType.ARCHIVE -> installArchive(
                operationId = operationId,
                path = intent.getStringExtra(InstallServiceController.EXTRA_PATH).orEmpty()
            )
            InstallOperationType.ARCHIVE_FOLDER -> installArchiveFolder(
                operationId = operationId,
                paths = intent.getStringArrayListExtra(InstallServiceController.EXTRA_PATHS).orEmpty()
            )
            InstallOperationType.LICENSE_FILE -> installLicense(
                path = intent.getStringExtra(InstallServiceController.EXTRA_PATH).orEmpty()
            )
            InstallOperationType.LICENSE_ZRIF -> installLicenseFromZrif(
                zrif = intent.getStringExtra(InstallServiceController.EXTRA_ZRIF).orEmpty()
            )
        }
    } catch (e: Exception) {
        InstallResult(
            status = InstallResultStatus.ERROR,
            message = getString(R.string.install_error_generic, e.message ?: "")
        )
    }

    private suspend fun installFirmware(operationId: Long, path: String): InstallResult {
        val deleteOptions = sourceDeleteOptions(
            deleteOption(R.string.install_delete_firmware_file, path)
        )
        val version = InstallRepository.installFirmware(path) { pct, status ->
            onProgress(operationId, pct, status)
        }
        return if (version.isNotEmpty()) {
            InstallResult(
                status = InstallResultStatus.SUCCESS,
                message = getString(R.string.install_success_firmware, version),
                deleteOptions = deleteOptions
            )
        } else {
            InstallResult(
                status = InstallResultStatus.ERROR,
                message = getString(R.string.install_failed_firmware),
                deleteOptions = deleteOptions
            )
        }
    }

    private suspend fun installPkg(
        operationId: Long,
        path: String,
        zrif: String,
        licensePath: String?
    ): InstallResult {
        val deleteOptions = sourceDeleteOptions(
            deleteOption(R.string.install_delete_package_file, path),
            deleteOption(R.string.install_delete_license_file, licensePath)
        )
        val success = InstallRepository.installPkg(path, zrif) { pct, status ->
            onProgress(operationId, pct, status)
        }
        return if (success) {
            InstallResult(
                status = InstallResultStatus.SUCCESS,
                message = getString(R.string.install_success_package),
                deleteOptions = deleteOptions
            )
        } else {
            InstallResult(
                status = InstallResultStatus.ERROR,
                message = getString(R.string.install_failed_package),
                deleteOptions = deleteOptions
            )
        }
    }

    private suspend fun installArchive(operationId: Long, path: String): InstallResult {
        val deleteOptions = sourceDeleteOptions(
            deleteOption(R.string.install_delete_archive_files, path)
        )
        val success = InstallRepository.installArchive(path, forceReinstall = true) { pct, status ->
            onProgress(operationId, pct, status)
        }
        return if (success) {
            InstallResult(
                status = InstallResultStatus.SUCCESS,
                message = getString(R.string.install_success_archive),
                deleteOptions = deleteOptions
            )
        } else {
            InstallResult(
                status = InstallResultStatus.ERROR,
                message = getString(R.string.install_failed_archive),
                deleteOptions = deleteOptions
            )
        }
    }

    private suspend fun installArchiveFolder(operationId: Long, paths: List<String>): InstallResult {
        if (paths.isEmpty()) {
            return InstallResult(
                status = InstallResultStatus.ERROR,
                message = getString(R.string.install_error_archive_folder_empty)
            )
        }

        val deleteOptions = sourceDeleteOptions(
            deleteOption(R.string.install_delete_archive_files, paths)
        )
        val total = paths.size
        var successCount = 0

        paths.forEachIndexed { index, path ->
            val archiveName = File(path).name.ifBlank { path }
            val batchStatus = getString(
                R.string.install_status_archive_batch,
                index + 1,
                total,
                archiveName
            )
            InstallServiceController.updateProgress(operationId, ((index * 100f) / total).toInt(), batchStatus)
            val success = InstallRepository.installArchive(path, forceReinstall = true) { pct, _ ->
                val overall = ((index + (pct / 100f)) / total.toFloat()) * 100f
                onProgress(operationId, overall.toInt(), batchStatus)
            }
            if (success) {
                successCount++
            }
        }

        val failureCount = total - successCount
        return when {
            successCount == total -> InstallResult(
                status = InstallResultStatus.SUCCESS,
                message = getString(R.string.install_success_archive_batch, successCount),
                deleteOptions = deleteOptions
            )
            successCount > 0 -> InstallResult(
                status = InstallResultStatus.PARTIAL,
                message = getString(R.string.install_partial_archive_batch, successCount, failureCount),
                deleteOptions = deleteOptions
            )
            else -> InstallResult(
                status = InstallResultStatus.ERROR,
                message = getString(R.string.install_failed_archive_batch, total),
                deleteOptions = deleteOptions
            )
        }
    }

    private suspend fun installLicense(path: String): InstallResult {
        val deleteOptions = sourceDeleteOptions(
            deleteOption(R.string.install_delete_license_file, path)
        )
        val success = InstallRepository.copyLicense(path)
        return if (success) {
            InstallResult(
                status = InstallResultStatus.SUCCESS,
                message = getString(R.string.install_success_license),
                deleteOptions = deleteOptions
            )
        } else {
            InstallResult(
                status = InstallResultStatus.ERROR,
                message = getString(R.string.install_failed_license),
                deleteOptions = deleteOptions
            )
        }
    }

    private suspend fun installLicenseFromZrif(zrif: String): InstallResult {
        val success = InstallRepository.createLicense(zrif)
        return if (success) {
            InstallResult(
                status = InstallResultStatus.SUCCESS,
                message = getString(R.string.install_success_license)
            )
        } else {
            InstallResult(
                status = InstallResultStatus.ERROR,
                message = getString(R.string.install_failed_license_zrif)
            )
        }
    }

    private fun onProgress(operationId: Long, percent: Int, status: String) {
        InstallServiceController.updateProgress(operationId, percent, status)
        updateNotification(status)
    }

    private fun buildNotification(status: String) =
        NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
            .setSmallIcon(R.mipmap.ic_launcher)
            .setContentTitle(getString(R.string.install_progress_title))
            .setContentText(status)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setCategory(NotificationCompat.CATEGORY_PROGRESS)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setContentIntent(
                PendingIntent.getActivity(
                    this,
                    0,
                    Intent(this, MainActivity::class.java).apply {
                        flags = Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_CLEAR_TOP
                    },
                    PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
                )
            )
            .build()

    private fun updateNotification(status: String) {
        ensureNotificationChannel()
        val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        manager.notify(NOTIFICATION_ID, buildNotification(status))
    }

    private fun ensureNotificationChannel() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return
        }

        val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        val channel = NotificationChannel(
            NOTIFICATION_CHANNEL_ID,
            getString(R.string.install_notification_channel_name),
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = getString(R.string.install_notification_channel_desc)
        }
        manager.createNotificationChannel(channel)
    }

    private fun acquireWakeLock() {
        if (wakeLock?.isHeld == true) {
            return
        }

        val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = powerManager.newWakeLock(
            PowerManager.PARTIAL_WAKE_LOCK,
            "org.vita3k.emulator:install"
        ).apply {
            setReferenceCounted(false)
            acquire(WAKE_LOCK_TIMEOUT_MS)
        }
    }

    private fun releaseWakeLock() {
        wakeLock?.let { lock ->
            if (lock.isHeld) {
                lock.release()
            }
        }
        wakeLock = null
    }

    private fun deleteOption(labelResId: Int, path: String?): DeleteSourceOption? =
        deleteOption(labelResId, listOfNotNull(path))

    private fun deleteOption(labelResId: Int, paths: List<String>): DeleteSourceOption? {
        val normalized = paths
            .filter(String::isNotBlank)
            .distinct()
        if (normalized.isEmpty()) {
            return null
        }

        return DeleteSourceOption(
            label = getString(labelResId),
            paths = normalized
        )
    }

    private fun sourceDeleteOptions(vararg options: DeleteSourceOption?): List<DeleteSourceOption> =
        options.filterNotNull()

    companion object {
        private const val NOTIFICATION_CHANNEL_ID = "install_progress"
        private const val NOTIFICATION_ID = 0x317
        private const val WAKE_LOCK_TIMEOUT_MS = 6L * 60L * 60L * 1000L
    }
}
