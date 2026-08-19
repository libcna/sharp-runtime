// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1999 (SR-AUD-148, cause U-I).
//
// #1999 changed System::UriTypeConverter::ConvertFrom's return type from `Uri` to
// `std::optional<Uri>`. The by-value Uri CANNOT EXPRESS .NET's null, so an empty string was
// forwarded straight to the Uri constructor and threw UriFormatException. .NET returns null:
//
//     if (value is string uriString)
//     {
//         if (string.IsNullOrEmpty(uriString)) { return null; }
//         // Let the Uri constructor throw any informative exceptions
//         return new Uri(uriString, UriKind.RelativeOrAbsolute);
//     }                                                    // UriTypeConverter.cs:40-51
//
// The empty case is the ONLY one .NET short-circuits, so the widening is exactly one input wide.
//
// This is a public VIRTUAL signature change, which is why it lands under SA-10 (whose list names
// "return type" explicitly) with SA-2's five conditions, rather than under SA-3 -- SA-3's
// exclusion is a change to the vtable's SHAPE (adding or removing a virtual, or changing the
// base), not a signature within an existing slot. Measured, there are ZERO overrides and ZERO
// derivations anywhere, so the design record's "mandatory migration for every override" cost is
// zero; what a future override would lose is site 3 below.
//
// Migration: bind the result to `auto` (or `std::optional<Uri>`) and test `has_value()`.
//
// Records: docs/Migration-UriTypeConverterOptional.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Uri
#include <optional>
#include <string>
#include <type_traits>

#include "System/UriTypeConverter.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Uri;
using System::UriTypeConverter;

int main() {
    const UriTypeConverter converter;

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(uritypeconverter-by-value-result): conversion from
    //     | cannot convert
    //     | no viable conversion
    Uri byValue = converter.ConvertFrom("http://example.com");
    (void)byValue;
#else
    auto byValue = converter.ConvertFrom("http://example.com");
    (void)byValue;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(uritypeconverter-direct-member-access): base operand of '->' has non-pointer type
    //     | no member named
    //     | base operand of
    // THE SPELLING MOST LIKELY TO SURVIVE A CARELESS MIGRATION: calling straight through the
    // result, which used to be a Uri and is now an optional.
    const std::string host = converter.ConvertFrom("http://example.com").getHostProperty();
    (void)host;
#else
    const std::string host = converter.ConvertFrom("http://example.com")->getHostProperty();
    (void)host;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(uritypeconverter-override-old-signature): invalid covariant return type
    //     | does not override
    //     | marked 'override', but does not override
    // What a FUTURE override loses. Measured, there are none today -- in this repository or in
    // either downstream consumer -- which is why the recorded migration cost was zero.
    //
    // The diagnostic is "invalid covariant return type" rather than "does not override", and that
    // is more precise than expected: gcc reads the old signature as an ATTEMPTED covariant
    // override of the new one and rejects it on the spot, naming the reason.
    struct MyConverter final : UriTypeConverter {
        Uri ConvertFrom(const std::string& text) const override { return Uri(text); }
    };
    (void)sizeof(MyConverter);
#else
    struct MyConverter final : UriTypeConverter {
        std::optional<Uri> ConvertFrom(const std::string& text) const override {
            return text.empty() ? std::optional<Uri>{} : std::optional<Uri>{Uri(text)};
        }
    };
    (void)sizeof(MyConverter);
#endif

    // UNCHANGED, and asserted so the fixture proves what did NOT break: ConvertTo still returns a
    // string by value, the type is still polymorphic, and the empty input no longer throws.
    static_assert(std::is_same_v<decltype(converter.ConvertTo(std::declval<const Uri&>())),
                                  std::string>,
                  "#1999 must not have touched ConvertTo");
    static_assert(std::is_polymorphic_v<UriTypeConverter>, "still polymorphic");
    return converter.ConvertFrom("").has_value() ? 1 : 0;
}
