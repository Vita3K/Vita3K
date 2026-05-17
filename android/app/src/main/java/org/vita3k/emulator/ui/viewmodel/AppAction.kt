package org.vita3k.emulator.ui.viewmodel

import androidx.annotation.StringRes
import org.vita3k.emulator.R

enum class AppActionGroup(
    @StringRes val labelResId: Int
) {
    DELETE(R.string.action_group_delete),
    OTHER(R.string.action_group_other)
}

enum class AppAction(
    @StringRes val labelResId: Int,
    val destructive: Boolean,
    val group: AppActionGroup,
    val maskBit: Int
) {
    DELETE_APPLICATION(R.string.app_action_application, true, AppActionGroup.DELETE, 1 shl 0),
    DELETE_SAVE_DATA(R.string.app_action_save_data, true, AppActionGroup.DELETE, 1 shl 1),
    DELETE_PATCH(R.string.app_action_patch, true, AppActionGroup.DELETE, 1 shl 2),
    DELETE_DLC(R.string.app_action_dlc, true, AppActionGroup.DELETE, 1 shl 3),
    DELETE_LICENSE(R.string.app_action_license, true, AppActionGroup.DELETE, 1 shl 4),
    DELETE_SHADER_CACHE(R.string.app_action_shader_cache, true, AppActionGroup.DELETE, 1 shl 5),
    DELETE_SHADER_LOG(R.string.app_action_shader_log, true, AppActionGroup.DELETE, 1 shl 6),
    DELETE_EXPORT_TEXTURES(R.string.app_action_export_textures, true, AppActionGroup.DELETE, 1 shl 7),
    DELETE_IMPORT_TEXTURES(R.string.app_action_import_textures, true, AppActionGroup.DELETE, 1 shl 8),
    RESET_LAST_PLAYED(R.string.app_action_reset_last_played, false, AppActionGroup.OTHER, 1 shl 9);

    companion object {
        val all: Set<AppAction> = entries.toSet()

        fun fromMask(mask: Int): Set<AppAction> {
            return entries.filterTo(linkedSetOf()) { action ->
                (mask and action.maskBit) != 0
            }
        }
    }
}
