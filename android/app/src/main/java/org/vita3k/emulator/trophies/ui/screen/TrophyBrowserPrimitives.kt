package org.vita3k.emulator.trophies.ui.screen

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.CompositingStrategy
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import coil.compose.rememberAsyncImagePainter
import org.vita3k.emulator.R
import org.vita3k.emulator.trophies.domain.model.TrophyCollection
import org.vita3k.emulator.trophies.domain.model.TrophyGrade
import org.vita3k.emulator.trophies.ui.theme.TrophyColors
import org.vita3k.emulator.trophies.ui.theme.TrophyDimensions
import org.vita3k.emulator.trophies.ui.theme.TrophyShapes

private const val TrophyBandFadeFraction = 0.15f
private const val TrophyBandSheenSplitFraction = 0.42f
private const val TrophyProgressSheenSplitFraction = 0.42f
private val CollectionCountSlotWidth = 34.dp
private val CollectionProgressMaxWidth = 190.dp
private val CollectionProgressClusterMaxWidth = 248.dp
private val CollectionCountsPanelHeight = 62.dp
private val CollectionArtworkMinWidth = 120.dp
private val CollectionArtworkMaxWidth = 220.dp
private val CollectionLegendIconSize = 15.dp
private val CollectionLegendReflectionHeight = 12.dp
private const val CollectionProgressWidthFraction = 0.42f
private val CollectionBandStartInset = TrophyDimensions.ScreenPadding + 28.dp
private val CollectionBandEndInset = TrophyDimensions.ScreenPadding + 10.dp

@Composable
internal fun TrophyBackground(
    backgroundResolved: Boolean,
    backgroundImagePath: String?,
    modifier: Modifier = Modifier,
    content: @Composable BoxScope.() -> Unit
) {
    Box(
        modifier = modifier
            .fillMaxSize()
            .background(
                brush = Brush.verticalGradient(
                    colors = listOf(
                        TrophyColors.BackgroundTop,
                        TrophyColors.BackgroundMid,
                        TrophyColors.BackgroundBottom
                    )
                )
            )
    ) {
        if (backgroundResolved && backgroundImagePath != null) {
            Image(
                painter = rememberAsyncImagePainter(model = backgroundImagePath),
                contentDescription = null,
                contentScale = ContentScale.Crop,
                modifier = Modifier.fillMaxSize()
            )
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(
                        Brush.verticalGradient(
                            colors = listOf(
                                TrophyColors.BackgroundTop.copy(alpha = 0.58f),
                                TrophyColors.BackgroundMid.copy(alpha = 0.5f),
                                TrophyColors.BackgroundBottom.copy(alpha = 0.78f)
                            )
                        )
                    )
            )
        } else if (backgroundResolved) {
            Canvas(modifier = Modifier.fillMaxSize()) {
                drawCircle(
                    color = TrophyColors.AtmosphereBlue.copy(alpha = 0.18f),
                    radius = size.minDimension * 0.62f,
                    center = Offset(size.width * 0.16f, size.height * 0.08f)
                )
                drawCircle(
                    color = TrophyColors.AtmosphereGold.copy(alpha = 0.10f),
                    radius = size.minDimension * 0.38f,
                    center = Offset(size.width * 0.86f, size.height * 0.2f)
                )
                drawRect(
                    brush = Brush.verticalGradient(
                        colors = listOf(
                            Color.White.copy(alpha = 0.08f),
                            Color.Transparent,
                            Color.Black.copy(alpha = 0.16f)
                        )
                    )
                )
                drawCircle(
                    color = Color.White.copy(alpha = 0.04f),
                    radius = size.minDimension * 0.72f,
                    center = Offset(size.width * 0.5f, size.height * -0.18f),
                    style = Stroke(width = size.minDimension * 0.01f)
                )
            }
        }
        Canvas(modifier = Modifier.fillMaxSize()) {
            drawRect(
                brush = Brush.verticalGradient(
                    colors = listOf(
                        Color.White.copy(alpha = 0.08f),
                        Color.Transparent,
                        Color.Black.copy(alpha = 0.16f)
                    )
                )
            )
        }
        content()
    }
}

@Composable
internal fun TrophyGlassPanel(
    modifier: Modifier = Modifier,
    contentPadding: PaddingValues = PaddingValues(TrophyDimensions.PanelPadding),
    glassColor: Color = TrophyColors.Panel,
    borderColor: Color = TrophyColors.PanelBorder,
    shape: Shape = TrophyShapes.Panel,
    content: @Composable BoxScope.() -> Unit
) {
    Box(
        modifier = modifier
            .clip(shape)
            .background(glassColor)
            .border(width = 1.dp, color = borderColor, shape = shape)
            .drawWithCache {
                val topLineHeight = 1.dp.toPx()
                onDrawBehind {
                    drawRect(
                        brush = Brush.verticalGradient(
                            colors = listOf(
                                Color.White.copy(alpha = 0.16f),
                                Color.White.copy(alpha = 0.08f),
                                Color.Transparent
                            )
                        ),
                        size = Size(size.width, size.height * 0.45f)
                    )
                    drawRect(
                        color = TrophyColors.PanelHighlight,
                        size = Size(size.width, topLineHeight)
                    )
                }
            }
    ) {
        Box(
            modifier = Modifier.padding(contentPadding),
            content = content
        )
    }
}

@Composable
internal fun TrophyGlassBand(
    modifier: Modifier = Modifier,
    contentPadding: PaddingValues = PaddingValues(0.dp),
    content: @Composable BoxScope.() -> Unit
) {
    Box(
        modifier = modifier
            .graphicsLayer(compositingStrategy = CompositingStrategy.Offscreen)
            .drawWithCache {
                val edgeLineHeight = 1.dp.toPx()
                val sheenHeight = size.height * TrophyBandSheenSplitFraction
                val bandFill = Brush.verticalGradient(
                    colorStops = arrayOf(
                        0f to TrophyColors.PanelStrong.copy(alpha = 0.25f),
                        0.2f to TrophyColors.Panel.copy(alpha = 0.19f),
                        TrophyBandSheenSplitFraction to TrophyColors.Panel.copy(alpha = 0.11f),
                        1f to TrophyColors.Panel.copy(alpha = 0.05f)
                    )
                )
                val bandSheen = Brush.verticalGradient(
                    colors = listOf(
                        Color.White.copy(alpha = 0.12f),
                        Color.White.copy(alpha = 0.05f),
                        Color.Transparent
                    )
                )
                val featherMask = Brush.horizontalGradient(
                    colorStops = arrayOf(
                        0f to Color.Transparent,
                        TrophyBandFadeFraction to Color.White,
                        1f - TrophyBandFadeFraction to Color.White,
                        1f to Color.Transparent
                    ),
                    startX = 0f,
                    endX = size.width
                )
                onDrawWithContent {
                    drawRect(brush = bandFill)
                    drawRect(
                        brush = bandSheen,
                        size = Size(size.width, sheenHeight)
                    )
                    drawRect(
                        color = Color.White.copy(alpha = 0.16f),
                        size = Size(size.width, edgeLineHeight)
                    )
                    drawRect(
                        color = Color.Black.copy(alpha = 0.18f),
                        topLeft = Offset(0f, size.height - edgeLineHeight),
                        size = Size(size.width, edgeLineHeight)
                    )
                    drawRect(brush = featherMask, blendMode = BlendMode.DstIn)
                    drawContent()
                }
            }
    ) {
        Box(modifier = Modifier.padding(contentPadding), content = content)
    }
}

@Composable
internal fun TrophyCollectionRow(
    collection: TrophyCollection,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    TrophyGlassBand(
        modifier = modifier
            .fillMaxWidth()
            .clickable(onClick = onClick),
        contentPadding = PaddingValues(
            start = CollectionBandStartInset,
            end = CollectionBandEndInset,
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

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Box(
                        modifier = Modifier.weight(1f),
                        contentAlignment = Alignment.CenterStart
                    ) {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth(CollectionProgressWidthFraction)
                                .widthIn(max = CollectionProgressClusterMaxWidth),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            TrophyProgressBar(
                                progress = collection.progressPercent / 100f,
                                modifier = Modifier
                                    .weight(1f)
                                    .widthIn(max = CollectionProgressMaxWidth),
                                barHeight = 10.dp
                            )
                            Text(
                                text = stringResource(R.string.trophy_progress_percent_format, collection.progressPercent),
                                color = TrophyColors.TextSecondary,
                                fontWeight = FontWeight.Medium
                            )
                        }
                    }
                }
            }

            TrophyCollectionCountsPanel(
                platinumCount = collection.platinumUnlockedCount,
                goldCount = collection.goldUnlockedCount,
                silverCount = collection.silverUnlockedCount,
                bronzeCount = collection.bronzeUnlockedCount,
                modifier = Modifier
                    .width(TrophyDimensions.CollectionCountsPanelWidth)
                    .height(CollectionCountsPanelHeight)
            )
        }
    }
}

@Composable
internal fun TrophyCollectionGradeLegend(modifier: Modifier = Modifier) {
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(0.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        listOf(
            TrophyGrade.PLATINUM,
            TrophyGrade.GOLD,
            TrophyGrade.SILVER,
            TrophyGrade.BRONZE
        ).forEach { grade ->
            TrophyLegendMarker(grade)
        }
    }
}

@Composable
internal fun TrophyCollectionArtworkBand(
    title: String,
    iconPath: String,
    artworkAspectRatio: Float,
    contentDescription: String?,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier,
        contentAlignment = Alignment.CenterStart
    ) {
        val artworkWidth = (TrophyDimensions.CollectionRowMinHeight * artworkAspectRatio)
            .coerceIn(CollectionArtworkMinWidth, CollectionArtworkMaxWidth)

        Box(
            modifier = Modifier
                .fillMaxHeight()
                .width(artworkWidth),
            contentAlignment = Alignment.CenterStart
        ) {
            if (iconPath.isNotBlank()) {
                Image(
                    painter = rememberAsyncImagePainter(model = iconPath),
                    contentDescription = contentDescription,
                    contentScale = ContentScale.Fit,
                    alignment = Alignment.CenterStart,
                    modifier = Modifier.fillMaxSize()
                )
            } else {
                Text(
                    text = title.take(1).uppercase(),
                    color = TrophyColors.TextMuted,
                    fontWeight = FontWeight.Bold
                )
            }
        }
    }
}

@Composable
private fun TrophyCollectionCountsPanel(
    platinumCount: Int,
    goldCount: Int,
    silverCount: Int,
    bronzeCount: Int,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .border(1.dp, TrophyColors.PanelBorder, TrophyShapes.CompactPanel)
            .padding(vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        listOf(platinumCount, goldCount, silverCount, bronzeCount).forEach { count ->
            Box(
                modifier = Modifier
                    .width(CollectionCountSlotWidth)
                    .fillMaxHeight(),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = count.toString(),
                    color = TrophyColors.TextPrimary,
                    fontWeight = FontWeight.Medium
                )
            }
        }
    }
}

@Composable
private fun TrophyLegendMarker(grade: TrophyGrade) {
    val painter = rememberAsyncImagePainter(model = trophyGradeAssetPath(grade))

    Column(
        modifier = Modifier
            .width(CollectionCountSlotWidth)
            .height(TrophyDimensions.CollectionLegendHeight),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Top
    ) {
        Image(
            painter = painter,
            contentDescription = null,
            modifier = Modifier.size(CollectionLegendIconSize)
        )
        Image(
            painter = painter,
            contentDescription = null,
            modifier = Modifier
                .width(CollectionLegendIconSize)
                .height(CollectionLegendReflectionHeight)
                .graphicsLayer {
                    scaleY = -1f
                    alpha = 0.34f
                    compositingStrategy = CompositingStrategy.Offscreen
                }
                .drawWithCache {
                    onDrawWithContent {
                        drawContent()
                        drawRect(
                            brush = Brush.verticalGradient(
                                colors = listOf(
                                    Color.Black,
                                    Color.Black.copy(alpha = 0.72f),
                                    Color.Transparent
                                )
                            ),
                            blendMode = BlendMode.DstIn
                        )
                    }
                }
        )
    }
}

@Composable
internal fun TrophyProgressBar(
    progress: Float,
    modifier: Modifier = Modifier,
    accentColor: Color = TrophyColors.AtmosphereBlue,
    barHeight: Dp = 4.dp
) {
    val clampedProgress = progress.coerceIn(0f, 1f)

    Box(
        modifier = modifier
            .height(barHeight)
            .defaultMinSize(minWidth = 120.dp)
            .clip(TrophyShapes.Badge)
            .background(
                Brush.verticalGradient(
                    colors = listOf(
                        Color.White.copy(alpha = 0.12f),
                        Color.White.copy(alpha = 0.06f),
                        Color.Black.copy(alpha = 0.22f)
                    )
                )
            )
            .border(1.dp, Color.White.copy(alpha = 0.12f), TrophyShapes.Badge)
            .drawWithCache {
                val sheenInset = 1.dp.toPx()
                val sheenHeight = (size.height - sheenInset * 2f) * 0.52f
                val sheenCornerRadius = CornerRadius(
                    x = ((size.height - sheenInset * 2f) / 2f).coerceAtLeast(0f),
                    y = ((size.height - sheenInset * 2f) / 2f).coerceAtLeast(0f)
                )
                onDrawBehind {
                    drawRoundRect(
                        brush = Brush.verticalGradient(
                            colors = listOf(
                                Color.White.copy(alpha = 0.16f),
                                Color.White.copy(alpha = 0.09f),
                                Color.Transparent
                            )
                        ),
                        topLeft = Offset(sheenInset, sheenInset),
                        size = Size(
                            width = (size.width - sheenInset * 2f).coerceAtLeast(0f),
                            height = sheenHeight.coerceAtLeast(0f)
                        ),
                        cornerRadius = sheenCornerRadius
                    )
                }
            }
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth(clampedProgress)
                .height(barHeight)
                .clip(TrophyShapes.Badge)
                .background(
                    Brush.verticalGradient(
                        colors = listOf(
                            accentColor.copy(alpha = 0.98f),
                            accentColor.copy(alpha = 0.88f),
                            accentColor.copy(alpha = 0.68f)
                        )
                    )
                )
                .drawWithCache {
                    val sheenInset = 1.dp.toPx()
                    val sheenHeight = (size.height - sheenInset * 2f) * 0.52f
                    val sheenCornerRadius = CornerRadius(
                        x = ((size.height - sheenInset * 2f) / 2f).coerceAtLeast(0f),
                        y = ((size.height - sheenInset * 2f) / 2f).coerceAtLeast(0f)
                    )
                    onDrawBehind {
                        drawRoundRect(
                            brush = Brush.verticalGradient(
                                colors = listOf(
                                    Color.White.copy(alpha = 0.16f),
                                    Color.White.copy(alpha = 0.08f),
                                    Color.Transparent
                                )
                            ),
                            topLeft = Offset(sheenInset, sheenInset),
                            size = Size(
                                width = (size.width - sheenInset * 2f).coerceAtLeast(0f),
                                height = sheenHeight.coerceAtLeast(0f)
                            ),
                            cornerRadius = sheenCornerRadius
                        )
                    }
                }
        )
    }
}

@Composable
internal fun TrophyCupSilhouette(
    grade: TrophyGrade,
    hidden: Boolean,
    earned: Boolean
) {
    val accent = if (hidden) TrophyColors.TextMuted else TrophyColors.gradeAccent(grade)
    Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Box(
            modifier = Modifier
                .size(width = 24.dp, height = 18.dp)
                .clip(TrophyShapes.CompactPanel)
                .background(accent.copy(alpha = if (earned) 0.78f else 0.28f))
        )
        Box(
            modifier = Modifier
                .width(12.dp)
                .height(4.dp)
                .clip(TrophyShapes.CompactPanel)
                .background(accent.copy(alpha = if (earned) 0.52f else 0.18f))
        )
    }
}

@Composable
internal fun TrophyGradeAssetIcon(
    grade: TrophyGrade,
    modifier: Modifier = Modifier
) {
    Image(
        painter = rememberAsyncImagePainter(model = trophyGradeAssetPath(grade)),
        contentDescription = null,
        modifier = modifier
    )
}

internal fun trophyGradeAssetPath(grade: TrophyGrade): String = when (grade) {
    TrophyGrade.PLATINUM -> "file:///android_asset/icons/platinum.png"
    TrophyGrade.GOLD -> "file:///android_asset/icons/gold.png"
    TrophyGrade.SILVER -> "file:///android_asset/icons/silver.png"
    TrophyGrade.BRONZE -> "file:///android_asset/icons/bronze.png"
    TrophyGrade.UNKNOWN -> "file:///android_asset/icons/bronze.png"
}
