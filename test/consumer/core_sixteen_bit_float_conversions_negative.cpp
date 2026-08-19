// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Negative compile fixture for ticket #2395: pins that every conversion INTO System::Half and
// System::Numerics::BFloat16 is EXPLICIT.
//
// #2395 renamed the raw-bits constructor to a named `FromBits` so the constructor signature was
// free for .NET's value conversions -- `Half(uint16_t)` was the RAW BIT PATTERN where .NET spends
// that signature on `explicit operator Half(ushort)`, which is `(Half)(float)value`, a NUMBER.
//
// WHAT THIS FIXTURE CANNOT DO, stated rather than implied: it cannot reject the pre-#2395
// spelling. `Half(0x3C00)` is valid under BOTH readings -- 1.0 as bits, 15360.0 as a number -- so
// an unmigrated site keeps compiling and silently changes meaning. No compile-time check can see
// that. It is why the rename landed FIRST, with no value-taking constructor present, so the
// compiler had to name every one of the 66 first-party sites before the conversions were added.
//
// What IS a compile-time guarantee is the explicitness, and that is what is pinned here. .NET
// makes the `byte` and `sbyte` conversions IMPLICIT (Half.cs:980,986; BFloat16.cs:824,830) and
// this port cannot: C++ permits a standard conversion BEFORE a user-defined one where C# does
// not, so an implicit converting constructor from `bytecs` makes EVERY integer argument
// ambiguous. Measured. What is lost is the implicitness, never the conversion.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by the compiler;
// with no site selected this file must compile with zero diagnostics.
//
// NEGATIVE-FIXTURE: component=Core.Base

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Half.hpp"
#include "System/Numerics/BFloat16.hpp"

using System::Half;
using System::Numerics::BFloat16;

namespace {

    void takesHalf(Half) {}
    void takesBFloat16(BFloat16) {}

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // .NET's byte -> Half is implicit; here it must not be. This is the site that would let an
    // integer argument silently become a 16-bit float at a call boundary.
    // NEGATIVE(byte-to-half-is-not-implicit): could not convert
    //     | cannot convert
    void site1() { takesHalf(static_cast<SharpRuntime::bytecs>(3)); }
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // The same for sbyte, which .NET also makes implicit.
    // NEGATIVE(sbyte-to-bfloat16-is-not-implicit): could not convert
    //     | cannot convert
    void site2() { takesBFloat16(static_cast<SharpRuntime::sbytecs>(-3)); }
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // int -> Half is explicit in .NET too, so this one is parity rather than a deviation. It is
    // pinned because it is the conversion an unmigrated raw-bits site would silently acquire.
    // NEGATIVE(int-to-half-is-not-implicit): could not convert
    //     | cannot convert
    void site3() { takesHalf(static_cast<SharpRuntime::intcs>(15360)); }
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // A copy-initialisation from double, the widest of the value conversions.
    // NEGATIVE(double-to-bfloat16-is-not-implicit): conversion from
    //     | could not convert
    //     | cannot convert
    void site4() { BFloat16 b = 1.5; (void)b; }
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // FromBits is the ONLY bit-pattern door. The types have user-declared constructors, so a
    // consumer cannot reach the storage by aggregate initialisation and re-acquire the old
    // meaning that way.
    // NEGATIVE(no-aggregate-initialisation-of-bits): no matching function for call to
    //     | cannot convert
    void site5() { BFloat16 b{}; b = BFloat16{0x3F80u, 0u}; (void)b; }
#endif

} // namespace

int main()
{
    // The migrated spellings, side by side, because the whole point of #2395 is that these two
    // are different things and now say so.
    const Half bitsHalf = Half::FromBits(0x3C00);          // 1.0
    const Half numberHalf(static_cast<SharpRuntime::ushortcs>(0x3C00));  // 15360.0
    (void)bitsHalf;
    (void)numberHalf;

    const BFloat16 bitsB = BFloat16::FromBits(0x3F80);     // 1.0
    const BFloat16 numberB(static_cast<SharpRuntime::ushortcs>(0x3F80)); // 16256.0
    (void)bitsB;
    (void)numberB;
    return 0;
}
