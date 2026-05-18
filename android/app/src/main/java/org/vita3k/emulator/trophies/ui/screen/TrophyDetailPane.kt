package org.vita3k.emulator.trophies.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import coil.compose.AsyncImage
import org.vita3k.emulator.R
import org.vita3k.emulator.trophies.domain.model.TrophyGrade
import org.vita3k.emulator.trophies.ui.model.TrophyDetailScreenState
import org.vita3k.emulator.trophies.ui.theme.TrophyColors
import org.vita3k.emulator.trophies.ui.theme.TrophyDimensions
import org.vita3k.emulator.trophies.ui.theme.displayName
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.time.format.FormatStyle

private val TrophyBrowserDetailContentStartInset = 58.dp
private val TrophyBrowserDetailContentEndInset = 92.dp
private val TrophyBrowserDetailContentMaxWidth = 760.dp
private val TrophyBrowserDetailLabelWidth = 108.dp
private val TrophyBrowserDetailIconSize = 52.dp
private val TrophyBrowserDetailTitleGap = 14.dp
private val TrophyBrowserDetailTopInset = 14.dp

@Composable
internal fun TrophyDetailPane(
    state: TrophyDetailScreenState
) {
    when {
        state.loading && state.trophy == null -> {
            Box(
                modifier = Modifier.fillMaxSize(),
                contentAlignment = Alignment.Center
            ) {
                CircularProgressIndicator()
            }
        }

        state.errorMessage != null && state.trophy == null -> {
            TrophyPaneMessage(
                title = stringResource(R.string.trophy_error_title),
                body = state.errorMessage
            )
        }

        state.trophy == null -> {
            TrophyPaneMessage(
                title = stringResource(R.string.trophy_not_found_title),
                body = stringResource(R.string.trophy_detail_not_found_body)
            )
        }

        else -> {
            val trophy = state.trophy
            val title = if (trophy.isHiddenLocked) stringResource(R.string.trophy_hidden_title) else trophy.name
            val detail = if (trophy.isHiddenLocked) stringResource(R.string.trophy_hidden_detail) else trophy.detail

            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(
                        start = TrophyBrowserDetailContentStartInset,
                        top = TrophyBrowserDetailTopInset,
                        end = TrophyBrowserDetailContentEndInset,
                        bottom = TrophyDimensions.SectionSpacing
                    )
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth(0.76f)
                        .widthIn(max = TrophyBrowserDetailContentMaxWidth),
                    verticalArrangement = Arrangement.spacedBy(18.dp)
                ) {
                    Row(
                        verticalAlignment = Alignment.Top,
                        horizontalArrangement = Arrangement.spacedBy(TrophyBrowserDetailTitleGap)
                    ) {
                        TrophyDetailIcon(
                            title = title,
                            iconPath = trophy.iconPath,
                            hidden = trophy.isHiddenLocked,
                            grade = trophy.grade,
                            earned = trophy.earned,
                            modifier = Modifier.size(TrophyBrowserDetailIconSize)
                        )
                        Column(
                            modifier = Modifier.padding(top = 4.dp),
                            verticalArrangement = Arrangement.spacedBy(4.dp)
                        ) {
                            Text(
                                text = title,
                                color = TrophyColors.TextPrimary,
                                fontWeight = FontWeight.SemiBold
                            )
                        }
                    }

                    TrophyDetailFieldRow(
                        label = stringResource(R.string.trophy_detail_grade_label)
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            TrophyGradeAssetIcon(grade = trophy.grade, modifier = Modifier.size(17.dp))
                            Text(
                                text = trophy.grade.displayName(),
                                color = TrophyColors.TextPrimary,
                                fontWeight = FontWeight.Medium
                            )
                        }
                    }

                    TrophyDetailFieldRow(
                        label = stringResource(R.string.trophy_detail_earned_label)
                    ) {
                        Text(
                            text = formatTrophyBrowserEarnedText(
                                trophy.timestampSec,
                                stringResource(R.string.trophy_not_yet_earned)
                            ),
                            color = TrophyColors.TextPrimary
                        )
                    }

                    TrophyDetailFieldRow(
                        label = stringResource(R.string.trophy_detail_description_label)
                    ) {
                        Text(
                            text = detail,
                            color = TrophyColors.TextPrimary
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun TrophyDetailFieldRow(
    label: String,
    value: @Composable () -> Unit
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(12.dp),
        verticalAlignment = Alignment.Top
    ) {
        Text(
            text = label,
            color = TrophyColors.TextSecondary,
            fontWeight = FontWeight.Medium,
            modifier = Modifier.width(TrophyBrowserDetailLabelWidth)
        )
        Box(modifier = Modifier.weight(1f)) {
            value()
        }
    }
}

@Composable
private fun TrophyDetailIcon(
    title: String,
    iconPath: String,
    hidden: Boolean,
    grade: TrophyGrade,
    earned: Boolean,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier,
        contentAlignment = Alignment.Center
    ) {
        if (iconPath.isNotBlank()) {
            AsyncImage(
                model = iconPath,
                contentDescription = title,
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

private fun formatTrophyBrowserEarnedText(timestampSec: Long, notEarnedText: String): String {
    if (timestampSec <= 0L) {
        return notEarnedText
    }

    return DateTimeFormatter.ofLocalizedDateTime(FormatStyle.MEDIUM)
        .withZone(ZoneId.systemDefault())
        .format(Instant.ofEpochSecond(timestampSec))
}
