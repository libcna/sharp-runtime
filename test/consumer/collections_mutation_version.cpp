// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Standalone public-header consumer fixture for ticket #1787
// (REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP). It compiles against only the public
// Collections.Core surface, so it fails if the repaired mutation counter starts to require a
// private header, another component, or a non-public helper -- and it is compiled with
// -Wall -Wextra -Wpedantic -Werror, so a new warning in a public header fails it too.
//
// What it asserts, as an ordinary consumer would experience it:
//
//   * the fail-fast enumerator contract still holds -- a structural mutation invalidates an
//     outstanding enumerator, and a rejected or absent one does not;
//   * ASSIGNING a whole collection now invalidates the enumerators outstanding over the
//     DESTINATION. Before this ticket the implicitly declared copy/move assignment operator
//     transplanted the source's counter, so those enumerators appeared valid over storage the
//     assignment had already destroyed -- an AddressSanitizer heap-use-after-free for the
//     node-based containers;
//   * copy CONSTRUCTION still leaves the source's enumerators alone;
//   * the counter is invisible: getCountProperty() still returns a plain intcs, and no
//     64-bit counter, counter class, or atomic appears anywhere in the public surface.
//
// The test-only seam SharpRuntime::Testing::CollectionVersionAccess<T> is deliberately NOT
// used here; the negative fixture collections_mutation_version_negative.cpp proves a
// consumer cannot use it at all.
#include <any>
#include <memory>
#include <string>
#include <type_traits>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Collections/ArrayList.hpp"
#include "System/Collections/BitArray.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/Generic/Dictionary.hpp"
#include "System/Collections/Generic/HashSet.hpp"
#include "System/Collections/Generic/LinkedList.hpp"
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/Generic/OrderedDictionary.hpp"
#include "System/Collections/Generic/Queue.hpp"
#include "System/Collections/Generic/SortedDictionary.hpp"
#include "System/Collections/Generic/SortedList.hpp"
#include "System/Collections/Generic/Stack.hpp"
#include "System/InvalidOperationException.hpp"

using SharpRuntime::intcs;

namespace NG = System::Collections;
namespace G = System::Collections::Generic;

namespace {

// A consumer that never names a concrete enumerator type and owns what GetEnumerator hands
// back, which is this port's documented convention.
template<typename Enumerator>
bool invalidatedByTheNextMoveNext(Enumerator* raw) {
    std::unique_ptr<Enumerator> owned(raw);
    try {
        (void)owned->MoveNext();
    } catch (const System::InvalidOperationException&) {
        return true;
    }
    return false;
}

bool listContract() {
    G::List<int> list;
    list.Add(1);
    list.Add(2);
    list.Add(3);

    auto* e = list.GetEnumerator();
    if (!e->MoveNext()) { delete e; return false; }
    if (list.Remove(987654)) { delete e; return false; }   // absent: bumps nothing
    bool ok = true;
    try { (void)e->MoveNext(); } catch (const System::InvalidOperationException&) { ok = false; }
    delete e;
    if (!ok) return false;

    auto* e2 = list.GetEnumerator();
    if (!e2->MoveNext()) { delete e2; return false; }
    list.Add(4);
    if (!invalidatedByTheNextMoveNext(e2)) return false;

    // Assignment must invalidate an enumerator outstanding over the destination.
    G::List<int> other;
    other.Add(7);
    auto* e3 = list.GetEnumerator();
    if (!e3->MoveNext()) { delete e3; return false; }
    list = other;
    return invalidatedByTheNextMoveNext(e3);
}

bool hashSetAndDictionaryContract() {
    G::HashSet<int> set;
    set.Add(1);
    set.Add(2);
    G::HashSet<int> otherSet;
    otherSet.Add(7);

    auto it = set.begin();
    if (set.Add(1)) return false;             // rejected duplicate: bumps nothing
    try { (void)*it; } catch (const System::InvalidOperationException&) { return false; }
    set = otherSet;                           // the pre-#1787 use-after-free shape
    try {
        (void)*it;
        return false;
    } catch (const System::InvalidOperationException&) {
    }

    G::Dictionary<std::string, intcs> dict;
    dict.Add("a", 1);
    dict.Add("b", 2);
    G::Dictionary<std::string, intcs> otherDict;
    otherDict.Add("z", 9);
    auto dit = dict.begin();
    dict = std::move(otherDict);
    try {
        (void)*dit;
        return false;
    } catch (const System::InvalidOperationException&) {
    }

    G::SortedDictionary<intcs, intcs> sorted;
    sorted.Add(1, 1);
    auto sit = sorted.begin();
    G::SortedDictionary<intcs, intcs> otherSorted;
    otherSorted.Add(9, 9);
    sorted = otherSorted;
    try {
        (void)*sit;
        return false;
    } catch (const System::InvalidOperationException&) {
    }

    G::OrderedDictionary<intcs, intcs> ordered;
    ordered.Add(1, 1);
    auto oit = ordered.begin();
    G::OrderedDictionary<intcs, intcs> otherOrdered;
    otherOrdered.Add(9, 9);
    ordered = otherOrdered;
    try {
        (void)*oit;
        return false;
    } catch (const System::InvalidOperationException&) {
    }
    return true;
}

bool copyConstructionLeavesTheSourceAlone() {
    G::List<int> source;
    source.Add(1);
    source.Add(2);
    auto* e = source.GetEnumerator();
    if (!e->MoveNext()) { delete e; return false; }
    const G::List<int> copy = source;          // a fresh object, nothing to invalidate
    bool ok = true;
    try { (void)e->MoveNext(); } catch (const System::InvalidOperationException&) { ok = false; }
    delete e;
    return ok && copy.getCountProperty() == 2;
}

bool nonGenericContract() {
    NG::ArrayList list;
    list.Add(std::any(1));
    list.Add(std::any(2));
    NG::ArrayList other;
    other.Add(std::any(7));
    auto* e = list.GetEnumerator();
    if (!e->MoveNext()) { delete e; return false; }
    list = other;
    if (!invalidatedByTheNextMoveNext(e)) return false;

    NG::Hashtable table;
    table.Add(std::string("a"), std::any(1));
    NG::Hashtable otherTable;
    otherTable.Add(std::string("z"), std::any(9));
    auto* te = table.GetEnumerator();
    if (!te->MoveNext()) { delete te; return false; }
    table = otherTable;
    if (!invalidatedByTheNextMoveNext(te)) return false;

    NG::BitArray bits(8);
    NG::BitArray otherBits(8, true);
    auto* be = bits.GetEnumerator();
    if (!be->MoveNext()) { delete be; return false; }
    bits = otherBits;
    return invalidatedByTheNextMoveNext(be);
}

bool queueStackAndLinkedListContract() {
    G::Queue<int> queue;
    queue.Enqueue(1);
    queue.Enqueue(2);
    auto* qe = queue.GetEnumerator();
    if (!qe->MoveNext()) { delete qe; return false; }
    queue.Enqueue(3);
    if (!invalidatedByTheNextMoveNext(qe)) return false;

    G::Stack<int> stack;
    stack.Push(1);
    G::Stack<int> otherStack;
    otherStack.Push(7);
    auto* se = stack.GetEnumerator();
    if (!se->MoveNext()) { delete se; return false; }
    stack = otherStack;
    if (!invalidatedByTheNextMoveNext(se)) return false;

    G::LinkedList<int> linked;
    linked.AddLast(1);
    linked.AddLast(2);
    G::LinkedList<int> otherLinked;
    otherLinked.AddLast(7);
    auto* le = linked.GetEnumerator();
    if (!le->MoveNext()) { delete le; return false; }
    linked = otherLinked;
    if (!invalidatedByTheNextMoveNext(le)) return false;

    G::SortedList<intcs, intcs> sortedList;
    sortedList.Add(1, 1);
    auto* sle = sortedList.GetEnumerator();
    if (!sle->MoveNext()) { delete sle; return false; }
    sortedList.Add(2, 2);
    return invalidatedByTheNextMoveNext(sle);
}

// The counter must remain entirely invisible through the public surface.
static_assert(std::is_same_v<decltype(std::declval<const G::List<int>&>().getCountProperty()),
                             intcs>);
static_assert(std::is_same_v<decltype(std::declval<const G::HashSet<int>&>().getCountProperty()),
                             intcs>);
static_assert(std::is_same_v<decltype(std::declval<const NG::ArrayList&>().getCountProperty()),
                             intcs>);
static_assert(std::is_same_v<decltype(std::declval<const NG::BitArray&>().getLengthProperty()),
                             intcs>);
static_assert(std::is_copy_assignable_v<G::List<int>>);
static_assert(std::is_move_assignable_v<G::List<int>>);
static_assert(std::is_copy_constructible_v<NG::BitArray>);

}  // namespace

int main() {
    if (!listContract()) return 1;
    if (!hashSetAndDictionaryContract()) return 2;
    if (!copyConstructionLeavesTheSourceAlone()) return 3;
    if (!nonGenericContract()) return 4;
    if (!queueStackAndLinkedListContract()) return 5;
    return 0;
}
