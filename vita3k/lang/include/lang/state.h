// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <ime/types.h>

#include <map>
#include <string>
#include <vector>

enum LiveArea {
    START,
    CONTINUE,
};

enum TypeLang {
    GUI,
    LIVE_AREA,
};

struct DialogLangState {
    std::map<std::string, std::string> common = {
        { "cancel", "Cancel" },
        { "delete", "Delete" },
        { "file_corrupted", "The file is corrupt." },
        { "no", "No" },
        { "select_all", "Select All" },
        { "select", "Select" },
        { "please_wait", "Please wait..." },
        { "yes", "Yes" }
    };
    std::map<std::string, std::string> trophy = { { "preparing_start_app", "Preparing to start the application..." } };
    struct SaveData {
        std::map<std::string, std::string> deleting = {
            { "cancel_deleting", "Do you want to cancel deleting?" },
            { "deletion_complete", "Deletion complete." },
            { "delete_saved_data", "Do you want to delete this saved data?" },
        };
        std::map<std::string, std::string> info = {
            { "details", "Details" },
            { "updated", "Updated" }
        };
        std::map<std::string, std::string> load = {
            { "title", "Load" },
            { "cancel_loading", "Do you want to cancel loading?" },
            { "no_saved_data", "There is no saved data." },
            { "load_complete", "Loading complete." },
            { "loading", "Loading..." },
            { "load_saved_data", "Do you want to load this saved data?" }
        };
        std::map<std::string, std::string> save = {
            { "title", "Save" },
            { "cancel_saving", "Do you want to cancel saving?" },
            { "could_not_save", "Could not save the file.\nThere is not enough free space on the memory card." },
            { "not_free_space", "There is not enough free space on the memory card." },
            { "new_saved_data", "New Saved Data" },
            { "saving_complete", "Saving complete." },
            { "save_the_data", "Do you want to save the data?" },
            { "saving", "Saving..." },
            { "warning_saving", "Saving...\nDo not power off the system or close the application." },
            { "overwrite_saved_data", "Do you want to overwrite this saved data?" }
        };
    };
    SaveData save_data;
};

struct ImeLangState {
    std::vector<std::pair<SceImeLanguage, std::string>> ime_keyboards = {
        { SCE_IME_LANGUAGE_DANISH, "Danish" }, { SCE_IME_LANGUAGE_GERMAN, "German" },
        { SCE_IME_LANGUAGE_ENGLISH_GB, "English (United Kingdom)" }, { SCE_IME_LANGUAGE_ENGLISH_US, "English (United States)" },
        { SCE_IME_LANGUAGE_SPANISH, "Spanish" }, { SCE_IME_LANGUAGE_FRENCH, "French" },
        { SCE_IME_LANGUAGE_ITALIAN, "Italian" }, { SCE_IME_LANGUAGE_DUTCH, "Dutch" },
        { SCE_IME_LANGUAGE_NORWEGIAN, "Norwegian" }, { SCE_IME_LANGUAGE_POLISH, "Polish" },
        { SCE_IME_LANGUAGE_PORTUGUESE_BR, "Portuguese (Brazil)" }, { SCE_IME_LANGUAGE_PORTUGUESE_PT, "Portuguese (Portugal)" },
        { SCE_IME_LANGUAGE_RUSSIAN, "Russian" }, { SCE_IME_LANGUAGE_FINNISH, "Finnish" },
        { SCE_IME_LANGUAGE_SWEDISH, "Swedish" }, { SCE_IME_LANGUAGE_TURKISH, "Turkish" }
    };
};

struct LangState {
    struct MainMenubar {
        std::map<std::string, std::string> file = {
            { "title", "File" },
            { "open_pref_path", "Open Pref Path" },
            { "install_firmware", "Install Firmware" },
            { "install_pkg", "Install .pkg" },
            { "install_zip", "Install .zip, .vpk" },
            { "install_license", "Install License" }
        };
        std::map<std::string, std::string> emulation = {
            { "title", "Emulation" },
            { "last_apps_used", "Last Apps used" }
        };
        std::map<std::string, std::string> configuration = {
            { "title", "Configuration" },
            { "settings", "Settings" },
            { "user_management", "User Management" }
        };
        std::map<std::string, std::string> controls = {
            { "title", "Controls" },
            { "keyboard_controls", "Keyboard Controls" }
        };
        std::map<std::string, std::string> help = {
            { "title", "Help" },
            { "welcome", "Welcome" }
        };
    };
    MainMenubar main_menubar;
    std::map<std::string, std::string> about = {
        { "title", "About" },
        { "vita3k", "Vita3K: a PS Vita/PS TV Emulator. The world's first functional PS Vita/PS TV emulator." },
        { "about_vita3k", "Vita3K is an experimental open-source PlayStation Vita/PlayStation TV emulator written in C++ for Windows and Linux operating systems." },
        { "note", "Note: The emulator is still in a very early stage of development." },
        { "github_website", "If you're interested in contributing, check out our GitHub:" },
        { "vita3k_website", "Visit our website for more info:" },
        { "vita3k_staff", "Vita3K Staff" },
        { "developers", "Developers" },
        { "contributors", "Contributors" }
    };
    std::map<std::string, std::string> app_context = {
        { "boot", "Boot" },
        { "check_app_compatibility", "Check App Compatibility" },
        { "copy_app_info", "Copy App Info" },
        { "id_and_name", "ID and Name" },
        { "custom_config", "Custom Config" },
        { "create", "Create" },
        { "edit", "Edit" },
        { "remove", "Remove" },
        { "open_folder", "Open Folder" },
        { "addcont", "DLCs" },
        { "license", "License" },
        { "shader_cache", "Shader Cache" },
        { "shader_log", "Shader Log" },
        { "search", "Search" },
        { "manual", "Manual" },
        { "update", "Update" },
        { "update_history", "Update History" },
        { "history_version", "Version {}" },
        { "information", "Information" },
        { "app_delete", "This application and all related data, including saved data, will be deleted." },
        { "app_delete_note", "Deleting a application may take a while\ndepending on its size and your hardware." },
        { "save_delete", "Do you want to delete this saved data?" },
        { "eligible", "Eligible" },
        { "ineligible", "Ineligible" },
        { "level", "Level" },
        { "name", "Name" },
        { "trophy_earning", "Trophy Earning" },
        { "parental_Controls", "Parental Controls" },
        { "updated", "Updated" },
        { "size", "Size" },
        { "version", "Version" },
        { "last_time_used", "Last time used" },
        { "never", "Never" },
        { "time_used", "Time used" }
    };
    struct ContentManager {
        std::map<std::string, std::string> main = {
            { "title", "Content Manager" },
            { "search", "Search" },
            { "theme", "Themes" },
            { "free_space", "Free Space" },
            { "clear_all", "Clear All" }
        };
        std::map<std::string, std::string> application = {
            { "title", "Application" },
            { "delete", "The selected applications and all related data, including saved data, will be deleted." },
            { "no_item", "There are no content items." }
        };
        std::map<std::string, std::string> saved_data = {
            { "title", "Saved Data" },
            { "delete", "The selected saved data items will be deleted." },
            { "no_saved_data", "There is no saved data." }
        };
    };
    ContentManager content_manager;
    std::map<std::string, std::string> controllers = {
        { "title", "Controllers" },
        { "connected", "Controllers connected" },
        { "name", "Name" },
        { "num", "Num" },
        { "not_connected", "No compatible controllers connected.\nConnect a controller that is compatible with SDL2." }
    };
    std::map<std::string, std::string> controls = {
        { "title", "Controls" },
        { "button", "Button" },
        { "mapped_button", "Mapped button" },
        { "left_stick_up", "Left stick up" },
        { "left_stick_down", "Left stick down" },
        { "left_stick_right", "Left stick right" },
        { "left_stick_left", "Left stick left" },
        { "right_stick_up", "Right stick up" },
        { "right_stick_down", "Right stick down" },
        { "right_stick_right", "Right stick right" },
        { "right_stick_left", "Right stick left" },
        { "d_pad_up", "D-pad up" },
        { "d_pad_down", "D-pad down" },
        { "d_pad_right", "D-pad right" },
        { "d_pad_left", "D-pad left" },
        { "square_button", "Square button" },
        { "cross_button", "Cross button" },
        { "circle_button", "Circle button" },
        { "triangle_button", "Triangle button" },
        { "start_button", "Start button" },
        { "select_button", "Select button" },
        { "ps_button", "PS button" },
        { "l1_button", "L1 button" },
        { "r1_button", "R1 button" },
        { "ps_tv_mode", "Only in PS TV mode." },
        { "l2_button", "L2 button" },
        { "r2_button", "R2 button" },
        { "l3_button", "L3 button" },
        { "r3_button", "R3 button" },
        { "gui", "GUI" },
        { "full_screen", "Full Screen" },
        { "toggle_touch", "Toggle Touch" },
        { "toggle_gui_visibility", "Toggle GUI Visibility" }
    };
    std::map<std::string, std::string> game_data = {
        { "app_close", "The following application will close." },
        { "data_delete", "Data to be Deleted:" }
    };
    std::map<std::string, std::string> home_screen = {
        { "filter", "Filter" },
        { "sort_app", "Sort Apps By" },
        { "all", "All" },
        { "by_region", "By Region" },
        { "usa", "USA" },
        { "europe", "Europe" },
        { "japan", "Japan" },
        { "asia", "ASIA" },
        { "by_type", "By Type" },
        { "commercial", "Commercial" },
        { "homebrew", "Homebrew" },
        { "ver", "Ver" },
        { "cat", "Cat" },
        { "last_time", "Last Time" },
        { "tit", "Title" },
        { "tit_id", "Title ID" },
        { "refresh", "Refresh" }
    };
    std::map<std::string, std::string> indicator = {
        { "app_added_home", "The application has been added to the home screen." },
        { "delete_all", "Delete All" },
        { "notif_deleted", "The notifications will be deleted." },
        { "install_failed", "Could not install." },
        { "install_complete", "Installation complete." },
        { "installing", "Installing..." },
        { "no_notif", "There are no notifications." },
        { "trophy_earned", "You have earned a trophy!" }
    };
    std::map<std::string, std::string> initial_setup = {
        { "back", "Back" },
        { "completed_setup", "You have now completed initial setup.\nYour Vita3K system is ready!" },
        { "select_language", "Select a language" },
        { "completed", "Completed." },
        { "next", "Next" }
    };
    std::map<std::string, std::string> install_dialog = {
        { "fw_installing", "Installation in progress, please wait..." },
        { "successed_install_fw", "Firmware successfully installed." },
        { "fw_version", "Firmware version:" },
        { "no_font_exist", "No firmware font package present.\nPlease download and install it." },
        { "download_firmware_font_package", "Download Firmware Font Package" },
        { "firmware_font_package_note", "Firmware font package is mandatory for some applications and also for Asian regional font support. (Generally Recommended)" },
        { "delete_fw", "Delete the firmware installation file?" },
        { "select_key_type", "Select key type" },
        { "select_work", "Select work.bin" },
        { "enter_zrif", "Enter zRIF" },
        { "enter_zrif_key", "Enter zRIF key" },
        { "input_zrif", "Please input your zRIF here" },
        { "copy_paste_zrif", "Ctrl(Cmd) + C to copy, Ctrl(Cmd) + V to paste." },
        { "delete_pkg", "Delete the pkg file?" },
        { "delete_work", "Delete the work.bin file?" },
        { "check_log", "Please check log for more details." },
        { "select_install_type", "Select install type" },
        { "select_file", "Select File" },
        { "select_directory", "Select Directory" },
        { "compatible_content", "{} archive(s) found with compatible contents." },
        { "successed_install_archive", "{} archive(s) contents successfully installed:" },
        { "update_app", "Update App to:" },
        { "failed_install_archive", "Failed to install {} archive(s) contents:" },
        { "not_compatible_content", "No compatible content found in {} archive(s):" },
        { "delete_archive", "Delete archive?" },
        { "select_license_type", "Select license type" },
        { "select_bin_rif", "Select work.bin/rif" },
        { "successed_install_license", "Successfully installed license." },
        { "content_id", "Content ID:" },
        { "title_id", "Title ID:" },
        { "delete_bin_rif", "Delete the work.bin/rif file?" }
    };
    struct Settings {
        std::map<std::string, std::string> main = { { "title", "Settings" } };
        struct ThemeBackground {
            std::map<std::string, std::string> main = {
                { "title", "Theme & Background" },
                { "default", "Default" }
            };
            struct Theme {
                std::map<std::string, std::string> main = {
                    { "title", "Theme" },
                    { "search", "Search" },
                    { "find_in_altervista", "Find in Altervista" },
                    { "delete", "This theme will be deleted." }
                };
                std::map<std::string, std::string> information = {
                    { "title", "Information" },
                    { "name", "Name" },
                    { "provider", "Provider" },
                    { "updated", "Updated" },
                    { "size", "Size" },
                    { "version", "Version" },
                    { "content_id", "Content ID" }
                };
            };
            Theme theme;
            std::map<std::string, std::string> start_screen = {
                { "title", "Start Screen" },
                { "image", "Image" }
            };
            std::map<std::string, std::string> home_screen_backgrounds = {
                { "title", "Home Screen Backgrounds" },
                { "delete_background", "Delete Background" },
                { "add_background", "Add Background" }
            };
        };
        ThemeBackground theme_background;
        struct DateTime {
            std::map<std::string, std::string> main = { { "title", "Date & Time" } };
            std::map<std::string, std::string> date_format = {
                { "title", "Date Format" },
                { "yyyy_mm_dd", "YYYY/MM/DD" },
                { "dd_mm_yyyy", "DD/MM/YYYY" },
                { "mm_dd_yyyy", "Date Format" }
            };
            std::map<std::string, std::string> time_format = {
                { "title", "Time Format" },
                { "clock_12_hour", "12-Hour Clock" },
                { "clock_24_hour", "24-Hour Clock" }
            };
        };
        DateTime date_time;
        struct Language {
            std::map<std::string, std::string> main = {
                { "title", "Language" },
                { "system_language", "System Language" }
            };
            std::map<std::string, std::string> input_language = { { "title", "Input Languages" } };
            std::map<std::string, std::string> keyboards = { { "title", "Keyboards" } };
        };
        Language language;
    };
    Settings settings;
    struct SettingsDialog {
        std::map<std::string, std::string> core = {
            { "title", "Core" },
            { "modules_mode", "Modules Mode" },
            { "modules_list", "Modules List" },
            { "select_modules", "Select your desired modules." },
            { "search_modules", "Search Modules" },
            { "clear_list", "Clear List" },
            { "no_modules", "No modules present.\nPlease download and install the last PS Vita firmware." },
            { "refresh_list", "Refresh List" }
        };
        std::map<std::string, std::string> cpu = {
            { "cpu_backend", "CPU Backend" },
            { "select_cpu_backend", "Select your preferred CPU backend." },
            { "enable_optimizations", "Enable optimizations" },
            { "enable_cpu_jit", "Check the box to enable additional CPU JIT optimizations." }
        };
        std::map<std::string, std::string> gpu = {
            { "reset", "Reset" },
            { "backend_renderer", "Backend Renderer" },
            { "select_backend_renderer", "Select your preferred backend renderer." },
            { "gpu", "GPU (Reboot to apply)" },
            { "select_gpu", "Select the GPU Vita3K should run on." },
            { "internal_resolution_upscaling", "Internal Resolution Upscaling" },
            { "enable_upscaling", "Enable upscaling for Vita3K.\nExperimental: games are not guaranteed to render properly at more than 1x." },
            { "disable_surface_sync", "Disable surface sync" },
            { "surface_sync_note", "Speed hack, check the box to disable surface syncing between CPU and GPU.\nSurface syncing is needed by a few games.\nGive a big performance boost if disabled (in particular when upscaling is on)." },
            { "enable_fxaa", "Enable anti-aliasing (FXAA)" },
            { "fxaa_note", "Anti-aliasing is a technique for smoothing out jagged edges.\n FXAA comes at almost no performance cost but makes games look slightly blurry." },
            { "v_sync", "V-Sync" },
            { "v_sync_note", "Disabling V-Sync can fix the speed issue in some games.\nIt is recommended to keep it enabled to avoid tearing." },
            { "anisotropic_filtering", "Anisotropic Filtering" },
            { "anisotropic_filtering_note", "Anisotropic filtering is a technique to enhance the image quality of surfaces which are slopped relative to the viewer.\nIt has no drawback but can impact performance." },
            { "shaders", "Shaders" },
            { "use_shader_cache", "Use shader cache" },
            { "shader_cache_note", "Check the box to enable shader cache to pre-compile it at game startup.\nUncheck to disable this feature." },
            { "use_spir_v_shader", "Use Spir-V shader (deprecated)" },
            { "spir_v_shader_note", "Pass generated Spir-V shader directly to driver.\nNote that some beneficial extensions will be disabled,\nand not all GPUs are compatible with this." },
            { "clean_shaders", "Clean Shaders Cache and Log" }
        };
        std::map<std::string, std::string> system = {
            { "title", "System" },
            { "select_enter_button", "Enter button assignment \nSelect your 'Enter' button." },
            { "enter_button_note", "This is the button that is used as 'Confirm' in applications dialogs. \nSome applications don't use this and get default confirmation button." },
            { "circle", "Circle" },
            { "cross", "Cross" },
            { "pstv_mode", "PS TV Mode" },
            { "pstv_mode_note", "Check the box to enable PS TV Emulated mode." }
        };
        std::map<std::string, std::string> emulator = {
            { "title", "Emulator" },
            { "enable_full_screen", "Boot apps in full screen" },
            { "enable_ngs_support", "Enable NGS support" },
            { "ngs_note", "Uncheck the box to disable support for advanced audio library NGS." },
            { "log_level", "Log Level" },
            { "select_log_level", "Select your preferred log level." },
            { "archive_log", "Archive Log" },
            { "enable_archive_log", "Check the box to enable Archive Log." },
            { "discord_rich_presence", "Enables Discord Rich Presence to show what application you're running on Discord." },
            { "texture_cache", "Texture Cache" },
            { "disable_texture_cache", "Uncheck the box to disable texture cache." },
            { "performance_overlay", "Performance overlay" },
            { "display_information", "Display performance information on the screen as an overlay." },
            { "detail", "Detail" },
            { "select_detail", "Select your preferred perfomance overley detail." },
            { "position", "Position" },
            { "select_position", "Select your preferred perfomance overley position." },
            { "enable_case_insensitive", "Check to enable case-insensitive path finding on case sensitive filesystems. \nRESETS ON RESTART." },
            { "allow_emu_searching", "Allows emulator to attempt searching for files regardless of case on non-Windows platforms." },
            { "emu_storage_folder", "Emulated System Storage Folder" },
            { "current_emu_path", "Current emulator path:" },
            { "change_emu_path", "Change Emulator Path" },
            { "change_emu_path_note", "Change Vita3K emulator folder path.\nYou will need to move your old folder to the new location manually." },
            { "reset_emu_path", "Reset Emulator Path" },
            { "reset_emu_path_note", "Reset Vita3K emulator path to default.\nYou will need to move your old folder to the default location manually." }
        };
        std::map<std::string, std::string> gui = {
            { "title", "GUI" },
            { "gui_visible", "GUI Visible" },
            { "gui_visible_note", "Check the box to show GUI after booting an application." },
            { "info_bar_visible", "Info Bar Visible" },
            { "info_bar_visible_note", "Check the box to show an info bar inside app selector." },
            { "livearea_app_screen", "Live Area App Screen" },
            { "livearea_app_screen_note", "Check the box to open the Live Area by default when clicking on an application.\nIf disabled, right click on an application to open it." },
            { "grid_mode", "Grid Mode" },
            { "grid_mode_note", "Check the box to set the app list to grid mode." },
            { "app_icon_size", "App Icon Size" },
            { "select_icon_size", "Select your preferred icon size." },
            { "font_support", "Font support" },
            { "asia_region", "Asia Region" },
            { "enable_font_support", "Check this box to enable font support for Korean and Traditional Chinese.\nEnabling this will use more memory and will require you to restart the emulator." },
            { "theme_background", "Theme & Background" },
            { "current_theme_content_id", "Current theme content id:" },
            { "reset_default_theme", "Reset Default Theme" },
            { "using_theme_background", "Using theme background" },
            { "clean_user_backgrounds", "Clean User Backgrounds" },
            { "current_start_background", "Current start background:" },
            { "reset_start_background", "Reset Start Background" },
            { "background_alpha", "Background Alpha" },
            { "transparent_background_effect", "Select your preferred transparent background effect.\nThe minimum slider is opaque and the maximum is transparent." },
            { "delay_backgrounds", "Delay for backgrounds" },
            { "changing_backgrounds", "Select the delay in seconds before changing backgrounds." },
            { "delay_start_screen", "Delay for start screen" },
            { "returning_start_screen", "Select the delay in seconds before returning to the start screen." }
        };
        std::map<std::string, std::string> button = {
            { "save_reboot", "Save & Reboot" },
            { "save_apply", "Save & Apply" },
            { "save", "Save" },
            { "keep_changes", "Click on Save to keep your changes." }
        };
    };
    SettingsDialog settings_dialog;
    std::map<std::string, std::string> trophy_collection = {
        { "search", "Search" },
        { "delete_trophy", "Delete Trophy" },
        { "trophy_deleted", "This trophy information saved on this user will be deleted." },
        { "details", "Details" },
        { "earned", "Earned" },
        { "name", "Name" },
        { "no_trophies", "There are no trophies.\nYou can earn trophies by using an application that supports the trophy feature." },
        { "not_earned", "Not Earned" },
        { "original", "Original" },
        { "sort", "Sort" },
        { "trophies", "Trophies" },
        { "grade", "Grade" },
        { "progress", "Progress" },
        { "updated", "Updated" }
    };
    std::map<std::string, std::string> user_management = {
        { "select_user", "Select your user" },
        { "create_user", "Create User" },
        { "user_created", "The following user has been created." },
        { "edit_user", "Edit User" },
        { "user_name_used", "This name is already in use." },
        { "delete_user", "Delete User" },
        { "user_delete", "Select the user you want to delete." },
        { "user_delete_msg", "The following user will be deleted." },
        { "user_delete_message", "If you delete the user, that user's saved data, trophies will be deleted." },
        { "user_delete_warn", "The user will be deleted.\nAre you sure you want to continue?" },
        { "user_deleted", "User deleted." },
        { "change_avatar", "Change Avatar" },
        { "reset_avatar", "Reset Avatar" },
        { "name", "Name" },
        { "user", "User" },
        { "confirm", "Confirm" },
        { "automatic_user_login", "Automatic User Login" }
    };
    std::map<std::string, std::string> vita3k_update = {
        { "title", "Vita3K Update" },
        { "new_version_available", "A new version of Vita3K is available." },
        { "back", "Back" },
        { "downloading", "Downloading...\nAfter the download is complete, Vita3K will restart automatically and then install the new Vita3K." },
        { "not_complete_update", "Could not complete the update." },
        { "next", "Next" },
        { "update_vita3k", "Do you want to update Vita3K" },
        { "later_version_already_installed", "The later version of Vita3K is already installed." },
        { "latest_version_already_installed", "The latest version of Vita3K is already installed." },
        { "new_features", "New Features in Version {}" },
        { "update", "Update" },
        { "version", "Version {}" }
    };
    std::map<std::string, std::string> welcome = {
        { "title", "Welcome to Vita3K" },
        { "line_first", "Vita3K PlayStation Vita Emulator" },
        { "line_second", "Vita3K is an open-source PlayStation Vita emulator written in C++ for Windows and Linux." },
        { "line_third", "The emulator is still in its early stages so any feedback and testing is greatly appreciated." },
        { "line_fourth", "To get started, please install the PS Vita firmware and font packages." },
        { "download_firmware", "Download Firmware" },
        { "line_sixth_part_one", "A comprehensive guide on how to set-up Vita3K can be found on the" },
        { "quickstart", "Quickstart" },
        { "line_sixth_part_two", "page." },
        { "line_seventh_part_one", "Consult the Commercial game" },
        { "compatibility", "compatibility" },
        { "line_seventh_part_two", "list and the Homebrew compatibility list to see what runs." },
        { "line_eighth", "Contributions are welcome!" },
        { "line_tenth", "Additional support can be found in the #help channel of the" },
        { "line_eleventh", "Vita3K does not condone piracy. You must dump your own games." },
        { "show_next_time", "Show next time" },
        { "close", "Close" }
    };
    struct Common {
        std::vector<std::string> wday = {
            "Sunday", "Monday", "Tuesday", "Wednesday",
            "Thursday", "Friday", "Saturday"
        };
        std::vector<std::string> ymonth = {
            "January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"
        };
        std::vector<std::string> small_ymonth = {
            "january", "february", "march", "april", "may", "june",
            "july", "august", "september", "october", "november", "december"
        };
        std::vector<std::string> mday = {
            "", "1", "2", "3", "4", "5",
            "6", "7", "8", "9", "10", "11",
            "12", "13", "14", "15", "16", "17",
            "18", "19", "20", "21", "22", "23",
            "24", "25", "26", "27", "28", "29",
            "30", "31"
        };
        std::vector<std::string> small_mday = {
            "", "1", "2", "3", "4", "5",
            "6", "7", "8", "9", "10", "11",
            "12", "13", "14", "15", "16", "17",
            "18", "19", "20", "21", "22", "23",
            "24", "25", "26", "27", "28", "29",
            "30", "31"
        };
        std::map<std::string, std::string> main = {
            { "hidden_trophy", "Hidden Trophy" },
            { "one_hour_ago", "1 Hour Ago" },
            { "one_minute_ago", "1 Minute Ago" },
            { "bronze", "Bronze" },
            { "gold", "Gold" },
            { "platinum", "Platinum" },
            { "silver", "Silver" },
            { "hours_ago", "{} Hours Ago" },
            { "minutes_ago", "{} Minutes Ago" }
        };
    };
    Common common;
    std::map<LiveArea, std::string> live_area = {
        { START, "Start" },
        { CONTINUE, "Continue" }
    };
    std::map<TypeLang, std::string> user_lang;
};
