// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <util/fs.h>
#include <util/string_utils.h>

#include <gtest/gtest.h>

// U+1F600, outside the Basic Multilingual Plane
static const std::string emoji_utf8 = "\xF0\x9F\x98\x80";

TEST(StringUtils, WideConversionOutsideBmp) {
    const std::wstring wide = string_utils::utf_to_wide(emoji_utf8);
    if constexpr (sizeof(wchar_t) == 2)
        EXPECT_EQ(wide, (std::wstring{ static_cast<wchar_t>(0xD83D), static_cast<wchar_t>(0xDE00) }));
    else
        EXPECT_EQ(wide, std::wstring(1, static_cast<wchar_t>(0x1F600)));
    EXPECT_EQ(string_utils::wide_to_utf(wide), emoji_utf8);
}

TEST(StringUtils, PathRoundTripOutsideBmp) {
    const std::string utf8 = "Vita3K " + emoji_utf8 + "/ux0";
    EXPECT_EQ(fs_utils::path_to_utf8(fs_utils::utf8_to_path(utf8)), utf8);
}
