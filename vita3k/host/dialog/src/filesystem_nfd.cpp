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

/**
 * @file filesystem.cpp
 * @brief Filesystem-related dialogs
 *
 * This file contains all the source code related to the implementation and
 * abstraction of user interface dialogs from the host operating system
 * related to filesystem interaction such as file or folder opening dialogs.
 *
 * The implementation is based on nativefiledialog-extended, a C library based
 * based on nativefiledialog with file filtering capabilities. Its file filters
 * are specified on every function call as a C array of `nfdnfilteritem_t` items
 * (in this implementation, the UTF-8 version is used), which are structs
 * containing two strings: one for the display name of the filter and another
 * for a comma-separated list of file extensions to filter (ex. `"txt,md"`).
 */

#include <host/dialog/filesystem.h>

#include <nfd.hpp>

/**
 * @brief Format the file extension list of a certain file filter to match the
 * format expected by the underlying file browser dialog implementation
 *
 * @param file_extensions_list File extensions list
 * @return std::string A string containing the properly formatted file extension list
 */
static std::string format_file_filter_extension_list(const std::vector<std::string> &file_extensions_list) {
    // Formatted string containing the properly formatted file extension list
    //
    // In the case of nativefiledialog, the expected file extension is a single
    // string containing comma-separated list of file extensions
    //
    // Example: "cpp,cc,txt,..."
    std::string formatted_string = "";

    // For every file extension in the filter list, append it to the formatted string
    for (size_t index = 0; index < file_extensions_list.size(); index++) {
        // Don't add comma before the first file extension
        if (index == 0) {
            formatted_string += file_extensions_list[index];
        } else {
            formatted_string += "," + file_extensions_list[index];
        }
    }

    return formatted_string;
}

namespace host::dialog::filesystem {
Result open_file(fs::path &resulting_path, const std::vector<FileFilter> &file_filters, const fs::path &default_path) {
    // Initialize NFD
    NFD::Guard nfd_guard;

    struct NFD_OpenDialog_Arguments {
        // Pointer to a C string (char) that will contain the resulting path.
        // This is required since the nativefiledialog-extended code is written
        // in C, which doesn't allow the creation of dynamically-allocated
        // arrays so manual memory management is needed instead to achieve
        // similar results. Using C++ smart pointer implementation for memory
        // to be automatically freed.
        NFD::UniquePathU8 outPath = nullptr;

        // Pointer to filter list array
        const nfdu8filteritem_t *filterList = nullptr;

        // The amount of filters that are being input into the function
        // This is related to manual memory management in C as well
        nfdfiltersize_t filterCount = 0;

        // Default path which the file browser should show when it's invoked
        const nfdu8char_t *defaultPath = nullptr;
    } arguments;

    /* --- Turn std::vector<FileFilter> into nfdfilteritem_t vector --- */

    // The file filters list input as an argument to this function but
    // formatted the way nativefiledialog-extended understands
    std::vector<nfdu8filteritem_t> file_filters_converted = {};

    // List of formatted file extension lists (strings) for every file filter
    // This is needed so that c_str() can be used without the pointers going EOL
    // after exiting the loop below
    std::vector<std::string> file_extensions_converted = {};

    // Reserve memory for the intermediate data structures to prevent the c_str()
    // pointers from referencing garbage data after the conversion process
    file_filters_converted.reserve(file_filters.size());
    file_extensions_converted.reserve(file_filters.size());

    // For every file filter in file_filters vector
    for (const FileFilter &file_filter : file_filters) {
        // Format file extension list and store the result
        file_extensions_converted.emplace_back(format_file_filter_extension_list(file_filter.file_extensions));

        // Convert filter and append to the list
        // File filter names can be used as they are, but the pointers of the
        // file extension lists have to point to the formatted strings
        file_filters_converted.push_back({ file_filter.display_name.c_str(), file_extensions_converted[file_extensions_converted.size() - 1].c_str() });
    }

    /* --- Then nativefiledialog can be called --- */

    // If the list of file filters isn't empty, specify the pointer to the filter list array
    if (!file_filters.empty()) {
        arguments.filterList = file_filters_converted.data();
    }

    // If the default path isn't empty, specify the pointer to the default path string
    const std::string default_path_u8 = fs_utils::path_to_utf8(default_path);
    if (!default_path.empty())
        arguments.defaultPath = default_path_u8.c_str();

    // Set the amount of filters to be input
    arguments.filterCount = (unsigned int)file_filters_converted.size();

    // Call nfd to invoke file explorer window to ask the user for a file path and store result code
    // Path gets stored in arguments.outPath
    nfdresult_t result = NFD::OpenDialog(arguments.outPath, arguments.filterList, arguments.filterCount, arguments.defaultPath);

    // Determine return value and return resulting path if possible
    switch (result) {
    case NFD_ERROR:
        return Result::ERROR;
    case NFD_OKAY:
        // Resolve differences between `char *` (nativefiledialog-extended) and `std::string &` (Vita3K)
        // by turning the C char array into a C++ string
        resulting_path = fs_utils::utf8_to_path(arguments.outPath.get());

        return Result::SUCCESS;
    case NFD_CANCEL:
        return Result::CANCEL;
    default:
        return Result::ERROR;
    }
}

Result pick_folder(fs::path &resulting_path, const fs::path &default_path) {
    // Initialize NFD
    NFD::Guard nfd_guard;

    struct NFD_OpenDialog_Arguments {
        // Pointer to a C string (char) that will contain the resulting path.
        // This is required since the nativefiledialog-extended code is written
        // in C, which doesn't allow the creation of dynamically-allocated
        // arrays so manual memory management is needed instead to achieve
        // similar results. Using C++ smart pointer implementation for memory
        // to be automatically freed.
        NFD::UniquePathU8 outPath = nullptr;

        // Default path which the file browser should show when it's invoked
        const nfdu8char_t *defaultPath = nullptr;
    } arguments;

    // If the default path isn't empty, specify the pointer to the default path string
    const std::string default_path_u8 = fs_utils::path_to_utf8(default_path);
    if (!default_path.empty())
        arguments.defaultPath = default_path_u8.c_str();

    // Call nfd to invoke explorer window and store result code
    nfdresult_t result = NFD::PickFolder(arguments.outPath, arguments.defaultPath);

    // Determine return value and return resulting path if possible
    switch (result) {
    case NFD_ERROR:
        return Result::ERROR;
    case NFD_OKAY:
        // Resolve differences between `char *` (nativefiledialog) and `std::string &` (Vita3K)
        // by turning the C char array into a C++ string
        resulting_path = fs_utils::utf8_to_path(arguments.outPath.get());

        return Result::SUCCESS;
    case NFD_CANCEL:
        return Result::CANCEL;
    default:
        return Result::ERROR;
    }
}

std::string get_error() {
    std::string error = "";

    // Retrieve error char array from nativefiledialog and turn it into a C++ string
    error.assign(NFD::GetError());

    return error;
}

} // namespace host::dialog::filesystem
