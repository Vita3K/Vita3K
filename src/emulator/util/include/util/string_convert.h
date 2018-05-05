#pragma once

#include <string>
#include <vector>

using ProgramArgsWide = std::vector<std::wstring>;

std::wstring utf_to_wide(const std::string &str);

std::string wide_to_utf(const std::wstring &str);

std::string utf16_to_utf8(const std::u16string &str);

std::u16string utf8_to_utf16(const std::string &str);

ProgramArgsWide process_args(int argc, char *argv[]);
