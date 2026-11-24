package org.vita3k.emulator.data

enum class UpdateCheckStatus {
    Failed,
    UpToDate,
    UpdateAvailable,
    CurrentBuildNewerThanLatest,
    CustomBuildCanUpdate
}

enum class UpdateCheckFailureReason {
    None,
    InvalidReleaseResponse,
    InvalidBuildMetadata
}

data class UpdateInfo(
    // Human-readable release name or tag.
    val version: String = "",
    // Numeric build identifier parsed from the release metadata.
    val buildNumber: Long = 0L,
    // Browser URL for the release page.
    val releaseUrl: String = "",
    // Direct Android package from the official continuous release, if present.
    val androidApkUrl: String = "",
    val androidApkSize: Long = 0L,
    // GitHub publication timestamp in ISO-8601 format.
    val publishedAt: String = "",
    // Release notes with updater metadata lines removed.
    val notes: String = ""
)

data class UpdateCheckResult(
    val status: UpdateCheckStatus,
    val message: String,
    // Parsed metadata for the latest available release.
    val info: UpdateInfo = UpdateInfo(),
    // Display version string for the installed build.
    val currentDisplayVersion: String = "",
    val failureReason: UpdateCheckFailureReason = UpdateCheckFailureReason.None
)
