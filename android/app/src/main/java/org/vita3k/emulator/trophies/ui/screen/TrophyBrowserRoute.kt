package org.vita3k.emulator.trophies.ui.screen

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import org.vita3k.emulator.R
import org.vita3k.emulator.trophies.ui.model.TrophyViewerPane
import org.vita3k.emulator.trophies.ui.state.rememberTrophyBrowserStateHolder
import org.vita3k.emulator.trophies.ui.theme.TrophyColors
import org.vita3k.emulator.trophies.ui.theme.TrophyDimensions
import org.vita3k.emulator.trophies.ui.viewmodel.TrophyViewerViewModel

private const val TrophyBrowserCollectionsExitRotationY = 92f
private const val TrophyBrowserCollectionsExitAlphaFloor = 0.24f
private const val TrophyBrowserAppExitRotationY = 92f
private const val TrophyBrowserAppExitAlphaFloor = 0.24f
private const val TrophyBrowserDetailFadeStartFraction = 0.68f

@Composable
internal fun TrophyBrowserRoute(
    viewModel: TrophyViewerViewModel,
    onExit: () -> Unit,
    modifier: Modifier = Modifier
) {
    val uiState by viewModel.uiState.collectAsStateWithLifecycle()
    val browserStateHolder = rememberTrophyBrowserStateHolder(initialPane = uiState.pane)

    LaunchedEffect(uiState.pane) {
        browserStateHolder.syncToPane(uiState.pane)
    }

    fun openCollection(npComId: String) {
        if (!browserStateHolder.tryStartNavigation()) {
            return
        }
        viewModel.openCollection(npComId)
    }

    fun openDetail(trophyId: Int) {
        val pane = uiState.pane as? TrophyViewerPane.App ?: return
        if (pane.detailTrophyId != null || !browserStateHolder.tryStartNavigation()) {
            return
        }
        viewModel.openDetail(trophyId)
    }

    fun backFromDetail() {
        val pane = uiState.pane as? TrophyViewerPane.App ?: return
        if (pane.detailTrophyId == null || !browserStateHolder.tryStartNavigation()) {
            return
        }
        viewModel.closeDetail()
    }

    fun backFromApp() {
        val pane = uiState.pane as? TrophyViewerPane.App ?: return
        if (!browserStateHolder.tryStartNavigation()) {
            return
        }
        viewModel.closeCollection()
    }

    BackHandler {
        when (val pane = uiState.pane) {
            TrophyViewerPane.Collections -> onExit()
            is TrophyViewerPane.App -> {
                if (pane.detailTrophyId != null) {
                    backFromDetail()
                } else {
                    backFromApp()
                }
            }
        }
    }

    TrophyBackground(
        backgroundResolved = uiState.backgroundResolved,
        backgroundImagePath = uiState.backgroundImagePath,
        modifier = modifier
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            TrophyBrowserHeader(
                pane = uiState.pane,
                headerLegendAlpha = browserStateHolder.headerLegendAlpha.value,
                onExit = onExit,
                onBackFromApp = ::backFromApp,
                onBackFromDetail = ::backFromDetail
            )

            BoxWithConstraints(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .clipToBounds()
            ) {
                if (browserStateHolder.renderedCollectionId == null) {
                    TrophyCollectionsPane(
                        state = uiState.collectionsState,
                        exitProgress = browserStateHolder.collectionsExitProgress.value,
                        exitRotationY = TrophyBrowserCollectionsExitRotationY,
                        exitAlphaFloor = TrophyBrowserCollectionsExitAlphaFloor,
                        onOpenCollection = ::openCollection
                    )
                } else {
                    val contentWidthPx = with(LocalDensity.current) { maxWidth.toPx() }
                    val activeDetailTrophyId = browserStateHolder.renderedDetailTrophyId

                    Box(modifier = Modifier.fillMaxSize()) {
                        Box(
                            modifier = Modifier
                                .fillMaxSize()
                                .graphicsLayer {
                                    translationX = contentWidthPx * (1f - browserStateHolder.appRevealProgress.value)
                                }
                        ) {
                            Box(
                                modifier = Modifier
                                    .fillMaxSize()
                                    .graphicsLayer {
                                        val progress = browserStateHolder.detailRevealProgress.value
                                        alpha = 1f - (1f - TrophyBrowserAppExitAlphaFloor) * progress
                                        rotationY = TrophyBrowserAppExitRotationY * progress
                                        transformOrigin = TransformOrigin(0f, 0.5f)
                                        cameraDistance = 18f * density
                                    }
                            ) {
                                TrophyAppPane(
                                    state = uiState.appState,
                                    onEarnedFilterChanged = viewModel::setEarnedFilter,
                                    onShowHiddenLockedChanged = viewModel::setShowHiddenLocked,
                                    onToggleGrade = viewModel::toggleGrade,
                                    onOpenDetail = ::openDetail
                                )
                            }
                        }

                        if (activeDetailTrophyId != null || browserStateHolder.detailRevealProgress.value > 0f) {
                            Box(
                                modifier = Modifier
                                    .fillMaxSize()
                                    .pointerInput(activeDetailTrophyId) {
                                        awaitPointerEventScope {
                                            while (true) {
                                                val event = awaitPointerEvent()
                                                event.changes.forEach { it.consume() }
                                            }
                                        }
                                    }
                                    .graphicsLayer {
                                        alpha = trophyBrowserDetailOverlayAlpha(
                                            browserStateHolder.detailRevealProgress.value
                                        )
                                    }
                            ) {
                                TrophyDetailPane(
                                    state = uiState.detailState
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun TrophyBrowserHeader(
    pane: TrophyViewerPane,
    headerLegendAlpha: Float,
    onExit: () -> Unit,
    onBackFromApp: () -> Unit,
    onBackFromDetail: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(
                start = TrophyDimensions.ScreenPadding,
                top = TrophyDimensions.HeaderSpacing,
                end = TrophyDimensions.ScreenPadding,
                bottom = TrophyDimensions.SectionSpacing
            ),
        verticalAlignment = Alignment.CenterVertically
    ) {
        val onBackClick = when (pane) {
            TrophyViewerPane.Collections -> onExit
            is TrophyViewerPane.App -> if (pane.detailTrophyId != null) onBackFromDetail else onBackFromApp
        }

        Row(
            modifier = Modifier
                .padding(top = 2.dp)
                .clickable(onClick = onBackClick),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            Icon(
                imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                contentDescription = null,
                tint = TrophyColors.TextPrimary
            )
            Text(
                text = stringResource(R.string.trophy_back_short),
                color = TrophyColors.TextPrimary,
                fontWeight = FontWeight.Medium
            )
        }

        Spacer(modifier = Modifier.weight(1f))

        TrophyCollectionGradeLegend(
            modifier = Modifier
                .padding(top = 2.dp, end = 10.dp)
                .graphicsLayer { alpha = headerLegendAlpha }
        )
    }
}

private fun trophyBrowserDetailOverlayAlpha(progress: Float): Float {
    val normalized = (progress - TrophyBrowserDetailFadeStartFraction) / (1f - TrophyBrowserDetailFadeStartFraction)
    return normalized.coerceIn(0f, 1f)
}
