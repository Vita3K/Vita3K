package org.vita3k.emulator.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.List
import androidx.compose.material.icons.filled.GridView
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.vita3k.emulator.R
import org.vita3k.emulator.data.SortOption
import org.vita3k.emulator.data.ViewMode
import org.vita3k.emulator.ui.theme.SCRIM_ALPHA

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun FilterSheet(
    sortOption: SortOption,
    viewMode: ViewMode,
    onSortChanged: (SortOption) -> Unit,
    onViewModeToggle: () -> Unit,
    onDismiss: () -> Unit
) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        scrimColor = Color.Black.copy(alpha = SCRIM_ALPHA)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp)
                .padding(bottom = 32.dp)
        ) {
            Text(
                stringResource(R.string.filter_title),
                style = MaterialTheme.typography.titleLarge,
                modifier = Modifier.padding(bottom = 16.dp)
            )

            // Sort section
            Text(
                stringResource(R.string.filter_section_sort),
                style = MaterialTheme.typography.labelLarge,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(bottom = 8.dp)
            )
            SortOption.entries.forEach { option ->
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    RadioButton(
                        selected = option == sortOption,
                        onClick = { onSortChanged(option) }
                    )
                    Text(
                        text = when (option) {
                            SortOption.TITLE -> stringResource(R.string.apps_list_sort_title)
                            SortOption.LAST_PLAYED -> stringResource(R.string.apps_list_sort_last_played)
                            SortOption.COMPATIBILITY -> stringResource(R.string.apps_list_sort_compatibility)
                        },
                        style = MaterialTheme.typography.bodyLarge,
                        modifier = Modifier
                            .weight(1f)
                            .padding(start = 8.dp)
                    )
                }
            }

            Spacer(modifier = Modifier.height(16.dp))

            // View mode section
            Text(
                stringResource(R.string.filter_section_view),
                style = MaterialTheme.typography.labelLarge,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(bottom = 8.dp)
            )
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                FilterChip(
                    selected = viewMode == ViewMode.LIST,
                    onClick = { if (viewMode != ViewMode.LIST) onViewModeToggle() },
                    label = { Text(stringResource(R.string.filter_view_list)) },
                    leadingIcon = {
                        Icon(
                            Icons.AutoMirrored.Filled.List,
                            contentDescription = null,
                            modifier = Modifier.size(18.dp)
                        )
                    }
                )
                FilterChip(
                    selected = viewMode == ViewMode.GRID,
                    onClick = { if (viewMode != ViewMode.GRID) onViewModeToggle() },
                    label = { Text(stringResource(R.string.filter_view_grid)) },
                    leadingIcon = {
                        Icon(
                            Icons.Default.GridView,
                            contentDescription = null,
                            modifier = Modifier.size(18.dp)
                        )
                    }
                )
            }
        }
    }
}
