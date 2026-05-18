package org.vita3k.emulator.trophies.domain.model

internal enum class TrophyGrade(val nativeValue: Int) {
    UNKNOWN(0),
    PLATINUM(1),
    GOLD(2),
    SILVER(3),
    BRONZE(4);

    companion object {
        fun fromNativeValue(value: Int): TrophyGrade =
            entries.find { it.nativeValue == value } ?: UNKNOWN
    }
}

internal enum class TrophyEarnedFilter {
    ALL,
    EARNED,
    NOT_EARNED
}

internal data class TrophyFilter(
    val earnedFilter: TrophyEarnedFilter = TrophyEarnedFilter.ALL,
    val showHiddenLocked: Boolean = false,
    val includedGrades: Set<TrophyGrade> = setOf(
        TrophyGrade.BRONZE,
        TrophyGrade.SILVER,
        TrophyGrade.GOLD,
        TrophyGrade.PLATINUM
    )
)

internal data class TrophyCollection(
    val npComId: String,
    val title: String,
    val iconPath: String,
    val artworkAspectRatio: Float,
    val totalCount: Int,
    val unlockedCount: Int,
    val platinumUnlockedCount: Int,
    val goldUnlockedCount: Int,
    val silverUnlockedCount: Int,
    val bronzeUnlockedCount: Int,
    val progressPercent: Int
)

internal data class TrophyItem(
    val npComId: String,
    val trophyId: Int,
    val name: String,
    val detail: String,
    val grade: TrophyGrade,
    val hidden: Boolean,
    val earned: Boolean,
    val timestampSec: Long,
    val iconPath: String
) {
    val isHiddenLocked: Boolean
        get() = hidden && !earned
}

internal data class TrophyCollectionSnapshot(
    val collection: TrophyCollection,
    val trophies: List<TrophyItem>
)
