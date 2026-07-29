// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1783 / SR-AUD-361: proves that
// System::Collections::Generic::SortedSet<T>::GetViewBetween can no longer be called on a
// const SortedSet<T>, because the object it returns is a mutable, write-through live path
// into the underlying set.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by the
// compiler; with no site selected the file compiles cleanly. Ticket #1801's tracked
// checker, scripts/check_negative_consumer_fixtures.py, compiles each site on its own and
// asserts the rejection, and docs/NegativeConsumerFixtureValidation.md is the record.
//
// Migration for a caller that hits it: take a non-const reference to the set when a live view
// is intended, or call ToSortedSet() on a copy when an independent range is wanted. Never
// const_cast -- the const qualifier is what documents that no mutable path exists.
//
// NEGATIVE-FIXTURE: component=Collections.Core
#include "System/Collections/Generic/SortedSet.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Collections::Generic::SortedSet;

int main() {
    const SortedSet<int> frozen{1, 2, 3, 4, 5};
    (void)frozen.getCountProperty();

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(const-getviewbetween): as 'this' argument discards qualifiers
    //     | discards qualifiers
    SortedSet<int> view = frozen.GetViewBetween(2, 4);
    (void)view;
#endif

    return 0;
}
