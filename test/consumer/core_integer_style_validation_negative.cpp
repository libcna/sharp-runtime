// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2269 (SR-AUD-178).
//
// #2269 made all eight integer wrappers validate the NumberStyles they are
// given, as .NET does at every integer overload
// (NumberFormatInfo.ValidateParseStyleInteger, NumberFormatInfo.cs:810-826).
// This port validated NOTHING: measured, Parse("42", (NumberStyles)0x8000)
// returned 42 and Parse("2A", NumberStyles::HexFloat) returned hexadecimal 42.
//
// The RUNTIME half of that break cannot be a compile fixture -- an invalid style
// now throws where it used to parse, which no compiler can see. What CAN be
// pinned here is the SIGNATURE half: four of the eight TryParse(style) overloads
// were noexcept and had to stop being so, because calling a throwing validator
// from a noexcept member would be std::terminate rather than a diagnostic.
// Validating only the four that could already throw would have left the port
// inconsistent with itself.
//
// Migration: stop passing undefined bits, and do not combine AllowHexSpecifier
// or AllowBinarySpecifier with anything but AllowLeadingWhite and
// AllowTrailingWhite. If your own noexcept was computed from one of these
// overloads, it is no longer noexcept.
//
// Records: docs/Migration-IntegerStyleValidation.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base allow=int128-extension
#include <string>
#include <utility>

#include "System/Byte.hpp"
#include "System/Globalization/NumberStyles.hpp"
#include "System/Int32.hpp"
#include "System/Int64.hpp"
#include "System/SByte.hpp"
#include "System/UInt32.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Globalization::NumberStyles;

int main() {
    const std::string text("42");
    SharpRuntime::bytecs  b   = 0;
    SharpRuntime::sbytecs sb  = 0;
    SharpRuntime::uintcs  u32 = 0;
    SharpRuntime::longcs  i64 = 0;
    SharpRuntime::intcs   i32 = 0;

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(byte-tryparse-style-still-noexcept): static assertion failed
    //     | static_assert
    static_assert(noexcept(System::Byte::TryParse(text, NumberStyles::Integer, nullptr, b)),
                  "Byte::TryParse(style) is expected to be noexcept");
#else
    static_assert(!noexcept(System::Byte::TryParse(text, NumberStyles::Integer, nullptr, b)),
                  "#2269: it validates the style, and an invalid one throws");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(sbyte-tryparse-style-still-noexcept): static assertion failed
    //     | static_assert
    static_assert(noexcept(System::SByte::TryParse(text, NumberStyles::Integer, nullptr, sb)),
                  "SByte::TryParse(style) is expected to be noexcept");
#else
    static_assert(!noexcept(System::SByte::TryParse(text, NumberStyles::Integer, nullptr, sb)),
                  "#2269");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(uint32-tryparse-style-still-noexcept): static assertion failed
    //     | static_assert
    static_assert(noexcept(System::UInt32::TryParse(text, NumberStyles::Integer, nullptr, u32)),
                  "UInt32::TryParse(style) is expected to be noexcept");
#else
    static_assert(!noexcept(System::UInt32::TryParse(text, NumberStyles::Integer, nullptr, u32)),
                  "#2269");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(int64-tryparse-style-still-noexcept): static assertion failed
    //     | static_assert
    static_assert(noexcept(System::Int64::TryParse(text, NumberStyles::Integer, nullptr, i64)),
                  "Int64::TryParse(style) is expected to be noexcept");
#else
    static_assert(!noexcept(System::Int64::TryParse(text, NumberStyles::Integer, nullptr, i64)),
                  "#2269");
#endif

    // UNCHANGED, and asserted so the fixture proves the drop was confined to the STYLE-taking
    // overloads. The simple two-argument TryParse never validated a style and keeps its noexcept,
    // which is what stops this from being a blanket relaxation.
    static_assert(noexcept(System::Byte::TryParse(text, b)),
                  "the style-less overload keeps its noexcept");
    static_assert(noexcept(System::Int64::TryParse(text, i64)),
                  "the style-less overload keeps its noexcept");

    return System::Int32::TryParse(text, NumberStyles::Integer, nullptr, i32) && i32 == 42 ? 0 : 1;
}
