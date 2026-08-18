// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2289 (SR-AUD-117).
//
// #2289 gave this repository its FIRST [[deprecated]] attributes, at all five
// sites where .NET marks a member [Obsolete] and this port carried only prose:
//
//   LoaderOptimization::DomainMask        LoaderOptimization.cs:10
//   LoaderOptimization::DisallowBindings  LoaderOptimization.cs:8
//   AppDomain::GetCurrentThreadId()       AppDomain.cs:228
//   CultureTypes::WindowsOnlyCultures     CultureTypes.cs:21
//   CultureTypes::FrameworkCultures       CultureTypes.cs:23
//
// ALL FIVE, not the two the finding named: deprecating two enumerators alone
// would leave the port inconsistent with itself, which the review said in terms.
//
// Under this repository's -Wall -Wextra -Werror every use becomes a hard ERROR,
// which is the whole point -- and is why this fixture can exist at all.
// Measured before landing: zero sites in cna, zero in mobile-eggbert, for every
// one of the five.
//
// Migration: stop naming them. .NET's own messages say what to use instead --
// for GetCurrentThreadId, Thread's ManagedThreadId.
//
// Records: docs/Migration-DeprecatedMembers.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base allow=int128-extension
#include "System/AppDomain.hpp"
#include "System/LoaderOptimization.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(loaderoptimization-domainmask-deprecated): is deprecated
    //     | deprecated-declarations
    const int v = static_cast<int>(System::LoaderOptimization::DomainMask);
    (void)v;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(loaderoptimization-disallowbindings-deprecated): is deprecated
    //     | deprecated-declarations
    const int v = static_cast<int>(System::LoaderOptimization::DisallowBindings);
    (void)v;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(appdomain-getcurrentthreadid-deprecated): is deprecated
    //     | deprecated-declarations
    const auto id = System::AppDomain::GetCurrentThreadId();
    (void)id;
#endif

    // UNCHANGED, and asserted so the fixture proves the deprecation was surgical: the
    // NON-obsolete enumerators of the same enum are untouched, and their values did not move.
    // Deprecating an enumerator must not change what it is.
    static_assert(static_cast<int>(System::LoaderOptimization::NotSpecified) == 0, "untouched");
    static_assert(static_cast<int>(System::LoaderOptimization::SingleDomain) == 1, "untouched");
    static_assert(static_cast<int>(System::LoaderOptimization::MultiDomain) == 2, "untouched");
    static_assert(static_cast<int>(System::LoaderOptimization::MultiDomainHost) == 3, "untouched");
    return 0;
}
