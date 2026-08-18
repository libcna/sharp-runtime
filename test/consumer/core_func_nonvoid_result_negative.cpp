// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2299 (SR-AUD-126).
//
// #2299 constrained every Func/FuncT* result type and both Converter
// declarations to a non-void type. Before it, `Func<void>` compiled and was
// THE SAME TYPE as Action -- not convertible to it, the same type, because an
// alias template introduces no new type. `Converter<T, void>` and `ActionT<T>`
// coincided the same way. .NET cannot express any of this: `void` is not a
// permitted C# generic argument.
//
// WHAT THIS DOES NOT DO, and cannot: the finding's second prescription --
// preventing APIs that require .NET parity from accepting the substitute
// aliases -- is STRUCTURALLY IMPOSSIBLE with aliases. There is one type, so no
// declaration can accept an Action and reject a Func-shaped callable.
// Constraining removes the SPELLING; it cannot create a category.
//
// Migration: spell Action where you meant a void-returning delegate, and
// ActionT<T> where you meant Converter<T, void>. Both already existed.
//
// Records: docs/Migration-FuncNonVoidResult.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <functional>
#include <string>
#include <type_traits>

#include "System/Action.hpp"
#include "System/Converter.hpp"
#include "System/Func.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

namespace {
/// Detection idiom over the alias template. It must be written over a DEPENDENT parameter: gcc
/// evaluates a constrained alias eagerly in a non-dependent `requires`, so
/// `requires { typename System::Func<void>; }` written directly is a hard ERROR rather than a
/// false value -- which would leak a diagnostic outside its own site region and make every
/// verdict in this file untrustworthy. Measured while writing this fixture.
template <typename R>
concept FuncIsSpellable = requires { typename System::Func<R>; };
}  // namespace

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(func-of-void): constraint
    //     | constraints not satisfied
    //     | template constraint failure
    System::Func<void> f;
    (void)f;
#else
    System::Action f;
    (void)f;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(funct-of-void): constraint
    //     | constraints not satisfied
    //     | template constraint failure
    System::FuncT<int, void> g;
    (void)g;
#else
    System::ActionT<int> g;
    (void)g;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(converter-to-void): constraint
    //     | constraints not satisfied
    //     | template constraint failure
    System::Converter<int, void> h;
    (void)h;
#else
    System::ActionT<int> h;
    (void)h;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(func-of-void-still-spellable): static assertion failed
    //     | static_assert
    // The shape that breaks a consumer SILENTLY: a detection idiom or a constrained template
    // that branches on whether the spelling is valid.
    static_assert(FuncIsSpellable<void>, "Func<void> is expected to be spellable");
#else
    static_assert(!FuncIsSpellable<void>, "#2299: Func<void> is ill-formed");
    static_assert(FuncIsSpellable<int>, "...and every ordinary result type still works");
#endif

    // UNCHANGED, and asserted so the fixture proves what did NOT break: every ordinary result
    // type still works, and Action -- which is what Func<void> collapsed onto -- is untouched.
    System::Func<int> ordinary = [] { return 1; };
    System::Converter<int, std::string> convert = [](int) { return std::string("x"); };
    static_assert(std::is_same_v<System::Action, std::function<void()>>, "Action is untouched");
    return ordinary() == 1 && convert(0) == "x" ? 0 : 1;
}
