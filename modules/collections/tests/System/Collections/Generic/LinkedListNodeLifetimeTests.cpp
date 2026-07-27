// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1769 / audit finding SR-AUD-357 (CCF-019).
// It replaces the focused ASan/UBSan reproduction that retained copied
// LinkedListNode<T> handles across removal, Clear, and destruction of the owning
// LinkedList<T> and reached a heap-use-after-free in the public accessors.
// The contract asserted here is recorded in docs/LinkedListNodeLifetime.md.

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "System/ArgumentNullException.hpp"
#include "System/Collections/Generic/LinkedList.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/NullReferenceException.hpp"

using System::Collections::Generic::LinkedList;
using System::Collections::Generic::LinkedListNode;

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

    template<typename Action>
    void ExpectArgumentNullMessage(Action action)
    {
        try {
            action();
            ADD_FAILURE() << "Expected System::ArgumentNullException";
        } catch (const System::ArgumentNullException& exception) {
            // Ticket #1776 (REMED-CORE-ARGNULL-MESSAGE / SR-AUD-090) corrected
            // ArgumentNullException(paramName) so the "(Parameter 'node')" marker is
            // appended exactly once instead of twice; this suite previously encoded the
            // pre-fix doubled marker as expected, honest behaviour, but now asserts the
            // single-marker message ticket #1776 restored.
            EXPECT_STREQ(exception.what(), "Value cannot be null. (Parameter 'node')");
        } catch (...) {
            ADD_FAILURE() << "Expected System::ArgumentNullException";
        }
    }

    std::vector<int> Collect(const LinkedList<int>& list)
    {
        std::vector<int> values;
        for (int value : list) values.push_back(value);
        return values;
    }

    // Asserts the documented detached state: a real node, no owning list, no
    // neighbours, and its value retained.
    void ExpectDetached(const LinkedListNode<int>& node, int expectedValue)
    {
        ASSERT_TRUE(static_cast<bool>(node));
        EXPECT_NE(node, nullptr);
        EXPECT_EQ(node.getListProperty(), nullptr);
        EXPECT_EQ(node.getValueProperty(), expectedValue);
        EXPECT_EQ(static_cast<int>(node), expectedValue);
        EXPECT_FALSE(static_cast<bool>(node.getNextProperty()));
        EXPECT_FALSE(static_cast<bool>(node.getPreviousProperty()));
    }
}

// ---------------------------------------------------------------------------
// Null handle
// ---------------------------------------------------------------------------

TEST(LinkedListNodeLifetimeTest, DefaultHandleIsNull)
{
    LinkedListNode<int> node;
    EXPECT_FALSE(static_cast<bool>(node));
    EXPECT_EQ(node, nullptr);
    EXPECT_FALSE(node != nullptr);
    EXPECT_EQ(node.getListProperty(), nullptr);
    EXPECT_FALSE(node == 0);
    EXPECT_TRUE(node != 0);
}

TEST(LinkedListNodeLifetimeTest, NullHandleValueAccessThrowsNullReference)
{
    LinkedListNode<int> node;
    const LinkedListNode<int>& constNode = node;
    EXPECT_THROW((void)node.getValueProperty(), System::NullReferenceException);
    EXPECT_THROW((void)constNode.getValueProperty(), System::NullReferenceException);
    EXPECT_THROW(node.setValueProperty(1), System::NullReferenceException);
    EXPECT_THROW((void)static_cast<int>(node), System::NullReferenceException);

    try {
        (void)node.getValueProperty();
        ADD_FAILURE() << "Expected System::NullReferenceException";
    } catch (const System::NullReferenceException& exception) {
        EXPECT_STREQ(exception.what(), "Object reference not set to an instance of an object.");
    }
}

TEST(LinkedListNodeLifetimeTest, NullHandleNavigationReturnsNullHandles)
{
    LinkedListNode<int> node;
    EXPECT_FALSE(static_cast<bool>(node.getNextProperty()));
    EXPECT_FALSE(static_cast<bool>(node.getPreviousProperty()));
}

TEST(LinkedListNodeLifetimeTest, NullHandlesCompareEqualToEachOther)
{
    LinkedListNode<int> first;
    LinkedListNode<int> second;
    EXPECT_TRUE(first == second);
    EXPECT_FALSE(first != second);

    LinkedList<int> list;
    LinkedListNode<int> attached = list.AddLast(1);
    EXPECT_TRUE(first != attached);
}

TEST(LinkedListNodeLifetimeTest, EmptyListAccessorsReturnNullHandles)
{
    LinkedList<int> list;
    EXPECT_FALSE(static_cast<bool>(list.getFirstProperty()));
    EXPECT_FALSE(static_cast<bool>(list.getLastProperty()));
    EXPECT_FALSE(static_cast<bool>(list.Find(1)));
    EXPECT_FALSE(static_cast<bool>(list.FindLast(1)));
}

// ---------------------------------------------------------------------------
// Attached nodes
// ---------------------------------------------------------------------------

TEST(LinkedListNodeLifetimeTest, AttachedNodeReportsOwnerAndNeighbours)
{
    LinkedList<int> list;
    LinkedListNode<int> first = list.AddLast(1);
    LinkedListNode<int> middle = list.AddLast(2);
    LinkedListNode<int> last = list.AddLast(3);

    EXPECT_EQ(first.getListProperty(), &list);
    EXPECT_EQ(middle.getListProperty(), &list);
    EXPECT_EQ(last.getListProperty(), &list);

    EXPECT_FALSE(static_cast<bool>(first.getPreviousProperty()));
    EXPECT_TRUE(first.getNextProperty() == middle);
    EXPECT_TRUE(middle.getPreviousProperty() == first);
    EXPECT_TRUE(middle.getNextProperty() == last);
    EXPECT_FALSE(static_cast<bool>(last.getNextProperty()));
}

TEST(LinkedListNodeLifetimeTest, SetValuePropertyUpdatesTheList)
{
    LinkedList<int> list;
    LinkedListNode<int> node = list.AddLast(1);
    node.setValueProperty(9);
    EXPECT_EQ(node.getValueProperty(), 9);
    EXPECT_EQ(list.getFirstProperty().getValueProperty(), 9);
    EXPECT_TRUE(list.Contains(9));
}

TEST(LinkedListNodeLifetimeTest, SingleNodeListHasNoNeighbours)
{
    LinkedList<int> list;
    LinkedListNode<int> only = list.AddLast(42);
    EXPECT_TRUE(list.getFirstProperty() == only);
    EXPECT_TRUE(list.getLastProperty() == only);
    EXPECT_FALSE(static_cast<bool>(only.getNextProperty()));
    EXPECT_FALSE(static_cast<bool>(only.getPreviousProperty()));
}

// ---------------------------------------------------------------------------
// Copied handles
// ---------------------------------------------------------------------------

TEST(LinkedListNodeLifetimeTest, CopiedHandlesShareOneNode)
{
    LinkedList<int> list;
    LinkedListNode<int> original = list.AddLast(5);
    LinkedListNode<int> copy = original;
    LinkedListNode<int> assigned;
    assigned = original;

    EXPECT_TRUE(original == copy);
    EXPECT_TRUE(original == assigned);

    copy.setValueProperty(6);
    EXPECT_EQ(original.getValueProperty(), 6);
    EXPECT_EQ(assigned.getValueProperty(), 6);
}

TEST(LinkedListNodeLifetimeTest, MovedHandleTransfersTheNode)
{
    LinkedList<int> list;
    LinkedListNode<int> original = list.AddLast(7);
    LinkedListNode<int> moved = std::move(original);

    EXPECT_TRUE(static_cast<bool>(moved));
    EXPECT_EQ(moved.getValueProperty(), 7);
    EXPECT_TRUE(moved == list.getFirstProperty());
}

TEST(LinkedListNodeLifetimeTest, DistinctNodesWithEqualValuesAreNotIdentical)
{
    LinkedList<int> list;
    LinkedListNode<int> first = list.AddLast(4);
    LinkedListNode<int> second = list.AddLast(4);
    EXPECT_TRUE(first != second);
    EXPECT_TRUE(first == 4);
    EXPECT_TRUE(second == 4);
}

// ---------------------------------------------------------------------------
// Removal
// ---------------------------------------------------------------------------

TEST(LinkedListNodeLifetimeTest, RemoveThroughOriginalHandleDetachesTheNode)
{
    LinkedList<int> list;
    list.AddLast(1);
    LinkedListNode<int> node = list.AddLast(2);
    list.AddLast(3);

    list.Remove(node);

    ExpectDetached(node, 2);
    EXPECT_EQ(list.getCountProperty(), 2);
    EXPECT_EQ(Collect(list), (std::vector<int>{1, 3}));
}

TEST(LinkedListNodeLifetimeTest, RemoveThroughAnotherCopiedHandleDetachesBoth)
{
    LinkedList<int> list;
    list.AddLast(1);
    LinkedListNode<int> original = list.AddLast(2);
    LinkedListNode<int> copy = original;
    list.AddLast(3);

    list.Remove(copy);

    ExpectDetached(original, 2);
    ExpectDetached(copy, 2);
    EXPECT_TRUE(original == copy);
    EXPECT_EQ(Collect(list), (std::vector<int>{1, 3}));
}

TEST(LinkedListNodeLifetimeTest, RemoveFirstDetachesTheHeadAndKeepsItUsable)
{
    LinkedList<int> list;
    LinkedListNode<int> head = list.AddLast(1);
    list.AddLast(2);

    list.RemoveFirst();

    ExpectDetached(head, 1);
    EXPECT_EQ(list.getFirstProperty().getValueProperty(), 2);
    EXPECT_EQ(list.getLastProperty().getValueProperty(), 2);
    EXPECT_EQ(list.getCountProperty(), 1);
}

TEST(LinkedListNodeLifetimeTest, RemoveLastDetachesTheTailAndKeepsItUsable)
{
    LinkedList<int> list;
    list.AddLast(1);
    LinkedListNode<int> tail = list.AddLast(2);

    list.RemoveLast();

    ExpectDetached(tail, 2);
    EXPECT_EQ(list.getLastProperty().getValueProperty(), 1);
    EXPECT_EQ(list.getCountProperty(), 1);
}

TEST(LinkedListNodeLifetimeTest, RemoveByValueDetachesTheMatchedNode)
{
    LinkedList<int> list;
    list.AddLast(1);
    LinkedListNode<int> node = list.AddLast(2);

    EXPECT_TRUE(list.Remove(2));

    ExpectDetached(node, 2);
    EXPECT_EQ(list.getCountProperty(), 1);
}

TEST(LinkedListNodeLifetimeTest, RemovingTheOnlyNodeEmptiesTheList)
{
    LinkedList<int> list;
    LinkedListNode<int> only = list.AddLast(1);

    list.Remove(only);

    ExpectDetached(only, 1);
    EXPECT_EQ(list.getCountProperty(), 0);
    EXPECT_FALSE(static_cast<bool>(list.getFirstProperty()));
    EXPECT_FALSE(static_cast<bool>(list.getLastProperty()));
    EXPECT_TRUE(Collect(list).empty());
}

TEST(LinkedListNodeLifetimeTest, RemovingAMiddleNodeRelinksItsNeighbours)
{
    LinkedList<int> list;
    LinkedListNode<int> first = list.AddLast(1);
    LinkedListNode<int> middle = list.AddLast(2);
    LinkedListNode<int> last = list.AddLast(3);

    list.Remove(middle);

    EXPECT_TRUE(first.getNextProperty() == last);
    EXPECT_TRUE(last.getPreviousProperty() == first);
    ExpectDetached(middle, 2);
}

TEST(LinkedListNodeLifetimeTest, RemovedNodeIsRejectedBySubsequentOperations)
{
    LinkedList<int> list;
    LinkedListNode<int> node = list.AddLast(1);
    list.Remove(node);

    ExpectInvalidOperationMessage(
        [&] { list.Remove(node); },
        "The LinkedList node does not belong to current LinkedList.");
    ExpectInvalidOperationMessage(
        [&] { (void)list.AddBefore(node, 2); },
        "The LinkedList node does not belong to current LinkedList.");
    ExpectInvalidOperationMessage(
        [&] { (void)list.AddAfter(node, 2); },
        "The LinkedList node does not belong to current LinkedList.");
}

// ---------------------------------------------------------------------------
// Clear and owner destruction
// ---------------------------------------------------------------------------

TEST(LinkedListNodeLifetimeTest, ClearDetachesEveryRetainedHandle)
{
    LinkedList<int> list;
    LinkedListNode<int> first = list.AddLast(1);
    LinkedListNode<int> middle = list.AddLast(2);
    LinkedListNode<int> last = list.AddLast(3);

    list.Clear();

    ExpectDetached(first, 1);
    ExpectDetached(middle, 2);
    ExpectDetached(last, 3);
    EXPECT_EQ(list.getCountProperty(), 0);
    EXPECT_TRUE(Collect(list).empty());
}

TEST(LinkedListNodeLifetimeTest, OwnerDestructionLeavesRetainedHandlesDetached)
{
    LinkedListNode<int> first;
    LinkedListNode<int> last;
    {
        LinkedList<int> list;
        first = list.AddLast(11);
        list.AddLast(22);
        last = list.AddLast(33);
        EXPECT_EQ(first.getListProperty(), &list);
    }

    // Before ticket #1769 this read freed std::list storage (ASan heap-use-after-free).
    ExpectDetached(first, 11);
    ExpectDetached(last, 33);
}

TEST(LinkedListNodeLifetimeTest, OwnerDestructionWithNonTrivialValuesKeepsThemIntact)
{
    LinkedListNode<std::string> retained;
    {
        LinkedList<std::string> list;
        retained = list.AddLast(std::string("retained value"));
    }
    ASSERT_TRUE(static_cast<bool>(retained));
    EXPECT_EQ(retained.getValueProperty(), "retained value");
    EXPECT_EQ(retained.getListProperty(), nullptr);
}

TEST(LinkedListNodeLifetimeTest, DetachedNodeOutlivesEveryOtherNodeOfItsFormerList)
{
    LinkedListNode<int> retained;
    {
        LinkedList<int> list;
        for (int value = 0; value < 64; ++value) list.AddLast(value);
        retained = list.Find(32);
    }
    ExpectDetached(retained, 32);
}

// ---------------------------------------------------------------------------
// Detached construction, reattachment, and duplicate attachment
// ---------------------------------------------------------------------------

TEST(LinkedListNodeLifetimeTest, DirectlyConstructedNodeIsDetached)
{
    LinkedListNode<int> node(99);
    ExpectDetached(node, 99);
}

TEST(LinkedListNodeLifetimeTest, AddFirstAttachesAnExistingNode)
{
    LinkedList<int> list;
    list.AddLast(2);
    LinkedListNode<int> node(1);

    list.AddFirst(node);

    EXPECT_EQ(node.getListProperty(), &list);
    EXPECT_TRUE(list.getFirstProperty() == node);
    EXPECT_EQ(Collect(list), (std::vector<int>{1, 2}));
}

TEST(LinkedListNodeLifetimeTest, AddFirstAttachesAnExistingNodeToAnEmptyList)
{
    LinkedList<int> list;
    LinkedListNode<int> node(1);

    list.AddFirst(node);

    EXPECT_TRUE(list.getFirstProperty() == node);
    EXPECT_TRUE(list.getLastProperty() == node);
    EXPECT_EQ(list.getCountProperty(), 1);
}

TEST(LinkedListNodeLifetimeTest, AddLastAttachesAnExistingNode)
{
    LinkedList<int> list;
    list.AddLast(1);
    LinkedListNode<int> node(2);

    list.AddLast(node);

    EXPECT_TRUE(list.getLastProperty() == node);
    EXPECT_EQ(Collect(list), (std::vector<int>{1, 2}));
}

TEST(LinkedListNodeLifetimeTest, AddBeforeAttachesAnExistingNode)
{
    LinkedList<int> list;
    LinkedListNode<int> head = list.AddLast(1);
    list.AddLast(3);
    LinkedListNode<int> inserted(0);

    list.AddBefore(head, inserted);

    EXPECT_TRUE(list.getFirstProperty() == inserted);
    EXPECT_EQ(Collect(list), (std::vector<int>{0, 1, 3}));
}

TEST(LinkedListNodeLifetimeTest, AddAfterAttachesAnExistingNodeAtTheTail)
{
    LinkedList<int> list;
    LinkedListNode<int> tail = list.AddLast(1);
    LinkedListNode<int> inserted(2);

    list.AddAfter(tail, inserted);

    EXPECT_TRUE(list.getLastProperty() == inserted);
    EXPECT_EQ(Collect(list), (std::vector<int>{1, 2}));
}

TEST(LinkedListNodeLifetimeTest, AddAfterAttachesAnExistingNodeInTheMiddle)
{
    LinkedList<int> list;
    LinkedListNode<int> head = list.AddLast(1);
    list.AddLast(3);
    LinkedListNode<int> inserted(2);

    list.AddAfter(head, inserted);

    EXPECT_EQ(Collect(list), (std::vector<int>{1, 2, 3}));
    EXPECT_TRUE(inserted.getPreviousProperty() == head);
}

TEST(LinkedListNodeLifetimeTest, RemovedNodeCanBeAttachedAgain)
{
    LinkedList<int> list;
    LinkedListNode<int> node = list.AddLast(1);
    list.AddLast(2);
    list.Remove(node);
    ExpectDetached(node, 1);

    list.AddLast(node);

    EXPECT_EQ(node.getListProperty(), &list);
    EXPECT_TRUE(list.getLastProperty() == node);
    EXPECT_EQ(Collect(list), (std::vector<int>{2, 1}));
}

TEST(LinkedListNodeLifetimeTest, NodeDetachedByOwnerDestructionCanJoinAnotherList)
{
    LinkedListNode<int> retained;
    {
        LinkedList<int> source;
        retained = source.AddLast(7);
    }

    LinkedList<int> destination;
    destination.AddLast(retained);

    EXPECT_EQ(retained.getListProperty(), &destination);
    EXPECT_EQ(Collect(destination), (std::vector<int>{7}));
}

TEST(LinkedListNodeLifetimeTest, AttachingAnAttachedNodeThrows)
{
    LinkedList<int> list;
    LinkedListNode<int> node = list.AddLast(1);
    LinkedList<int> other;

    ExpectInvalidOperationMessage(
        [&] { list.AddLast(node); },
        "The LinkedList node already belongs to a LinkedList.");
    ExpectInvalidOperationMessage(
        [&] { other.AddFirst(node); },
        "The LinkedList node already belongs to a LinkedList.");
    ExpectInvalidOperationMessage(
        [&] { other.AddLast(node); },
        "The LinkedList node already belongs to a LinkedList.");
    EXPECT_EQ(list.getCountProperty(), 1);
    EXPECT_EQ(other.getCountProperty(), 0);
}

TEST(LinkedListNodeLifetimeTest, AttachingANullNodeThrowsArgumentNull)
{
    LinkedList<int> list;
    LinkedListNode<int> attached = list.AddLast(1);
    LinkedListNode<int> nullNode;

    ExpectArgumentNullMessage([&] { list.AddFirst(nullNode); });
    ExpectArgumentNullMessage([&] { list.AddLast(nullNode); });
    ExpectArgumentNullMessage([&] { list.AddBefore(attached, nullNode); });
    ExpectArgumentNullMessage([&] { list.AddAfter(attached, nullNode); });
    ExpectArgumentNullMessage([&] { list.AddBefore(nullNode, LinkedListNode<int>(2)); });
    ExpectArgumentNullMessage([&] { list.AddAfter(nullNode, LinkedListNode<int>(2)); });
    ExpectArgumentNullMessage([&] { list.Remove(nullNode); });
    EXPECT_EQ(list.getCountProperty(), 1);
}

// ---------------------------------------------------------------------------
// Cross-list operations
// ---------------------------------------------------------------------------

TEST(LinkedListNodeLifetimeTest, CrossListOperationsAreRejectedDeterministically)
{
    LinkedList<int> first;
    LinkedList<int> second;
    LinkedListNode<int> foreign = second.AddLast(1);

    ExpectInvalidOperationMessage(
        [&] { first.Remove(foreign); },
        "The LinkedList node does not belong to current LinkedList.");
    ExpectInvalidOperationMessage(
        [&] { (void)first.AddBefore(foreign, 2); },
        "The LinkedList node does not belong to current LinkedList.");
    ExpectInvalidOperationMessage(
        [&] { (void)first.AddAfter(foreign, 2); },
        "The LinkedList node does not belong to current LinkedList.");
    ExpectInvalidOperationMessage(
        [&] { first.AddBefore(foreign, LinkedListNode<int>(2)); },
        "The LinkedList node does not belong to current LinkedList.");
    ExpectInvalidOperationMessage(
        [&] { first.AddAfter(foreign, LinkedListNode<int>(2)); },
        "The LinkedList node does not belong to current LinkedList.");

    EXPECT_EQ(first.getCountProperty(), 0);
    EXPECT_EQ(second.getCountProperty(), 1);
    EXPECT_EQ(foreign.getListProperty(), &second);
}

TEST(LinkedListNodeLifetimeTest, DetachedNodeIsRejectedAsAnInsertionPosition)
{
    LinkedList<int> list;
    LinkedListNode<int> detached(1);

    ExpectInvalidOperationMessage(
        [&] { (void)list.AddBefore(detached, 2); },
        "The LinkedList node does not belong to current LinkedList.");
    ExpectInvalidOperationMessage(
        [&] { list.Remove(detached); },
        "The LinkedList node does not belong to current LinkedList.");
}

TEST(LinkedListNodeLifetimeTest, EmptyListRemovalStillThrowsTheDotNetMessage)
{
    LinkedList<int> list;
    ExpectInvalidOperationMessage([&] { list.RemoveFirst(); }, "The LinkedList is empty.");
    ExpectInvalidOperationMessage([&] { list.RemoveLast(); }, "The LinkedList is empty.");
}

// ---------------------------------------------------------------------------
// List copy and move
// ---------------------------------------------------------------------------

TEST(LinkedListNodeLifetimeTest, CopyConstructionProducesIndependentNodes)
{
    LinkedList<int> source;
    LinkedListNode<int> sourceNode = source.AddLast(1);
    source.AddLast(2);

    LinkedList<int> copy(source);

    EXPECT_EQ(Collect(copy), (std::vector<int>{1, 2}));
    EXPECT_EQ(sourceNode.getListProperty(), &source);
    EXPECT_TRUE(copy.getFirstProperty() != sourceNode);

    copy.getFirstProperty().setValueProperty(9);
    EXPECT_EQ(sourceNode.getValueProperty(), 1);
}

TEST(LinkedListNodeLifetimeTest, CopyAssignmentDetachesTheOverwrittenNodes)
{
    LinkedList<int> source;
    source.AddLast(1);

    LinkedList<int> destination;
    LinkedListNode<int> replaced = destination.AddLast(7);

    destination = source;

    ExpectDetached(replaced, 7);
    EXPECT_EQ(Collect(destination), (std::vector<int>{1}));
}

TEST(LinkedListNodeLifetimeTest, SelfCopyAssignmentKeepsTheListIntact)
{
    LinkedList<int> list;
    LinkedListNode<int> node = list.AddLast(1);
    list.AddLast(2);

    const LinkedList<int>& alias = list;
    list = alias;

    EXPECT_EQ(Collect(list), (std::vector<int>{1, 2}));
    EXPECT_EQ(node.getListProperty(), &list);
}

TEST(LinkedListNodeLifetimeTest, MoveConstructionRepointsRetainedHandles)
{
    LinkedList<int> source;
    LinkedListNode<int> node = source.AddLast(1);
    source.AddLast(2);

    LinkedList<int> moved(std::move(source));

    EXPECT_EQ(node.getListProperty(), &moved);
    EXPECT_EQ(Collect(moved), (std::vector<int>{1, 2}));
    EXPECT_EQ(moved.getCountProperty(), 2);
    moved.Remove(node);
    ExpectDetached(node, 1);
}

TEST(LinkedListNodeLifetimeTest, MoveAssignmentDetachesTheOverwrittenNodes)
{
    LinkedList<int> source;
    LinkedListNode<int> moving = source.AddLast(1);

    LinkedList<int> destination;
    LinkedListNode<int> replaced = destination.AddLast(7);

    destination = std::move(source);

    ExpectDetached(replaced, 7);
    EXPECT_EQ(moving.getListProperty(), &destination);
    EXPECT_EQ(Collect(destination), (std::vector<int>{1}));
}

TEST(LinkedListNodeLifetimeTest, MovedFromListIsEmptyAndReusable)
{
    LinkedList<int> source;
    source.AddLast(1);
    LinkedList<int> moved(std::move(source));

    LinkedList<int>& emptied = source;
    EXPECT_EQ(emptied.getCountProperty(), 0);
    EXPECT_FALSE(static_cast<bool>(emptied.getFirstProperty()));
    LinkedListNode<int> reused = emptied.AddLast(5);
    EXPECT_EQ(reused.getListProperty(), &emptied);
    EXPECT_EQ(Collect(emptied), (std::vector<int>{5}));
    EXPECT_EQ(Collect(moved), (std::vector<int>{1}));
}

// ---------------------------------------------------------------------------
// Iteration
// ---------------------------------------------------------------------------

TEST(LinkedListNodeLifetimeTest, RangeIterationIsUnchanged)
{
    LinkedList<int> list;
    list.AddLast(1); list.AddLast(2); list.AddLast(3);
    EXPECT_EQ(Collect(list), (std::vector<int>{1, 2, 3}));
}

TEST(LinkedListNodeLifetimeTest, MutableIterationCanWriteThroughTheIterator)
{
    LinkedList<int> list;
    list.AddLast(1); list.AddLast(2);
    for (int& value : list) value *= 10;
    EXPECT_EQ(Collect(list), (std::vector<int>{10, 20}));
}

TEST(LinkedListNodeLifetimeTest, ConstIterationAndAlgorithmsWork)
{
    LinkedList<int> list;
    list.AddLast(1); list.AddLast(2); list.AddLast(3);
    const LinkedList<int>& constList = list;

    std::vector<int> collected;
    for (int value : constList) collected.push_back(value);
    EXPECT_EQ(collected, (std::vector<int>{1, 2, 3}));

    EXPECT_NE(std::find(list.begin(), list.end(), 2), list.end());
    EXPECT_EQ(std::find(list.begin(), list.end(), 9), list.end());
    EXPECT_EQ(std::count(constList.begin(), constList.end(), 3), 1);
}

TEST(LinkedListNodeLifetimeTest, IteratorsAreBidirectional)
{
    LinkedList<int> list;
    list.AddLast(1); list.AddLast(2); list.AddLast(3);

    auto it = list.end();
    --it;
    EXPECT_EQ(*it, 3);
    --it;
    EXPECT_EQ(*it, 2);
    ++it;
    EXPECT_EQ(*it, 3);

    auto post = list.begin();
    EXPECT_EQ(*post++, 1);
    EXPECT_EQ(*post, 2);

    LinkedList<int>::const_iterator converted = list.begin();
    EXPECT_EQ(*converted, 1);
    EXPECT_TRUE(list.cbegin() != list.cend());
}

TEST(LinkedListNodeLifetimeTest, IteratorReachesMemberValues)
{
    LinkedList<std::string> list;
    list.AddLast(std::string("abc"));
    EXPECT_EQ(list.begin()->size(), 3u);
}

TEST(LinkedListNodeLifetimeTest, EnumeratorStillIteratesAndGuardsItsLifecycle)
{
    LinkedList<int> list;
    list.AddLast(1); list.AddLast(2);

    std::unique_ptr<System::Collections::Generic::IEnumerator<int>> enumerator(list.GetEnumerator());
    EXPECT_THROW((void)enumerator->Current(), System::InvalidOperationException);
    ASSERT_TRUE(enumerator->MoveNext());
    EXPECT_EQ(enumerator->Current(), 1);
    ASSERT_TRUE(enumerator->MoveNext());
    EXPECT_EQ(enumerator->Current(), 2);
    EXPECT_FALSE(enumerator->MoveNext());
    EXPECT_THROW((void)enumerator->Current(), System::InvalidOperationException);

    enumerator->Reset();
    ASSERT_TRUE(enumerator->MoveNext());
    EXPECT_EQ(enumerator->Current(), 1);

    list.AddLast(3);
    EXPECT_THROW((void)enumerator->MoveNext(), System::InvalidOperationException);
}
