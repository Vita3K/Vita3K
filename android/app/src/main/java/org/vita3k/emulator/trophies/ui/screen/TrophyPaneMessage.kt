package org.vita3k.emulator.trophies.ui.screen

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import org.vita3k.emulator.trophies.ui.theme.TrophyDimensions

@Composable
internal fun TrophyPaneMessage(
    title: String,
    body: String
) {
    Column(modifier = Modifier.padding(horizontal = TrophyDimensions.ScreenPadding)) {
        TrophyMessagePanel(
            title = title,
            body = body
        )
    }
}
