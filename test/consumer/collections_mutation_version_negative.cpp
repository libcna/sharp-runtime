// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1787
// (REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP): proves that the test-only access seam
// SharpRuntime::Testing::CollectionVersionAccess<T> gives a consumer NOTHING. The seam is
// declared in System/Collections/detail/MutationCounter.hpp and befriended by every affected
// collection so that a regression can position the mutation counter near a boundary, but the
// primary template is never defined in production code -- since ticket #1800 it is defined in
// exactly one file, modules/collections/tests/support/CollectionVersionSeam.hpp, which no
// consumer can reach. scripts/check_version_seam_odr.py enforces that ownership; this file
// enforces the consumer-side half of it.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by the compiler;
// with no site selected the file compiles cleanly. Ticket #1801's tracked checker,
// scripts/check_negative_consumer_fixtures.py, compiles each site on its own and asserts the
// rejection; the record is docs/NegativeConsumerFixtureValidation.md.
//
// Migration for a caller that hits it: there is none, and that is the point. The mutation
// counter is a private implementation detail with no public accessor by design. A consumer
// that wants to know whether a collection changed should hold an enumerator and let the
// fail-fast contract tell it, or track its own revision alongside the collection.
//
// NEGATIVE-FIXTURE: component=Collections.Core
#include "System/Collections/detail/MutationCounter.hpp"
#include "System/Collections/Generic/List.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Collections::Generic::List;

int main() {
    List<int> list;
    list.Add(1);

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(version-read-incomplete): incomplete type
    //     | used in nested name specifier
    (void)SharpRuntime::Testing::CollectionVersionAccess<List<int>>::version(list);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(version-position-incomplete): incomplete type
    //     | used in nested name specifier
    SharpRuntime::Testing::CollectionVersionAccess<List<int>>::positionVersion(list, 1);
#endif

    return 0;
}
