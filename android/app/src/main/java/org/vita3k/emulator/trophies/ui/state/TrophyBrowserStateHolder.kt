package org.vita3k.emulator.trophies.ui.state

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.input.nestedscroll.NestedScrollConnection
import androidx.compose.ui.input.nestedscroll.NestedScrollSource
import androidx.compose.ui.unit.Velocity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import org.vita3k.emulator.trophies.ui.model.TrophyViewerPane
import kotlin.math.abs

private const val HeaderFadeDurationMillis = 180
private const val AppExitDurationMillis = 260
private const val CollectionsExitDurationMillis = 280
private const val AppRevealDurationMillis = 320
private const val DetailHideDurationMillis = 280
private const val DetailRevealDurationMillis = 320

internal data class TrophyOverscrollConfig(
    val max: Float,
    val dragRatio: Float,
    val resistance: Float,
    val stretchStrength: Float,
    val stretchDecay: Float
)

@Composable
internal fun rememberTrophyBrowserStateHolder(
    initialPane: TrophyViewerPane
): TrophyBrowserStateHolder {
    val coroutineScope = rememberCoroutineScope()
    return remember(coroutineScope) {
        TrophyBrowserStateHolder(
            initialPane = initialPane,
            coroutineScope = coroutineScope
        )
    }
}

@Composable
internal fun rememberTrophyOverscrollState(
    listState: LazyListState,
    config: TrophyOverscrollConfig
): TrophyOverscrollState {
    val coroutineScope = rememberCoroutineScope()
    return remember(listState, coroutineScope, config) {
        TrophyOverscrollState(
            listState = listState,
            coroutineScope = coroutineScope,
            config = config
        )
    }
}

@Stable
internal class TrophyBrowserStateHolder(
    initialPane: TrophyViewerPane,
    private val coroutineScope: CoroutineScope
) {
    private var navigationInFlight = false

    var renderedCollectionId by mutableStateOf(initialPane.collectionId)
        private set

    var renderedDetailTrophyId by mutableStateOf(initialPane.detailTrophyId)
        private set

    val collectionsExitProgress = Animatable(if (initialPane is TrophyViewerPane.App) 1f else 0f)
    val appRevealProgress = Animatable(if (initialPane is TrophyViewerPane.App) 1f else 0f)
    val headerLegendAlpha = Animatable(if (initialPane is TrophyViewerPane.App) 0f else 1f)
    val detailRevealProgress = Animatable(if (initialPane.detailTrophyId != null) 1f else 0f)

    fun tryStartNavigation(): Boolean {
        if (navigationInFlight) {
            return false
        }

        navigationInFlight = true
        return true
    }

    suspend fun syncToPane(pane: TrophyViewerPane) {
        when (pane) {
            TrophyViewerPane.Collections -> {
                if (renderedDetailTrophyId != null) {
                    detailRevealProgress.animateTo(
                        targetValue = 0f,
                        animationSpec = tween(durationMillis = DetailHideDurationMillis, easing = FastOutSlowInEasing)
                    )
                    renderedDetailTrophyId = null
                } else if (detailRevealProgress.value != 0f) {
                    detailRevealProgress.snapTo(0f)
                }

                if (renderedCollectionId != null) {
                    appRevealProgress.animateTo(
                        targetValue = 0f,
                        animationSpec = tween(durationMillis = AppExitDurationMillis, easing = FastOutSlowInEasing)
                    )
                    renderedCollectionId = null
                    launchAnimatedHeaderFade(targetValue = 1f)
                    collectionsExitProgress.animateTo(
                        targetValue = 0f,
                        animationSpec = tween(durationMillis = AppExitDurationMillis, easing = FastOutSlowInEasing)
                    )
                } else {
                    collectionsExitProgress.snapTo(0f)
                    appRevealProgress.snapTo(0f)
                    headerLegendAlpha.snapTo(1f)
                }
            }

            is TrophyViewerPane.App -> {
                if (renderedCollectionId == null) {
                    launchAnimatedHeaderFade(targetValue = 0f)
                    collectionsExitProgress.animateTo(
                        targetValue = 1f,
                        animationSpec = tween(durationMillis = CollectionsExitDurationMillis, easing = FastOutSlowInEasing)
                    )
                    renderedCollectionId = pane.npComId
                    appRevealProgress.snapTo(0f)
                    appRevealProgress.animateTo(
                        targetValue = 1f,
                        animationSpec = tween(durationMillis = AppRevealDurationMillis, easing = FastOutSlowInEasing)
                    )
                } else if (renderedCollectionId != pane.npComId) {
                    renderedCollectionId = pane.npComId
                    renderedDetailTrophyId = null
                    collectionsExitProgress.snapTo(1f)
                    appRevealProgress.snapTo(1f)
                    headerLegendAlpha.snapTo(0f)
                    detailRevealProgress.snapTo(0f)
                }

                when {
                    pane.detailTrophyId != null && renderedDetailTrophyId != pane.detailTrophyId -> {
                        renderedDetailTrophyId = pane.detailTrophyId
                        detailRevealProgress.snapTo(0f)
                        detailRevealProgress.animateTo(
                            targetValue = 1f,
                            animationSpec = tween(durationMillis = DetailRevealDurationMillis, easing = FastOutSlowInEasing)
                        )
                    }

                    pane.detailTrophyId == null && renderedDetailTrophyId != null -> {
                        detailRevealProgress.animateTo(
                            targetValue = 0f,
                            animationSpec = tween(durationMillis = DetailHideDurationMillis, easing = FastOutSlowInEasing)
                        )
                        renderedDetailTrophyId = null
                    }

                    pane.detailTrophyId == null && detailRevealProgress.value != 0f -> {
                        detailRevealProgress.snapTo(0f)
                    }
                }
            }
        }

        navigationInFlight = false
    }

    private fun launchAnimatedHeaderFade(targetValue: Float) {
        coroutineScope.launch {
            headerLegendAlpha.animateTo(
                targetValue = targetValue,
                animationSpec = tween(durationMillis = HeaderFadeDurationMillis, easing = FastOutSlowInEasing)
            )
        }
    }
}

@Stable
internal class TrophyOverscrollState(
    private val listState: LazyListState,
    private val coroutineScope: CoroutineScope,
    private val config: TrophyOverscrollConfig
) {
    private val offset = Animatable(0f)

    val value: Float
        get() = offset.value

    val nestedScrollConnection: NestedScrollConnection = object : NestedScrollConnection {
        override fun onPreScroll(available: Offset, source: NestedScrollSource): Offset {
            if (source != NestedScrollSource.UserInput) {
                return Offset.Zero
            }

            val current = offset.value
            val deltaY = available.y
            if (current == 0f) {
                return Offset.Zero
            }

            val isReturningToRest = (current > 0f && deltaY < 0f) || (current < 0f && deltaY > 0f)
            if (!isReturningToRest) {
                return Offset.Zero
            }

            val updated = moveTowardRest(current = current, delta = deltaY)
            coroutineScope.launch {
                offset.stop()
                offset.snapTo(updated)
            }
            return Offset(x = 0f, y = deltaY)
        }

        override fun onPostScroll(consumed: Offset, available: Offset, source: NestedScrollSource): Offset {
            if (source != NestedScrollSource.UserInput) {
                return Offset.Zero
            }

            val deltaY = available.y
            val pullingPastTop = deltaY > 0f && !listState.canScrollBackward
            val pullingPastBottom = deltaY < 0f && !listState.canScrollForward
            if (!pullingPastTop && !pullingPastBottom) {
                return Offset.Zero
            }

            val updated = damp(current = offset.value, delta = deltaY)
            coroutineScope.launch {
                offset.stop()
                offset.snapTo(updated)
            }
            return Offset.Zero
        }

        override suspend fun onPreFling(available: Velocity): Velocity {
            animateToRest()
            return Velocity.Zero
        }

        override suspend fun onPostFling(consumed: Velocity, available: Velocity): Velocity {
            animateToRest()
            return Velocity.Zero
        }
    }

    fun itemTranslationY(index: Int, lastIndex: Int): Float {
        val overscroll = offset.value
        if (overscroll == 0f) {
            return 0f
        }

        val distanceFromPulledEdge = if (overscroll > 0f) index.toFloat() else (lastIndex - index).toFloat()
        val stretchGain = 1f + (
            1f - 1f / (1f + distanceFromPulledEdge * config.stretchDecay)
            ) * config.stretchStrength
        return overscroll * stretchGain
    }

    private fun damp(current: Float, delta: Float): Float {
        val resistance = 1f + (abs(current) / config.max) / config.resistance
        val adjustedDelta = delta * config.dragRatio / resistance
        return (current + adjustedDelta).coerceIn(-config.max, config.max)
    }

    private fun moveTowardRest(current: Float, delta: Float): Float {
        val updated = current + delta
        return when {
            current > 0f && updated < 0f -> 0f
            current < 0f && updated > 0f -> 0f
            else -> updated
        }
    }

    private suspend fun animateToRest() {
        if (offset.value == 0f) {
            return
        }

        offset.animateTo(
            targetValue = 0f,
            animationSpec = spring(
                dampingRatio = Spring.DampingRatioMediumBouncy,
                stiffness = Spring.StiffnessLow
            )
        )
    }
}

private val TrophyViewerPane.collectionId: String?
    get() = when (this) {
        TrophyViewerPane.Collections -> null
        is TrophyViewerPane.App -> npComId
    }

private val TrophyViewerPane.detailTrophyId: Int?
    get() = when (this) {
        TrophyViewerPane.Collections -> null
        is TrophyViewerPane.App -> detailTrophyId
    }
