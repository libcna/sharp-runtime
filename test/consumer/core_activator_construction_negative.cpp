// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2266 (finding SR-AUD-109).
//
// Activator::CreateInstance returned `T{std::forward<Args>(args)...}`, and braces
// prefer an initializer_list constructor over every other candidate, so
// CreateInstance<std::vector<int>>(3, 7) built the two-element sequence {3, 7}
// rather than the three sevens the caller asked for. The audited repair was to
// switch unconditionally to parentheses. Measured over twelve type categories
// (docs/CoreActivatorConstructionPathPlan.md section 3), that repair is a public
// SOURCE BREAK -- std::array<int, 3>(1, 2, 3), nested-aggregate brace elision,
// std::vector<int>(1, 2, 3) and every initializer_list-only type stop compiling --
// and it additionally starts ACCEPTING narrowing conversions that braces reject.
//
// #2266 therefore forwards to a constructor only where the type has one to
// forward to, and keeps braces otherwise. THIS FILE IS THE PROOF OF THE SECOND
// HALF: that the braced path is genuinely still taken for the categories that
// need it, observable only as a compile rejection. The positive half -- that
// aggregates, brace elision and initializer_list calls all still produce their
// old values -- is pinned at run time by ActivatorConstructionPathTests in
// Core_Base.
//
// A narrowing conversion is exactly the observation that distinguishes the two
// candidate repairs: `T{1, 2.5}` is ill-formed, `T(1, 2.5)` compiles and silently
// truncates. If a future change routes either category below through
// parentheses, these sites start compiling and the guarantee is gone. That
// cannot be detected from inside the library, because a `static_assert` in a
// body that is never instantiated enforces nothing, and a SFINAE probe on
// CreateInstance detects nothing either: both templates are unconstrained, so an
// ill-formed body is a hard error rather than a substitution failure.
//
// Migration for a caller either site rejects: pass an argument that does not
// narrow (`2` rather than `2.5`), or convert explicitly at the call site
// (`static_cast<int>(2.5)`) so the truncation is written down rather than
// implied. Both spellings are in the `#else` branches.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include "System/Activator.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

namespace {

// No user-declared constructor, so there is nothing for CreateInstance to
// forward to and braced initialization is retained -- along with its refusal to
// narrow.
struct Aggregate {
    int x;
    int y;
};

} // namespace

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(activator-aggregate-narrowing): narrowing conversion
    //     | cannot be narrowed
    //     | narrowing conversion
    const Aggregate aggregate = System::Activator::CreateInstance<Aggregate>(1, 2.5);
#else
    // Accepted spelling: an argument that does not narrow.
    const Aggregate aggregate = System::Activator::CreateInstance<Aggregate>(1, 2);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(activator-scalar-narrowing): narrowing conversion
    //     | cannot be narrowed
    //     | narrowing conversion
    const int scalar = System::Activator::CreateInstance<int>(2.5);
#else
    // Accepted spelling: the truncation is written down rather than implied.
    const int scalar = System::Activator::CreateInstance<int>(static_cast<int>(2.5));
#endif

    (void)aggregate;
    (void)scalar;
    return 0;
}
