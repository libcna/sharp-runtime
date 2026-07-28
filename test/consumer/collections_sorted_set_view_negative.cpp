// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1783 / SR-AUD-361: proves that
// System::Collections::Generic::SortedSet<T>::GetViewBetween can no longer be called on a
// const SortedSet<T>, because the object it returns is a mutable, write-through live path
// into the underlying set. This file is deliberately excluded from every normal build target
// and must never compile successfully.
//
// Expected diagnostic (GCC 14):
//   error: passing 'const System::Collections::Generic::SortedSet<int>' as 'this' argument
//   discards qualifiers [-fpermissive]
//   note:   in call to 'System::Collections::Generic::SortedSet<T> System::Collections::
//   Generic::SortedSet<T>::GetViewBetween(const T&, const T&) [with T = int]'
//
// Migration for a caller that hits it: take a non-const reference to the set when a live view
// is intended, or call ToSortedSet() on a copy when an independent range is wanted. Never
// const_cast -- the const qualifier is what documents that no mutable path exists.
#include "System/Collections/Generic/SortedSet.hpp"

using System::Collections::Generic::SortedSet;

int main() {
    const SortedSet<int> frozen{1, 2, 3, 4, 5};
    // must fail: GetViewBetween is non-const since ticket #1783
    SortedSet<int> view = frozen.GetViewBetween(2, 4);
    return view.getCountProperty() == 3 ? 0 : 1;
}
