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
            { "install_license", "Install License" },
        };
        std::map<std::string, std::string> emulation = {
            { "title", "Emulation" },
            { "last_apps_used", "Last Apps used" },
        };
        std::map<std::string, std::string> configuration = {
            { "title", "Configuration" },
            { "settings", "Settings" },
            { "user_management", "User Management" },
        };
        std::map<std::string, std::string> controls = {
            { "title", "Controls" },
            { "keyboard_controls", "Keyboard Controls" },
            { "controllers", "Controllers" },
        };
        std::map<std::string, std::string> help = {
            { "title", "Help" },
            { "about", "About" },
            { "welcome", "Welcome" }
        };
    };
    MainMenubar main_menubar;
    std::map<std::string, std::string> app_context = {
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
    std::map<std::string, std::string> game_data = {
        { "app_close", "The following application will close." }
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
    struct Settings {
        std::map<std::string, std::string> main = { { "title", "Settings" } };
        struct ThemeBackground {
            std::map<std::string, std::string> main = {
                { "title", "Theme & Background" },
                { "default", "Default" },
                { "home_screen_backgrounds", "Home Screen Backgrounds" }
            };
            struct Theme {
                std::map<std::string, std::string> main = {
                    { "title", "Theme" },
                    { "delete", "This theme will be deleted." }
                };
                std::map<std::string, std::string> information = {
                    { "title", "Information" },
                    { "name", "Name" },
                    { "provider", "Provider" },
                    { "updated", "Updated" },
                    { "size", "Size" },
                    { "version", "Version" }
                };
            };
            Theme theme;
            std::map<std::string, std::string> start_screen = {
                { "title", "Start Screen" },
                { "image", "Image" }
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
            std::map<std::string, std::string> input_langague = { { "title", "Input Languages" } };
            std::map<std::string, std::string> Keyboards = { { "title", "Keyboards" } };
        };
        Language language;
    };
    Settings settings;
    std::map<std::string, std::string> trophy_collection = {
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
        { "create_user", "Create User" },
        { "edit_user", "Edit User" },
        { "delete_user", "Delete User" },
        { "change_avatar", "Change Avatar" },
        { "name", "Name" },
        { "user", "User" },
        { "confirm", "Confirm" },
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
