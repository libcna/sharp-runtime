// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2313 (SR-AUD-106).
//
// #2313 changed System::Environment's variable API so a variable that is
// PRESENT WITH AN EMPTY VALUE can be told apart from one that is ABSENT --
// a distinction .NET makes (Environment.Variables.Unix.cs:19-33 and 49-57)
// and POSIX makes, and which this port previously destroyed at the public
// boundary. GetEnvironmentVariable now returns std::optional<std::string>,
// and SetEnvironmentVariable takes an optional value where std::nullopt --
// not "" -- is what removes.
//
// The SETTER is source-compatible for every non-empty call, because
// std::optional<std::string> converts implicitly from const char* and
// std::string. The GETTER is not: assigning its result to a std::string, or
// calling a std::string member on it, no longer compiles. Those are the
// spellings pinned below, each compiled on its own so one broken line cannot
// hide another. The #else branches are the migrated spellings.
//
// Migration: hold the result in `auto` / std::optional<std::string> and ask
// has_value(); or, where the old "" answer was all you needed, spell the
// fallback explicitly with value_or("").
//
// Records: docs/Migration-EnvironmentNullableVariables.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <optional>
#include <string>
#include <type_traits>

#include "System/Environment.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Environment;

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(environment-get-assigned-to-string): conversion from
    //     | cannot convert
    //     | no viable conversion
    std::string value = Environment::GetEnvironmentVariable("PATH");
    (void)value;
#else
    const std::optional<std::string> value = Environment::GetEnvironmentVariable("PATH");
    (void)value;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(environment-get-string-member): has no member named
    //     | no member named
    bool absent = Environment::GetEnvironmentVariable("SHARP_RUNTIME_FIXTURE_ABSENT").empty();
    (void)absent;
#else
    const bool absent = !Environment::GetEnvironmentVariable("SHARP_RUNTIME_FIXTURE_ABSENT").has_value();
    (void)absent;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(environment-get-targeted-assigned-to-string): conversion from
    //     | cannot convert
    //     | no viable conversion
    std::string targeted =
        Environment::GetEnvironmentVariable("PATH", System::EnvironmentVariableTarget::Process);
    (void)targeted;
#else
    const auto targeted =
        Environment::GetEnvironmentVariable("PATH", System::EnvironmentVariableTarget::Process);
    (void)targeted;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(environment-get-return-type-still-string): static assertion failed
    //     | static_assert
    // The shape that breaks a consumer SILENTLY rather than loudly: code whose own type is
    // deduced or asserted from the return type.
    static_assert(std::is_same_v<decltype(Environment::GetEnvironmentVariable("X")), std::string>,
                  "GetEnvironmentVariable is expected to return std::string");
#else
    static_assert(std::is_same_v<decltype(Environment::GetEnvironmentVariable("X")),
                                 std::optional<std::string>>,
                  "#2313: the getter is nullable");
#endif

    // UNCHANGED, and asserted so the fixture also proves what did NOT break: every non-empty
    // setter call is source-compatible, because optional<string> converts implicitly. This is
    // what keeps the 98 measured downstream Set sites compiling untouched.
    Environment::SetEnvironmentVariable("SHARP_RUNTIME_FIXTURE_VAR", "a value");
    Environment::SetEnvironmentVariable("SHARP_RUNTIME_FIXTURE_VAR", std::string("a value"));
    Environment::SetEnvironmentVariable("SHARP_RUNTIME_FIXTURE_VAR", std::nullopt);
    Environment::SetEnvironmentVariable("SHARP_RUNTIME_FIXTURE_VAR", "a value",
                                        System::EnvironmentVariableTarget::Process);
    return 0;
}
