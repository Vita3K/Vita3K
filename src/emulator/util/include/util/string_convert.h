#pragma once

#include <vector>
#include <string>

using ProgramArgsWide = std::vector<std::wstring>;

std::wstring utf_to_wide(const std::string& str);

std::string wide_to_utf(const std::wstring& str);

ProgramArgsWide process_args(int argc, char* argv[]);
