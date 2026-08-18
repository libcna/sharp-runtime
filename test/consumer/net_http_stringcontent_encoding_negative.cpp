// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2070 (SR-AUD-317): proves that
// System::Net::Http::StringContent's second parameter is no longer a charset STRING.
//
// The old signature let the declared charset and the emitted bytes contradict each other --
// StringContent("\xc3\xa9", "utf-16") announced charset=utf-16 and emitted the two UTF-8 bytes
// c3 a9, which a conforming server decodes as one UTF-16 code unit, U+A9C3. .NET makes that
// state UNREPRESENTABLE rather than validating against it: its constructor takes an Encoding,
// serialises through it (StringContent.cs:90-98) and labels the header with that same object's
// WebName (:73), so there is one source of truth and the two cannot disagree.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by the compiler;
// with no site selected the file compiles cleanly.
// scripts/check_negative_consumer_fixtures.py compiles each site on its own and asserts the
// rejection; the record is docs/NegativeConsumerFixtureValidation.md.
//
// Migration: pass a System::Text::Encoding instead of a charset name.
//
//     StringContent body(text, "utf-16", "text/plain");                       // before
//     StringContent body(text, System::Text::Encoding::Unicode(), "text/plain");  // after
//
// A caller that passed "utf-8", or nothing, needs no change beyond the spelling: the default is
// still UTF-8, and `nullptr` means the same thing (.NET's `encoding ??= DefaultStringEncoding`).
//
// NEGATIVE-FIXTURE: component=Net.Http
#include "System/Net/Http/StringContent.hpp"
#include "System/Text/Encoding.hpp"
#include <string>

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Net::Http::StringContent;

void charsetIsNoLongerAString() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(charset-string-literal): no matching function for call
    //     | cannot convert
    //     | invalid conversion
    StringContent literal("body", "utf-16", "text/plain");
    (void)literal;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(charset-std-string): no matching function for call
    //     | cannot convert
    //     | invalid conversion
    const std::string charset = "utf-16";
    StringContent named("body", charset, "text/plain");
    (void)named;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // A two-argument call is the shape most likely to survive a careless migration, because
    // "utf-8" looks harmless -- and it is exactly the shape that used to lie.
    // NEGATIVE(charset-two-argument): no matching function for call
    //     | cannot convert
    //     | invalid conversion
    StringContent shortForm("body", "utf-8");
    (void)shortForm;
#endif

    // The migrated spellings, which must all compile -- and which the clean baseline needs, so
    // that a per-site verdict can be attributed to the site rather than to this file.
    StringContent viaEncoding("body", System::Text::Encoding::Unicode(), "text/plain");
    StringContent viaDefault("body");
    StringContent viaNull("body", nullptr, "text/plain");
    StringContent viaUtf8("body", System::Text::Encoding::UTF8());
    (void)viaEncoding;
    (void)viaDefault;
    (void)viaNull;
    (void)viaUtf8;
}
