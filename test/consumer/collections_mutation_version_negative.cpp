// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1787
// (REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP): proves that the test-only access seam
// SharpRuntime::Testing::CollectionVersionAccess<T> gives a consumer NOTHING. The seam is
// declared in System/Collections/detail/MutationCounter.hpp and befriended by every affected
// collection so that a regression can position the mutation counter near a boundary, but the
// primary template is never defined in production code -- it is defined in exactly one
// translation unit, the permanent test suite. This file is deliberately excluded from every
// normal build target and must never compile successfully.
//
// Expected diagnostic (GCC 14), one per use:
//   error: incomplete type 'SharpRuntime::Testing::CollectionVersionAccess<
//   System::Collections::Generic::List<int> >' used in nested name specifier
//
// Migration for a caller that hits it: there is none, and that is the point. The mutation
// counter is a private implementation detail with no public accessor by design. A consumer
// that wants to know whether a collection changed should hold an enumerator and let the
// fail-fast contract tell it, or track its own revision alongside the collection.
#include "System/Collections/detail/MutationCounter.hpp"
#include "System/Collections/Generic/List.hpp"

using System::Collections::Generic::List;

int main() {
    List<int> list;
    list.Add(1);
    // must fail: CollectionVersionAccess is declared, never defined outside the test suite
    const auto version = SharpRuntime::Testing::CollectionVersionAccess<List<int>>::version(list);
    SharpRuntime::Testing::CollectionVersionAccess<List<int>>::positionVersion(list, version + 1);
    return version == 1 ? 0 : 1;
}
