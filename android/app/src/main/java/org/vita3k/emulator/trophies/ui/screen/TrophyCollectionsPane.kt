package org.vita3k.emulator.trophies.ui.screen

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.vita3k.emulator.R
import org.vita3k.emulator.trophies.ui.model.TrophyCollectionsScreenState
import org.vita3k.emulator.trophies.ui.state.TrophyOverscrollConfig
import org.vita3k.emulator.trophies.ui.state.rememberTrophyOverscrollState
import org.vita3k.emulator.trophies.ui.theme.TrophyDimensions

private const val TrophyBrowserCollectionsOverscrollDragRatio = 0.34f
private const val TrophyBrowserCollectionsOverscrollResistance = 0.012f
private const val TrophyBrowserCollectionsOverscrollStretchStrength = 1.3f
private const val TrophyBrowserCollectionsOverscrollStretchDecay = 0.7f

@Composable
internal fun TrophyCollectionsPane(
    state: TrophyCollectionsScreenState,
    exitProgress: Float,
    exitRotationY: Float,
    exitAlphaFloor: Float,
    onOpenCollection: (String) -> Unit
) {
    val listState = rememberLazyListState()
    val maxOverscrollPx = with(LocalDensity.current) { 96.dp.toPx() }
    val overscrollConfig = remember(maxOverscrollPx) {
        TrophyOverscrollConfig(
            max = maxOverscrollPx,
            dragRatio = TrophyBrowserCollectionsOverscrollDragRatio,
            resistance = TrophyBrowserCollectionsOverscrollResistance,
            stretchStrength = TrophyBrowserCollectionsOverscrollStretchStrength,
            stretchDecay = TrophyBrowserCollectionsOverscrollStretchDecay
        )
    }
    val overscrollState = rememberTrophyOverscrollState(
        listState = listState,
        config = overscrollConfig
    )

    Box(
        modifier = Modifier
            .fillMaxSize()
            .graphicsLayer {
                alpha = 1f - (1f - exitAlphaFloor) * exitProgress
                rotationY = exitRotationY * exitProgress
                transformOrigin = TransformOrigin(0f, 0.5f)
                cameraDistance = 18f * density
            }
    ) {
        when {
            state.loading && state.collections.isEmpty() -> {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(horizontal = TrophyDimensions.ScreenPadding),
                    contentAlignment = Alignment.Center
                ) {
                    CircularProgressIndicator()
                }
            }

            state.errorMessage != null && state.collections.isEmpty() -> {
                TrophyPaneMessage(
                    title = stringResource(R.string.trophy_error_title),
                    body = state.errorMessage
                )
            }

            state.collections.isEmpty() -> {
                TrophyPaneMessage(
                    title = stringResource(R.string.trophy_empty_title),
                    body = stringResource(R.string.trophy_empty_body)
                )
            }

            else -> {
                LazyColumn(
                    modifier = Modifier
                        .fillMaxSize()
                        .nestedScroll(overscrollState.nestedScrollConnection),
                    state = listState,
                    contentPadding = PaddingValues(bottom = 12.dp)
                ) {
                    itemsIndexed(state.collections, key = { _, item -> item.npComId }) { index, collection ->
                        TrophyCollectionRow(
                            modifier = Modifier.graphicsLayer {
                                translationY = overscrollState.itemTranslationY(
                                    index = index,
                                    lastIndex = state.collections.lastIndex
                                )
                            },
                            collection = collection,
                            onClick = { onOpenCollection(collection.npComId) }
                        )
                    }
                }
            }
        }
    }
}
