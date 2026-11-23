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

// A list declared while its T is incomplete, as the kernel's wait queues are
struct Later;
template <class T>
struct LaterLink : MemberLink<&T::link> {};
struct Holder {
    IntrusiveList<Later, LaterLink<Later>> list;
};
struct Later {
    IntrusiveLink<Later> link;
};

struct Item {
    explicit Item(int value)
        : value(value) {}
    int value;
    IntrusiveLink<Item> link;
    IntrusiveLink<Item> other_link;
    IntrusiveForwardLink<Item> forward_link;
};

using List = IntrusiveList<Item, MemberLink<&Item::link>>;
using OtherList = IntrusiveList<Item, MemberLink<&Item::other_link>>;
using ForwardList = IntrusiveForwardList<Item, MemberLink<&Item::forward_link>>;

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

TEST(IntrusiveList, IncompleteType) {
    Holder holder;
    Later later;
    holder.list.push_back(later);
    EXPECT_EQ(&holder.list.front(), &later);
    holder.list.remove(later);
    EXPECT_TRUE(holder.list.empty());
}

TEST(IntrusiveList, InsertPositions) {
    Item a{ 1 }, b{ 2 }, c{ 3 }, d{ 4 };
    List list;
    // Into an empty list, before the first, at the end and in the middle
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

    // The middle, the first, the last and the only one
    list.remove(b);
    EXPECT_FALSE(b.link.linked());
    EXPECT_EQ(values(list), (std::vector{ 1, 3, 4 }));
    list.remove(a);
    EXPECT_EQ(values(list), (std::vector{ 3, 4 }));
    EXPECT_EQ(list.front().value, 3);
    list.remove(d);
    EXPECT_EQ(values(list), (std::vector{ 3 }));
    list.remove(c);
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0u);

    // A list emptied can be used again
    list.push_back(d);
    list.push_back(a);
    EXPECT_EQ(values(list), (std::vector{ 4, 1 }));
    list.remove(d);
    list.remove(a);
}

TEST(IntrusiveList, Linked) {
    Item a{ 1 }, b{ 2 };
    List list;
    list.push_back(a);
    EXPECT_TRUE(a.link.linked());
    EXPECT_FALSE(b.link.linked());

    // Removing a T that is in no list does nothing
    list.remove(b);
    EXPECT_EQ(list.size(), 1u);
    EXPECT_EQ(values(list), (std::vector{ 1 }));

    list.remove(a);
    list.remove(a);
    EXPECT_FALSE(a.link.linked());
    EXPECT_TRUE(list.empty());
}

TEST(IntrusiveList, IteratorAtLastAfterInsertAtFront) {
    // A T added in front isn't reached from the last one, which still leads to the end
    Item a{ 1 }, b{ 2 }, c{ 3 }, d{ 4 };
    List list;
    for (Item *item : { &a, &b, &c })
        list.push_back(*item);
    auto it = list.begin();
    ++it;
    ++it;
    EXPECT_EQ(it->value, 3);
    list.insert(list.begin(), d);
    ++it;
    EXPECT_EQ(it, list.end());

    // A T added behind the last one is reached
    Item e{ 5 };
    it = list.begin();
    while (&*it != &c)
        ++it;
    list.push_back(e);
    ++it;
    EXPECT_EQ(it->value, 5);
    EXPECT_EQ(values(list), (std::vector{ 4, 1, 2, 3, 5 }));

    for (Item *item : { &a, &b, &c, &d, &e })
        list.remove(*item);
}

TEST(IntrusiveList, TwoLinks) {
    // Two links of one kind are separate, so a T can be in two lists at once
    Item a{ 1 }, b{ 2 };
    List list;
    OtherList other;
    list.push_back(a);
    list.push_back(b);
    other.push_back(b);
    other.push_back(a);
    EXPECT_EQ(values(list), (std::vector{ 1, 2 }));
    EXPECT_EQ(other.front().value, 2);
    list.remove(a);
    EXPECT_FALSE(a.link.linked());
    EXPECT_TRUE(a.other_link.linked());
    EXPECT_EQ(other.size(), 2u);
    list.remove(b);
    other.remove(a);
    other.remove(b);
}

TEST(IntrusiveForwardList, Empty) {
    ForwardList list;
    EXPECT_TRUE(list.empty());
}

TEST(IntrusiveForwardList, PushPop) {
    Item a{ 1 }, b{ 2 }, c{ 3 };
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
    Item a{ 1 }, b{ 2 };
    ForwardList list;
    EXPECT_FALSE(a.forward_link.linked());
    list.push_front(a);
    EXPECT_TRUE(a.forward_link.linked());
    EXPECT_FALSE(b.forward_link.linked());

    // The last T of the list is linked too
    list.push_front(b);
    EXPECT_TRUE(a.forward_link.linked());
    EXPECT_TRUE(b.forward_link.linked());

    list.pop_front();
    EXPECT_FALSE(b.forward_link.linked());
    EXPECT_TRUE(a.forward_link.linked());
    list.pop_front();
    EXPECT_FALSE(a.forward_link.linked());

    // A T taken off can be added again
    list.push_front(a);
    EXPECT_EQ(list.pop_front().value, 1);
}

TEST(IntrusiveForwardList, TurnedAround) {
    // Moving every T to a second list gives them back in the order they were added
    Item a{ 1 }, b{ 2 }, c{ 3 };
    ForwardList found;
    for (Item *item : { &a, &b, &c })
        found.push_front(*item);
    ForwardList ordered;
    while (!found.empty())
        ordered.push_front(found.pop_front());
    EXPECT_EQ(ordered.pop_front().value, 1);
    EXPECT_EQ(ordered.pop_front().value, 2);
    EXPECT_EQ(ordered.pop_front().value, 3);
}

TEST(IntrusiveLists, OneItemInBoth) {
    // A T's list link and forward link are separate, so it can be in both lists at once
    Item a{ 1 };
    List list;
    ForwardList forward;
    list.push_back(a);
    forward.push_front(a);
    EXPECT_TRUE(a.link.linked());
    EXPECT_TRUE(a.forward_link.linked());
    list.remove(a);
    EXPECT_FALSE(a.link.linked());
    EXPECT_TRUE(a.forward_link.linked());
    EXPECT_EQ(&forward.pop_front(), &a);
}

struct Counted : RefCounted {
    explicit Counted(int value, int &destroyed)
        : value(value)
        , destroyed(destroyed) {}
    ~Counted() override {
        ++destroyed;
    }
    int value;
    int &destroyed;
    IntrusiveForwardLink<Counted> link;
};

using CountedList = RefForwardList<Counted, MemberLink<&Counted::link>>;

TEST(RefForwardList, HoldsReferences) {
    int destroyed = 0;
    {
        CountedList list;
        Ref<Counted> one = Ref<Counted>::make(1, destroyed);
        list.push_front(one);
        list.push_front(Ref<Counted>::make(2, destroyed));
        one = nullptr;
        // The list keeps the objects alive
        EXPECT_EQ(destroyed, 0);
        EXPECT_EQ(list.front().value, 2);

        Ref<Counted> first = list.pop_front();
        EXPECT_EQ(first->value, 2);
        EXPECT_FALSE(first->link.linked());
        first = nullptr;
        EXPECT_EQ(destroyed, 1);
    }
    // The list drops the reference to what is left on it
    EXPECT_EQ(destroyed, 2);
}

TEST(RefForwardList, MovesReferences) {
    // Moving from one list to another keeps the one reference the first list held
    int destroyed = 0;
    CountedList found;
    for (int value : { 1, 2, 3 })
        found.push_front(Ref<Counted>::make(value, destroyed));
    CountedList wakes;
    while (!found.empty())
        wakes.push_front(found.pop_front());
    EXPECT_EQ(destroyed, 0);
    for (int value : { 1, 2, 3 }) {
        EXPECT_EQ(wakes.front().value, value);
        // Dropping the popped reference destroys the object, so the list held exactly one
        wakes.pop_front();
        EXPECT_EQ(destroyed, value);
    }
}
