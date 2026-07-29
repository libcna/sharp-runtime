// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Standalone public-header consumer fixture for ticket #1788
// (REMED-COLL-LINKEDLIST-VERSION-WIDEN), which widened LinkedList<T>'s mutation counter and
// its enumerator's snapshot from 32 to 64 bits, growing sizeof(LinkedList<T>) from 40 to 48
// on LP64 under explicit user approval.
//
// It compiles against ONLY the public SharpRuntime::Collections.Core surface, with
// -Wall -Wextra -Wpedantic -Werror, and it RUNS. What it is here to prove:
//
//   1. Nothing about the widening is visible in a consumer's source. Every construction,
//      node operation, iteration form and exception below is spelled exactly as it was
//      before the ticket; the entire change is one private field's type.
//   2. The fail-fast contract still works through the public API alone.
//   3. The node lifetime contract from tickets #1768/#1769 -- detached nodes, retained
//      values, reattachment, owner destruction -- is unaffected.
//   4. sizeof(LinkedList<T>) is 48 on LP64. A consumer CAN observe that, which is exactly
//      why the change needed approval and why every consumer must rebuild completely.
//
// What it deliberately does NOT do: reach the mutation counter. There is no public accessor
// and this ticket added none. Near-boundary behaviour is reachable only through the test-only
// seam SharpRuntime::Testing::CollectionVersionAccess<T>, which is declared in a production
// header and defined only under modules/collections/tests/support/ -- a consumer cannot name
// a complete type at all, as test/consumer/collections_mutation_version_negative.cpp proves
// by failing to compile. Exposing a version-setting API to consumers in order to test a
// boundary would defeat the design, so the boundary cases live in the permanent suite
// (modules/collections/tests/System/Collections/Generic/LinkedListVersionWideningTests.cpp)
// and nowhere else.
#include <cstddef>
#include <string>
#include <vector>

#include "System/ArgumentNullException.hpp"
#include "System/Collections/Generic/LinkedList.hpp"
#include "System/InvalidOperationException.hpp"

using System::Collections::Generic::IEnumerator;
using System::Collections::Generic::LinkedList;
using System::Collections::Generic::LinkedListNode;

namespace {

/** Owns the enumerator so nothing leaks when a check returns early. */
template <typename T>
class Enumerating {
    IEnumerator<T>* e_;

public:
    explicit Enumerating(LinkedList<T>& l) : e_(l.GetEnumerator()) {}
    Enumerating(const Enumerating&) = delete;
    Enumerating& operator=(const Enumerating&) = delete;
    ~Enumerating() { delete e_; }
    IEnumerator<T>* operator->() const { return e_; }
};

template <typename Action>
bool throwsInvalidOperation(Action action) {
    try {
        action();
        return false;
    } catch (const System::InvalidOperationException&) {
        return true;
    } catch (...) {
        return false;
    }
}

// --- 1. Ordinary construction and every mutating member, spelled unchanged ---------------
bool ordinaryConstructionAndMutation() {
    LinkedList<int> list;
    if (list.getCountProperty() != 0) return false;

    LinkedListNode<int> two = list.AddLast(2);
    list.AddFirst(1);
    list.AddAfter(two, 3);
    list.AddBefore(two, 99);
    if (list.getCountProperty() != 4) return false;
    if (!list.Remove(99)) return false;

    LinkedListNode<int> four(4);
    list.AddLast(four);
    list.AddFirst(LinkedListNode<int>(0));
    if (list.getCountProperty() != 5) return false;

    list.RemoveFirst();
    list.RemoveLast();
    if (list.getCountProperty() != 3) return false;
    if (list.Remove(987654)) return false;              // absent: returns false

    if (!list.Contains(2)) return false;
    if (!static_cast<bool>(list.Find(2))) return false;
    if (!static_cast<bool>(list.FindLast(3))) return false;
    if (static_cast<bool>(list.Find(987654))) return false;

    std::vector<int> dest(3);
    list.CopyTo(dest, 0);
    if (dest[0] != 1 || dest[1] != 2 || dest[2] != 3) return false;

    list.Clear();
    return list.getCountProperty() == 0;
}

// --- 2. The fail-fast contract, through the public API only -------------------------------
bool structuralMutationInvalidatesAnEnumerator() {
    LinkedList<int> list;
    list.AddLast(1);
    list.AddLast(2);
    list.AddLast(3);

    // Every structural mutation invalidates; a read does not.
    {
        Enumerating<int> e(list);
        if (!e->MoveNext()) return false;
        if (e->Current() != 1) return false;
        if (!e->MoveNext()) return false;              // reading never invalidates
        list.AddLast(4);
        if (!throwsInvalidOperation([&] { (void)e->MoveNext(); })) return false;
    }
    {
        Enumerating<int> e(list);
        if (!e->MoveNext()) return false;
        list.RemoveFirst();
        if (!throwsInvalidOperation([&] { e->Reset(); })) return false;   // Reset guards too
    }
    {
        Enumerating<int> e(list);
        if (!e->MoveNext()) return false;
        list.Clear();
        if (!throwsInvalidOperation([&] { (void)e->MoveNext(); })) return false;
    }
    // A rejected operation is not a mutation and must not invalidate.
    {
        LinkedList<int> other;
        other.AddLast(1);
        other.AddLast(2);
        Enumerating<int> e(other);
        if (!e->MoveNext()) return false;
        if (other.Remove(987654)) return false;
        if (!e->MoveNext()) return false;
        if (e->Current() != 2) return false;
    }
    // A whole-list assignment invalidates enumerators over the DESTINATION and leaves the
    // source's own alone -- ticket #1769's guarded operator=, unchanged by #1788.
    {
        LinkedList<int> a;
        a.AddLast(1);
        a.AddLast(2);
        LinkedList<int> b;
        b.AddLast(7);
        Enumerating<int> ea(a);
        Enumerating<int> eb(b);
        if (!ea->MoveNext()) return false;
        if (!eb->MoveNext()) return false;
        a = b;
        if (!throwsInvalidOperation([&] { (void)ea->MoveNext(); })) return false;
        if (eb->MoveNext()) return false;               // b is intact, just exhausted
        if (a.getCountProperty() != 1) return false;
    }
    // Self-assignment destroys nothing and must invalidate nothing.
    {
        LinkedList<int> a;
        a.AddLast(1);
        a.AddLast(2);
        Enumerating<int> e(a);
        if (!e->MoveNext()) return false;
        a = a;  // NOLINT(clang-diagnostic-self-assign-overloaded)
        if (!e->MoveNext()) return false;
        if (a.getCountProperty() != 2) return false;
    }
    return true;
}

// --- 3. Node lifetime (#1768 / #1769) is untouched by the widening ------------------------
bool nodeLifetimeIsUnaffected() {
    // A retained handle survives its owner and keeps its value.
    LinkedListNode<std::string> retained;
    {
        LinkedList<std::string> owner;
        owner.AddLast(std::string("first"));
        owner.AddLast(std::string("second"));
        retained = owner.getFirstProperty();
        if (retained.getListProperty() != &owner) return false;
    }
    if (!static_cast<bool>(retained)) return false;
    if (retained.getListProperty() != nullptr) return false;
    if (retained.getValueProperty() != "first") return false;
    if (static_cast<bool>(retained.getNextProperty())) return false;

    // Removal through one copied handle detaches the node both handles share, and the
    // detached node can rejoin a list.
    LinkedList<int> list;
    list.AddLast(1);
    list.AddLast(2);
    LinkedListNode<int> node = list.getFirstProperty();
    LinkedListNode<int> alias = node;
    list.Remove(alias);
    if (node.getListProperty() != nullptr) return false;
    if (alias.getListProperty() != nullptr) return false;
    if (node.getValueProperty() != 1) return false;
    if (!(node == alias)) return false;

    list.AddLast(node);
    if (node.getListProperty() != &list) return false;
    if (list.getCountProperty() != 2) return false;

    // Duplicate attachment, cross-list insertion, and a null handle stay rejected.
    if (!throwsInvalidOperation([&] { list.AddLast(node); })) return false;
    LinkedList<int> foreign;
    foreign.AddLast(5);
    if (!throwsInvalidOperation([&] { list.Remove(foreign.getFirstProperty()); })) return false;
    try {
        list.AddLast(LinkedListNode<int>());
        return false;
    } catch (const System::ArgumentNullException&) {
        // expected
    }

    // Copying a list deep-copies its nodes; handles into the source still refer to the source.
    LinkedList<int> copy = list;
    if (copy.getCountProperty() != 2) return false;
    if (node.getListProperty() != &list) return false;
    copy.AddLast(3);
    if (list.getCountProperty() != 2) return false;

    // Moving repoints every node's owner at the destination and empties the source.
    LinkedList<int> moved = std::move(copy);
    if (moved.getCountProperty() != 3) return false;
    if (copy.getCountProperty() != 0) return false;
    return true;
}

// --- 4. Iteration in every public form ----------------------------------------------------
bool iterationRoundTrips() {
    LinkedList<int> list;
    list.AddLast(1);
    list.AddLast(2);
    list.AddLast(3);

    std::vector<int> collected;
    for (int value : list) collected.push_back(value);

    const LinkedList<int>& constList = list;
    for (LinkedList<int>::const_iterator it = constList.cbegin(); it != constList.cend(); ++it) {
        collected.push_back(*it);
    }

    LinkedList<int>::iterator last = list.end();
    --last;
    if (*last != 3) return false;

    int enumerated = 0;
    Enumerating<int> e(list);
    while (e->MoveNext()) ++enumerated;

    return collected.size() == 6u && collected[0] == 1 && collected[5] == 3 && enumerated == 3;
}

// --- 5. The approved object-layout consequence, observable from a consumer ----------------
bool theObjectSizeIsTheApprovedOne() {
    if (sizeof(void*) != 8) return true;               // the published figure is LP64
    // 40 before ticket #1788, 48 after. A consumer that stores a LinkedList<T> by value in
    // its own type, embeds one in an array, or passes one across a translation-unit boundary
    // compiled against the older header has a layout mismatch that NO link error announces.
    // That is why adopting this revision requires a complete rebuild.
    if (sizeof(LinkedList<int>) != 48u) return false;
    if (sizeof(LinkedList<std::string>) != 48u) return false;
    if (alignof(LinkedList<int>) != 8u) return false;
    // The node handle and the STL iterators carry no snapshot and did not grow.
    if (sizeof(LinkedListNode<int>) != 16u) return false;
    if (sizeof(LinkedList<int>::iterator) != 16u) return false;
    return true;
}

}  // namespace

int main() {
    return ordinaryConstructionAndMutation()
        && structuralMutationInvalidatesAnEnumerator()
        && nodeLifetimeIsUnaffected()
        && iterationRoundTrips()
        && theObjectSizeIsTheApprovedOne() ? 0 : 1;
}
