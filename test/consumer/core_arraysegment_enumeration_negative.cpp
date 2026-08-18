// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2215 (SR-AUD-054 residual, family CMS-B).
//
// #2215 dropped `noexcept` from all four ArraySegment<T> enumeration doors --
// begin(), end() and their const overloads -- so that a DEFAULT segment throws
// InvalidOperationException the way .NET's GetEnumerator() does
// (`ArraySegment.cs:95-99`) instead of silently iterating zero times. #2214 had
// guarded the other ten array-touching doors and could not touch these two,
// because the guard requires exactly this exception-specification change.
//
// A `noexcept` drop breaks NO runtime spelling: every call that compiled before
// still compiles and still returns the same pointer for a non-default segment.
// What it DOES break is compile-domain code that depends on the specification --
// a `noexcept(...)` assertion, a `static_assert`, or a function whose own
// `noexcept` is COMPUTED from these. Those are the only sites a consumer can
// have, and each is compiled on its own below so one broken line cannot hide
// another. The `#else` branches are the migrated spellings and are what the
// clean baseline compiles.
//
// Migration: stop asserting the specification, or -- if you genuinely need a
// non-throwing traversal -- test the segment first with getArrayProperty(),
// which is still noexcept and is the intended way to ask.
//
// Records: docs/Migration-ArraySegmentEnumerationGuard.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <type_traits>
#include <utility>
#include <vector>

#include "System/ArraySegment.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::ArraySegment;

// A consumer whose own noexcept is COMPUTED from the door's. This is the shape that breaks
// silently rather than loudly: before #2215 it was noexcept(true), and a caller may have relied
// on that in its own specification chain.
template <typename T>
auto firstElementPointer(ArraySegment<T>& segment) noexcept(noexcept(segment.begin())) -> T* {
    return segment.begin();
}

int main() {
    std::vector<int> data{1, 2, 3};
    ArraySegment<int> segment(data, 0, 3);
    const ArraySegment<int>& constSegment = segment;

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(arraysegment-begin-still-noexcept): static assertion failed
    //     | static_assert
    static_assert(noexcept(std::declval<ArraySegment<int>&>().begin()),
                  "begin() is expected to be noexcept");
#else
    static_assert(!noexcept(std::declval<ArraySegment<int>&>().begin()),
                  "#2215: begin() throws for a default segment");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(arraysegment-end-still-noexcept): static assertion failed
    //     | static_assert
    static_assert(noexcept(std::declval<ArraySegment<int>&>().end()),
                  "end() is expected to be noexcept");
#else
    static_assert(!noexcept(std::declval<ArraySegment<int>&>().end()),
                  "#2215: end() throws for a default segment");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(arraysegment-const-begin-still-noexcept): static assertion failed
    //     | static_assert
    static_assert(noexcept(std::declval<const ArraySegment<int>&>().begin()),
                  "the const begin() is expected to be noexcept");
#else
    static_assert(!noexcept(std::declval<const ArraySegment<int>&>().begin()),
                  "#2215: the const overload moved with the non-const one");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(arraysegment-computed-noexcept-chain): static assertion failed
    //     | static_assert
    // The computed-specification shape: a consumer function that inherits the door's noexcept.
    static_assert(noexcept(firstElementPointer(segment)),
                  "a traversal helper built on begin() is expected to be noexcept");
#else
    static_assert(!noexcept(firstElementPointer(segment)),
                  "#2215: a helper that inherits the specification inherits the drop too");
#endif

    // UNCHANGED, and asserted here so the fixture also proves what did NOT move: the accessors
    // stay noexcept, so a consumer that needs a non-throwing question still has one.
    static_assert(noexcept(std::declval<const ArraySegment<int>&>().getArrayProperty()),
                  "getArrayProperty() must stay noexcept -- it is the migration route");
    static_assert(noexcept(std::declval<const ArraySegment<int>&>().getCountProperty()),
                  "getCountProperty() must stay noexcept");

    (void)constSegment;
    (void)firstElementPointer(segment);
    return 0;
}
