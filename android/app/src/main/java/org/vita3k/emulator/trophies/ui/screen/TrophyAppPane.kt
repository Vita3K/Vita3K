package org.vita3k.emulator.trophies.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import org.vita3k.emulator.R
import org.vita3k.emulator.trophies.domain.model.TrophyEarnedFilter
import org.vita3k.emulator.trophies.domain.model.TrophyGrade
import org.vita3k.emulator.trophies.ui.model.TrophyAppScreenState
import org.vita3k.emulator.trophies.ui.state.TrophyOverscrollConfig
import org.vita3k.emulator.trophies.ui.state.rememberTrophyOverscrollState
import org.vita3k.emulator.trophies.ui.theme.TrophyColors
import org.vita3k.emulator.trophies.ui.theme.TrophyDimensions

private const val TrophyBrowserAppOverscrollDragRatio = 0.34f
private const val TrophyBrowserAppOverscrollResistance = 0.012f
private const val TrophyBrowserAppOverscrollStretchStrength = 1.3f
private const val TrophyBrowserAppOverscrollStretchDecay = 0.7f

@Composable
internal fun TrophyAppPane(
    state: TrophyAppScreenState,
    onEarnedFilterChanged: (TrophyEarnedFilter) -> Unit,
    onShowHiddenLockedChanged: (Boolean) -> Unit,
    onToggleGrade: (TrophyGrade) -> Unit,
    onOpenDetail: (Int) -> Unit
) {
    val listState = rememberLazyListState()
    var filtersExpanded by rememberSaveable { mutableStateOf(false) }
    val maxOverscrollPx = with(LocalDensity.current) { 96.dp.toPx() }
    val overscrollConfig = remember(maxOverscrollPx) {
        TrophyOverscrollConfig(
            max = maxOverscrollPx,
            dragRatio = TrophyBrowserAppOverscrollDragRatio,
            resistance = TrophyBrowserAppOverscrollResistance,
            stretchStrength = TrophyBrowserAppOverscrollStretchStrength,
            stretchDecay = TrophyBrowserAppOverscrollStretchDecay
        )
    }
    val overscrollState = rememberTrophyOverscrollState(
        listState = listState,
        config = overscrollConfig
    )

    Box(modifier = Modifier.fillMaxSize()) {
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .nestedScroll(overscrollState.nestedScrollConnection),
            state = listState,
            contentPadding = PaddingValues(bottom = 24.dp)
        ) {
            when {
                state.loading && state.collection == null -> {
                    item(key = "loading") {
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(
                                    start = TrophyDimensions.ScreenPadding,
                                    top = TrophyDimensions.SectionSpacing,
                                    end = TrophyDimensions.ScreenPadding
                                )
                                .heightIn(min = 280.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            TrophyGlassPanel(modifier = Modifier.fillMaxWidth()) {
                                Box(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .padding(vertical = 36.dp),
                                    contentAlignment = Alignment.Center
                                ) {
                                    CircularProgressIndicator()
                                }
                            }
                        }
                    }
                }

                state.errorMessage != null && state.collection == null -> {
                    item(key = "error") {
                        TrophyPaneMessage(
                            title = stringResource(R.string.trophy_error_title),
                            body = state.errorMessage
                        )
                    }
                }

                state.collection == null -> {
                    item(key = "not_found") {
                        TrophyPaneMessage(
                            title = stringResource(R.string.trophy_not_found_title),
                            body = stringResource(R.string.trophy_not_found_body)
                        )
                    }
                }

                else -> {
                    item(key = "collection") {
                        TrophyCollectionSummaryBand(
                            collection = state.collection,
                            modifier = Modifier.graphicsLayer {
                                translationY = overscrollState.itemTranslationY(
                                    index = 0,
                                    lastIndex = state.visibleTrophies.size
                                )
                            }
                        )
                    }

                    if (state.visibleTrophies.isEmpty()) {
                        item(key = "empty") {
                            Column(
                                modifier = Modifier.padding(
                                    start = TrophyDimensions.ScreenPadding,
                                    end = TrophyDimensions.ScreenPadding,
                                    top = TrophyDimensions.SectionSpacing
                                ),
                                verticalArrangement = Arrangement.spacedBy(6.dp)
                            ) {
                                Text(
                                    text = stringResource(R.string.trophy_filter_empty_title),
                                    color = TrophyColors.TextPrimary,
                                    fontWeight = FontWeight.SemiBold
                                )
                                Text(
                                    text = stringResource(R.string.trophy_filter_empty_body),
                                    color = TrophyColors.TextSecondary
                                )
                            }
                        }
                    } else {
                        itemsIndexed(state.visibleTrophies, key = { _, item -> item.trophyId }) { index, trophy ->
                            TrophyAchievementBand(
                                trophy = trophy,
                                onClick = { onOpenDetail(trophy.trophyId) },
                                modifier = Modifier.graphicsLayer {
                                    translationY = overscrollState.itemTranslationY(
                                        index = index + 1,
                                        lastIndex = state.visibleTrophies.size
                                    )
                                }
                            )
                        }
                    }
                }
            }
        }

        if (state.collection != null) {
            TrophyFilterMenu(
                earnedFilter = state.filter.earnedFilter,
                showHiddenLocked = state.filter.showHiddenLocked,
                includedGrades = state.filter.includedGrades,
                expanded = filtersExpanded,
                onExpandedChange = { filtersExpanded = it },
                onEarnedFilterChanged = onEarnedFilterChanged,
                onShowHiddenLockedChanged = onShowHiddenLockedChanged,
                onToggleGrade = onToggleGrade,
                modifier = Modifier
                    .align(Alignment.BottomEnd)
                    .padding(bottom = 4.dp)
            )
        }
    }
}
