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
            { "keyboard_controls", "Keyboard Controls" },
            { "controllers", "Controllers" }
        };
        std::map<std::string, std::string> help = {
            { "title", "Help" },
            { "about", "About" },
            { "welcome", "Welcome" }
        };
    };
    MainMenubar main_menubar;
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
        { "application", "Application" },
        { "addcont", "DLCs" },
        { "license", "License" },
        { "save_data", "Save Data" },
        { "shader_cache", "Shader Cache" },
        { "shader_log", "Shader Log" },
        { "update_history", "Update History" },
        { "information", "Information" },
        { "app_delete", "This application and all related data, including saved data, will be deleted." },
        { "save_delete", "Do you want to delete this saved data ?" },
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
        struct Application {
            std::map<std::string, std::string> main = {
                { "title", "Application" },
                { "delete", "The selected applications and all related data, including saved data, will be deleted." },
                { "no_item", "There are no content items." }
            };
        };
        Application application;
        struct SavedData {
            std::map<std::string, std::string> main = {
                { "title", "Saved Data" },
                { "delete", "The selected saved data items will be deleted." },
                { "no_saved_data", "There is no saved data." }
            };
        };
        SavedData saved_data;
    };
    ContentManager content_manager;
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
        { "completed_setup", "You have now completed initial setup.\nYour Vita3K system is ready !" },
        { "select_language", "Select a language" },
        { "next", "Next" }
    };
    std::map<std::string, std::string> install_dialog = {
        { "select_key_type", "Select key type" },
        { "select_work", "Select work.bin" },
        { "enter_zrif", "Enter zRIF" },
        { "enter_zrif_key", "Enter zRIF key" },
        { "input_zrif", "Please input your zRIF here" },
        { "copy_paste_zrif", "Ctrl(Cmd) + C for copy, Ctrl(Cmd) + V to paste." },
        { "delete_pkg", "Delete the pkg file?" },
        { "delete_work", "Delete the work.bin file?" },
        { "check_log", "Please check log for more details." },
        { "select_install_type", "Select install type" },
        { "select_file", "Select File" },
        { "select_directory", "Select Directory" },
        { "compatible_content", "archive(s) found with compatible contents." },
        { "successed_install", "contents of this archive have successed installed:" },
        { "update_app", "Update App to:" },
        { "failed_install", "content have failed installed:" },
        { "not_compatible_content", "archive(s) did no found compatible contents:" },
        { "delete_archive", "Delete archive?" }
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
        { "user_created", "The following user has been created" },
        { "edit_user", "Edit User" },
        { "user_name_used", "! This name is already in use." },
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
        { "new_version_available", "A new version of the Vita3K is available." },
        { "back", "Back" },
        { "downloading", "Downloading...\nAfter the download is complete, Vita3K will restart automatically and then install the new Vita3K." },
        { "not_complete_update", "Could not complete the update." },
        { "next", "Next" },
        { "update_vita3k", "Do you want to update Vita3K" },
        { "later_version_already_installed", "The later version of the Vita3K software is already installed." },
        { "latest_version_already_installed", "The latest version of the Vita3K is already installed." },
        { "new_features", "New Features in Version {}" },
        { "update", "Update" },
        { "version", "Version {}" }
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
