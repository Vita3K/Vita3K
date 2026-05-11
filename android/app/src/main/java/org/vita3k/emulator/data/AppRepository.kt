package org.vita3k.emulator.data

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.vita3k.emulator.NativeLib
import org.vita3k.emulator.ui.viewmodel.AppAction
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

internal object AppRepository {
    private const val TAG = "Vita3K"
    private const val COMPAT_VERSION_URL =
        "https://api.github.com/repos/Vita3K/compatibility/releases/latest"
    private const val COMPAT_DB_URL =
        "https://github.com/Vita3K/compatibility/releases/download/compat_db/app_compat_db.xml.zip"
    private const val UPDATE_RELEASE_URL =
        "https://api.github.com/repos/Vita3K/Vita3K/releases/tags/continuous"
    private const val UPDATE_PAGE_URL =
        "https://github.com/Vita3K/Vita3K/releases/tag/continuous"
    private val compatVersionRegex =
        Regex("""Last updated: (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}Z)""")
    private val updateBuildRegex = Regex("""Vita3K Build:\s*(\d+)""")
    private val currentBuildRegex = Regex("""^[^-]+-(\d+)(?:-|$)""")
    private val updateMetadataLineRegex =
        Regex("""^\s*(Corresponding commit:|Vita3K Build:)""", RegexOption.IGNORE_CASE)

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
        val currentDisplayVersion = currentDisplayVersion(appVersion)
        val response = httpGetString(UPDATE_RELEASE_URL)
            ?: return@withContext UpdateCheckResult(
                status = UpdateCheckStatus.Failed,
                message = "Could not retrieve the latest Vita3K release information. Please try again later.",
                currentDisplayVersion = currentDisplayVersion
            )

        val root = try {
            JSONObject(response)
        } catch (_: Exception) {
            return@withContext UpdateCheckResult(
                status = UpdateCheckStatus.Failed,
                message = "The GitHub release response could not be parsed.",
                currentDisplayVersion = currentDisplayVersion
            )
        }

        val body = root.optString("body").orEmpty()
        val latestBuildNumber = updateBuildRegex.find(body)?.groupValues?.getOrNull(1)?.toLongOrNull()
            ?: return@withContext UpdateCheckResult(
                status = UpdateCheckStatus.Failed,
                message = "The latest release does not advertise a usable Vita3K build number.",
                currentDisplayVersion = currentDisplayVersion
            )

        val releaseName = root.optString("name").trim()
        val tagName = root.optString("tag_name").trim()
        val releaseUrl = root.optString("html_url").trim().ifBlank { UPDATE_PAGE_URL }
        val info = UpdateInfo(
            version = releaseName.ifBlank { tagName },
            buildNumber = latestBuildNumber,
            releaseUrl = releaseUrl,
            publishedAt = root.optString("published_at").trim(),
            notes = normalizedUpdateNotes(body)
        )

        val currentBuildNumber = currentBuildRegex.find(appVersion)?.groupValues?.getOrNull(1)?.toLongOrNull() ?: 0L
        val buildDelta = latestBuildNumber - currentBuildNumber

        when {
            !officialBuild -> UpdateCheckResult(
                status = UpdateCheckStatus.CustomBuildCanUpdate,
                message = "",
                info = info,
                currentDisplayVersion = currentDisplayVersion
            )
            buildDelta > 0L -> UpdateCheckResult(
                status = UpdateCheckStatus.UpdateAvailable,
                message = "",
                info = info,
                currentDisplayVersion = currentDisplayVersion
            )
            buildDelta < 0L -> UpdateCheckResult(
                status = UpdateCheckStatus.CurrentBuildNewerThanLatest,
                message = "This installation appears newer than the latest official continuous build.",
                info = info,
                currentDisplayVersion = currentDisplayVersion
            )
            else -> UpdateCheckResult(
                status = UpdateCheckStatus.UpToDate,
                message = "You already have the latest official build installed.",
                info = info,
                currentDisplayVersion = currentDisplayVersion
            )
        }
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

    private fun currentDisplayVersion(appVersion: String): String {
        if (appVersion.isBlank()) {
            return ""
        }

        val parts = appVersion.split("-", limit = 3)
        val version = parts.getOrNull(0).orEmpty().ifBlank { appVersion }
        val build = parts.getOrNull(1).orEmpty()
        return if (build.isBlank()) version else "$version ($build)"
    }

    private fun normalizedUpdateNotes(body: String): String {
        val cleaned = body
            .replace("\r\n", "\n")
            .lineSequence()
            .filterNot { line -> updateMetadataLineRegex.containsMatchIn(line.trim()) }
            .dropWhile { it.isBlank() }
            .toList()
            .dropLastWhile { it.isBlank() }

        return cleaned.joinToString("\n").trim()
    }

    private fun httpGetString(url: String): String? =
        httpGetBytes(url)?.toString(Charsets.UTF_8)

    private fun httpGetBytes(url: String): ByteArray? {
        val connection = (URL(url).openConnection() as HttpURLConnection).apply {
            requestMethod = "GET"
            instanceFollowRedirects = true
            connectTimeout = 10000
            readTimeout = 15000
            setRequestProperty("User-Agent", "Vita3K-Android")
            setRequestProperty("Accept", "application/vnd.github+json")
        }

        return try {
            connection.connect()
            if (connection.responseCode !in 200..299) {
                null
            } else {
                connection.inputStream.use { it.readBytes() }
            }
        } finally {
            connection.disconnect()
        }
    }
}
