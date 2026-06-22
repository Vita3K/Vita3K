package org.vita3k.emulator.ui.screens

import android.view.Gravity
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.ArrowForward
import androidx.compose.material3.Button
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import org.vita3k.emulator.R
import org.vita3k.emulator.data.FirmwareInstallState
import org.vita3k.emulator.data.FirmwareLinks
import org.vita3k.emulator.ui.components.HtmlText

private const val INITIAL_SETUP_INFO_HTML =
    """<div align="center">To get started, please install all PS Vita firmware files.<br><br>A comprehensive guide is available on the <a href="https://vita3k.org/quickstart.html">Quickstart</a> page.<br>Check the <a href="https://vita3k.org/compatibility.html">commercial</a> and <a href="https://vita3k.org/compatibility-homebrew.html">homebrew</a> compatibility lists to see what currently runs.<br><br>Contributions are welcome on <a href="https://github.com/Vita3K/Vita3K">GitHub</a>, and additional help is available on <a href="https://discord.gg/6aGwQzh">Discord</a>.</div>"""

private val setupPanelColor = Color(0xFF1F1D1C)
private val setupCardColor = Color(0xFF2B2B2B)
private val setupTextColor = Color(0xFFF7F3EC)

@Composable
fun InitialSetupScreen(
    firmwareInstallState: FirmwareInstallState,
    preferredLanguageIndex: Int,
    onInstallFirmware: () -> Unit,
    dismissLabel: String,
    onSkip: () -> Unit,
    onFinish: () -> Unit
) {
    val systemBars = WindowInsets.systemBars.asPaddingValues()
    var page by rememberSaveable { mutableIntStateOf(0) }
    var firmwareLocaleIndex by rememberSaveable {
        mutableIntStateOf(FirmwareLinks.coerceLocaleIndex(preferredLanguageIndex))
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    listOf(
                        MaterialTheme.colorScheme.primary.copy(alpha = 0.16f),
                        MaterialTheme.colorScheme.background,
                        MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.38f)
                    )
                )
            )
    ) {
        BackgroundOrb(
            modifier = Modifier
                .align(Alignment.TopStart)
                .padding(start = 24.dp, top = 88.dp),
            size = 180.dp,
            color = MaterialTheme.colorScheme.primary.copy(alpha = 0.09f)
        )
        BackgroundOrb(
            modifier = Modifier
                .align(Alignment.BottomEnd)
                .padding(end = 8.dp, bottom = 72.dp),
            size = 220.dp,
            color = MaterialTheme.colorScheme.tertiary.copy(alpha = 0.08f)
        )

        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(
                    start = 20.dp,
                    end = 20.dp,
                    top = systemBars.calculateTopPadding() + 8.dp,
                    bottom = systemBars.calculateBottomPadding() + 20.dp
                )
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                if (page > 0) {
                    TextButton(onClick = { page-- }) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = null)
                        Spacer(modifier = Modifier.width(6.dp))
                        Text(stringResource(R.string.initial_setup_back))
                    }
                } else {
                    Spacer(modifier = Modifier.width(96.dp))
                }
                Spacer(modifier = Modifier.weight(1f))
                TextButton(onClick = onSkip) {
                    Text(dismissLabel)
                }
            }

            Spacer(modifier = Modifier.height(12.dp))

            Surface(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth(),
                shape = RoundedCornerShape(32.dp),
                color = setupPanelColor,
                contentColor = setupTextColor,
                tonalElevation = 6.dp,
                border = BorderStroke(
                    1.dp,
                    MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.35f)
                )
            ) {
                Crossfade(
                    targetState = page,
                    animationSpec = tween(durationMillis = 220),
                    label = "initial-setup-page"
                ) { currentPage ->
                    when (currentPage) {
                        0 -> WelcomePage()
                        else -> FirmwareSetupPage(
                            firmwareInstallState = firmwareInstallState,
                            firmwareLocaleIndex = firmwareLocaleIndex,
                            onFirmwareLocaleSelected = { firmwareLocaleIndex = it },
                            onInstallFirmware = onInstallFirmware
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.height(18.dp))

            SetupPageIndicator(
                currentPage = page,
                totalPages = 2,
                modifier = Modifier.align(Alignment.CenterHorizontally)
            )

            Spacer(modifier = Modifier.height(18.dp))

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Spacer(modifier = Modifier.width(1.dp))
                if (page == 0) {
                    Button(onClick = { page = 1 }) {
                        Text(stringResource(R.string.initial_setup_next))
                        Spacer(modifier = Modifier.width(8.dp))
                        Icon(Icons.AutoMirrored.Filled.ArrowForward, contentDescription = null)
                    }
                } else {
                    Button(onClick = onFinish) {
                        Text(stringResource(R.string.initial_setup_open_library))
                    }
                }
            }
        }
    }
}

@Composable
private fun BackgroundOrb(
    modifier: Modifier = Modifier,
    size: androidx.compose.ui.unit.Dp,
    color: Color
) {
    Box(
        modifier = modifier
            .size(size)
            .clip(CircleShape)
            .background(color)
    )
}

@Composable
private fun WelcomePage() {
    BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 28.dp, vertical = 32.dp)
                .heightIn(min = maxHeight - 64.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            Image(
                painter = painterResource(id = R.mipmap.ic_launcher),
                contentDescription = stringResource(R.string.apps_list_app_title),
                modifier = Modifier
                    .size(112.dp)
                    .alpha(0.96f)
            )

            Spacer(modifier = Modifier.height(20.dp))

            Text(
                text = stringResource(R.string.initial_setup_welcome_title),
                style = MaterialTheme.typography.headlineMedium,
                fontWeight = FontWeight.SemiBold,
                color = setupTextColor,
                textAlign = TextAlign.Center
            )

            Spacer(modifier = Modifier.height(10.dp))

            Text(
                text = stringResource(R.string.initial_setup_welcome_body),
                style = MaterialTheme.typography.bodyLarge,
                color = setupTextColor,
                textAlign = TextAlign.Center
            )

            Spacer(modifier = Modifier.height(18.dp))

            HtmlText(
                html = INITIAL_SETUP_INFO_HTML,
                modifier = Modifier.fillMaxWidth(),
                textStyle = MaterialTheme.typography.bodyLarge,
                textColor = setupTextColor,
                gravity = Gravity.CENTER
            )

            Spacer(modifier = Modifier.height(18.dp))

            Text(
                text = stringResource(R.string.initial_setup_piracy_notice),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.error,
                fontWeight = FontWeight.Medium,
                textAlign = TextAlign.Center
            )
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun FirmwareSetupPage(
    firmwareInstallState: FirmwareInstallState,
    firmwareLocaleIndex: Int,
    onFirmwareLocaleSelected: (Int) -> Unit,
    onInstallFirmware: () -> Unit
) {
    val uriHandler = LocalUriHandler.current

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 20.dp, vertical = 24.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        Text(
            text = stringResource(R.string.initial_setup_firmware_title),
            style = MaterialTheme.typography.headlineSmall,
            fontWeight = FontWeight.SemiBold,
            color = setupTextColor
        )
        Text(
            text = stringResource(R.string.initial_setup_firmware_body),
            style = MaterialTheme.typography.bodyLarge,
            color = setupTextColor
        )

        FirmwareCard(
            title = stringResource(R.string.initial_setup_preinstall_title),
            installed = firmwareInstallState.components.preinstalled,
            missingStatusText = stringResource(R.string.initial_setup_status_optional)
        ) {
            if (!firmwareInstallState.components.preinstalled) {
                FlowRow(
                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    FilledTonalButton(onClick = { uriHandler.openUri(FirmwareLinks.PREINSTALL_URL) }) {
                        Text(stringResource(R.string.initial_setup_download))
                    }
                    Button(onClick = onInstallFirmware) {
                        Text(stringResource(R.string.initial_setup_install_pup))
                    }
                }
            }
        }

        FirmwareCard(
            title = stringResource(R.string.initial_setup_main_title),
            installed = firmwareInstallState.components.main
        ) {
            if (!firmwareInstallState.components.main) {
                FirmwareLanguagePicker(
                    selectedIndex = firmwareLocaleIndex,
                    onSelected = onFirmwareLocaleSelected
                )
                Spacer(modifier = Modifier.height(12.dp))
                FlowRow(
                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    FilledTonalButton(onClick = {
                        uriHandler.openUri(FirmwareLinks.firmwareDownloadUrl(firmwareLocaleIndex))
                    }) {
                        Text(stringResource(R.string.initial_setup_download))
                    }
                    Button(onClick = onInstallFirmware) {
                        Text(stringResource(R.string.initial_setup_install_pup))
                    }
                }
            }
        }

        FirmwareCard(
            title = stringResource(R.string.initial_setup_font_title),
            installed = firmwareInstallState.components.fontPackage
        ) {
            if (!firmwareInstallState.components.fontPackage) {
                FlowRow(
                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    FilledTonalButton(onClick = { uriHandler.openUri(FirmwareLinks.FONT_PACKAGE_URL) }) {
                        Text(stringResource(R.string.initial_setup_download))
                    }
                    Button(onClick = onInstallFirmware) {
                        Text(stringResource(R.string.initial_setup_install_pup))
                    }
                }
            }
        }
    }
}

@Composable
private fun FirmwareLanguagePicker(
    selectedIndex: Int,
    onSelected: (Int) -> Unit
) {
    var expanded by remember { mutableStateOf(false) }

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(
            text = stringResource(R.string.initial_setup_select_firmware_language),
            style = MaterialTheme.typography.labelLarge,
            color = setupTextColor
        )
        Box {
            OutlinedButton(onClick = { expanded = true }) {
                Text(stringResource(FirmwareLinks.locales[FirmwareLinks.coerceLocaleIndex(selectedIndex)].nameResId))
            }
            DropdownMenu(
                expanded = expanded,
                onDismissRequest = { expanded = false }
            ) {
                FirmwareLinks.locales.forEachIndexed { index, locale ->
                    DropdownMenuItem(
                        text = { Text(stringResource(locale.nameResId)) },
                        onClick = {
                            onSelected(index)
                            expanded = false
                        }
                    )
                }
            }
        }
    }
}

@Composable
private fun FirmwareCard(
    title: String,
    installed: Boolean,
    missingStatusText: String? = null,
    content: @Composable ColumnScope.() -> Unit
) {
    val resolvedMissingStatusText = missingStatusText ?: stringResource(R.string.initial_setup_status_missing)

    Surface(
        shape = RoundedCornerShape(28.dp),
        color = setupCardColor,
        contentColor = setupTextColor,
        border = BorderStroke(
            1.dp,
            MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.4f)
        )
    ) {
        Column(
            modifier = Modifier.padding(horizontal = 20.dp, vertical = 18.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                    color = setupTextColor,
                    modifier = Modifier.weight(1f)
                )
                Spacer(modifier = Modifier.width(12.dp))
                StatusBadge(
                    installed = installed,
                    missingStatusText = resolvedMissingStatusText
                )
            }
            content()
        }
    }
}

@Composable
private fun StatusBadge(installed: Boolean, missingStatusText: String) {
    val color = if (installed) Color(0xFF1B8A5A) else MaterialTheme.colorScheme.tertiary

    Surface(
        shape = CircleShape,
        color = color.copy(alpha = 0.13f)
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 12.dp, vertical = 6.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(
                modifier = Modifier
                    .size(6.dp)
                    .clip(CircleShape)
                    .background(color)
            )
            Text(
                text = if (installed) {
                    stringResource(R.string.initial_setup_status_installed)
                } else {
                    missingStatusText
                },
                style = MaterialTheme.typography.labelLarge,
                color = color,
                fontWeight = FontWeight.SemiBold
            )
        }
    }
}

@Composable
private fun SetupPageIndicator(
    currentPage: Int,
    totalPages: Int,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.Center,
        verticalAlignment = Alignment.CenterVertically
    ) {
        repeat(totalPages) { page ->
            val width by animateDpAsState(
                targetValue = if (page == currentPage) 22.dp else 8.dp,
                animationSpec = tween(durationMillis = 220),
                label = "initial-setup-indicator"
            )
            Box(
                modifier = Modifier
                    .padding(horizontal = 4.dp)
                    .height(8.dp)
                    .width(width)
                    .clip(CircleShape)
                    .background(
                        if (page == currentPage) MaterialTheme.colorScheme.primary
                        else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.25f)
                    )
            )
        }
    }
}
