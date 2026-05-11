package org.vita3k.emulator.ui.viewmodel

import android.app.Application
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.launch
import org.vita3k.emulator.R
import org.vita3k.emulator.data.NativeUser
import org.vita3k.emulator.data.UserRepository

data class UserManagementResult(
    val message: String,
    val isError: Boolean
)

class UserManagementViewModel(application: Application) : AndroidViewModel(application) {

    private fun str(resId: Int, vararg args: Any): String =
        getApplication<Application>().getString(resId, *args)

    var users by mutableStateOf<List<NativeUser>>(emptyList())
        private set

    var loading by mutableStateOf(false)
        private set

    var busy by mutableStateOf(false)
        private set

    var operationResult by mutableStateOf<UserManagementResult?>(null)
        private set

    fun load() {
        if (loading) return

        loading = true
        viewModelScope.launch {
            try {
                users = UserRepository.getUsers()
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = UserManagementResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            } finally {
                loading = false
            }
        }
    }

    fun createUser(name: String, onUsersChanged: () -> Unit = {}) {
        val trimmedName = name.trim()
        if (busy || trimmedName.isEmpty()) return

        busy = true
        viewModelScope.launch {
            try {
                val userId = UserRepository.createUser(trimmedName)
                if (userId.isEmpty()) {
                    operationResult = UserManagementResult(
                        str(R.string.user_management_create_failed),
                        true
                    )
                    return@launch
                }

                users = UserRepository.getUsers()
                operationResult = UserManagementResult(
                    str(R.string.user_management_created_success, trimmedName),
                    false
                )
                onUsersChanged()
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = UserManagementResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            } finally {
                busy = false
            }
        }
    }

    fun activateUser(user: NativeUser, onUsersChanged: () -> Unit = {}) {
        if (busy || user.active) return

        busy = true
        viewModelScope.launch {
            try {
                val success = UserRepository.activateUser(user.id)
                if (!success) {
                    operationResult = UserManagementResult(
                        str(R.string.user_management_select_failed),
                        true
                    )
                    return@launch
                }

                users = UserRepository.getUsers()
                operationResult = UserManagementResult(
                    str(R.string.user_management_selected_success, user.name),
                    false
                )
                onUsersChanged()
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = UserManagementResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            } finally {
                busy = false
            }
        }
    }

    fun deleteUser(user: NativeUser, onUsersChanged: () -> Unit = {}) {
        if (busy) return

        busy = true
        viewModelScope.launch {
            try {
                val success = UserRepository.deleteUser(user.id)
                if (!success) {
                    operationResult = UserManagementResult(
                        str(R.string.user_management_delete_failed),
                        true
                    )
                    return@launch
                }

                users = UserRepository.getUsers()
                operationResult = UserManagementResult(
                    str(R.string.user_management_deleted_success, user.name),
                    false
                )
                onUsersChanged()
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = UserManagementResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            } finally {
                busy = false
            }
        }
    }

    fun dismissOperationResult() {
        operationResult = null
    }
}
