// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2295 (SR-AUD-116).
//
// #2295 made ObsoleteAttribute's Message, DiagnosticId and UrlFormat
// std::optional<std::string>, matching .NET, where all three are `string?`
// (ObsoleteAttribute.cs:34-40). Before it, three non-nullable std::strings made
// an ABSENT value and an EMPTY one the same state -- measured, a default
// attribute and one constructed with std::string{} compared equal -- and the
// boundary was on the way IN as well as on the way out, so no getter change
// alone could have closed it.
//
// The review priced this route as breaking three of the four call shapes that
// exist: .empty(), binding the result to const std::string&, and passing it to
// a function taking const std::string&. Only equality against a literal
// survives. Each is compiled on its own below; the #else branches are the
// migrated spellings.
//
// Migration: hold the result in `auto` / std::optional<std::string> and ask
// has_value(); or, where the old collapsed answer was all you needed, spell the
// fallback with value_or("").
//
// Records: docs/Migration-ObsoleteAttributeNullableComponents.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <optional>
#include <string>
#include <type_traits>

#include "System/ObsoleteAttribute.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::ObsoleteAttribute;

namespace {
bool consumesAString(const std::string& s) { return s.empty(); }
}  // namespace

int main() {
    const ObsoleteAttribute attr("Use the modern overload instead.");

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(obsoleteattribute-message-empty): has no member named
    //     | no member named
    bool none = attr.getMessageProperty().empty();
    (void)none;
#else
    const bool none = !attr.getMessageProperty().has_value();
    (void)none;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(obsoleteattribute-bind-to-string-ref): cannot bind
    //     | invalid initialization
    //     | conversion from
    const std::string& bound = attr.getMessageProperty();
    (void)bound;
#else
    const std::string bound = attr.getMessageProperty().value_or("");
    (void)bound;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(obsoleteattribute-pass-to-string-parameter): cannot convert
    //     | invalid initialization
    //     | no matching function
    const bool passed = consumesAString(attr.getUrlFormatProperty());
    (void)passed;
#else
    const bool passed = consumesAString(attr.getUrlFormatProperty().value_or(""));
    (void)passed;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(obsoleteattribute-getter-still-returns-string): static assertion failed
    //     | static_assert
    // The shape that breaks a consumer SILENTLY: a trait or decltype on the return type.
    static_assert(std::is_same_v<decltype(attr.getMessageProperty()), const std::string&>,
                  "the getter is expected to return a plain string reference");
#else
    static_assert(std::is_same_v<decltype(attr.getMessageProperty()),
                                 const std::optional<std::string>&>,
                  "#2295: the three components are nullable");
#endif

    // UNCHANGED, and asserted so the fixture proves what did NOT break. The review measured that
    // equality against a literal is the one surviving call shape; it survives because
    // std::optional compares against a value of its own type. IsError was never nullable in
    // .NET and is untouched.
    const bool sameMessage = attr.getMessageProperty() == "Use the modern overload instead.";
    static_assert(std::is_same_v<decltype(attr.getIsErrorProperty()), bool>,
                  "#2295: IsError is `bool` in .NET too -- not nullable");
    return sameMessage ? 0 : 1;
}
