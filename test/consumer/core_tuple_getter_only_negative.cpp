// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2330 (SR-AUD-063).
//
// #2330 made System::TupleN's elements private with getter-only accessors,
// matching .NET, where TupleN holds private readonly fields behind getter-only
// properties. This port published them as public MUTABLE data members, so
// `Tuple::Create(1, 2).Item1 = 99` compiled and stuck.
//
// Under CLAUDE.md rule 5 the accessor is getItemNProperty(), and Tuple8's Rest
// becomes getRestProperty(). The accessors return a CONST reference, so a
// caller cannot write through them either -- a `T&` return would have satisfied
// rule 5 while leaving the finding intact.
//
// THE BOUNDARY MATTERS AND IS PINNED HERE TOO: .NET's ValueTuple is a struct
// with public mutable fields, so ValueTuple is DELIBERATELY unchanged. A sweep
// that "finished the job" by moving it as well would be wrong.
//
// Records: docs/Migration-TupleGetterOnlyElements.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <string>
#include <type_traits>

#include "System/Tuple.hpp"
#include "System/ValueTuple.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

int main() {
    auto pair = System::Tuple::Create(1, std::string("two"));
    auto eight = System::Tuple::Create(1, 2, 3, 4, 5, 6, 7, 8);

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(tuple-item-read): is private within this context
    //     | private
    int first = pair.Item1;
    (void)first;
#else
    int first = pair.getItem1Property();
    (void)first;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(tuple-item-write): is private within this context
    //     | private
    pair.Item1 = 99;
#else
    pair = System::Tuple::Create(99, std::string("two"));
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(tuple8-rest-read): is private within this context
    //     | private
    int nested = eight.Rest.Item1;
    (void)nested;
#else
    int nested = eight.getRestProperty().getItem1Property();
    (void)nested;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(tuple-accessor-returns-mutable-ref): static assertion failed
    //     | static_assert
    // The half rule 5 alone would NOT have fixed: an accessor returning `T&` satisfies the naming
    // convention while leaving the element writable, i.e. leaving the finding in place.
    static_assert(std::is_same_v<decltype(pair.getItem1Property()), int&>,
                  "the accessor is expected to return a mutable reference");
#else
    static_assert(std::is_same_v<decltype(pair.getItem1Property()), const int&>,
                  "#2330: getter-only means a const reference");
#endif

    // UNCHANGED, and asserted so the fixture proves the boundary. .NET's ValueTuple is a struct
    // with PUBLIC MUTABLE fields, so it is deliberately untouched by #2330.
    System::ValueTuple1<int> value(42);
    value.Item1 = 7;
    static_assert(std::is_same_v<decltype(value.Item1), int>,
                  "ValueTuple keeps its public field, matching .NET's struct");
    return (first == 1 || first == 99) && nested == 8 && value.Item1 == 7 ? 0 : 1;
}
