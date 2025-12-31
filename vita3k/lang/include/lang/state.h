
// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <compat/state.h>
#include <ime/types.h>

#include <map>
#include <string>
#include <vector>

enum TypeLang {
    GUI,
    LIVE_AREA,
};

struct DialogLangState {
    std::map<std::string, std::string> common = {
        { "an_error_occurred", "An error occurred.\nError code: {}" },
        { "cancel", "Cancel" },
        { "download", "Download" },
        { "close", "Close" },
        { "delete", "Delete" },
        { "error", "Error" },
        { "file_corrupted", "The file is corrupt." },
        { "microphone_disabled", "Enable the microphone." },
        { "no", "No" },
        { "ok", "OK" },
        { "please_wait", "Please wait..." },
        { "search", "Search" },
        { "select_all", "Select All" },
        { "select", "Select" },
        { "submit", "Submit" },
        { "yes", "Yes" },
        { "download_failed", "Could not download." }
    };
    std::map<std::string, std::string> message = { { "load_app_failed", "Failed to load \"{}\".\nCheck vita3k.log to see console output for details.\n1. Have you installed the firmware?\n2. Re-dump your own PS Vita app/game and install it on Vita3K.\n3. If you want to install or boot Vitamin, it is not supported." } };
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
            { "open_textures_path", "Open Textures Path" },
            { "install_firmware", "Install Firmware" },
            { "install_pkg", "Install .pkg" },
            { "install_zip", "Install .zip, .vpk" },
            { "install_license", "Install License" },
            { "exit", "Exit" }
        };
        std::map<std::string, std::string> emulation = {
            { "title", "Emulation" },
            { "last_apps_used", "Last Apps used" },
            { "empty", "Empty" }
        };
        std::map<std::string, std::string> debug = {
            { "title", "Debug" },
            { "threads", "Threads" },
            { "semaphores", "Semaphores" },
            { "mutexes", "Mutexes" },
            { "lightweight_mutexes", "Lightweight Mutexes" },
            { "condition_variables", "Condition Variables" },
            { "lightweight_condition_variables", "Lightweight Condition Variables" },
            { "event_flags", "Event Flags" },
            { "memory_allocations", "Memory Allocations" },
            { "disassembly", "Disassembly" }
        };
        std::map<std::string, std::string> configuration = {
            { "title", "Configuration" },
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
        { "about_vita3k", "Vita3K is an experimental open-source PlayStation Vita/PlayStation TV emulator written in C++ for Windows, Linux, macOS and Android operating systems." },
        { "special_credit", "Special credit: The Vita3K icon was designed by:" },
        { "github_website", "If you're interested in contributing, check out our:" },
        { "vita3k_website", "Visit our website for more info:" },
        { "ko-fi_website", "If you want to support us, you can make a donation or subscribe to our:" },
        { "vita3k_staff", "Vita3K Staff" },
        { "developers", "Developers" },
        { "contributors", "Contributors" },
        { "supporters", "Supporters" }
    };
    struct AppContext {
        std::map<std::string, std::string> main = {
            { "check_app_state", "Check App state" },
            { "copy_vita3k_summary", "Copy Vita3K Summary" },
            { "open_state_report", "Open state report" },
            { "create_state_report", "Create state report" },
            { "update_database", "Update database" },
            { "copy_app_info", "Copy App info" },
            { "name_and_id", "Name and Title ID" },
            { "app_summary", "App Summary" },
            { "create_shortcut", "Create Shortcut" },
            { "custom_config", "Custom Config" },
            { "create", "Create" },
            { "edit", "Edit" },
            { "remove", "Remove" },
            { "open_folder", "Open Folder" },
            { "addcont", "DLC" },
            { "license", "License" },
            { "shaders_cache", "Shaders Cache" },
            { "shaders_log", "Shaders Log" },
            { "manual", "Manual" },
            { "update", "Update" },
            { "update_history", "Update History" },
            { "history_version", "Version {}" }
        };
        std::map<std::string, std::string> deleting = {
            { "app_delete", "This application and all related data, including saved data, will be deleted." },
            { "app_delete_description", "Deleting an application may take a while,\ndepending on its size and your hardware." },
            { "addcont_delete", "Do you want to delete this add-on data?" },
            { "license_delete", "Do you want to delete this license?" },
            { "saved_data_delete", "Do you want to delete this saved data?" }
        };
        std::map<std::string, std::string> info = {
            { "title", "Information" },
            { "eligible", "Eligible" },
            { "ineligible", "Ineligible" },
            { "level", "Level" },
            { "name", "Name" },
            { "trophy_earning", "Trophy Earning" },
            { "parental_controls", "Parental Controls" },
            { "updated", "Updated" },
            { "size", "Size" },
            { "version", "Version" },
            { "title_id", "Title ID" },
            { "last_time_used", "Last time used" },
            { "time_used", "Time used" },
            { "never", "Never" }
        };
        std::map<std::string, std::string> time_used = {
            { "time_used_seconds", "{}s" },
            { "time_used_minutes", "{}m:{}s" },
            { "time_used_hours", "{}h:{}m:{}s" },
            { "time_used_days", "{}d:{}h:{}m:{}s" },
            { "time_used_weeks", "{}w:{}d:{}h:{}m:{}s" }
        };
    };
    AppContext app_context;
    struct Compatibility {
        std::string name = "Compatibility";
        std::map<compat::CompatibilityState, std::string> states = {
            { compat::UNKNOWN, "Unknown" },
            { compat::NOTHING, "Nothing" },
            { compat::BOOTABLE, "Bootable" },
            { compat::INTRO, "Intro" },
            { compat::MENU, "Menu" },
            { compat::INGAME_LESS, "Ingame -" },
            { compat::INGAME_MORE, "Ingame +" },
            { compat::PLAYABLE, "Playable" }
        };
    };
    Compatibility compatibility;
    std::map<std::string, std::string> compat_db = {
        { "get_failed", "Failed to get current compatibility database, check firewall/internet access, try again later." },
        { "download_failed", "Failed to download Applications compatibility database updated at: {}" },
        { "load_failed", "Failed to load Applications compatibility database downloaded updated at: {}" },
        { "new_app_listed", "The compatibility database was successfully updated from {} to {}.\n\n{} new application(s) are listed!" },
        { "app_listed", "The compatibility database was successfully updated from {} to {}.\n\n{} applications are listed!" },
        { "download_app_listed", "The compatibility database updated at {} has been successfully downloaded and loaded.\n\n{} applications are listed!" }
    };
    std::map<std::string, std::string> compile_shaders = {
        { "compiling_shaders", "Compiling Shaders" },
        { "pipelines_compiled", "{} pipelines compiled" },
        { "shaders_compiled", "{} shaders compiled" }
    };
    struct ContentManager {
        std::map<std::string, std::string> main = {
            { "title", "Content Manager" },
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
        { "connected", "{} controllers connected" },
        { "num", "Num" },
        { "motion_support", "Motion Support" },
        { "rebind_controls", "Rebind Controls" },
        { "led_color", "LED Color" },
        { "use_custom_color", "Use Custom Color" },
        { "use_custom_color_description", "Check this box to use custom color for the controller's LED." },
        { "red", "Red" },
        { "green", "Green" },
        { "blue", "Blue" },
        { "not_connected", "No compatible controllers connected.\nConnect a controller that is compatible with SDL3." },
        { "disable_motion", "Disable Motion" },
        { "reset_controller_binding", "Reset Controller Binding" }
    };
    std::map<std::string, std::string> controls = {
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
        { "ps_tv_mode", "Only in PS TV mode" },
        { "l2_button", "L2 button" },
        { "r2_button", "R2 button" },
        { "l3_button", "L3 button" },
        { "r3_button", "R3 button" },
        { "gui", "GUI" },
        { "full_screen", "Full Screen" },
        { "toggle_touch", "Toggle Touch" },
        { "toggle_touch_description", "Toggles between back touch and screen touch." },
        { "toggle_gui_visibility", "Toggle GUI Visibility" },
        { "toggle_gui_visibility_description", "Toggles between showing and hiding the GUI at the top of the screen while the app is running." },
        { "miscellaneous", "Miscellaneous" },
        { "toggle_texture_replacement", "Toggle Texture Replacement" },
        { "take_screenshot", "Take A Screenshot" },
        { "error_duplicate_key", "The key is used for other bindings or it is reserved." }
    };
    std::map<std::string, std::string> game_data = {
        { "app_close", "The following application will close." },
        { "data_delete", "Data to be Deleted:" },
    };
    std::map<std::string, std::string> home_screen = {
        { "filter", "Filter" },
        { "sort_app", "Sort Apps By" },
        { "all", "All" },
        { "by_region", "By Region" },
        { "usa", "USA" },
        { "europe", "Europe" },
        { "japan", "Japan" },
        { "asia", "Asia" },
        { "by_type", "By Type" },
        { "commercial", "Commercial" },
        { "homebrew", "Homebrew" },
        { "by_compatibility_state", "By Compatibility State" },
        { "ver", "Ver" },
        { "cat", "Cat" },
        { "comp", "Comp" },
        { "last_time", "Last Time" },
        { "refresh", "Refresh" }
    };
    std::map<std::string, std::string> indicator = {
        { "app_added_home", "The application has been added to the home screen." },
        { "cancel_download", "Do you want to cancel this download ?" },
        { "cancel_install", "Do you want to cancel this installation?\nIf you cancel, the files downloaded for this installation will also be deleted." },
        { "delete_all", "Delete All" },
        { "notif_deleted", "The notifications will be deleted." },
        { "waiting_download", "Waiting to download..." },
        { "install_failed", "Could not install." },
        { "waiting_install", "Waiting to install..." },
        { "install_complete", "Installation complete." },
        { "installing", "Installing..." },
        { "minutes_left", "{} Minutes Left" },
        { "no_notif", "There are no notifications." },
        { "pause_all", "Pause All" },
        { "pause", "Pause" },
        { "paused", "Paused" },
        { "resume_all", "Resume All" },
        { "resume", "Resume" },
        { "seconds_left", "{} Seconds Left" },
        { "trophy_earned", "You have earned a trophy!" }
    };
    std::map<std::string, std::string> initial_setup = {
        { "back", "Back" },
        { "completed_setup", "You have now completed initial setup.\nYour Vita3K system is ready!" },
        { "select_language", "Select a language." },
        { "select_pref_path", "Select a pref path." },
        { "current_emu_path", "Current emulator path" },
        { "change_emu_path", "Change Emulator Path" },
        { "reset_emu_path", "Reset Emulator Path" },
        { "install_firmware", "Install Firmware." },
        { "install_highly_recommended", "Installing all firmware files is highly recommended." },
        { "installed", "Installed:" },
        { "download_font_package", "Download Font Package" },
        { "install_firmware_file", "Install Firmware File" },
        { "select_interface_settings", "Select interface settings." },
        { "show_info_bar", "Info Bar Visible" },
        { "info_bar_description", "Check the box to show info bar inside app selector.\nInfo bar is clock, battery level and notification center." },
        { "show_live_area_screen", "Live Area App Screen" },
        { "live_area_screen_description", "Check the box to open Live Area by default when clicking on an application.\nIf disabled, right click on an application to open it." },
        { "apps_list_grid", "Grid Mode" },
        { "apps_list_grid_description", "Check the box to set the app list to grid mode like of PS Vita." },
        { "icon_size", "App Icon Size" },
        { "select_icon_size", "Select your preferred icon size." },
        { "completed", "Completed." },
        { "next", "Next" }
    };
    struct InstallDialog {
        std::map<std::string, std::string> firmware_install = {
            { "firmware_installation", "Firmware Installation" },
            { "firmware_installing", "Installation in progress, please wait..." },
            { "successed_install_firmware", "Firmware successfully installed." },
            { "firmware_version", "Firmware version:" },
            { "no_font_exist", "No firmware font package present, please download and install it." },
            { "firmware_font_package_description", "Firmware font package is needed for some applications\nand also for Asian regional font support. (Generally Recommended)" },
            { "delete_firmware", "Delete the firmware installation file?" }
        };
        std::map<std::string, std::string> pkg_install = {
            { "select_license_type", "Select license type" },
            { "select_bin_rif", "Select work.bin/rif" },
            { "enter_zrif", "Enter zRIF" },
            { "enter_zrif_key", "Enter zRIF key" },
            { "input_zrif", "Please input your zRIF here" },
            { "copy_paste_zrif", "Ctrl (Cmd) + C to copy, Ctrl (Cmd) + V to paste." },
            { "delete_pkg", "Delete the pkg file?" },
            { "delete_bin_rif", "Delete the work.bin/rif file?" },
            { "failed_install_package", "Failed to install package.\nCheck pkg and work.bin/rif file or zRIF key." }
        };
        std::map<std::string, std::string> archive_install = {
            { "select_install_type", "Select install type" },
            { "select_file", "Select File" },
            { "select_directory", "Select Directory" },
            { "compatible_content", "{} archive(s) found with compatible contents." },
            { "successed_install_archive", "{} archive(s) contents successfully installed:" },
            { "update_app", "Update App to:" },
            { "failed_install_archive", "Failed to install {} archive(s) contents:" },
            { "not_compatible_content", "No compatible content found in {} archive(s):" },
            { "delete_archive", "Delete archive?" }
        };
        std::map<std::string, std::string> license_install = {
            { "successed_install_license", "Successfully installed license." },
            { "failed_install_license", "Failed to install license.\nCheck work.bin/rif file or zRIF key." }
        };
        std::map<std::string, std::string> reinstall = {
            { "reinstall_content", "Reinstall this content?" },
            { "already_installed", "This content is already installed." },
            { "reinstall_overwrite", "Do you want to reinstall it and overwrite existing data?" }
        };
    };
    InstallDialog install_dialog;
    struct LiveArea {
        std::map<std::string, std::string> main = {
            { "start", "Start" },
            { "continue", "Continue" }
        };
        std::map<std::string, std::string> help = {
            { "control_setting", "Using configuration set for keyboard in control setting" },
            { "firmware_not_detected", "Firmware not detected. Installation is highly recommended" },
            { "firmware_font_not_detected", "Firmware font not detected. Installing it is recommended for font text in Live Area" },
            { "live_area_help", "Live Area Help" },
            { "browse_app", "Browse in app list" },
            { "browse_app_control", "D-pad, Left Stick, Wheel in Up/Down or using Slider" },
            { "start_app", "Start App" },
            { "start_app_control", "Click on Start or Press on Cross" },
            { "show_hide", "Show/Hide Live Area during app run" },
            { "show_hide_control", "Press on PS" },
            { "exit_livearea", "Exit Live Area" },
            { "exit_livearea_control", "Click on Esc or Press on Circle" },
            { "manual_help", "Manual Help" },
            { "browse_page", "Browse page" },
            { "browse_page_control", "D-pad, Left Stick, Wheel in Up/Down or using Slider, Click on </>" },
            { "hide_show", "Hide/Show button" },
            { "hide_show_control", "Left Click or Press on Triangle" },
            { "exit_manual", "Exit Manual" },
            { "exit_manual_control", "Click on Esc or Press on PS" }
        };
    };
    LiveArea live_area;
    std::map<std::string, std::string> performance_overlay = {
        { "avg", "Avg" },
        { "min", "Min" },
        { "max", "Max" }
    };
    std::map<std::string, std::string> patch_check = {
        { "new_app_version_available", "A new version of the application is available." },
        { "downloading_app_update", "Downloading the application update file...\nYou can check the progress of the download in the notification list." },
        { "download", "Download" },
        { "not_enough_space", "Could not download.\nThere is not enough free space on the memory card.Create at least {} free space on the memory card, and then try to download again." },
        { "version", "Version {}" }
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
                    { "find_a_psvita_custom_themes", "Find a PSVita Custom Themes" },
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
                { "image", "Image" },
                { "add_image", "Add Image" }
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
        std::map<std::string, std::string> main_window = {
            { "title", "Settings" },
            { "save_reboot", "Save & Reboot" },
            { "save_apply", "Save & Apply" },
            { "keep_changes", "Click on Save to keep your changes." },
            { "save_close", "Save & Close" }
        };
        std::map<std::string, std::string> core = {
            { "title", "Core" },
            { "modules_mode", "Modules Mode" },
            { "modules_list", "Modules List" },
            { "select_modules", "Select your desired modules." },
            { "search_modules", "Search Modules" },
            { "clear_list", "Clear List" },
            { "no_modules", "No modules present.\nPlease download and install the last PS Vita firmware." },
            { "refresh_list", "Refresh List" },
            { "automatic", "Automatic" },
            { "automatic_description", "Select Automatic mode to use a preset list of modules." },
            { "auto_manual", "Auto & Manual" },
            { "auto_manual_description", "Select this mode to load Automatic module and selected modules from the list below." },
            { "manual", "Manual" },
            { "manual_description", "Select Manual mode to load selected modules from the list below." }
        };
        std::map<std::string, std::string> cpu = {
            { "cpu_opt", "Enable optimizations" },
            { "cpu_opt_description", "Check the box to enable additional CPU JIT optimizations." }
        };
        std::map<std::string, std::string> gpu = {
            { "reset", "Reset" },
            { "backend_renderer", "Backend Renderer" },
            { "select_backend_renderer", "Select your preferred backend renderer." },
            { "gpu", "GPU (Reboot to apply)" },
            { "select_gpu", "Select the GPU Vita3K should run on." },
            { "standard", "Standard" },
            { "high", "High" },
            { "renderer_accuracy", "Renderer Accuracy" },
            { "v_sync", "V-Sync" },
            { "v_sync_description", "Disabling V-Sync can fix the speed issue in some games.\nIt is recommended to keep it enabled to avoid visual tearing." },
            { "disable_surface_sync", "Disable Surface Sync" },
            { "surface_sync_description", "Speed hack, check the box to disable surface syncing between CPU and GPU.\nSurface syncing is needed by a few games.\nGives a big performance boost if disabled (in particular when upscaling is on)." },
            { "async_pipeline_compilation", "Asynchronous Pipeline Compilation" },
            { "async_pipeline_compilation_description", "Allow pipelines to be compiled concurrently on multiple concurrent threads.\nThis decreases pipeline compilation stutter at the cost of temporary graphical glitches." },
            { "nearest", "Nearest" },
            { "bilinear", "Bilinear" },
            { "bicubic", "Bicubic" },
            { "screen_filter", "Screen Filter" },
            { "screen_filter_description", "Set post-processing filter to apply." },
            { "internal_resolution_upscaling", "Internal Resolution Upscaling" },
            { "internal_resolution_upscaling_description", "Enable upscaling for Vita3K.\nExperimental: games are not guaranteed to render properly at more than 1x." },
            { "anisotropic_filtering", "Anisotropic Filtering" },
            { "anisotropic_filtering_description", "Anisotropic filtering is a technique to enhance the image quality of surfaces\nwhich are sloped relative to the viewer.\nIt has no drawback but can impact performance." },
            { "texture_replacement", "Texture Replacement" },
            { "export_textures", "Export Textures" },
            { "import_textures", "Import Textures" },
            { "texture_exporting_format", "Texture Exporting Format" },
            { "shaders", "Shaders" },
            { "shader_cache", "Use Shader Cache" },
            { "shader_cache_description", "Check the box to enable shader cache to pre-compile it at game startup.\nUncheck to disable this feature." },
            { "mapping_method", "Memory mapping method" },
            { "mapping_method_description", "Memory mapping improved performances, reduces memory usage and fixes many graphical issues.\nHowever, it may be unstable on some GPUs" },
            { "spirv_shader", "Use Spir-V Shader (deprecated)" },
            { "spirv_shader_description", "Pass generated Spir-V shader directly to driver.\nNote that some beneficial extensions will be disabled,\nand not all GPUs are compatible with this." },
            { "clean_shaders", "Clean Shaders Cache and Log" },
            { "fps_hack", "FPS Hack" },
            { "fps_hack_description", "Game hack which allows some games running at 30 FPS to run at 60 FPS on the emulator.\nNote that this is a hack and will only work on some games.\nOn other games, it may have no effect or make them run twice as fast." }
        };
        std::map<std::string, std::string> audio = {
            { "title", "Audio" },
            { "audio_backend", "Audio Backend" },
            { "select_audio_backend", "Select your preferred audio backend." },
            { "audio_volume", "Audio Volume" },
            { "audio_volume_description", "Adjusts the volume percentage of all audio outputs." },
            { "enable_ngs_support", "Enable NGS support" },
            { "ngs_description", "Uncheck the box to disable support for advanced audio library NGS." }
        };
        std::map<std::string, std::string> system = {
            { "title", "System" },
            { "select_enter_button", "Enter button assignment\nSelect your 'Enter' button." },
            { "enter_button_description", "This is the button that is used as 'Confirm' in applications dialogs.\nSome applications don't use this and get default confirmation button." },
            { "circle", "Circle" },
            { "cross", "Cross" },
            { "pstv_mode", "PS TV Mode" },
            { "pstv_mode_description", "Check the box to enable PS TV Emulated mode." },
            { "show_mode", "Show Mode" },
            { "show_mode_description", "Check the box to enable Show mode." },
            { "demo_mode", "Demo Mode" },
            { "demo_mode_description", "Check the box to enable Demo mode." }
        };
        std::map<std::string, std::string> emulator = {
            { "title", "Emulator" },
            { "boot_apps_full_screen", "Boot apps in full screen" },
            { "trace", "Trace" },
            { "info", "Info" },
            { "warning", "Warning" },
            { "error", "Error" },
            { "critical", "Critical" },
            { "off", "Off" },
            { "log_level", "Log Level" },
            { "select_log_level", "Select your preferred log level." },
            { "archive_log", "Archive Log" },
            { "archive_log_description", "Check the box to enable Archive Log." },
            { "discord_rich_presence", "Enables Discord Rich Presence to show what application you're running on Discord." },
            { "texture_cache", "Texture Cache" },
            { "texture_cache_description", "Uncheck the box to disable texture cache." },
            { "show_compile_shaders", "Show Compile Shaders" },
            { "compile_shaders_description", "Uncheck the box to disable the display of the compile shaders dialog." },
            { "show_touchpad_cursor", "Show Touchpad Cursor" },
            { "touchpad_cursor_description", "Uncheck the box to disable showing the touchpad cursor on-screen." },
            { "log_compat_warn", "Log Compatibility Warning" },
            { "log_compat_warn_description", "Check the box to enable log compatibility warning of GitHub issue." },
            { "check_for_updates", "Check for updates" },
            { "check_for_updates_description", "Automatically check for updates at startup." },
            { "performance_overlay", "Performance overlay" },
            { "performance_overlay_description", "Display performance information on the screen as an overlay." },
            { "minimum", "Minimum" },
            { "low", "Low" },
            { "medium", "Medium" },
            { "maximum", "Maximum" },
            { "detail", "Detail" },
            { "select_detail", "Select your preferred performance overlay detail." },
            { "top_left", "Top Left" },
            { "top_center", "Top Center" },
            { "top_right", "Top Right" },
            { "bottom_left", "Bottom Left" },
            { "bottom_center", "Bottom Center" },
            { "bottom_right", "Bottom Right" },
            { "position", "Position" },
            { "select_position", "Select your preferred performance overlay position." },
            { "case_insensitive", "Check to enable case-insensitive path finding on case sensitive filesystems.\nRESETS ON RESTART" },
            { "case_insensitive_description", "Allows emulator to attempt to search for files regardless of case\non non-Windows platforms." },
            { "emu_storage_folder", "Emulated System Storage Folder" },
            { "current_emu_path", "Current emulator path:" },
            { "change_emu_path", "Change Emulator Path" },
            { "change_emu_path_description", "Change Vita3K emulator folder path.\nYou will need to move your old folder to the new location manually." },
            { "reset_emu_path", "Reset Emulator Path" },
            { "reset_emu_path_description", "Reset Vita3K emulator path to the default.\nYou will need to move your old folder to the new location manually." },
            { "custom_config_settings", "Custom Config Settings" },
            { "clear_custom_config", "Clear Custom Config" },
            { "screenshot_image_type", "screenshot image type" },
            { "null", "NULL" },
            { "screenshot_format", "Screenshot format" }
        };
        std::map<std::string, std::string> gui = {
            { "title", "GUI" },
            { "show_gui", "GUI Visible" },
            { "gui_description", "Check the box to show GUI after booting an application." },
            { "show_info_bar", "Info Bar Visible" },
            { "info_bar_description", "Check the box to show an info bar inside the app selector." },
            { "user_lang", "GUI Language" },
            { "select_user_lang", "Select your user language." },
            { "display_info_message", "Display Info Message" },
            { "display_info_message_description", "Uncheck the box to display info message in log only." },
            { "display_system_apps", "Display System Apps" },
            { "display_system_apps_description", "Uncheck the box to disable the display of system apps on the home screen.\nThey will be shown in the main menu bar only." },
            { "show_live_area_screen", "Live Area App Screen" },
            { "live_area_screen_description", "Check the box to open the Live Area by default when clicking on an application\nIf disabled, right click on an application to open it." },
            { "stretch_the_display_area", "Stretch The Display Area" },
            { "stretch_the_display_area_description", "Check the box to enlarge the display area to fit the screen size." },
            { "fullscreen_hd_res_pixel_perfect", "Fullscreen HD res pixel perfect" },
            { "fullscreen_hd_res_pixel_perfect_description", "Check the box to get a pixel perfect rendering with HD resolutions (1080p, 4K) in fullscreen\nat the price of slight cropping at the top and the bottom of the screen." },
            { "apps_list_grid", "Grid Mode" },
            { "apps_list_grid_description", "Check the box to set the app list to grid mode." },
            { "icon_size", "App Icon Size" },
            { "select_icon_size", "Select your preferred icon size." },
            { "font_support", "Font support" },
            { "asia_font_support", "Asia Region" },
            { "asia_font_support_description", "Check this box to enable font support for Chinese and Korean.\nEnabling this will use more memory and will require you to restart the emulator." },
            { "firmware_font_package_description", "Firmware font package is mandatory for some applications\nand also for Asian region font support in GUI.\nIt is also generally recommended for GUI." },
            { "theme_background", "Theme & Background" },
            { "current_theme_content_id", "Current theme content id:" },
            { "reset_default_theme", "Reset Default Theme" },
            { "using_theme_background", "Using theme background" },
            { "clean_user_backgrounds", "Clean User Backgrounds" },
            { "current_start_background", "Current start background:" },
            { "reset_start_background", "Reset Start Background" },
            { "background_alpha", "Background Alpha" },
            { "select_background_alpha", "Select your preferred background transparency.\nThe minimum is opaque and the maximum is transparent." },
            { "delay_background", "Delay for backgrounds" },
            { "select_delay_background", "Select the delay (in seconds) before changing backgrounds." },
            { "delay_start", "Delay for start screen" },
            { "select_delay_start", "Select the delay (in seconds) before returning to the start screen." }
        };
        std::map<std::string, std::string> network = {
            { "title", "Network" },
            { "psn_signed_in", "Signed in PSN" },
            { "psn_signed_in_description", "If checked, games will consider the user is connected to the PSN network (but offline)." },
            { "ip_address", "IP Address" },
            { "ip_address_description", "Select the IP address to be used in adhoc." },
            { "subnet_mask", "Subnet Mask" },
            { "enable_http", "Enable HTTP" },
            { "enable_http_description", "Check this box to enable games to use the HTTP protocol on the internet." },
            { "timeout_attempts", "HTTP Timeout Attempts" },
            { "timeout_attempts_description", "How many attempts to do when the server doesn't respond.\nCould be useful if you have very unstable or VERY SLOW internet." },
            { "timeout_sleep", "HTTP Timeout Sleep" },
            { "timeout_sleep_description", "Attempt sleep time when the server doesn't answer.\nCould be useful if you have very unstable or VERY SLOW internet." },
            { "read_end_attempts", "HTTP Read End Attempts" },
            { "read_end_attempts_description", "How many attempts to do when there isn't more data to read,\nlower can improve performance but can make games unstable if you have bad enough internet." },
            { "read_end_sleep", "HTTP Read End Sleep" },
            { "read_end_sleep_description", "Attempt sleep time when there isn't more data to read,\nlower can improve performance but can make games unstable if you have bad enough internet." }
        };
        std::map<std::string, std::string> debug = {
            { "log_imports", "Import logging" },
            { "log_imports_description", "Log module import symbols." },
            { "log_exports", "Export logging" },
            { "log_exports_description", "Log module export symbols." },
            { "log_active_shaders", "Shader logging" },
            { "log_active_shaders_description", "Log shaders being used on each draw call." },
            { "log_uniforms", "Uniform logging" },
            { "log_uniforms_description", "Log shader uniform names and values." },
            { "color_surface_debug", "Save color surfaces" },
            { "color_surface_debug_description", "Save color surfaces to files." },
            { "dump_elfs", "ELF dumping" },
            { "dump_elfs_description", "Dump loaded code as ELFs." },
            { "validation_layer", "Validation Layer (Reboot required)" },
            { "validation_layer_description", "Enable Vulkan validation layer." },
            { "unwatch_code", "Unwatch Code" },
            { "watch_code", "Watch Code" },
            { "unwatch_memory", "Unwatch Memory" },
            { "watch_memory", "Watch Memory" },
            { "unwatch_import_calls", "Unwatch Import Calls" },
            { "watch_import_calls", "Watch Import Calls" }
        };
    };
    SettingsDialog settings_dialog;
    std::map<std::string, std::string> sys_apps_title = {
        { "browser", "Browser" },
        { "internet_browser", "Internet Browser" },
        { "trophies", "Trophies" },
        { "trophy_collection", "Trophy Collection" },
        { "settings", "Settings" },
        { "content_manager", "Content Manager" }
    };
    std::map<std::string, std::string> trophy_collection = {
        { "delete_trophy", "Delete Trophy" },
        { "trophy_deleted", "This trophy information saved on this user will be deleted." },
        { "locked", "Locked" },
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
        { "updated", "Updated" },
        { "advance", "Advance" },
        { "show_hidden", "Show Hidden Trophies" }
    };
    std::map<std::string, std::string> user_management = {
        { "select_user", "Select User" },
        { "create_user", "Create User" },
        { "user_created", "The following user has been created." },
        { "edit_user", "Edit User" },
        { "open_user_folder", "Open User Folder" },
        { "user_name_used", "This name is already in use." },
        { "delete_user", "Delete User" },
        { "user_delete", "Select the user you want to delete." },
        { "user_delete_msg", "The following user will be deleted." },
        { "user_delete_message", "If you delete the user, that user's saved data, trophies will be deleted." },
        { "user_delete_warn", "The user will be deleted.\nAre you sure you want to continue?" },
        { "user_deleted", "User deleted." },
        { "choose_avatar", "Choose Avatar" },
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
        { "cancel_update_resume", "Do you want to cancel the update?\nIf you cancel, the next time you update, Vita3K will start downloading from this point." },
        { "downloading", "Downloading...\nAfter the download is complete, Vita3K will restart automatically and then install the new update." },
        { "not_complete_update", "Could not complete the update." },
        { "minutes_left", "{} Minutes Left" },
        { "next", "Next" },
        { "seconds_left", "{} Seconds Left" },
        { "update_vita3k", "Do you want to update Vita3K" },
        { "later_version_already_installed", "The later version of Vita3K is already installed." },
        { "latest_version_already_installed", "The latest version of Vita3K is already installed." },
        { "new_features", "New Features in Version {}" },
        { "authors", "Authors" },
        { "comments", "Comments" },
        { "update", "Update" },
        { "version", "Version {}" }
    };
    std::map<std::string, std::string> welcome = {
        { "title", "Welcome to Vita3K" },
        { "vita3k", "Vita3K PlayStation Vita Emulator" },
        { "about_vita3k", "Vita3K is an open-source PlayStation Vita emulator written in C++ for Windows, Linux, macOS and Android." },
        { "development_stage", "The emulator is still in its development stages so any feedback and testing is greatly appreciated." },
        { "about_firmware", "To get started, please install all PS Vita firmware files." },
        { "download_preinst_firmware", "Download Preinst Firmware" },
        { "download_firmware", "Download Firmware" },
        { "download_firmware_font_package", "Download Firmware Font Package" },
        { "vita3k_quickstart", "A comprehensive guide on how to set-up Vita3K can be found on the" },
        { "quickstart", "Quickstart" },
        { "page", "page." },
        { "check_compatibility", "Consult the Commercial game and the Homebrew compatibility list to see what currently runs." },
        { "commercial_compatibility_list", "Commercial Compatibility List" },
        { "homebrew_compatibility_list", "Homebrew Compatibility List" },
        { "welcome_contribution", "Contributions are welcome!" },
        { "discord_help", "Additional support can be found in the #help channel of the" },
        { "no_piracy", "Vita3K does not condone piracy. You must dump your own games." },
        { "show_next_time", "Show next time" }
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
    std::map<TypeLang, std::string> user_lang;
};
