package org.vita3k.emulator.trophies.ui.theme

import androidx.compose.runtime.Composable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.vita3k.emulator.R
import org.vita3k.emulator.trophies.domain.model.TrophyGrade

internal object TrophyColors {
    val BackgroundTop = Color(0xFF0A1824)
    val BackgroundMid = Color(0xFF08111D)
    val BackgroundBottom = Color(0xFF02060B)
    val AtmosphereBlue = Color(0xFF3E84C8)
    val AtmosphereGold = Color(0xFFBF8A2C)
    val Panel = Color(0x14FFFFFF)
    val PanelStrong = Color(0x1BFFFFFF)
    val PanelBorder = Color(0x2AFFFFFF)
    val PanelHighlight = Color(0x73FFFFFF)
    val TextPrimary = Color(0xFFF4F8FD)
    val TextSecondary = Color(0xA6F4F8FD)
    val TextMuted = Color(0x66F4F8FD)
    val LockedScrim = Color(0x7A02060B)
    val Earned = Color(0xFFBDEED3)
    val Error = Color(0xFFFF8A8A)
    val FilterOrbTop = Color(0xFFC7D2F2)
    val FilterOrbMid = Color(0xFF7183B6)
    val FilterOrbBottom = Color(0xFF243355)
    val FilterOrbShadow = Color(0xFF11192C)
    val FilterMenuTop = Color(0xFF4B5D80)
    val FilterMenuMid = Color(0xFF2F3C58)
    val FilterMenuBottom = Color(0xFF182231)
    val FilterMenuBorder = Color(0x66F4F8FD)
    val FilterMenuDivider = Color(0x2EF4F8FD)
    val FilterPillTop = Color(0xAA55688F)
    val FilterPillBottom = Color(0xAA25324C)
    val FilterPillSelectedTop = Color(0xFF7F97CE)
    val FilterPillSelectedBottom = Color(0xFF3D517C)

    fun gradeAccent(grade: TrophyGrade): Color = when (grade) {
        TrophyGrade.PLATINUM -> Color(0xFFA0C4FF)
        TrophyGrade.GOLD -> Color(0xFFFFD700)
        TrophyGrade.SILVER -> Color(0xFFC8CCD4)
        TrophyGrade.BRONZE -> Color(0xFFCD7F32)
        TrophyGrade.UNKNOWN -> Color(0xFF8F98A7)
    }

    fun gradeGlow(grade: TrophyGrade): Color = when (grade) {
        TrophyGrade.PLATINUM -> Color(0x339CCBFF)
        TrophyGrade.GOLD -> Color(0x33FFCB52)
        TrophyGrade.SILVER -> Color(0x33D8DEE8)
        TrophyGrade.BRONZE -> Color(0x33D18B54)
        TrophyGrade.UNKNOWN -> Color(0x228F98A7)
    }
}

internal object TrophyDimensions {
    val ScreenPadding = 20.dp
    val HeaderSpacing = 14.dp
    val SectionSpacing = 12.dp
    val PanelPadding = 16.dp
    val CompactPanelPadding = 12.dp
    val RowIconSize = 58.dp
    val CollectionRowIconSize = 62.dp
    val CollectionRowMinHeight = 78.dp
    val CollectionLegendHeight = 34.dp
    val CollectionCountsPanelWidth = 136.dp
    val LargeIconSize = 88.dp
}

internal object TrophyShapes {
    val Panel = RoundedCornerShape(8.dp)
    val Band = RoundedCornerShape(3.dp)
    val CompactPanel = RoundedCornerShape(6.dp)
    val Pill = RoundedCornerShape(9.dp)
    val Badge = RoundedCornerShape(999.dp)
    val IconFrame = RoundedCornerShape(6.dp)
}

@Composable
internal fun TrophyGrade.displayName(): String = stringResource(
    when (this) {
        TrophyGrade.PLATINUM -> R.string.trophy_grade_platinum
        TrophyGrade.GOLD -> R.string.trophy_grade_gold
        TrophyGrade.SILVER -> R.string.trophy_grade_silver
        TrophyGrade.BRONZE -> R.string.trophy_grade_bronze
        TrophyGrade.UNKNOWN -> R.string.trophy_grade_unknown
    }
)
