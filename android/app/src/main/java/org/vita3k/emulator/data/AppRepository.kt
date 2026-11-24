package org.vita3k.emulator.data

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.withContext
import org.vita3k.emulator.NativeLib
import org.vita3k.emulator.ui.viewmodel.AppAction
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.util.UUID

internal object AppRepository {
    private const val TAG = "Vita3K"
    private const val COMPAT_VERSION_URL =
        "https://api.github.com/repos/Vita3K/compatibility/releases/latest"
    private const val COMPAT_DB_URL =
        "https://github.com/Vita3K/compatibility/releases/download/compat_db/app_compat_db.xml.zip"
    private val compatVersionRegex =
        Regex("""Last updated: (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}Z)""")

    suspend fun initialize(storagePath: String): Boolean = withContext(Dispatchers.IO) {
        NativeLib.init(storagePath)
    }

    suspend fun getAppVersion(): String = withContext(Dispatchers.IO) {
        NativeLib.getAppVersion()
    }

    suspend fun getFirmwareInstallState(): FirmwareInstallState = withContext(Dispatchers.IO) {
        FirmwareInstallState.fromMask(NativeLib.getFirmwareInstallStateMask())
    }

    suspend fun syncCompatibilityDatabase(): Boolean = withContext(Dispatchers.IO) {
        try {
            val currentVersion = NativeLib.getCompatibilityDatabaseVersion()
            val versionResponse = httpGetString(COMPAT_VERSION_URL) ?: return@withContext false
            val latestVersion = compatVersionRegex.find(versionResponse)?.groupValues?.getOrNull(1)
                ?: return@withContext false

            if (latestVersion == currentVersion) {
                return@withContext false
            }

            val zipData = httpGetBytes(COMPAT_DB_URL) ?: return@withContext false
            NativeLib.installCompatibilityDatabase(zipData, latestVersion)
        } catch (error: Exception) {
            Log.w(TAG, "Failed to sync compatibility database", error)
            false
        }
    }

    suspend fun getAppList(): List<AppInfo> = withContext(Dispatchers.IO) {
        NativeLib.getAppListDetailed().map(::toAppInfo)
    }

    suspend fun refreshAppsList() = withContext(Dispatchers.IO) {
        NativeLib.refreshAppsList()
    }

    suspend fun runAppAction(titleId: String, action: AppAction): Boolean =
        withContext(Dispatchers.IO) {
            NativeLib.performAppAction(titleId, action.maskBit)
        }

    suspend fun getAvailableAppActions(titleId: String): Set<AppAction> =
        withContext(Dispatchers.IO) {
            AppAction.fromMask(NativeLib.getAppActionAvailabilityMask(titleId))
        }

    suspend fun getAppInstallSize(titleId: String): Long = withContext(Dispatchers.IO) {
        NativeLib.getAppInstallSize(titleId)
    }

    suspend fun checkForUpdates(
        appVersion: String,
        officialBuild: Boolean
    ): UpdateCheckResult = withContext(Dispatchers.IO) {
        val response = httpGetString(NativeLib.getReleaseApiUrl(), validateReleaseApiUrl = true)
            ?: return@withContext UpdateCheckResult(
                status = UpdateCheckStatus.Failed,
                message = "Could not retrieve the latest Vita3K release information. Please try again later.",
                currentDisplayVersion = formatCurrentVersion(appVersion)
            )
        val fields = NativeLib.checkReleaseResponse(response, appVersion, officialBuild)
        val info = UpdateInfo(
            version = fields[2],
            buildNumber = fields[3].toLongOrNull() ?: 0L,
            releaseUrl = fields[4],
            androidApkUrl = fields[8],
            androidApkSize = fields[9].toLongOrNull() ?: 0L,
            publishedAt = fields[5],
            notes = fields[6]
        )
        UpdateCheckResult(
            status = UpdateCheckStatus.valueOf(fields[0]),
            message = fields[1],
            info = info,
            currentDisplayVersion = fields[7],
            failureReason = UpdateCheckFailureReason.valueOf(fields[10])
        )
    }

    suspend fun downloadAndroidUpdateApk(
        downloadUrl: String,
        expectedSize: Long,
        cacheDirectory: File,
        onProgress: suspend (downloadedBytes: Long, totalBytes: Long) -> Unit
    ): File {
        var renamedApkFile: File? = null
        try {
            val apkFile = withContext(Dispatchers.IO) {
                if (!NativeLib.isTrustedReleaseAssetUrl(downloadUrl)) {
                    throw IOException("The Android update link is not a trusted Vita3K release asset.")
                }
                if (expectedSize <= 0L || expectedSize > MAX_ANDROID_UPDATE_SIZE) {
                    throw IOException("The Android update package has an invalid size.")
                }

                val updateDirectory = File(cacheDirectory, "updates")
                if (!updateDirectory.isDirectory && !updateDirectory.mkdirs() && !updateDirectory.isDirectory) {
                    throw IOException("The update download folder could not be created.")
                }
                val updateId = UUID.randomUUID().toString()
                val partialFile = File(updateDirectory, "android-update-$updateId.apk.part")
                val apkFile = File(updateDirectory, "android-update-$updateId.apk")
                var connection: HttpURLConnection? = null

                try {
                    connection = openReleaseAssetConnection(downloadUrl)
                    var downloadedBytes = 0L
                    var lastProgressUpdate = 0L
                    val totalBytes = expectedSize
                    connection.inputStream.use { input ->
                        FileOutputStream(partialFile).use { output ->
                            val buffer = ByteArray(64 * 1024)
                            while (true) {
                                val count = input.read(buffer)
                                if (count < 0) break
                                if (count == 0) continue
                                downloadedBytes += count
                                if (downloadedBytes > expectedSize || downloadedBytes > MAX_ANDROID_UPDATE_SIZE) {
                                    throw IOException("The downloaded Android update is larger than expected.")
                                }
                                output.write(buffer, 0, count)
                                val now = System.currentTimeMillis()
                                if (now - lastProgressUpdate >= PROGRESS_UPDATE_INTERVAL_MS || downloadedBytes == expectedSize) {
                                    withContext(Dispatchers.Main.immediate) {
                                        onProgress(downloadedBytes, totalBytes)
                                    }
                                    lastProgressUpdate = now
                                }
                            }
                            output.fd.sync()
                        }
                    }

                    if (downloadedBytes != expectedSize || !partialFile.isFile || partialFile.length() != expectedSize) {
                        throw IOException("The Android update download was incomplete.")
                    }
                    if (!partialFile.inputStream().use { input -> input.read() == 'P'.code && input.read() == 'K'.code }) {
                        throw IOException("The downloaded file is not a valid Android package.")
                    }
                    if (!partialFile.renameTo(apkFile)) {
                        throw IOException("The downloaded Android update could not be prepared for installation.")
                    }
                    renamedApkFile = apkFile
                    withContext(Dispatchers.Main.immediate) {
                        onProgress(expectedSize, totalBytes)
                    }
                    apkFile
                } catch (error: Exception) {
                    partialFile.delete()
                    apkFile.delete()
                    throw error
                } finally {
                    connection?.disconnect()
                }
            }
            renamedApkFile = null
            apkFile
        } catch (error: Exception) {
            renamedApkFile?.delete()
            throw error
        }
    }

    private suspend fun openReleaseAssetConnection(downloadUrl: String): HttpURLConnection {
        var currentUrl = URL(downloadUrl)
        repeat(MAX_RELEASE_REDIRECTS + 1) { redirectCount ->
            currentCoroutineContext().ensureActive()
            val host = currentUrl.host.lowercase()
            val trustedHost = host == "github.com" || host == "release-assets.githubusercontent.com" || host == "objects.githubusercontent.com"
            if (currentUrl.protocol != "https" || !trustedHost ||
                (host == "github.com" && !NativeLib.isTrustedReleaseAssetUrl(currentUrl.toString()))
            ) {
                throw IOException("The Android update download redirected to an untrusted location.")
            }

            val connection = (currentUrl.openConnection() as HttpURLConnection).apply {
                requestMethod = "GET"
                instanceFollowRedirects = false
                connectTimeout = HTTP_TIMEOUT_MS
                readTimeout = HTTP_TIMEOUT_MS
                setRequestProperty("User-Agent", "Vita3K-Android")
                setRequestProperty("Accept", "application/vnd.android.package-archive, application/octet-stream")
            }
            var transferOwnership = false
            try {
                val status = connection.responseCode
                if (status in 300..399) {
                    val location = connection.getHeaderField("Location")
                    if (redirectCount == MAX_RELEASE_REDIRECTS || location.isNullOrBlank()) {
                        throw IOException("The Android update download could not be reached.")
                    }
                    currentUrl = URL(currentUrl, location)
                    return@repeat
                }
                if (status !in 200..299) {
                    throw IOException("The Android update server returned HTTP $status.")
                }
                transferOwnership = true
                return connection
            } finally {
                if (!transferOwnership) {
                    connection.disconnect()
                }
            }
        }
        throw IOException("The Android update download redirected too many times.")
    }

    private fun toAppInfo(native: NativeAppInfo): AppInfo = AppInfo(
        titleId = native.titleId,
        title = native.title,
        category = native.category,
        appVer = native.appVer,
        iconPath = native.iconPath.ifEmpty { null },
        hasCustomConfig = native.hasCustomConfig,
        compatibility = CompatibilityState.fromValue(native.compatibility),
        lastPlayed = native.lastPlayed,
        playtime = native.playtime
    )

    private fun formatCurrentVersion(appVersion: String): String {
        if (appVersion.isBlank()) return ""
        val parts = appVersion.split("-", limit = 3)
        val version = parts.getOrNull(0).orEmpty().ifBlank { appVersion }
        val build = parts.getOrNull(1).orEmpty()
        return if (build.isBlank()) version else "$version ($build)"
    }

    private fun httpGetString(url: String, validateReleaseApiUrl: Boolean = false): String? =
        httpGetBytes(url, validateReleaseApiUrl)?.toString(Charsets.UTF_8)

    private fun httpGetBytes(url: String, validateReleaseApiUrl: Boolean = false): ByteArray? {
        val connection = (URL(url).openConnection() as HttpURLConnection).apply {
            requestMethod = "GET"
            // The update metadata endpoint must remain on GitHub's API host.
            instanceFollowRedirects = !validateReleaseApiUrl
            connectTimeout = 10000
            readTimeout = 15000
            setRequestProperty("User-Agent", "Vita3K-Android")
            setRequestProperty("Accept", "application/vnd.github+json")
        }

        return try {
            connection.connect()
            if (validateReleaseApiUrl && !NativeLib.isTrustedReleaseApiUrl(connection.url.toString())) {
                return null
            }
            if (connection.responseCode !in 200..299) {
                null
            } else {
                connection.inputStream.use { it.readBytes() }
            }
        } finally {
            connection.disconnect()
        }
    }

    private const val MAX_RELEASE_REDIRECTS = 5
    private const val HTTP_TIMEOUT_MS = 15000
    private const val PROGRESS_UPDATE_INTERVAL_MS = 150L
}

internal const val MAX_ANDROID_UPDATE_SIZE = 150L * 1024L * 1024L
