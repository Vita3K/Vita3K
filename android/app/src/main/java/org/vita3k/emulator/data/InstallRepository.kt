package org.vita3k.emulator.data

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.vita3k.emulator.NativeLib

internal object InstallRepository {

    suspend fun installFirmware(path: String, onProgress: (Int, String) -> Unit): String =
        withContext(Dispatchers.IO) {
            NativeLib.installFirmware(path, progressCallback(onProgress))
        }

    suspend fun installPkg(path: String, zrif: String = "", onProgress: (Int, String) -> Unit): Boolean =
        withContext(Dispatchers.IO) {
            NativeLib.installPkg(path, zrif, progressCallback(onProgress))
        }

    suspend fun installArchive(path: String, forceReinstall: Boolean, onProgress: (Int, String) -> Unit): Boolean =
        withContext(Dispatchers.IO) {
            NativeLib.installArchive(path, progressCallback(onProgress), forceReinstall)
        }

    suspend fun copyLicense(path: String): Boolean = withContext(Dispatchers.IO) {
        NativeLib.copyLicense(path)
    }

    suspend fun createLicense(zrif: String): Boolean = withContext(Dispatchers.IO) {
        NativeLib.createLicense(zrif)
    }

    suspend fun findPkgZrif(pkgPath: String): String = withContext(Dispatchers.IO) {
        NativeLib.findPkgZrif(pkgPath)
    }

    suspend fun convertRifToZrif(path: String): String = withContext(Dispatchers.IO) {
        NativeLib.convertRifToZrif(path)
    }

    private fun progressCallback(onProgress: (Int, String) -> Unit) = object : InstallCallback {
        override fun onProgress(percent: Int, status: String) {
            onProgress(percent, status)
        }
    }
}
