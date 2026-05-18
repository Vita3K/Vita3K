package org.vita3k.emulator.trophies.ui.screen

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import coil.compose.AsyncImage
import org.vita3k.emulator.R
import org.vita3k.emulator.trophies.domain.model.TrophyCollection
import org.vita3k.emulator.trophies.domain.model.TrophyEarnedFilter
import org.vita3k.emulator.trophies.domain.model.TrophyGrade
import org.vita3k.emulator.trophies.domain.model.TrophyItem
import org.vita3k.emulator.trophies.ui.theme.TrophyColors
import org.vita3k.emulator.trophies.ui.theme.TrophyDimensions
import org.vita3k.emulator.trophies.ui.theme.TrophyShapes
import org.vita3k.emulator.trophies.ui.theme.displayName

private val TrophySummaryBandContentStartInset = TrophyDimensions.ScreenPadding
private val TrophyAchievementBandContentStartInset = TrophyDimensions.ScreenPadding + 24.dp
private val TrophyBandContentEndInset = TrophyDimensions.ScreenPadding + 10.dp
private val TrophyBandGradeIconSize = 16.dp
private val TrophyFilterButtonSize = 68.dp
private val TrophyFilterButtonClipOffset = 9.dp
private val TrophyFilterPanelWidth = 278.dp
private val TrophyFilterPanelShape = RoundedCornerShape(11.dp)

@Composable
internal fun TrophyMessagePanel(
    title: String,
    body: String
) {
    TrophyGlassPanel(modifier = Modifier.fillMaxWidth()) {
        Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text(text = title, color = TrophyColors.TextPrimary, fontWeight = FontWeight.SemiBold)
            Text(text = body, color = TrophyColors.TextSecondary)
        }
    }
}

@Composable
internal fun TrophyCollectionSummaryBand(
    collection: TrophyCollection,
    modifier: Modifier = Modifier
) {
    TrophyGlassBand(
        modifier = modifier.fillMaxWidth(),
        contentPadding = PaddingValues(
            start = TrophySummaryBandContentStartInset,
            end = TrophyBandContentEndInset
        )
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(TrophyDimensions.CollectionRowMinHeight),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            TrophyCollectionArtworkBand(
                title = collection.title,
                iconPath = collection.iconPath,
                artworkAspectRatio = collection.artworkAspectRatio,
                contentDescription = collection.title,
                modifier = Modifier.fillMaxHeight()
            )
            Column(
                modifier = Modifier
                    .weight(1f)
                    .padding(vertical = 10.dp),
                verticalArrangement = Arrangement.spacedBy(5.dp)
            ) {
                Text(
                    text = collection.title,
                    color = TrophyColors.TextPrimary,
                    fontWeight = FontWeight.SemiBold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Text(
                    text = stringResource(R.string.trophy_unlocked_count_format, collection.unlockedCount, collection.totalCount),
                    color = TrophyColors.TextSecondary,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
        }
    }
}

@Composable
internal fun TrophyAchievementBand(
    trophy: TrophyItem,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    val title = if (trophy.isHiddenLocked) stringResource(R.string.trophy_hidden_title) else trophy.name
    val detail = if (trophy.isHiddenLocked) stringResource(R.string.trophy_hidden_detail) else trophy.detail

    TrophyGlassBand(
        modifier = modifier
            .fillMaxWidth()
            .clickable(onClick = onClick),
        contentPadding = PaddingValues(
            start = TrophyAchievementBandContentStartInset,
            end = TrophyBandContentEndInset
        )
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .heightIn(min = TrophyDimensions.CollectionRowMinHeight)
                .height(TrophyDimensions.CollectionRowMinHeight),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            TrophyAchievementArtworkBand(
                title = title,
                iconPath = trophy.iconPath,
                contentDescription = title,
                modifier = Modifier.fillMaxHeight(),
                hidden = trophy.isHiddenLocked,
                grade = trophy.grade,
                earned = trophy.earned
            )
            Column(
                modifier = Modifier.weight(1f),
                verticalArrangement = Arrangement.spacedBy(5.dp)
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    TrophyGradeAssetIcon(
                        grade = trophy.grade,
                        modifier = Modifier.size(TrophyBandGradeIconSize)
                    )
                    Text(
                        text = title,
                        color = TrophyColors.TextPrimary,
                        fontWeight = FontWeight.SemiBold,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                }
                Text(
                    text = detail,
                    color = if (trophy.earned) TrophyColors.TextSecondary else TrophyColors.TextMuted,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis
                )
            }
        }
    }
}

@Composable
internal fun TrophyFilterMenu(
    earnedFilter: TrophyEarnedFilter,
    showHiddenLocked: Boolean,
    includedGrades: Set<TrophyGrade>,
    expanded: Boolean,
    onExpandedChange: (Boolean) -> Unit,
    onEarnedFilterChanged: (TrophyEarnedFilter) -> Unit,
    onShowHiddenLockedChanged: (Boolean) -> Unit,
    onToggleGrade: (TrophyGrade) -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier.clipToBounds(),
        contentAlignment = Alignment.BottomEnd
    ) {
        AnimatedVisibility(
            visible = expanded,
            enter = fadeIn(animationSpec = tween(110)),
            exit = fadeOut(animationSpec = tween(90))
        ) {
            TrophySkeuomorphicFilterPanel(
                modifier = Modifier
                    .padding(
                        end = TrophyFilterButtonSize - TrophyFilterButtonClipOffset - 2.dp,
                        bottom = TrophyFilterButtonSize - TrophyFilterButtonClipOffset + 2.dp
                    )
                    .width(TrophyFilterPanelWidth)
            ) {
                Column(verticalArrangement = Arrangement.spacedBy(7.dp)) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        TrophyFilterPill(
                            text = stringResource(R.string.trophy_filter_all),
                            selected = earnedFilter == TrophyEarnedFilter.ALL,
                            onClick = { onEarnedFilterChanged(TrophyEarnedFilter.ALL) },
                            modifier = Modifier.weight(1f)
                        )
                        TrophyFilterPill(
                            text = stringResource(R.string.trophy_filter_earned),
                            selected = earnedFilter == TrophyEarnedFilter.EARNED,
                            onClick = { onEarnedFilterChanged(TrophyEarnedFilter.EARNED) },
                            modifier = Modifier.weight(1f)
                        )
                        TrophyFilterPill(
                            text = stringResource(R.string.trophy_filter_locked),
                            selected = earnedFilter == TrophyEarnedFilter.NOT_EARNED,
                            onClick = { onEarnedFilterChanged(TrophyEarnedFilter.NOT_EARNED) },
                            modifier = Modifier.weight(1f)
                        )
                    }

                    TrophyFilterPill(
                        text = stringResource(R.string.trophy_filter_show_hidden),
                        selected = showHiddenLocked,
                        onClick = { onShowHiddenLockedChanged(!showHiddenLocked) },
                        modifier = Modifier.fillMaxWidth()
                    )

                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(1.dp)
                            .background(
                                Brush.horizontalGradient(
                                    colors = listOf(
                                        Color.Transparent,
                                        TrophyColors.FilterMenuDivider,
                                        Color.Transparent
                                    )
                                )
                            )
                    )

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        listOf(TrophyGrade.PLATINUM, TrophyGrade.GOLD).forEach { grade ->
                            TrophyFilterPill(
                                text = grade.displayName(),
                                selected = grade in includedGrades,
                                onClick = { onToggleGrade(grade) },
                                modifier = Modifier.weight(1f),
                                leadingContent = {
                                    TrophyGradeAssetIcon(grade = grade, modifier = Modifier.size(13.dp))
                                }
                            )
                        }
                    }

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        listOf(TrophyGrade.SILVER, TrophyGrade.BRONZE).forEach { grade ->
                            TrophyFilterPill(
                                text = grade.displayName(),
                                selected = grade in includedGrades,
                                onClick = { onToggleGrade(grade) },
                                modifier = Modifier.weight(1f),
                                leadingContent = {
                                    TrophyGradeAssetIcon(grade = grade, modifier = Modifier.size(13.dp))
                                }
                            )
                        }
                    }
                }
            }
        }

        TrophyFilterOrbButton(
            expanded = expanded,
            onClick = { onExpandedChange(!expanded) },
            modifier = Modifier.offset(x = TrophyFilterButtonClipOffset, y = TrophyFilterButtonClipOffset)
        )
    }
}

@Composable
private fun TrophyFilterOrbButton(
    expanded: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .size(TrophyFilterButtonSize)
            .clip(CircleShape)
            .background(
                Brush.verticalGradient(
                    colors = listOf(
                        TrophyColors.FilterOrbTop,
                        TrophyColors.FilterOrbMid,
                        TrophyColors.FilterOrbBottom
                    )
                )
            )
            .border(1.dp, Color.White.copy(alpha = 0.42f), CircleShape)
            .drawWithCache {
                val topGlossHeight = size.height * 0.48f
                val lowerShadowStart = size.height * 0.56f
                onDrawBehind {
                    drawCircle(
                        brush = Brush.radialGradient(
                            colors = listOf(
                                Color.White.copy(alpha = 0.28f),
                                Color.Transparent
                            ),
                            center = Offset(size.width * 0.38f, size.height * 0.28f),
                            radius = size.minDimension * 0.6f
                        )
                    )
                    drawRect(
                        brush = Brush.verticalGradient(
                            colors = listOf(
                                Color.White.copy(alpha = 0.18f),
                                Color.Transparent
                            )
                        ),
                        size = Size(size.width, topGlossHeight)
                    )
                    drawRect(
                        brush = Brush.verticalGradient(
                            colors = listOf(
                                Color.Transparent,
                                TrophyColors.FilterOrbShadow.copy(alpha = 0.78f)
                            )
                        ),
                        topLeft = Offset(0f, lowerShadowStart),
                        size = Size(size.width, size.height - lowerShadowStart)
                    )
                }
            }
            .clickable(onClick = onClick)
            .padding(horizontal = 12.dp),
        contentAlignment = Alignment.Center
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(16.dp)
                .align(Alignment.TopCenter)
                .background(
                    Brush.verticalGradient(
                        colors = listOf(
                            Color.White.copy(alpha = 0.3f),
                            Color.Transparent
                        )
                    ),
                    shape = CircleShape
                )
        )
        Row(
            horizontalArrangement = Arrangement.spacedBy(4.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            repeat(3) { index ->
                val alpha = if (expanded) 1f - index * 0.08f else 0.92f
                Box(
                    modifier = Modifier
                        .size(4.dp)
                        .clip(CircleShape)
                        .background(Color.White.copy(alpha = alpha))
                )
            }
        }
    }
}

@Composable
private fun TrophySkeuomorphicFilterPanel(
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit
) {
    Box(
        modifier = modifier
            .clip(TrophyFilterPanelShape)
            .background(
                Brush.verticalGradient(
                    colors = listOf(
                        TrophyColors.FilterMenuTop,
                        TrophyColors.FilterMenuMid,
                        TrophyColors.FilterMenuBottom
                    )
                )
            )
            .border(1.dp, TrophyColors.FilterMenuBorder, TrophyFilterPanelShape)
            .drawWithCache {
                val edgeLineHeight = 1.dp.toPx()
                val glossHeight = size.height * 0.38f
                onDrawBehind {
                    drawRect(
                        brush = Brush.verticalGradient(
                            colors = listOf(
                                Color.White.copy(alpha = 0.16f),
                                Color.White.copy(alpha = 0.06f),
                                Color.Transparent
                            )
                        ),
                        size = Size(size.width, glossHeight)
                    )
                    drawRect(
                        brush = Brush.horizontalGradient(
                            colors = listOf(
                                Color.White.copy(alpha = 0.06f),
                                Color.Transparent
                            )
                        ),
                        size = Size(size.width * 0.2f, size.height)
                    )
                    drawRect(
                        color = Color.White.copy(alpha = 0.18f),
                        size = Size(size.width, edgeLineHeight)
                    )
                    drawRect(
                        color = Color.Black.copy(alpha = 0.34f),
                        topLeft = Offset(0f, size.height - edgeLineHeight),
                        size = Size(size.width, edgeLineHeight)
                    )
                }
            }
            .padding(horizontal = 10.dp, vertical = 10.dp),
    ) {
        content()
    }
}

@Composable
private fun TrophyFilterPill(
    text: String,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    leadingContent: @Composable () -> Unit = {}
) {
    Box(
        modifier = modifier
            .clip(TrophyShapes.Pill)
            .background(
                Brush.verticalGradient(
                    colors = if (selected) {
                        listOf(
                            TrophyColors.FilterPillSelectedTop,
                            TrophyColors.FilterPillSelectedBottom
                        )
                    } else {
                        listOf(
                            TrophyColors.FilterPillTop,
                            TrophyColors.FilterPillBottom
                        )
                    }
                )
            )
            .border(
                width = 1.dp,
                color = if (selected) {
                    Color.White.copy(alpha = 0.4f)
                } else {
                    Color.White.copy(alpha = 0.18f)
                },
                shape = TrophyShapes.Pill
            )
            .drawWithCache {
                val glossHeight = size.height * 0.55f
                onDrawBehind {
                    drawRect(
                        brush = Brush.verticalGradient(
                            colors = listOf(
                                Color.White.copy(alpha = if (selected) 0.18f else 0.12f),
                                Color.Transparent
                            )
                        ),
                        size = Size(size.width, glossHeight)
                    )
                }
            }
            .clickable(onClick = onClick)
            .padding(horizontal = 10.dp, vertical = 8.dp),
        contentAlignment = Alignment.Center
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            leadingContent()
            Text(
                text = text,
                color = TrophyColors.TextPrimary,
                fontSize = 12.sp,
                fontWeight = if (selected) FontWeight.SemiBold else FontWeight.Medium,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }
    }
}

@Composable
private fun TrophyAchievementArtworkBand(
    title: String,
    iconPath: String,
    contentDescription: String?,
    hidden: Boolean,
    grade: TrophyGrade,
    earned: Boolean,
    modifier: Modifier = Modifier
) {
    BoxWithConstraints(
        modifier = modifier,
        contentAlignment = Alignment.Center
    ) {
        val artworkWidth = maxHeight
        Box(
            modifier = Modifier
                .fillMaxHeight()
                .width(artworkWidth),
            contentAlignment = Alignment.Center
        ) {
            if (iconPath.isNotBlank()) {
                AsyncImage(
                    model = iconPath,
                    contentDescription = contentDescription,
                    contentScale = ContentScale.Fit,
                    modifier = Modifier.fillMaxSize(),
                    alpha = if (hidden || !earned) 0.5f else 1f
                )
            } else {
                TrophyCupSilhouette(
                    grade = if (hidden) TrophyGrade.UNKNOWN else grade,
                    hidden = hidden,
                    earned = earned
                )
            }
        }
    }
}
