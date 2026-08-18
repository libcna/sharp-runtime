// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2334 (SR-AUD-110, approval-gated clause
// split out of #2333).
//
// #2334 REMOVED System::RuntimeType, under docs/StandingApprovals.md SA-9,
// whose rule is that a type existing only because .NET has one wears .NET's
// public shape -- and that members .NET does not have are removed.
//
// .NET's RuntimeType is `internal sealed class RuntimeType : TypeInfo`
// (RuntimeType.BoxCache.cs:11 and siblings). It is NOT PUBLIC API AT ALL, and
// it is not an enumeration. This port published a public `enum class` under
// that name with six values that have no .NET original -- #2333 measured and
// withdrew the two claims that said otherwise -- used by nothing except its own
// test file, and referenced nowhere downstream.
//
// Reflection is a permanent deviation per CLAUDE.md, so the .NET class whose
// name this occupied is never going to be ported and will never need the name
// back. Removal was chosen over renaming because there was nothing to migrate:
// zero production consumers here, zero sites in either consumer repository.
//
// Records: docs/Migration-RuntimeTypeRemoval.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <type_traits>

#include "System/RuntimeTypeHandle.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
// NEGATIVE(runtimetype-header-gone): No such file or directory
//     | file not found
#include "System/RuntimeType.hpp"
#endif

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(runtimetype-name-gone): is not a member of
    //     | has not been declared
    //     | no member named
    System::RuntimeType category = System::RuntimeType::Primitive;
    (void)category;
#endif

    // UNCHANGED, and asserted so the fixture proves the removal was surgical:
    // System::RuntimeTypeHandle is a REAL .NET public type and is untouched. The two names are
    // adjacent enough that a careless sweep could have taken both.
    System::RuntimeTypeHandle handle;
    (void)handle;
    static_assert(std::is_default_constructible_v<System::RuntimeTypeHandle>,
                  "#2334 removed RuntimeType only -- RuntimeTypeHandle is a real .NET type");
    return 0;
}
