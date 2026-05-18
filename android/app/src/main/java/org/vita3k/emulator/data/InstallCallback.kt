package org.vita3k.emulator.data

interface InstallCallback {
    fun onProgress(percent: Int, status: String)
}
