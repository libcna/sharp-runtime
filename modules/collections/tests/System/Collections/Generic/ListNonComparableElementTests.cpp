// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Collections/Generic/List.hpp"
#include "System/NotSupportedException.hpp"

using namespace System::Collections::Generic;

namespace {

// A element type with no equality of any kind, the C++ counterpart of a plain C# class
// that overrides neither Equals nor operator==.
struct NoEquality {
    float x = 0.0f;
    int tag = 0;
};

} // namespace

// A .NET List<T> is instantiable for every T, because the equality it needs comes from
// EqualityComparer<T>.Default rather than from T itself. Everything that does not compare
// elements must therefore work here too.
TEST(ListNonComparableElementTests, StoresAndRetrievesElementsWithoutEquality) {
    List<NoEquality> list;
    list.Add(NoEquality{1.0f, 10});
    list.Add(NoEquality{2.0f, 20});
    list.Add(NoEquality{3.0f, 30});

    EXPECT_EQ(list.getCountProperty(), 3);
    EXPECT_EQ(list.getItem(0).tag, 10);
    EXPECT_EQ(list.getItem(2).tag, 30);
}

TEST(ListNonComparableElementTests, IndexBasedMutationWorksWithoutEquality) {
    List<NoEquality> list;
    list.Add(NoEquality{1.0f, 10});
    list.Add(NoEquality{2.0f, 20});

    list.setItem(0, NoEquality{9.0f, 90});
    EXPECT_EQ(list.getItem(0).tag, 90);

    list.Insert(1, NoEquality{5.0f, 50});
    EXPECT_EQ(list.getCountProperty(), 3);
    EXPECT_EQ(list.getItem(1).tag, 50);

    list.RemoveAt(1);
    EXPECT_EQ(list.getCountProperty(), 2);
    EXPECT_EQ(list.getItem(1).tag, 20);

    list.Clear();
    EXPECT_EQ(list.getCountProperty(), 0);
}

TEST(ListNonComparableElementTests, EnumerationWorksWithoutEquality) {
    List<NoEquality> list;
    list.Add(NoEquality{1.0f, 10});
    list.Add(NoEquality{2.0f, 20});

    int sum = 0;
    for (const NoEquality& item : list) {
        sum += item.tag;
    }
    EXPECT_EQ(sum, 30);
}

// The members that do compare elements have no counterpart for such a T, and say so
// instead of silently answering with something arbitrary.
TEST(ListNonComparableElementTests, ContainsThrowsNotSupportedWithoutEquality) {
    List<NoEquality> list;
    list.Add(NoEquality{1.0f, 10});

    EXPECT_THROW((void)list.Contains(NoEquality{1.0f, 10}), System::NotSupportedException);
}

TEST(ListNonComparableElementTests, IndexOfThrowsNotSupportedWithoutEquality) {
    List<NoEquality> list;
    list.Add(NoEquality{1.0f, 10});

    EXPECT_THROW((void)list.IndexOf(NoEquality{1.0f, 10}), System::NotSupportedException);
}

TEST(ListNonComparableElementTests, RemoveThrowsNotSupportedWithoutEquality) {
    List<NoEquality> list;
    list.Add(NoEquality{1.0f, 10});

    EXPECT_THROW((void)list.Remove(NoEquality{1.0f, 10}), System::NotSupportedException);
    EXPECT_EQ(list.getCountProperty(), 1);
}

// An element type that does declare equality keeps the ordinary searching behaviour.
namespace {

struct WithEquality {
    int tag = 0;
    bool operator==(const WithEquality& other) const { return tag == other.tag; }
};

} // namespace

TEST(ListNonComparableElementTests, SearchingStillWorksWhenEqualityExists) {
    List<WithEquality> list;
    list.Add(WithEquality{10});
    list.Add(WithEquality{20});

    EXPECT_TRUE(list.Contains(WithEquality{20}));
    EXPECT_EQ(list.IndexOf(WithEquality{20}), 1);
    EXPECT_TRUE(list.Remove(WithEquality{10}));
    EXPECT_EQ(list.getCountProperty(), 1);
    EXPECT_EQ(list.getItem(0).tag, 20);
}
