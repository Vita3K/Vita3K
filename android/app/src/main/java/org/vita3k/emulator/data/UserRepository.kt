package org.vita3k.emulator.data

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.vita3k.emulator.NativeLib

internal object UserRepository {

    suspend fun getUsers(): List<NativeUser> = withContext(Dispatchers.IO) {
        NativeLib.getUsers().toList()
    }

    suspend fun createUser(name: String): String = withContext(Dispatchers.IO) {
        NativeLib.createUser(name)
    }

    suspend fun activateUser(userId: String): Boolean = withContext(Dispatchers.IO) {
        NativeLib.activateUser(userId)
    }

    suspend fun deleteUser(userId: String): Boolean = withContext(Dispatchers.IO) {
        NativeLib.deleteUser(userId)
    }
}
