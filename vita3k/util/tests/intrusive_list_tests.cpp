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

#include <util/intrusive_list.h>

#include <gtest/gtest.h>

#include <vector>

struct Item {
    int value;
    IntrusiveLink<Item> first;
    IntrusiveLink<Item> second;
};

using List = IntrusiveList<Item, &Item::first>;
using OtherList = IntrusiveList<Item, &Item::second>;

static std::vector<int> values(const List &list) {
    std::vector<int> result;
    for (const Item &item : list)
        result.push_back(item.value);
    return result;
}

TEST(IntrusiveList, InsertAndRemove) {
    Item a{ 1 }, b{ 2 }, c{ 3 };
    List list;
    EXPECT_TRUE(list.empty());

    list.push_back(a);
    list.push_back(c);
    auto pos = list.begin();
    ++pos;
    list.insert(pos, b);
    EXPECT_EQ(values(list), (std::vector{ 1, 2, 3 }));
    EXPECT_EQ(list.size(), 3u);

    EXPECT_TRUE(list.remove(b));
    EXPECT_FALSE(list.remove(b));
    EXPECT_FALSE(b.first.linked());
    EXPECT_EQ(values(list), (std::vector{ 1, 3 }));

    list.insert(list.begin(), b);
    EXPECT_EQ(list.front().value, 2);

    // Removing through a post-incremented iterator keeps the iterator valid
    auto it = list.begin();
    while (it != list.end())
        EXPECT_TRUE(list.remove(*it++));
    EXPECT_TRUE(list.empty());
}

TEST(IntrusiveList, TwoLinksOnOneItem) {
    Item a{ 1 }, b{ 2 };
    List first;
    OtherList second;
    first.push_back(a);
    second.push_back(b);
    second.push_back(a);

    EXPECT_TRUE(first.remove(a));
    EXPECT_FALSE(a.first.linked());
    EXPECT_TRUE(a.second.linked());
    EXPECT_EQ(second.front().value, 2);

    EXPECT_TRUE(second.remove(a));
    EXPECT_TRUE(second.remove(b));
}
