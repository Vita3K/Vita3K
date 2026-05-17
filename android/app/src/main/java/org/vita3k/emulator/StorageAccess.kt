package org.vita3k.emulator

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.Settings
import androidx.core.content.ContextCompat
import androidx.documentfile.provider.DocumentFile

object StorageAccess {

    @JvmStatic
    fun hasStorageAccess(context: Context): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Environment.isExternalStorageManager()
        } else {
            missingStoragePermissions(context).isEmpty()
        }
    }

    @JvmStatic
    fun missingStoragePermissions(context: Context): Array<String> {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return emptyArray()
        }

        return arrayOf(
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE
        ).filter { permission ->
            ContextCompat.checkSelfPermission(context, permission) != PackageManager.PERMISSION_GRANTED
        }.toTypedArray()
    }

    @JvmStatic
    fun createManageAllFilesIntent(context: Context): Intent {
        return Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION)
            .setData(Uri.parse("package:${context.packageName}"))
    }

    @JvmStatic
    fun createFilePickerIntent(mimeTypes: Array<String>): Intent {
        val normalizedMimeTypes = mimeTypes
            .map(String::trim)
            .filter(String::isNotEmpty)
            .distinct()

        val primaryMimeType = if (normalizedMimeTypes.size == 1) {
            normalizedMimeTypes.first()
        } else {
            "*/*"
        }

        return Intent(Intent.ACTION_GET_CONTENT)
            .addCategory(Intent.CATEGORY_OPENABLE)
            .setType(primaryMimeType)
            .putExtra(Intent.EXTRA_LOCAL_ONLY, true)
            .apply {
                if (normalizedMimeTypes.size > 1) {
                    putExtra(Intent.EXTRA_MIME_TYPES, normalizedMimeTypes.toTypedArray())
                }
            }
    }

    @JvmStatic
    fun createInstallFilePickerIntent(): Intent {
        return Intent(Intent.ACTION_GET_CONTENT)
            .addCategory(Intent.CATEGORY_OPENABLE)
            .setType("*/*")
    }

    @JvmStatic
    fun createFolderPickerIntent(): Intent {
        return Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
            .putExtra(Intent.EXTRA_LOCAL_ONLY, true)
    }

    @JvmStatic
    fun createInstallFolderPickerIntent(): Intent {
        return Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
    }

    @JvmStatic
    fun resolveUriToPath(context: Context, uri: Uri): String? {
        return runCatching {
            val descriptor = context.contentResolver.openFileDescriptor(uri, "r") ?: return null
            val rawPath = descriptor.use { android.system.Os.readlink("/proc/self/fd/${it.fd}") }
            normalizeResolvedPath(rawPath)
        }.getOrNull()
    }

    @JvmStatic
    fun resolveTreeUriToPath(context: Context, treeUri: Uri): String? {
        val resolvedTreeUri = DocumentFile.fromTreeUri(context, treeUri)?.uri ?: treeUri
        return resolveUriToPath(context, resolvedTreeUri)
    }

    @JvmStatic
    fun resolveTreeFilePaths(context: Context, treeUri: Uri, allowedExtensions: Set<String>): List<String> {
        val tree = DocumentFile.fromTreeUri(context, treeUri) ?: return emptyList()
        val normalizedExtensions = allowedExtensions
            .map(String::lowercase)
            .toSet()

        return tree.listFiles()
            .asSequence()
            .filter(DocumentFile::isFile)
            .filter { document -> matchesAllowedExtension(document.name, normalizedExtensions) }
            .mapNotNull { document -> resolveUriToPath(context, document.uri) }
            .filter(String::isNotBlank)
            .distinct()
            .sorted()
            .toList()
    }

    @JvmStatic
    fun matchesAllowedExtension(pathOrName: String?, allowedExtensions: Set<String>): Boolean {
        if (allowedExtensions.isEmpty()) {
            return true
        }

        val lowerName = pathOrName?.lowercase() ?: return false
        return allowedExtensions.any(lowerName::endsWith)
    }

    private fun normalizeResolvedPath(rawPath: String): String {
        if (!rawPath.startsWith("/mnt/user/")) {
            return rawPath
        }

        val relativePath = rawPath.removePrefix("/mnt/user/")
        return "/storage" + relativePath.substring(relativePath.indexOf('/'))
    }
}
