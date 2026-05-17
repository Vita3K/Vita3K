package org.vita3k.emulator.data

data class NativeAppInfo(
    val titleId: String,
    val title: String,
    val category: String,
    val appVer: String,
    val iconPath: String,
    val hasCustomConfig: Boolean,
    val compatibility: Int,
    val lastPlayed: Long,
    val playtime: Long
)
