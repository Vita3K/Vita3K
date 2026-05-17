package org.vita3k.emulator.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.AssistChip
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import org.vita3k.emulator.R
import org.vita3k.emulator.data.NativeUser
import org.vita3k.emulator.ui.theme.ApplyDialogDim
import org.vita3k.emulator.ui.viewmodel.UserManagementViewModel

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun UserManagementScreen(
    viewModel: UserManagementViewModel,
    onBack: () -> Unit,
    onUsersChanged: () -> Unit
) {
    var showCreateDialog by rememberSaveable { mutableStateOf(false) }
    var pendingDeletion by remember { mutableStateOf<NativeUser?>(null) }
    val activeUser = viewModel.users.firstOrNull { it.active }

    LaunchedEffect(Unit) {
        viewModel.load()
    }

    Scaffold(
        containerColor = MaterialTheme.colorScheme.background,
        topBar = {
            TopAppBar(
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surface,
                    titleContentColor = MaterialTheme.colorScheme.onSurface,
                    actionIconContentColor = MaterialTheme.colorScheme.onSurfaceVariant,
                    navigationIconContentColor = MaterialTheme.colorScheme.onSurfaceVariant
                ),
                title = { Text(stringResource(R.string.user_management_title)) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(
                            imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                            contentDescription = stringResource(R.string.action_back)
                        )
                    }
                },
                actions = {
                    IconButton(onClick = viewModel::load, enabled = !viewModel.loading && !viewModel.busy) {
                        Icon(
                            imageVector = Icons.Default.Refresh,
                            contentDescription = stringResource(R.string.user_management_refresh)
                        )
                    }
                }
            )
        },
        floatingActionButton = {
            ExtendedFloatingActionButton(
                text = { Text(stringResource(R.string.user_management_create_user)) },
                icon = { Icon(Icons.Default.Add, contentDescription = null) },
                onClick = { showCreateDialog = true },
                expanded = true
            )
        }
    ) { padding ->
        when {
            viewModel.loading && viewModel.users.isEmpty() -> {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(padding),
                    contentAlignment = Alignment.Center
                ) {
                    CircularProgressIndicator()
                }
            }

            else -> {
                LazyColumn(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(padding),
                    contentPadding = PaddingValues(start = 16.dp, top = 16.dp, end = 16.dp, bottom = 96.dp),
                    verticalArrangement = Arrangement.spacedBy(14.dp)
                ) {
                    item {
                        ActiveUserHero(
                            activeUser = activeUser,
                            onCreateUser = { showCreateDialog = true }
                        )
                    }

                    if (viewModel.users.isEmpty()) {
                        item {
                            EmptyUsersCard(onCreateUser = { showCreateDialog = true })
                        }
                    } else {
                        items(viewModel.users, key = { it.id }) { user ->
                            UserCard(
                                user = user,
                                busy = viewModel.busy,
                                onActivate = { viewModel.activateUser(user, onUsersChanged) },
                                onDelete = { pendingDeletion = user }
                            )
                        }
                    }
                }
            }
        }
    }

    if (showCreateDialog) {
        CreateUserDialog(
            busy = viewModel.busy,
            onDismiss = { showCreateDialog = false },
            onCreate = { name ->
                showCreateDialog = false
                viewModel.createUser(name, onUsersChanged)
            }
        )
    }

    pendingDeletion?.let { user ->
        AlertDialog(
            onDismissRequest = { pendingDeletion = null },
            title = { Text(stringResource(R.string.user_management_delete_title)) },
            text = {
                ApplyDialogDim()
                Text(stringResource(R.string.user_management_delete_message, user.name))
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        pendingDeletion = null
                        viewModel.deleteUser(user, onUsersChanged)
                    }
                ) {
                    Text(
                        text = stringResource(R.string.action_delete),
                        color = MaterialTheme.colorScheme.error
                    )
                }
            },
            dismissButton = {
                TextButton(onClick = { pendingDeletion = null }) {
                    Text(stringResource(R.string.action_cancel))
                }
            }
        )
    }

    viewModel.operationResult?.let { result ->
        AlertDialog(
            onDismissRequest = { viewModel.dismissOperationResult() },
            title = {
                Text(
                    if (result.isError) {
                        stringResource(R.string.settings_opt_error)
                    } else {
                        stringResource(R.string.settings_success_title)
                    }
                )
            },
            text = {
                ApplyDialogDim()
                Text(result.message)
            },
            confirmButton = {
                TextButton(onClick = { viewModel.dismissOperationResult() }) {
                    Text(stringResource(R.string.action_ok))
                }
            }
        )
    }
}

@Composable
private fun ActiveUserHero(
    activeUser: NativeUser?,
    onCreateUser: () -> Unit
) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(28.dp),
        color = MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.9f)
    ) {
        Column(
            modifier = Modifier.padding(20.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(14.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Surface(
                    color = MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha = 0.1f),
                    shape = RoundedCornerShape(20.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Person,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.onPrimaryContainer,
                        modifier = Modifier.padding(12.dp)
                    )
                }
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = stringResource(R.string.user_management_active_user),
                        style = MaterialTheme.typography.labelLarge,
                        color = MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha = 0.72f)
                    )
                    Text(
                        text = activeUser?.name ?: stringResource(R.string.user_management_active_none),
                        style = MaterialTheme.typography.headlineSmall.copy(fontWeight = FontWeight.SemiBold),
                        color = MaterialTheme.colorScheme.onPrimaryContainer,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                    activeUser?.let {
                        Text(
                            text = it.id,
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha = 0.78f)
                        )
                    }
                }
            }
            FilledTonalButton(onClick = onCreateUser) {
                Icon(Icons.Default.Add, contentDescription = null)
                Spacer(modifier = Modifier.padding(horizontal = 4.dp))
                Text(stringResource(R.string.user_management_create_user))
            }
        }
    }
}

@Composable
private fun EmptyUsersCard(
    onCreateUser: () -> Unit
) {
    ElevatedCard(shape = RoundedCornerShape(24.dp)) {
        Column(
            modifier = Modifier.padding(20.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            Text(
                text = stringResource(R.string.user_management_empty_title),
                style = MaterialTheme.typography.titleLarge,
                fontWeight = FontWeight.SemiBold
            )
            Text(
                text = stringResource(R.string.user_management_empty_message),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            FilledTonalButton(onClick = onCreateUser) {
                Text(stringResource(R.string.user_management_create_user))
            }
        }
    }
}

@Composable
private fun UserCard(
    user: NativeUser,
    busy: Boolean,
    onActivate: () -> Unit,
    onDelete: () -> Unit
) {
    ElevatedCard(shape = RoundedCornerShape(24.dp)) {
        Column(
            modifier = Modifier.padding(18.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = user.name,
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.SemiBold,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                    Text(
                        text = user.id,
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                if (user.active) {
                    AssistChip(
                        onClick = {},
                        enabled = false,
                        label = { Text(stringResource(R.string.user_management_active_badge)) }
                    )
                }
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                FilledTonalButton(
                    onClick = onActivate,
                    enabled = !busy && !user.active,
                    modifier = Modifier.weight(1f)
                ) {
                    Text(
                        text = if (user.active) {
                            stringResource(R.string.user_management_active_badge)
                        } else {
                            stringResource(R.string.user_management_switch_user)
                        }
                    )
                }
                OutlinedButton(
                    onClick = onDelete,
                    enabled = !busy,
                    modifier = Modifier.weight(1f)
                ) {
                    Icon(Icons.Default.Delete, contentDescription = null)
                    Spacer(modifier = Modifier.padding(horizontal = 4.dp))
                    Text(stringResource(R.string.user_management_delete_user))
                }
            }
        }
    }
}

@Composable
private fun CreateUserDialog(
    busy: Boolean,
    onDismiss: () -> Unit,
    onCreate: (String) -> Unit
) {
    var userName by rememberSaveable { mutableStateOf("Vita3K") }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.user_management_create_user)) },
        text = {
            ApplyDialogDim()
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(stringResource(R.string.user_management_create_prompt))
                OutlinedTextField(
                    value = userName,
                    onValueChange = { userName = it },
                    singleLine = true,
                    label = { Text(stringResource(R.string.user_management_name_label)) },
                    modifier = Modifier.fillMaxWidth()
                )
            }
        },
        confirmButton = {
            TextButton(
                enabled = !busy && userName.trim().isNotEmpty(),
                onClick = { onCreate(userName.trim()) }
            ) {
                Text(stringResource(R.string.action_ok))
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text(stringResource(R.string.action_cancel))
            }
        }
    )
}
