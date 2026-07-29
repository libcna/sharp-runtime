// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Standalone public-header consumer fixture for ticket #1789
// (REMED-COLL-BITARRAY-VERSION-WIDEN), which widened BitArray's mutation counter and its
// PUBLIC nested Enumerator's snapshot from 32 to 64 bits, growing
// sizeof(BitArray::Enumerator) from 32 to 40 on LP64 under explicit user approval.
// sizeof(BitArray) itself stayed 48.
//
// It compiles against ONLY the public SharpRuntime::Collections.Core surface, with
// -Wall -Wextra -Wpedantic -Werror, and it RUNS. What it is here to prove:
//
//   1. Nothing about the widening is visible in a consumer's source. Every construction,
//      mutation, iteration form and exception below is spelled exactly as it was before the
//      ticket; the entire change is two private fields' types.
//   2. The fail-fast contract still works through the public API alone.
//   3. The enumerator lifecycle contract from ticket #1767 and the owning `Current` from
//      ticket #1793 are unaffected.
//   4. sizeof(BitArray::Enumerator) is 40 on LP64. A consumer CAN observe that -- Enumerator
//      is a public nested class it may name and store by value -- which is exactly why the
//      change needed approval and why every consumer must rebuild completely.
//
// What it deliberately does NOT do: reach the mutation counter. There is no public accessor
// and this ticket added none. Near-boundary behaviour is reachable only through the test-only
// seam SharpRuntime::Testing::CollectionVersionAccess<T>, which is declared in a production
// header and defined only under modules/collections/tests/support/ -- a consumer cannot name
// a complete type at all, as test/consumer/collections_mutation_version_negative.cpp proves
// by failing to compile. Exposing a version-setting API to consumers in order to test a
// boundary would defeat the design, so the boundary cases live in the permanent suite
// (modules/collections/tests/System/Collections/BitArrayVersionWideningTests.cpp) and nowhere
// else.
#include <any>
#include <cstddef>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/BitArray.hpp"
#include "System/InvalidOperationException.hpp"

using SharpRuntime::bytecs;
using SharpRuntime::intcs;
using System::Collections::BitArray;
using System::Collections::IEnumerator;

namespace {

/** Owns the enumerator so nothing leaks when a check returns early. */
class Enumerating {
    IEnumerator* e_;

public:
    explicit Enumerating(BitArray& b) : e_(b.GetEnumerator()) {}
    Enumerating(const Enumerating&) = delete;
    Enumerating& operator=(const Enumerating&) = delete;
    ~Enumerating() { delete e_; }
    IEnumerator* operator->() const { return e_; }
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

template <typename Action>
bool throwsArgument(Action action) {
    try {
        action();
        return false;
    } catch (const System::ArgumentException&) {
        return true;
    } catch (...) {
        return false;
    }
}

// --- 1. Ordinary construction and every mutating member, spelled unchanged ----------------
bool ordinaryConstructionAndMutation() {
    BitArray empty(intcs{0});
    if (empty.getLengthProperty() != 0) return false;

    BitArray b(8);
    if (b.getLengthProperty() != 8) return false;
    if (b.getCountProperty() != 8) return false;
    if (b.getIsReadOnlyProperty()) return false;
    if (b.getIsSynchronizedProperty()) return false;
    if (b.getSyncRootProperty() == nullptr) return false;
    if (b.HasAnySet()) return false;

    b.Set(1, true);
    b.Set(4, true);
    if (!b.Get(1) || b.Get(2)) return false;
    const BitArray& cb = b;
    if (!cb[4]) return false;
    if (b.PopCount() != 2) return false;

    b.SetAll(true);
    if (!b.HasAllSet()) return false;
    b.Not();
    if (b.HasAnySet()) return false;

    BitArray other(8, true);
    b.Or(other);
    if (!b.HasAllSet()) return false;
    b.And(other);
    if (!b.HasAllSet()) return false;
    b.Xor(other);
    if (b.HasAnySet()) return false;

    b.Set(0, true);
    b.LeftShift(1);
    if (b.Get(0) || !b.Get(1)) return false;
    b.RightShift(1);
    if (!b.Get(0)) return false;

    b.setLengthProperty(16);
    if (b.getLengthProperty() != 16 || b.Get(15)) return false;
    b.setLengthProperty(4);
    if (b.getLengthProperty() != 4) return false;

    // Every argument rejection still throws the .NET-matching exception type, and none of
    // them is a silent no-op.
    if (!throwsArgument([] { BitArray bad(-1); })) return false;
    if (!throwsArgument([&] { b.Set(4, true); })) return false;
    if (!throwsArgument([&] { b.Set(-1, true); })) return false;
    if (!throwsArgument([&] { b.setLengthProperty(-1); })) return false;
    if (!throwsArgument([&] { b.LeftShift(-1); })) return false;
    if (!throwsArgument([&] { b.RightShift(-1); })) return false;
    if (!throwsArgument([&] { BitArray mismatched(3); b.And(mismatched); })) return false;
    if (!throwsArgument([&] { BitArray mismatched(3); b.Or(mismatched); })) return false;
    if (!throwsArgument([&] { BitArray mismatched(3); b.Xor(mismatched); })) return false;

    // The vector-based constructors and both CopyTo overloads.
    const std::vector<bool> bools{true, false, true, true};
    BitArray fromBools(bools);
    if (fromBools.getLengthProperty() != 4 || fromBools.PopCount() != 3) return false;

    const std::vector<bytecs> bytes{static_cast<bytecs>(0x81)};
    BitArray fromBytes(bytes);
    if (fromBytes.getLengthProperty() != 8) return false;
    if (!fromBytes.Get(0) || !fromBytes.Get(7) || fromBytes.Get(1)) return false;

    const std::vector<intcs> ints{intcs{5}};
    BitArray fromInts(ints);
    if (fromInts.getLengthProperty() != 32) return false;
    if (!fromInts.Get(0) || fromInts.Get(1) || !fromInts.Get(2)) return false;

    std::vector<bool> outBools;
    fromBytes.CopyTo(outBools);
    if (outBools.size() != 8u || !outBools[0] || !outBools[7]) return false;

    std::vector<bytecs> outBytes;
    fromBytes.CopyTo(outBytes);
    if (outBytes.size() != 1u || outBytes[0] != static_cast<bytecs>(0x81)) return false;

    return true;
}

// --- 2. The fail-fast contract, through the public API alone ------------------------------
bool effectiveMutationInvalidatesAnEnumerator() {
    // Every mutation family, one at a time, each against a fresh array.
    for (int op = 0; op < 9; ++op) {
        BitArray b(8);
        BitArray other(8, true);
        Enumerating e(b);
        if (!e->MoveNext()) return false;
        switch (op) {
            case 0: b.Set(0, true); break;
            case 1: b.SetAll(true); break;
            case 2: b.Not(); break;
            case 3: b.And(other); break;
            case 4: b.Or(other); break;
            case 5: b.Xor(other); break;
            case 6: b.LeftShift(1); break;
            case 7: b.RightShift(1); break;
            case 8: b.setLengthProperty(16); break;
            default: break;
        }
        if (!throwsInvalidOperation([&] { e->MoveNext(); })) return false;
    }

    // Reset reaches the same guard.
    {
        BitArray b(8);
        Enumerating e(b);
        if (!e->MoveNext()) return false;
        b.Set(3, true);
        if (!throwsInvalidOperation([&] { e->Reset(); })) return false;
    }

    // A REJECTED mutation invalidates nothing.
    {
        BitArray b(8);
        Enumerating e(b);
        if (!e->MoveNext()) return false;
        if (!throwsArgument([&] { b.Set(99, true); })) return false;
        if (!throwsArgument([&] { BitArray mismatched(2); b.Xor(mismatched); })) return false;
        if (!e->MoveNext()) return false;
    }

    // Reading invalidates nothing.
    {
        BitArray b(4, true);
        Enumerating e(b);
        int seen = 0;
        while (e->MoveNext()) {
            (void)b.PopCount();
            (void)b.HasAllSet();
            ++seen;
        }
        if (seen != 4) return false;
        e->Reset();
        if (!e->MoveNext()) return false;
    }
    return true;
}

// --- 3. Copy, assignment and the independence a consumer relies on ------------------------
bool copyAndAssignmentBehaveAsDocumented() {
    // A copy is independent in both directions.
    {
        BitArray a(8, true);
        BitArray copy = a;
        Enumerating onOriginal(a);
        if (!onOriginal->MoveNext()) return false;
        copy.SetAll(false);
        if (!onOriginal->MoveNext()) return false;       // the original is undisturbed
        if (!a.HasAllSet()) return false;
        if (copy.HasAnySet()) return false;
    }
    // Clone() is likewise independent, and equal.
    {
        BitArray a(8);
        a.Set(2, true);
        BitArray c = a.Clone();
        if (c.getLengthProperty() != 8 || !c.Get(2)) return false;
        c.Set(3, true);
        if (a.Get(3)) return false;
    }
    // Whole-object assignment replaces the contents AND invalidates enumerators over the
    // destination -- ticket #1787's repair, which this ticket preserved at the new width.
    {
        BitArray a(8);
        BitArray b(8, true);
        Enumerating e(a);
        if (!e->MoveNext()) return false;
        a = b;
        if (!a.HasAllSet()) return false;
        if (!throwsInvalidOperation([&] { e->MoveNext(); })) return false;
    }
    {
        BitArray a(8);
        BitArray b(8, true);
        Enumerating e(a);
        if (!e->MoveNext()) return false;
        a = std::move(b);
        if (!a.HasAllSet()) return false;
        if (!throwsInvalidOperation([&] { e->MoveNext(); })) return false;
    }
    // Assignment leaves the SOURCE's own enumerators alone.
    {
        BitArray a(8);
        BitArray b(8, true);
        Enumerating onSource(b);
        if (!onSource->MoveNext()) return false;
        a = b;
        if (!onSource->MoveNext()) return false;
    }
    return true;
}

// --- 4. Iteration in every public form, and the enumerator's lifecycle --------------------
bool iterationRoundTrips() {
    BitArray b(3);
    b.Set(1, true);

    // Range-for over the raw, deliberately NOT version-checked, STL iterators.
    std::vector<bool> collected;
    for (bool value : b) collected.push_back(value);
    for (auto it = b.begin(); it != b.end(); ++it) collected.push_back(*it);
    if (collected.size() != 6u) return false;
    if (collected[0] || !collected[1] || collected[2]) return false;

    // Through IEnumerator*, with ticket #1793's owning std::any Current.
    {
        Enumerating e(b);
        int enumerated = 0;
        bool sawTheSetBit = false;
        while (e->MoveNext()) {
            const std::any boxed = e->getCurrentProperty();
            if (boxed.type() != typeid(bool)) return false;
            if (std::any_cast<bool>(boxed)) sawTheSetBit = true;
            ++enumerated;
        }
        if (enumerated != 3 || !sawTheSetBit) return false;
    }

    // Ticket #1767's lifecycle guard: Current throws before the start and after the end.
    {
        Enumerating e(b);
        if (!throwsInvalidOperation([&] { (void)e->getCurrentProperty(); })) return false;
        if (!e->MoveNext()) return false;
        (void)e->getCurrentProperty();
        while (e->MoveNext()) {}
        if (!throwsInvalidOperation([&] { (void)e->getCurrentProperty(); })) return false;
        e->Reset();
        if (!throwsInvalidOperation([&] { (void)e->getCurrentProperty(); })) return false;
        if (!e->MoveNext()) return false;
    }

    // The boxed value OWNS its bit: a later mutation cannot reach it.
    {
        BitArray owned(2, true);
        IEnumerator* e = owned.GetEnumerator();
        if (!e->MoveNext()) { delete e; return false; }
        const std::any boxed = e->getCurrentProperty();
        owned.SetAll(false);
        const bool survived = std::any_cast<bool>(boxed);
        delete e;
        if (!survived) return false;
    }

    // The PUBLIC nested Enumerator, named and stored directly by a consumer. This is the
    // whole reason the layout change needed approval.
    {
        BitArray direct(2, true);
        BitArray::Enumerator named(&direct);
        if (!named.MoveNext()) return false;
        if (!std::any_cast<bool>(named.getCurrentProperty())) return false;
        direct.Set(0, false);
        if (!throwsInvalidOperation([&] { named.MoveNext(); })) return false;
    }

    // An empty array enumerates to completion without yielding anything.
    {
        BitArray none(intcs{0});
        Enumerating e(none);
        if (e->MoveNext()) return false;
        if (e->MoveNext()) return false;
    }
    return true;
}

// --- 5. The approved object-layout consequence, observable from a consumer ----------------
bool theObjectSizesAreTheApprovedOnes() {
    if (sizeof(void*) != 8) return true;               // the published figures are LP64
    // sizeof(BitArray) is UNCHANGED at 48: the counter grew from four bytes to eight and the
    // growth landed entirely in the four bytes of tail padding the container already had.
    if (sizeof(BitArray) != 48u) return false;
    if (alignof(BitArray) != 8u) return false;
    // sizeof(BitArray::Enumerator) is 32 before ticket #1789 and 40 after. Enumerator is a
    // PUBLIC nested class, so a consumer that stores one by value in its own type, embeds one
    // in an array, or passes one across a translation-unit boundary compiled against the older
    // header has a layout mismatch that NO link error announces. That is why adopting this
    // revision requires a complete rebuild.
    if (sizeof(BitArray::Enumerator) != 40u) return false;
    if (alignof(BitArray::Enumerator) != 8u) return false;
    return true;
}

}  // namespace

int main() {
    return ordinaryConstructionAndMutation()
        && effectiveMutationInvalidatesAnEnumerator()
        && copyAndAssignmentBehaveAsDocumented()
        && iterationRoundTrips()
        && theObjectSizesAreTheApprovedOnes() ? 0 : 1;
}
