// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2291 (SR-AUD-117).
//
// #2291 took all four of the review's decisions toward .NET:
//   1. the constructor rejects an empty name (no signature change);
//   2. the public key token is a byte container cloned on the way IN and OUT,
//      where it was a std::string stored verbatim -- .NET's is
//      `byte[]` with `(byte[])_publicKeyToken.Clone()` at both ends;
//   3. Culture and ProcessorArchitecture are std::optional, matching `string?`;
//   4. ToString() adopts .NET's grammar.
//
// (2) and (3) are the source break. Each broken spelling is compiled on its own
// below; the #else branches are the migrated ones.
//
// Migration: pass and receive std::vector<SharpRuntime::bytecs> for the token,
// and hold the two optional components in std::optional -- a string literal
// still converts implicitly, so only READING them changes.
//
// Records: docs/Migration-ApplicationIdDotNetShape.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "System/ApplicationId.hpp"
#include "System/Version.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::ApplicationId;
using System::Version;

int main() {
    const std::vector<SharpRuntime::bytecs> token{0xDE, 0xAD};

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(applicationid-string-token-argument): no matching function
    //     | cannot convert
    //     | no known conversion
    ApplicationId id("token123", "MyApp", Version(1, 0), "amd64", "neutral");
    (void)id;
#else
    ApplicationId id(token, "MyApp", Version(1, 0), "amd64", "neutral");
    (void)id;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(applicationid-token-compared-to-string): no match for
    //     | no operator
    //     | invalid operands
    const bool same = id.getPublicKeyTokenProperty() == "token123";
    (void)same;
#else
    const bool same = id.getPublicKeyTokenProperty() == token;
    (void)same;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(applicationid-culture-bound-to-string-ref): cannot bind
    //     | invalid initialization
    //     | conversion from
    const std::string& culture = id.getCultureProperty();
    (void)culture;
#else
    const std::string culture = id.getCultureProperty().value_or("");
    (void)culture;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(applicationid-token-getter-still-string): static assertion failed
    //     | static_assert
    // The shape that breaks a consumer SILENTLY: a trait on the getter's return type. It also
    // catches a subtler regression -- a getter changed back to `const&` would still be a byte
    // container but would stop being the defensive copy .NET makes on every access.
    static_assert(std::is_same_v<decltype(id.getPublicKeyTokenProperty()), const std::string&>,
                  "the token getter is expected to return a string reference");
#else
    static_assert(std::is_same_v<decltype(id.getPublicKeyTokenProperty()),
                                 std::vector<SharpRuntime::bytecs>>,
                  "#2291: the token getter returns a COPY by value, as .NET's Clone() does");
    static_assert(std::is_same_v<decltype(id.getCultureProperty()),
                                 const std::optional<std::string>&>,
                  "#2291: culture is nullable");
#endif

    // UNCHANGED, and asserted so the fixture proves what did NOT break: a string literal still
    // converts implicitly into the optional parameters, so a CONSTRUCTION site that supplies both
    // components needs no edit beyond the token.
    ApplicationId stillFine(token, "MyApp", Version(1, 0), "amd64", "neutral");
    return stillFine.getNameProperty() == "MyApp" ? 0 : 1;
}
