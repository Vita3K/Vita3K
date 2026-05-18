package org.vita3k.emulator.trophies.ui.model

import org.vita3k.emulator.trophies.domain.model.TrophyCollection
import org.vita3k.emulator.trophies.domain.model.TrophyFilter
import org.vita3k.emulator.trophies.domain.model.TrophyItem

internal sealed interface TrophyViewerPane {
    data object Collections : TrophyViewerPane
    data class App(
        val npComId: String,
        val detailTrophyId: Int? = null
    ) : TrophyViewerPane
}

internal data class TrophyCollectionsScreenState(
    val loading: Boolean = false,
    val collections: List<TrophyCollection> = emptyList(),
    val errorMessage: String? = null
)

internal data class TrophyAppScreenState(
    val loading: Boolean = false,
    val collection: TrophyCollection? = null,
    val trophies: List<TrophyItem> = emptyList(),
    val visibleTrophies: List<TrophyItem> = emptyList(),
    val filter: TrophyFilter = TrophyFilter(),
    val errorMessage: String? = null
)

internal data class TrophyDetailScreenState(
    val loading: Boolean = false,
    val trophy: TrophyItem? = null,
    val errorMessage: String? = null
)

internal data class TrophyViewerUiState(
    val backgroundResolved: Boolean = false,
    val backgroundImagePath: String? = null,
    val pane: TrophyViewerPane = TrophyViewerPane.Collections,
    val collectionsState: TrophyCollectionsScreenState = TrophyCollectionsScreenState(),
    val appState: TrophyAppScreenState = TrophyAppScreenState(),
    val detailState: TrophyDetailScreenState = TrophyDetailScreenState()
)
