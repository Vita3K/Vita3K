#pragma once

#include <string>
#include <vector>

namespace string_utils {

std::vector<std::string> split_string(const std::string &str, char delimiter);

std::wstring utf_to_wide(const std::string &str);
std::string wide_to_utf(const std::wstring &str);
std::string utf16_to_utf8(const std::u16string &str);
std::u16string utf8_to_utf16(const std::string &str);
std::string remove_special_chars(std::string str);
void replace(std::string &str, const std::string &in, const std::string &out);
std::string toupper(const std::string &s);

} // namespace string_utils
