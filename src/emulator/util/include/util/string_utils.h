#pragma once

#include <string>
#include <vector>

using ProgramArgsWide = std::vector<std::wstring>;

namespace string_utils {

std::vector<std::string> split_string(const std::string &str, char delimiter);

std::wstring utf_to_wide(const std::string &str);
std::string wide_to_utf(const std::wstring &str);
std::string utf16_to_utf8(const std::u16string &str);
std::u16string utf8_to_utf16(const std::string &str);

} // namespace string_utils

ProgramArgsWide process_args(int argc, char *argv[]);
