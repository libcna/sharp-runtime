// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2250 (SR-AUD-103, switch half).
//
// #2250 made AppDomain::IsCompatibilitySwitchSet consult the AppContext switch
// registry, as .NET does (`AppDomain.cs:171-174`). It used to `return false`
// unconditionally, so a switch a caller had explicitly SET TO TRUE still
// reported as unset.
//
// Two approval-bound changes had to land together, and SA-10 covers both:
//   * the return type is std::optional<bool>, because a C++ bool cannot
//     distinguish an explicitly-FALSE switch from an UNSET one -- which is the
//     whole reason .NET's is bool?;
//   * the noexcept is gone, because AppContext::TryGetSwitch raises for an
//     empty name and takes a mutex whose lock() can throw. Forwarding from a
//     noexcept member would have been std::terminate, so the drop is the only
//     safe way to forward rather than a stylistic relaxation.
//
// Migration: hold the result in `auto` / std::optional<bool> and ask
// has_value(); or spell the old collapsed answer with value_or(false).
//
// Records: docs/Migration-AppDomainCompatibilitySwitch.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base allow=int128-extension
#include <optional>
#include <string>
#include <type_traits>

#include "System/AppDomain.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::AppDomain;

int main() {
    AppDomain& domain = AppDomain::CurrentDomain();
    const std::string name("consumer-fixture-switch");

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(appdomain-switch-assigned-to-bool): conversion from
    //     | cannot convert
    //     | no viable conversion
    { bool set = domain.IsCompatibilitySwitchSet(name); (void)set; }
#else
    { const std::optional<bool> set = domain.IsCompatibilitySwitchSet(name); (void)set; }
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(appdomain-switch-still-noexcept): static assertion failed
    //     | static_assert
    // The shape that breaks a consumer SILENTLY: a noexcept assertion, or a function whose own
    // specification is COMPUTED from this one.
    static_assert(noexcept(domain.IsCompatibilitySwitchSet(name)),
                  "IsCompatibilitySwitchSet is expected to be noexcept");
#else
    static_assert(!noexcept(domain.IsCompatibilitySwitchSet(name)),
                  "#2250: it forwards to a throwing, mutex-taking call");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(appdomain-switch-return-still-bool): static assertion failed
    //     | static_assert
    static_assert(std::is_same_v<decltype(domain.IsCompatibilitySwitchSet(name)), bool>,
                  "the switch query is expected to return bool");
#else
    static_assert(std::is_same_v<decltype(domain.IsCompatibilitySwitchSet(name)),
                                 std::optional<bool>>,
                  "#2250: explicitly-false and unset must be distinguishable");
#endif

    // UNCHANGED, and asserted so the fixture proves the change was surgical: the neighbouring
    // stub accessors keep their types and their noexcept.
    static_assert(noexcept(domain.getShadowCopyFilesProperty()), "neighbour untouched");
    return 0;
}
