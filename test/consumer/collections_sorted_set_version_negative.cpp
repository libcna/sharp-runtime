// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1803
// (REMED-TOOLING-SORTEDSET-SEAM-NEGATIVE-FIXTURE): proves that the test-only access seam
// SharpRuntime::Testing::SortedSetVersionAccess<T>, and the private SortedSet<T> state it
// reaches, give an ordinary consumer NOTHING.
//
// SortedSet.hpp declares that seam and befriends it so that ticket #1786's permanent
// regressions can position the shared 64-bit mutation counter at a boundary, and read the
// per-handle Count-cache tag that #1784 made atomic, without performing billions of real
// mutations. Production defines nothing: the one definition lives in
// modules/collections/tests/System/Collections/Generic/SortedSetVersionOverflowTests.cpp,
// which no consumer include path reaches. scripts/check_version_seam_odr.py (ticket #1800)
// enforces that single-definition ownership from the source text; THIS file enforces the
// consumer-side half of it, by compilation.
//
// It is the SortedSet counterpart of collections_mutation_version_negative.cpp, which does the
// same for the other seam, CollectionVersionAccess. Before ticket #1803 only that one had a
// consumer-side fixture; the asymmetry is recorded in
// docs/NegativeConsumerFixtureValidation.md section 16.4 item 4.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by the compiler;
// with no site selected the file compiles cleanly. Ticket #1801's tracked checker,
// scripts/check_negative_consumer_fixtures.py, compiles each site on its own and asserts the
// rejection; the record is docs/NegativeConsumerFixtureValidation.md.
//
// Migration for a caller that hits one of these: there is none, and that is the point. The
// mutation counter, the shared State and the lazy Count cache are private implementation
// detail with no public accessor by design. A consumer that wants to know whether the set
// changed should hold an iterator and let the fail-fast contract tell it; a consumer that
// wants the element count of a view should call getCountProperty(), which is exactly what the
// cache serves.
//
// One restriction this fixture deliberately does NOT claim, because it is not true: a consumer
// that writes its own explicit specialisation of the seam in namespace SharpRuntime::Testing
// does obtain the access the friend declaration grants. That is well-formed ISO C++, it is
// equally true of CollectionVersionAccess, and no C++ mechanism prevents a third party from
// defining a class a header befriends by name. It is measured and recorded in
// docs/NegativeConsumerFixtureValidation.md section 18.5 rather than papered over here.
//
// NEGATIVE-FIXTURE: component=Collections.Core

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
// The single defining translation unit sits at modules/collections/tests/System/Collections/
// Generic/SortedSetVersionOverflowTests.cpp -- the same System/Collections/Generic/ sub-path as
// SortedSet.hpp itself, but rooted at tests/ rather than include/. Only modules/*/include is on
// a component's consumer include surface, so the shadow path resolves to nothing.
// NEGATIVE(seam-definition-not-on-include-path): No such file or directory
//     | file not found
#include "System/Collections/Generic/SortedSetVersionOverflowTests.cpp"
#endif

#include "System/Collections/Generic/SortedSet.hpp"

#include "SharpRuntime/SharpRuntimeHelper.hpp"

using System::Collections::Generic::SortedSet;

int main() {
    SortedSet<int> set{1, 2, 3, 4, 5};
    SortedSet<int> view = set.GetViewBetween(2, 4);
    SortedSet<int>::Iterator iterator = set.begin();
    (void)set;
    (void)view;
    (void)iterator;

    // ---------------------------------------------------------------------------------
    // The seam itself is an incomplete type to a consumer: declared by SortedSet.hpp,
    // defined nowhere a consumer can reach.
    // ---------------------------------------------------------------------------------

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(seam-version-read-incomplete): incomplete type
    //     | used in nested name specifier
    //     | named in nested name specifier
    (void)SharpRuntime::Testing::SortedSetVersionAccess<int>::version(set);
#else
    // There is no public replacement, and none is wanted: hold an iterator and let the
    // fail-fast contract report the modification.
    SortedSet<int>::Iterator watcher = set.begin();
    (void)watcher;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(seam-version-position-incomplete): incomplete type
    //     | used in nested name specifier
    //     | named in nested name specifier
    SharpRuntime::Testing::SortedSetVersionAccess<int>::positionVersion(set, 1);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(seam-count-cache-read-incomplete): incomplete type
    //     | used in nested name specifier
    //     | named in nested name specifier
    (void)SharpRuntime::Testing::SortedSetVersionAccess<int>::cachedTag(view);
#else
    // The public surface the cache exists to serve.
    (void)view.getCountProperty();
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // NEGATIVE(seam-object-incomplete): has incomplete type and cannot be defined
    //     | incomplete type
    SharpRuntime::Testing::SortedSetVersionAccess<int> access;
    (void)access;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // NEGATIVE(seam-nested-type-incomplete): invalid use of incomplete type
    //     | incomplete type
    typename SharpRuntime::Testing::SortedSetVersionAccess<int>::Set* reached = nullptr;
    (void)reached;
#endif

    // ---------------------------------------------------------------------------------
    // The state the seam reaches is private, so the seam is not merely undefined -- there
    // is no second route to the same storage.
    // ---------------------------------------------------------------------------------

#if SHARP_RUNTIME_NEGATIVE_SITE == 7
    // NEGATIVE(state-handle-private): is private within this context
    //     | is a private member of
    (void)set.state_;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 8
    // NEGATIVE(state-type-private): is private within this context
    //     | is a private member of
    SortedSet<int>::State* state = nullptr;
    (void)state;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 9
    // NEGATIVE(bump-version-private): is private within this context
    //     | is a private member of
    set.bumpVersion();
#else
    // The counter advances through effective mutation, and only through it.
    (void)set.Add(6);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 10
    // NEGATIVE(cached-count-field-private): is private within this context
    //     | is a private member of
    (void)view.cachedCount_.load();
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 11
    // NEGATIVE(cached-count-tag-field-private): is private within this context
    //     | is a private member of
    (void)view.cachedCountVersion_.load();
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 12
    // NEGATIVE(count-cache-tag-function-private): is private within this context
    //     | is a private member of
    (void)SortedSet<int>::countCacheTag(0);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 13
    // NEGATIVE(max-cacheable-version-private): is private within this context
    //     | is a private member of
    (void)SortedSet<int>::kMaxCacheableVersion;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 14
    // NEGATIVE(count-not-cached-private): is private within this context
    //     | is a private member of
    (void)SortedSet<int>::kCountNotCached;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 15
    // NEGATIVE(iterator-version-snapshot-private): is private within this context
    //     | is a private member of
    (void)iterator.version_;
#endif

    return 0;
}
