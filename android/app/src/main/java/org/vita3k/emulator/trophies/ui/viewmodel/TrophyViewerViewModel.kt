package org.vita3k.emulator.trophies.ui.viewmodel

import androidx.lifecycle.SavedStateHandle
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.createSavedStateHandle
import androidx.lifecycle.viewModelScope
import androidx.lifecycle.viewmodel.initializer
import androidx.lifecycle.viewmodel.viewModelFactory
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import org.vita3k.emulator.trophies.data.repository.TrophyViewerRepository
import org.vita3k.emulator.trophies.domain.model.TrophyCollection
import org.vita3k.emulator.trophies.domain.model.TrophyCollectionSnapshot
import org.vita3k.emulator.trophies.domain.model.TrophyEarnedFilter
import org.vita3k.emulator.trophies.domain.model.TrophyFilter
import org.vita3k.emulator.trophies.domain.model.TrophyGrade
import org.vita3k.emulator.trophies.domain.model.TrophyItem
import org.vita3k.emulator.trophies.ui.model.TrophyAppScreenState
import org.vita3k.emulator.trophies.ui.model.TrophyCollectionsScreenState
import org.vita3k.emulator.trophies.ui.model.TrophyDetailScreenState
import org.vita3k.emulator.trophies.ui.model.TrophyViewerPane
import org.vita3k.emulator.trophies.ui.model.TrophyViewerUiState

internal class TrophyViewerViewModel(
    private val savedStateHandle: SavedStateHandle,
    private val repository: TrophyViewerRepository = TrophyViewerRepository()
) : ViewModel() {
    private val mutableUiState = MutableStateFlow(restoreInitialUiState())
    val uiState: StateFlow<TrophyViewerUiState> = mutableUiState.asStateFlow()

    private var snapshotsByCollection: Map<String, TrophyCollectionSnapshot> = emptyMap()
    private var collections: List<TrophyCollection> = emptyList()

    init {
        loadBackgroundImagePath()
        loadViewerData()
    }

    fun openCollection(npComId: String) {
        val normalizedNpComId = npComId.trim()
        if (normalizedNpComId.isEmpty()) {
            return
        }

        val filter = mutableUiState.value.appState.filter
        val pane = TrophyViewerPane.App(npComId = normalizedNpComId)
        persistPane(pane)
        mutableUiState.update { current ->
            current.copy(
                pane = pane,
                appState = emptyAppState(filter = filter),
                detailState = TrophyDetailScreenState()
            )
        }
        syncCurrentPane()
    }

    fun closeCollection() {
        persistPane(TrophyViewerPane.Collections)
        mutableUiState.update { current ->
            current.copy(
                pane = TrophyViewerPane.Collections,
                collectionsState = current.collectionsState.copy(errorMessage = null),
                detailState = TrophyDetailScreenState()
            )
        }
    }

    fun openDetail(trophyId: Int) {
        val pane = mutableUiState.value.pane as? TrophyViewerPane.App ?: return
        if (pane.detailTrophyId == trophyId) {
            return
        }

        val detailPane = pane.copy(detailTrophyId = trophyId)
        persistPane(detailPane)
        mutableUiState.update { current ->
            current.copy(
                pane = detailPane,
                detailState = current.detailState.copy(
                    loading = false,
                    trophy = current.appState.trophies.firstOrNull { it.trophyId == trophyId },
                    errorMessage = null
                )
            )
        }
        syncCurrentPane()
    }

    fun closeDetail() {
        val pane = mutableUiState.value.pane as? TrophyViewerPane.App ?: return
        if (pane.detailTrophyId == null) {
            return
        }

        val appPane = pane.copy(detailTrophyId = null)
        persistPane(appPane)
        mutableUiState.update { current ->
            current.copy(
                pane = appPane
            )
        }
    }

    fun setEarnedFilter(earnedFilter: TrophyEarnedFilter) {
        updateFilter(mutableUiState.value.appState.filter.copy(earnedFilter = earnedFilter))
    }

    fun setShowHiddenLocked(showHiddenLocked: Boolean) {
        updateFilter(mutableUiState.value.appState.filter.copy(showHiddenLocked = showHiddenLocked))
    }

    fun toggleGrade(grade: TrophyGrade) {
        val currentFilter = mutableUiState.value.appState.filter
        val includedGrades = currentFilter.includedGrades.toMutableSet()
        if (!includedGrades.add(grade)) {
            includedGrades.remove(grade)
        }

        val nextGrades = if (includedGrades.isEmpty()) {
            currentFilter.includedGrades
        } else {
            includedGrades.toSet()
        }

        updateFilter(currentFilter.copy(includedGrades = nextGrades))
    }

    private fun updateFilter(filter: TrophyFilter) {
        persistFilter(filter)
        mutableUiState.update { current ->
            current.copy(
                appState = current.appState.copy(
                    filter = filter,
                    visibleTrophies = applyFilter(current.appState.trophies, filter)
                )
            )
        }
    }

    private fun loadBackgroundImagePath() {
        viewModelScope.launch {
            val backgroundImagePath = repository.getBackgroundImagePath()
            mutableUiState.update { current ->
                current.copy(
                    backgroundResolved = true,
                    backgroundImagePath = backgroundImagePath
                )
            }
        }
    }

    private fun loadViewerData() {
        viewModelScope.launch {
            try {
                val snapshots = repository.loadSnapshots()
                snapshotsByCollection = snapshots.associateBy { it.collection.npComId }
                collections = snapshots.map(TrophyCollectionSnapshot::collection)
                syncCurrentPane()
            } catch (error: Exception) {
                handleLoadError(error)
            }
        }
    }

    private fun syncCurrentPane() {
        mutableUiState.update { current ->
            val nextCollectionsState = current.collectionsState.copy(
                loading = false,
                collections = collections,
                errorMessage = null
            )

            when (val pane = current.pane) {
                TrophyViewerPane.Collections -> current.copy(
                    collectionsState = nextCollectionsState,
                    detailState = TrophyDetailScreenState()
                )

                is TrophyViewerPane.App -> {
                    val snapshot = snapshotsByCollection[pane.npComId]
                    val filter = current.appState.filter
                    val nextAppState = snapshot?.let {
                        current.appState.copy(
                            loading = false,
                            collection = it.collection,
                            trophies = it.trophies,
                            visibleTrophies = applyFilter(it.trophies, filter),
                            errorMessage = null
                        )
                    } ?: emptyAppState(filter = filter)

                    val nextDetailState = if (pane.detailTrophyId != null) {
                        current.detailState.copy(
                            loading = false,
                            trophy = snapshot?.trophies?.firstOrNull { it.trophyId == pane.detailTrophyId },
                            errorMessage = null
                        )
                    } else {
                        TrophyDetailScreenState()
                    }

                    current.copy(
                        collectionsState = nextCollectionsState,
                        appState = nextAppState,
                        detailState = nextDetailState
                    )
                }
            }
        }
    }

    private fun handleLoadError(error: Exception) {
        val message = error.message ?: "Failed to load trophies."
        mutableUiState.update { current ->
            when (val pane = current.pane) {
                TrophyViewerPane.Collections -> current.copy(
                    collectionsState = current.collectionsState.copy(
                        loading = false,
                        errorMessage = message
                    )
                )

                is TrophyViewerPane.App -> {
                    val nextCollectionsState = current.collectionsState.copy(loading = false)
                    if (pane.detailTrophyId != null) {
                        current.copy(
                            collectionsState = nextCollectionsState,
                            appState = current.appState.copy(loading = false),
                            detailState = current.detailState.copy(
                                loading = false,
                                errorMessage = error.message ?: "Failed to load trophy detail."
                            )
                        )
                    } else {
                        current.copy(
                            collectionsState = nextCollectionsState,
                            appState = current.appState.copy(
                                loading = false,
                                errorMessage = message
                            )
                        )
                    }
                }
            }
        }
    }

    private fun restoreInitialUiState(): TrophyViewerUiState {
        val filter = restoreFilter()
        val pane = restorePane()
        return when (pane) {
            TrophyViewerPane.Collections -> TrophyViewerUiState(
                pane = pane,
                collectionsState = TrophyCollectionsScreenState(loading = true),
                appState = emptyAppState(filter = filter),
                detailState = TrophyDetailScreenState()
            )

            is TrophyViewerPane.App -> TrophyViewerUiState(
                pane = pane,
                collectionsState = TrophyCollectionsScreenState(),
                appState = emptyAppState(filter = filter, loading = true),
                detailState = TrophyDetailScreenState(loading = pane.detailTrophyId != null)
            )
        }
    }

    private fun restorePane(): TrophyViewerPane {
        val savedPane = savedStateHandle.get<String>(KEY_ACTIVE_PANE)
        val npComId = savedStateHandle.get<String>(KEY_ACTIVE_COLLECTION_ID)
            ?.trim()
            ?.takeIf { it.isNotEmpty() }
        val detailTrophyId = savedStateHandle.get<Int>(KEY_ACTIVE_DETAIL_TROPHY_ID)

        return if (savedPane == SAVED_PANE_APP && npComId != null) {
            TrophyViewerPane.App(npComId = npComId, detailTrophyId = detailTrophyId)
        } else {
            TrophyViewerPane.Collections
        }
    }

    private fun restoreFilter(): TrophyFilter {
        val earnedFilter = savedStateHandle.get<String>(KEY_FILTER_EARNED)
            ?.let(::trophyEarnedFilterFromName)
            ?: TrophyFilter().earnedFilter
        val showHiddenLocked = savedStateHandle.get<Boolean>(KEY_FILTER_SHOW_HIDDEN_LOCKED) ?: false
        val includedGrades = savedStateHandle.get<ArrayList<String>>(KEY_FILTER_INCLUDED_GRADES)
            ?.mapNotNull(::trophyGradeFromName)
            ?.toSet()
            ?.takeIf { it.isNotEmpty() }
            ?: TrophyFilter().includedGrades

        return TrophyFilter(
            earnedFilter = earnedFilter,
            showHiddenLocked = showHiddenLocked,
            includedGrades = includedGrades
        )
    }

    private fun persistPane(pane: TrophyViewerPane) {
        when (pane) {
            TrophyViewerPane.Collections -> {
                savedStateHandle[KEY_ACTIVE_PANE] = SAVED_PANE_COLLECTIONS
                savedStateHandle.remove<String>(KEY_ACTIVE_COLLECTION_ID)
                savedStateHandle.remove<Int>(KEY_ACTIVE_DETAIL_TROPHY_ID)
            }

            is TrophyViewerPane.App -> {
                savedStateHandle[KEY_ACTIVE_PANE] = SAVED_PANE_APP
                savedStateHandle[KEY_ACTIVE_COLLECTION_ID] = pane.npComId
                savedStateHandle[KEY_ACTIVE_DETAIL_TROPHY_ID] = pane.detailTrophyId
            }
        }
    }

    private fun persistFilter(filter: TrophyFilter) {
        savedStateHandle[KEY_FILTER_EARNED] = filter.earnedFilter.name
        savedStateHandle[KEY_FILTER_SHOW_HIDDEN_LOCKED] = filter.showHiddenLocked
        savedStateHandle[KEY_FILTER_INCLUDED_GRADES] = ArrayList(filter.includedGrades.map(TrophyGrade::name))
    }

    private fun emptyAppState(
        filter: TrophyFilter,
        loading: Boolean = false
    ): TrophyAppScreenState = TrophyAppScreenState(
        loading = loading,
        filter = filter
    )

    private fun applyFilter(
        trophies: List<TrophyItem>,
        filter: TrophyFilter
    ): List<TrophyItem> {
        return trophies.filter { trophy ->
            if (trophy.grade !in filter.includedGrades) {
                return@filter false
            }
            if (!filter.showHiddenLocked && trophy.isHiddenLocked) {
                return@filter false
            }
            when (filter.earnedFilter) {
                TrophyEarnedFilter.ALL -> true
                TrophyEarnedFilter.EARNED -> trophy.earned
                TrophyEarnedFilter.NOT_EARNED -> !trophy.earned
            }
        }
    }

    companion object {
        private const val KEY_ACTIVE_PANE = "trophy_active_pane"
        private const val KEY_ACTIVE_COLLECTION_ID = "trophy_active_collection_id"
        private const val KEY_ACTIVE_DETAIL_TROPHY_ID = "trophy_active_detail_trophy_id"
        private const val KEY_FILTER_EARNED = "trophy_filter_earned"
        private const val KEY_FILTER_SHOW_HIDDEN_LOCKED = "trophy_filter_show_hidden_locked"
        private const val KEY_FILTER_INCLUDED_GRADES = "trophy_filter_included_grades"

        private const val SAVED_PANE_COLLECTIONS = "collections"
        private const val SAVED_PANE_APP = "app"

        fun factory(): ViewModelProvider.Factory = viewModelFactory {
            initializer {
                TrophyViewerViewModel(
                    savedStateHandle = createSavedStateHandle()
                )
            }
        }
    }
}

private fun trophyEarnedFilterFromName(value: String): TrophyEarnedFilter? =
    runCatching { enumValueOf<TrophyEarnedFilter>(value) }.getOrNull()

private fun trophyGradeFromName(value: String): TrophyGrade? =
    runCatching { enumValueOf<TrophyGrade>(value) }.getOrNull()
