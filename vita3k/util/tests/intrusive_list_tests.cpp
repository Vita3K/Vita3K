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
#include <util/ref.h>

#include <gtest/gtest.h>

#include <vector>

struct Item : IntrusiveLink<Item> {
    explicit Item(int value)
        : value(value) {}
    int value;
};

using List = IntrusiveList<Item>;

static std::vector<int> values(const List &list) {
    std::vector<int> result;
    for (const Item &item : list)
        result.push_back(item.value);
    return result;
}

TEST(IntrusiveList, Empty) {
    List list;
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0u);
    EXPECT_EQ(list.begin(), list.end());
}

TEST(IntrusiveList, InsertPositions) {
    Item a{ 1 }, b{ 2 }, c{ 3 }, d{ 4 };
    List list;
    list.push_back(b);
    list.insert(list.begin(), a);
    list.insert(list.end(), d);
    auto pos = list.begin();
    ++pos;
    ++pos;
    list.insert(pos, c);
    EXPECT_EQ(values(list), (std::vector{ 1, 2, 3, 4 }));
    EXPECT_EQ(list.size(), 4u);
    EXPECT_EQ(list.front().value, 1);

    // Removing through a post-incremented iterator keeps the iterator valid
    auto it = list.begin();
    while (it != list.end())
        list.remove(*it++);
    EXPECT_TRUE(list.empty());
}

TEST(IntrusiveList, Remove) {
    Item a{ 1 }, b{ 2 }, c{ 3 }, d{ 4 };
    List list;
    for (Item *item : { &a, &b, &c, &d })
        list.push_back(*item);

    list.remove(b);
    EXPECT_FALSE(b.linked());
    EXPECT_EQ(values(list), (std::vector{ 1, 3, 4 }));
    list.remove(a);
    EXPECT_EQ(values(list), (std::vector{ 3, 4 }));
    list.remove(d);
    EXPECT_EQ(values(list), (std::vector{ 3 }));
    list.remove(c);
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0u);
}

TEST(IntrusiveList, Linked) {
    Item a{ 1 }, b{ 2 };
    List list;
    list.push_back(a);
    EXPECT_TRUE(a.linked());
    EXPECT_FALSE(b.linked());

    // Removing a T that is in no list does nothing
    list.remove(b);
    EXPECT_EQ(list.size(), 1u);
    EXPECT_EQ(values(list), (std::vector{ 1 }));

    list.remove(a);
    list.remove(a);
    EXPECT_FALSE(a.linked());
    EXPECT_TRUE(list.empty());
}

struct ForwardItem : IntrusiveForwardLink<ForwardItem> {
    explicit ForwardItem(int value)
        : value(value) {}
    int value;
};

using ForwardList = IntrusiveForwardList<ForwardItem>;

TEST(IntrusiveForwardList, Empty) {
    ForwardList list;
    EXPECT_TRUE(list.empty());
}

TEST(IntrusiveForwardList, PushPop) {
    ForwardItem a{ 1 }, b{ 2 }, c{ 3 };
    ForwardList list;
    list.push_front(a);
    list.push_front(b);
    list.push_front(c);
    EXPECT_FALSE(list.empty());
    EXPECT_EQ(list.front().value, 3);

    // The last T added is the first taken
    EXPECT_EQ(list.pop_front().value, 3);
    EXPECT_EQ(list.pop_front().value, 2);
    EXPECT_EQ(list.front().value, 1);
    EXPECT_EQ(list.pop_front().value, 1);
    EXPECT_TRUE(list.empty());
}

TEST(IntrusiveForwardList, Linked) {
    ForwardItem a{ 1 }, b{ 2 };
    ForwardList list;
    EXPECT_FALSE(ForwardList::linked(a));
    list.push_front(a);
    EXPECT_TRUE(ForwardList::linked(a));
    EXPECT_FALSE(ForwardList::linked(b));

    // The last T of the list is linked too
    list.push_front(b);
    EXPECT_TRUE(ForwardList::linked(a));
    EXPECT_TRUE(ForwardList::linked(b));

    list.pop_front();
    EXPECT_FALSE(ForwardList::linked(b));
    EXPECT_TRUE(ForwardList::linked(a));
    list.pop_front();
    EXPECT_FALSE(ForwardList::linked(a));

    // A T taken off can be added again
    list.push_front(a);
    EXPECT_EQ(list.pop_front().value, 1);
}

struct Counted : RefCounted, IntrusiveForwardLink<Counted> {
    explicit Counted(int value, int &destroyed)
        : value(value)
        , destroyed(destroyed) {}
    ~Counted() override {
        ++destroyed;
    }
    int value;
    int &destroyed;
};

TEST(RefForwardList, HoldsReferences) {
    int destroyed = 0;
    {
        RefForwardList<Counted> list;
        list.push_front(Ref<Counted>::make(1, destroyed));
        list.push_front(Ref<Counted>::make(2, destroyed));
        // The list keeps the objects alive
        EXPECT_EQ(destroyed, 0);

        Ref<Counted> first = list.pop_front();
        EXPECT_EQ(first->value, 2);
        EXPECT_FALSE(IntrusiveForwardList<Counted>::linked(*first));
        first = nullptr;
        EXPECT_EQ(destroyed, 1);
    }
    // The list drops the reference to what is left on it
    EXPECT_EQ(destroyed, 2);
}
