package org.vita3k.emulator.trophies.data.repository

import android.graphics.BitmapFactory
import android.net.Uri
import org.vita3k.emulator.data.NativeTrophy
import org.vita3k.emulator.data.NativeTrophyCollection
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.vita3k.emulator.NativeLib
import org.vita3k.emulator.trophies.domain.model.TrophyCollection
import org.vita3k.emulator.trophies.domain.model.TrophyCollectionSnapshot
import org.vita3k.emulator.trophies.domain.model.TrophyGrade
import org.vita3k.emulator.trophies.domain.model.TrophyItem
import java.io.File

private const val TROPHY_BACKGROUND_RELATIVE_PATH = "vs0/app/NPXS10008/sce_sys/pic0.png"
private const val TROPHY_COLLECTION_ARTWORK_FALLBACK_ASPECT_RATIO = 1.55f

internal class TrophyViewerRepository(
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO
) {
    suspend fun loadSnapshots(): List<TrophyCollectionSnapshot> = withContext(ioDispatcher) {
        NativeLib.getTrophyCollections()
            .orEmpty()
            .map(NativeTrophyCollection::toSnapshotModel)
            .sortedBy { it.collection.title.lowercase() }
    }

    suspend fun getBackgroundImagePath(): String? = withContext(ioDispatcher) {
        File(NativeLib.getCurrentEmulatorPath(), TROPHY_BACKGROUND_RELATIVE_PATH)
            .takeIf(File::exists)
            ?.absolutePath
    }
}

private fun NativeTrophyCollection.toCollectionModel(): TrophyCollection = TrophyCollection(
    npComId = npComId,
    title = title,
    iconPath = iconPath,
    artworkAspectRatio = resolveImageAspectRatio(iconPath) ?: TROPHY_COLLECTION_ARTWORK_FALLBACK_ASPECT_RATIO,
    totalCount = totalCount,
    unlockedCount = unlockedCount,
    platinumUnlockedCount = platinumUnlockedCount,
    goldUnlockedCount = goldUnlockedCount,
    silverUnlockedCount = silverUnlockedCount,
    bronzeUnlockedCount = bronzeUnlockedCount,
    progressPercent = progressPercent
)

private fun NativeTrophy.toModel(): TrophyItem = TrophyItem(
    npComId = npComId,
    trophyId = trophyId,
    name = name,
    detail = detail,
    grade = TrophyGrade.fromNativeValue(grade),
    hidden = hidden,
    earned = earned,
    timestampSec = timestampSec,
    iconPath = iconPath
)

private fun NativeTrophyCollection.toSnapshotModel(): TrophyCollectionSnapshot = TrophyCollectionSnapshot(
    collection = toCollectionModel(),
    trophies = trophies.map(NativeTrophy::toModel)
)

private fun resolveImageAspectRatio(path: String): Float? {
    val filePath = resolveLocalImagePath(path) ?: return null
    val bounds = BitmapFactory.Options().apply {
        inJustDecodeBounds = true
    }

    return runCatching {
        BitmapFactory.decodeFile(filePath, bounds)
        if (bounds.outWidth > 0 && bounds.outHeight > 0) {
            bounds.outWidth.toFloat() / bounds.outHeight.toFloat()
        } else {
            null
        }
    }.getOrNull()
}

private fun resolveLocalImagePath(path: String): String? {
    val trimmed = path.trim()
    if (trimmed.isEmpty()) {
        return null
    }

    return when (val uri = Uri.parse(trimmed)) {
        null -> null
        else -> when {
            uri.scheme.isNullOrBlank() -> trimmed
            uri.scheme.equals("file", ignoreCase = true) -> uri.path
            else -> null
        }
    }?.takeIf { it.isNotBlank() && File(it).exists() }
}
