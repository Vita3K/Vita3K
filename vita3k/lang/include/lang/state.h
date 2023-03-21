// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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
        { "delete", "Delete" },
        { "file_corrupted", "The file is corrupt." },
        { "microphone_disabled", "Enable the microphone." },
        { "no", "No" },
        { "ok", "OK" },
        { "please_wait", "Please wait..." },
        { "select_all", "Select All" },
        { "select", "Select" },
        { "submit", "Submit" },
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
        { "about_vita3k", "Vita3K is an experimental open-source PlayStation Vita/PlayStation TV emulator written in C++ for Windows, Linux, macOS and Android operating systems." },
        { "note", "Note: The emulator is still in a very early stage of development." },
        { "github_website", "If you're interested in contributing, check out our GitHub:" },
        { "vita3k_website", "Visit our website for more info:" },
        { "vita3k_staff", "Vita3K Staff" },
        { "developers", "Developers" },
        { "contributors", "Contributors" }
    };
    std::map<std::string, std::string> app_context = {
        { "check_app_state", "Check App state" },
        { "copy_vita3k_summary", "Copy Vita3K Summary" },
        { "open_state_report", "Open state report" },
        { "create_state_report", "Create state report" },
        { "update_database", "Update database" },
        { "copy_app_info", "Copy App info" },
        { "name_and_id", "Name and Title ID" },
        { "app_summary", "App Summary" },
        { "custom_config", "Custom Config" },
        { "create", "Create" },
        { "edit", "Edit" },
        { "remove", "Remove" },
        { "open_folder", "Open Folder" },
        { "addcont", "DLCs" },
        { "license", "License" },
        { "shaders_cache", "Shaders Cache" },
        { "shaders_log", "Shaders Log" },
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
        { "parental_controls", "Parental Controls" },
        { "updated", "Updated" },
        { "size", "Size" },
        { "version", "Version" },
        { "title_id", "Title ID" },
        { "last_time_used", "Last time used" },
        { "never", "Never" },
        { "time_used", "Time used" }
    };
    struct Compatibility {
        std::string name = "Compatibility";
        std::map<compat::CompatibilityState, std::string> states = {
            { compat::Unknown, "Unknown" },
            { compat::Nothing, "Nothing" },
            { compat::Bootable, "Bootable" },
            { compat::Intro, "Intro" },
            { compat::Menu, "Menu" },
            { compat::Ingame_Less, "Ingame -" },
            { compat::Ingame_More, "Ingame +" },
            { compat::Playable, "Playable" }
        };
    };
    Compatibility compatibility;
    std::map<std::string, std::string> compile_shaders = {
        { "compiling_shaders", "Compiling Shaders" },
        { "shaders_compiled", "shaders compiled" }
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
        { "toggle_gui_visibility", "Toggle GUI Visibility" },
        { "error", "Error" },
        { "error_duplicate_key", "The key is used for other bindings or it is reserved." }
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
        { "asia", "Asia" },
        { "by_type", "By Type" },
        { "commercial", "Commercial" },
        { "homebrew", "Homebrew" },
        { "ver", "Ver" },
        { "cat", "Cat" },
        { "comp", "Comp" },
        { "last_time", "Last Time" },
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
        { "install_firmware", "Install Firmware." },
        { "install_highly_recommended", "Installing both firmware files is highly recommended." },
        { "installed", "Installed:" },
        { "download_firmware", "Download Firmware" },
        { "download_font_package", "Download Font Package" },
        { "install_firmware_file", "Install Firmware File" },
        { "completed", "Completed." },
        { "next", "Next" }
    };
    struct InstallDialog {
        std::map<std::string, std::string> firmware_install = {
            { "firmware_installing", "Installation in progress, please wait..." },
            { "successed_install_firmware", "Firmware successfully installed." },
            { "firmware_version", "Firmware version:" },
            { "no_font_exist", "No firmware font package present.\nPlease download and install it." },
            { "download_firmware_font_package", "Download Firmware Font Package" },
            { "firmware_font_package_note", "Firmware font package is needed for some applications and also for Asian regional font support. (Generally Recommended)" },
            { "delete_firmware", "Delete the firmware installation file?" }
        };
        std::map<std::string, std::string> pkg_install = {
            { "select_key_type", "Select key type" },
            { "select_work", "Select work.bin" },
            { "enter_zrif", "Enter zRIF" },
            { "enter_zrif_key", "Enter zRIF key" },
            { "input_zrif", "Please input your zRIF here" },
            { "copy_paste_zrif", "Ctrl (Cmd) + C to copy, Ctrl (Cmd) + V to paste." },
            { "delete_pkg", "Delete the pkg file?" },
            { "delete_work", "Delete the work.bin file?" },
            { "check_file", "Please check the pkg file or correct the work.bin/zrif." }
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
            { "select_license_type", "Select license type" },
            { "select_bin_rif", "Select work.bin/rif" },
            { "successed_install_license", "Successfully installed license." },
            { "delete_bin_rif", "Delete the work.bin/rif file?" },
            { "check_file", "Please check correct the work.bin/rif file." }
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
            { "browse_page_control", "D-pad, Left Stick in Left/Right or Wheel in Up/Down or Click on" },
            { "hide_show", "Hide/Show button" },
            { "hide_show_control", "Right Click" },
            { "zoom_available", "Zoom if available" },
            { "zoom_available_control", "Double Left Click" },
            { "scroll_zoom", "Scroll in zoom" },
            { "scroll_zoom_control", "Wheel Up/Down" },
            { "exit_manual", "Exit Manual" },
            { "exit_manual_control", "Click on Esc or Press on PS" }
        };
    };
    LiveArea live_area;
    std::map<std::string, std::string> performance_overlay = {
        { "fps", "FPS" },
        { "avg", "Avg" },
        { "min", "Min" },
        { "max", "Max" }
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
        { "open_user_folder", "Open User Folder" },
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
        { "authors", "Authors" },
        { "comments", "Comments" },
        { "update", "Update" },
        { "version", "Version {}" }
    };
    std::map<std::string, std::string> welcome = {
        { "title", "Welcome to Vita3K" },
        { "line_first", "Vita3K PlayStation Vita Emulator" },
        { "line_second", "Vita3K is an open-source PlayStation Vita emulator written in C++ for Windows, Linux, macOS and Android." },
        { "line_third", "The emulator is still in its early stages so any feedback and testing is greatly appreciated." },
        { "line_fourth", "To get started, please install the PS Vita firmware and font packages." },
        { "download_firmware", "Download Firmware" },
        { "line_sixth_part_one", "A comprehensive guide on how to set-up Vita3K can be found on the" },
        { "quickstart", "Quickstart" },
        { "line_sixth_part_two", "page." },
        { "line_seventh", "Consult the Commercial game and the Homebrew compatibility list to see what currently runs." },
        { "commercial_compatibility_list", "Commercial Compatibility List" },
        { "homebrew_compatibility_list", "Homebrew Compatibility List" },
        { "line_ninth", "Contributions are welcome!" },
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
    std::map<TypeLang, std::string> user_lang;
};
