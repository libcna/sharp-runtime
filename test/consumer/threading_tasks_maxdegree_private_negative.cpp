// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2388 (SR-AUD-232's remaining half, split out of #1969).
//
// #2388 made System::Threading::Tasks::ParallelOptions::MaxDegreeOfParallelism a private member
// behind getMaxDegreeOfParallelismProperty()/setMaxDegreeOfParallelismProperty(), matching .NET's
// validating property (Parallel.cs:82-91).
//
// Ticket #1966 had already landed the validation, but at the entry of every Parallel method,
// because a public data member has nowhere to put a check -- and its doc-comment recorded that
// as a forced choice awaiting an approval. SA-8 granted it. The observable difference is real:
// an invalid degree used to be STORED and survive until a loop ran, so a caller that assigned
// and never looped got no diagnostic at all, and one that assigned and read the value back read
// a number .NET would never have let it hold.
//
// Migration: `opts.MaxDegreeOfParallelism = n` becomes
// `opts.setMaxDegreeOfParallelismProperty(n)`, and the read becomes
// `opts.getMaxDegreeOfParallelismProperty()`. 0 and every value below -1 now throw
// ArgumentOutOfRangeException at ASSIGNMENT, with parameter name "MaxDegreeOfParallelism" --
// .NET's nameof(MaxDegreeOfParallelism), deliberately NOT #1969's "value" (the reference is
// inconsistent between its two option types and both are transcribed as they are).
//
// Records: docs/Migration-ParallelOptionsMaxDegreeSetter.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Threading.Tasks
#include <type_traits>

#include "System/Threading/Tasks/Parallel.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Threading::Tasks::ParallelOptions;

int main() {
    ParallelOptions opts;

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(paralleloptions-maxdegree-direct-write): is private within this context
    //     | private
    opts.MaxDegreeOfParallelism = 4;
#else
    opts.setMaxDegreeOfParallelismProperty(4);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(paralleloptions-maxdegree-direct-read): is private within this context
    //     | private
    SharpRuntime::intcs degree = opts.MaxDegreeOfParallelism;
    (void)degree;
#else
    SharpRuntime::intcs degree = opts.getMaxDegreeOfParallelismProperty();
    (void)degree;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(paralleloptions-maxdegree-invalid-write): is private within this context
    //     | private
    // THE SITE THIS TICKET EXISTS FOR: the spelling that used to compile AND STICK. #1966 made
    // the loop reject it, but only when a loop was eventually run -- a caller that assigned and
    // never looped, or that read the value back, saw nothing wrong at all.
    opts.MaxDegreeOfParallelism = 0;
#else
    // The migrated spelling throws at assignment, which a compile fixture cannot assert, so
    // ParallelOptionsSetterTests.Fix2388_ZeroAndBelowMinusOneAreRejectedAtAssignment does.
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(paralleloptions-still-aggregate): static assertion failed
    //     | static_assert
    // The shape that breaks a consumer SILENTLY rather than loudly: brace initialisation, or a
    // template constrained on aggregate-ness. A private member ends it.
    static_assert(std::is_aggregate_v<ParallelOptions>,
                  "ParallelOptions is expected to be an aggregate");
#else
    static_assert(!std::is_aggregate_v<ParallelOptions>,
                  "#2388: the degree is private, so the type is no longer an aggregate");
#endif

    // UNCHANGED, and asserted so the fixture proves what did NOT break: default construction,
    // copyability, and the -1 default.
    const ParallelOptions defaulted;
    ParallelOptions       copied = opts;
    static_assert(std::is_copy_constructible_v<ParallelOptions>, "still copyable");
    return (defaulted.getMaxDegreeOfParallelismProperty() == -1 &&
            copied.getMaxDegreeOfParallelismProperty() == 4) ? 0 : 1;
}
