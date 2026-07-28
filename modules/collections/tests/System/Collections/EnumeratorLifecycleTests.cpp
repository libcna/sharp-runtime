// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include <any>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "System/InvalidOperationException.hpp"
#include "System/Collections/BitArray.hpp"
#include "System/Collections/Concurrent/ConcurrentBag.hpp"
#include "System/Collections/Concurrent/ConcurrentQueue.hpp"
#include "System/Collections/Concurrent/ConcurrentStack.hpp"
#include "System/Collections/Generic/LinkedList.hpp"
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/Generic/Queue.hpp"
#include "System/Collections/Generic/SortedList.hpp"
#include "System/Collections/Generic/Stack.hpp"
#include "System/Collections/ObjectModel/Collection.hpp"
#include "System/Collections/ObjectModel/ReadOnlyCollection.hpp"

namespace
{
    template<typename Action>
    void ExpectInvalidOperationMessage(Action action, const char* expectedMessage)
    {
        try {
            action();
            ADD_FAILURE() << "Expected System::InvalidOperationException";
        } catch (const System::InvalidOperationException& exception) {
            EXPECT_STREQ(exception.what(), expectedMessage);
        } catch (...) {
            ADD_FAILURE() << "Expected System::InvalidOperationException";
        }
    }

    template<typename T>
    void ExpectGenericEnumeratorLifecycle(
        System::Collections::Generic::IEnumerator<T>* rawEnumerator,
        const T& expected)
    {
        std::unique_ptr<System::Collections::Generic::IEnumerator<T>> enumerator(rawEnumerator);

        ExpectInvalidOperationMessage(
            [&] { (void)enumerator->Current(); },
            "Enumeration has not started. Call MoveNext.");
        EXPECT_THROW((void)enumerator->getCurrentProperty(), System::InvalidOperationException);

        ASSERT_TRUE(enumerator->MoveNext());
        EXPECT_EQ(enumerator->Current(), expected);
        EXPECT_EQ(std::any_cast<T>(enumerator->getCurrentProperty()), expected);

        EXPECT_FALSE(enumerator->MoveNext());
        EXPECT_FALSE(enumerator->MoveNext());
        ExpectInvalidOperationMessage(
            [&] { (void)enumerator->Current(); },
            "Enumeration already finished.");
        EXPECT_THROW((void)enumerator->getCurrentProperty(), System::InvalidOperationException);

        enumerator->Reset();
        ExpectInvalidOperationMessage(
            [&] { (void)enumerator->Current(); },
            "Enumeration has not started. Call MoveNext.");
        ASSERT_TRUE(enumerator->MoveNext());
        EXPECT_EQ(enumerator->Current(), expected);
    }

    template<typename Mutator>
    void ExpectBitArrayMutationInvalidates(
        const char* mutationName,
        Mutator mutator)
    {
        SCOPED_TRACE(mutationName);
        System::Collections::BitArray bits(2);
        std::unique_ptr<System::Collections::IEnumerator> enumerator(bits.GetEnumerator());
        ASSERT_TRUE(enumerator->MoveNext());

        mutator(bits);

        ExpectInvalidOperationMessage(
            [&] { (void)enumerator->MoveNext(); },
            "Collection was modified; enumeration operation may not execute.");
        ExpectInvalidOperationMessage(
            [&] { enumerator->Reset(); },
            "Collection was modified; enumeration operation may not execute.");
    }
}

TEST(EnumeratorLifecycleTest, List)
{
    System::Collections::Generic::List<int> collection;
    collection.Add(11);
    ExpectGenericEnumeratorLifecycle(collection.GetEnumerator(), 11);
}

TEST(EnumeratorLifecycleTest, Queue)
{
    System::Collections::Generic::Queue<int> collection;
    collection.Enqueue(12);
    ExpectGenericEnumeratorLifecycle(collection.GetEnumerator(), 12);
}

TEST(EnumeratorLifecycleTest, Stack)
{
    System::Collections::Generic::Stack<int> collection;
    collection.Push(13);
    ExpectGenericEnumeratorLifecycle(collection.GetEnumerator(), 13);
}

TEST(EnumeratorLifecycleTest, SortedList)
{
    System::Collections::Generic::SortedList<int, int> collection;
    collection.Add(1, 14);
    ExpectGenericEnumeratorLifecycle(collection.GetEnumerator(), 14);
}

TEST(EnumeratorLifecycleTest, LinkedList)
{
    System::Collections::Generic::LinkedList<int> collection;
    collection.AddLast(15);
    ExpectGenericEnumeratorLifecycle(collection.GetEnumerator(), 15);
}

TEST(EnumeratorLifecycleTest, ObjectModelCollection)
{
    System::Collections::ObjectModel::Collection<int> collection;
    collection.Add(16);
    ExpectGenericEnumeratorLifecycle(collection.GetEnumerator(), 16);
}

TEST(EnumeratorLifecycleTest, ReadOnlyCollection)
{
    System::Collections::ObjectModel::ReadOnlyCollection<int> collection(std::vector<int>{17});
    ExpectGenericEnumeratorLifecycle(collection.GetEnumerator(), 17);
}

TEST(EnumeratorLifecycleTest, ConcurrentBagSnapshot)
{
    System::Collections::Concurrent::ConcurrentBag<int> collection;
    collection.Add(18);
    ExpectGenericEnumeratorLifecycle(collection.GetEnumerator(), 18);
}

TEST(EnumeratorLifecycleTest, ConcurrentQueueSnapshot)
{
    System::Collections::Concurrent::ConcurrentQueue<int> collection;
    collection.Enqueue(19);
    ExpectGenericEnumeratorLifecycle(collection.GetEnumerator(), 19);
}

TEST(EnumeratorLifecycleTest, ConcurrentStackSnapshot)
{
    System::Collections::Concurrent::ConcurrentStack<int> collection;
    collection.Push(20);
    ExpectGenericEnumeratorLifecycle(collection.GetEnumerator(), 20);
}

TEST(EnumeratorLifecycleTest, EmptyEnumeratorTransitionsDirectlyToAfterLast)
{
    System::Collections::Generic::List<int> collection;
    std::unique_ptr<System::Collections::Generic::IEnumerator<int>> enumerator(collection.GetEnumerator());

    EXPECT_THROW((void)enumerator->Current(), System::InvalidOperationException);
    EXPECT_FALSE(enumerator->MoveNext());
    EXPECT_FALSE(enumerator->MoveNext());
    EXPECT_THROW((void)enumerator->Current(), System::InvalidOperationException);
}

TEST(EnumeratorLifecycleTest, BitArray)
{
    System::Collections::BitArray bits(std::vector<bool>{true});
    std::unique_ptr<System::Collections::IEnumerator> enumerator(bits.GetEnumerator());

    ExpectInvalidOperationMessage(
        [&] { (void)enumerator->getCurrentProperty(); },
        "Enumeration has not started. Call MoveNext.");
    ASSERT_TRUE(enumerator->MoveNext());
    EXPECT_TRUE(std::any_cast<bool>(enumerator->getCurrentProperty()));
    EXPECT_FALSE(enumerator->MoveNext());
    EXPECT_FALSE(enumerator->MoveNext());
    ExpectInvalidOperationMessage(
        [&] { (void)enumerator->getCurrentProperty(); },
        "Enumeration already finished.");

    enumerator->Reset();
    ExpectInvalidOperationMessage(
        [&] { (void)enumerator->getCurrentProperty(); },
        "Enumeration has not started. Call MoveNext.");
    ASSERT_TRUE(enumerator->MoveNext());
    EXPECT_TRUE(std::any_cast<bool>(enumerator->getCurrentProperty()));
}

TEST(EnumeratorLifecycleTest, EveryBitArrayMutationInvalidatesEnumerator)
{
    ExpectBitArrayMutationInvalidates("Set", [](auto& bits) { bits.Set(0, true); });
    ExpectBitArrayMutationInvalidates("SetAll", [](auto& bits) { bits.SetAll(true); });
    ExpectBitArrayMutationInvalidates("Length", [](auto& bits) { bits.setLengthProperty(2); });
    ExpectBitArrayMutationInvalidates("And", [](auto& bits) {
        const System::Collections::BitArray other(2, true);
        bits.And(other);
    });
    ExpectBitArrayMutationInvalidates("Or", [](auto& bits) {
        const System::Collections::BitArray other(2, true);
        bits.Or(other);
    });
    ExpectBitArrayMutationInvalidates("Xor", [](auto& bits) {
        const System::Collections::BitArray other(2, true);
        bits.Xor(other);
    });
    ExpectBitArrayMutationInvalidates("Not", [](auto& bits) { bits.Not(); });
    ExpectBitArrayMutationInvalidates("LeftShift", [](auto& bits) { bits.LeftShift(0); });
    ExpectBitArrayMutationInvalidates("RightShift", [](auto& bits) { bits.RightShift(0); });
}
