// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2325 (SR-AUD-123).
//
// #2325 changed System::ResolveEventHandler's return from std::string to
// std::optional<std::string>, matching .NET's `delegate Assembly?
// ResolveEventHandler(...)` (ResolveEventHandler.cs:8), where null means "I
// could not resolve this" and the runtime tries the next handler. The port's
// total function had no way to say that, and the empty string could not be
// borrowed for it -- empty already means "absent requesting assembly"
// elsewhere in ResolveEventArgs.
//
// The change is a WIDENING on the HANDLER side: a lambda returning std::string
// still binds, because std::string converts implicitly to the optional. What
// breaks is a CALLER that assigns the result to a std::string, or asserts the
// alias's signature. Each is compiled on its own below.
//
// Migration: hold the result in `auto` / std::optional<std::string> and test
// has_value(); or spell the old collapsed answer with value_or("").
//
// Records: docs/Migration-ResolveEventHandlerOptional.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <optional>
#include <string>
#include <type_traits>

#include "System/ResolveEventArgs.hpp"
#include "System/ResolveEventHandler.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

int main() {
    System::ResolveEventArgs args("Some.Assembly");
    System::ResolveEventHandler handler = [](void*, System::ResolveEventArgs& a) {
        return std::optional<std::string>(a.getNameProperty() + ".dll");
    };

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(resolveeventhandler-result-to-string): conversion from
    //     | cannot convert
    //     | no viable conversion
    std::string resolved = handler(nullptr, args);
    (void)resolved;
#else
    const std::optional<std::string> resolved = handler(nullptr, args);
    (void)resolved;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(resolveeventhandler-result-string-member): has no member named
    //     | no member named
    bool none = handler(nullptr, args).empty();
    (void)none;
#else
    const bool none = !handler(nullptr, args).has_value();
    (void)none;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(resolveeventhandler-signature-still-string): static assertion failed
    //     | static_assert
    // The shape that breaks a consumer SILENTLY: a trait or a result_type assertion.
    static_assert(std::is_same_v<System::ResolveEventHandler::result_type, std::string>,
                  "the handler is expected to return std::string");
#else
    static_assert(std::is_same_v<System::ResolveEventHandler::result_type,
                                 std::optional<std::string>>,
                  "#2325: the handler's result is nullable");
#endif

    // UNCHANGED, and asserted so the fixture proves what did NOT break: a handler that always
    // succeeds still binds without an edit, because std::string converts to the optional.
    System::ResolveEventHandler legacyShape =
        [](void*, System::ResolveEventArgs& a) -> std::string { return a.getNameProperty(); };
    return legacyShape(nullptr, args).has_value() ? 0 : 1;
}
