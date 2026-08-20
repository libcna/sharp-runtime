// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// #1997 group A-4 (SR-AUD-146). `UriParser`'s three override hooks were PUBLIC here where .NET's
// are `protected`, so any caller holding a `UriParser&` could invoke another parser's hook
// directly. They exist to be OVERRIDDEN, never CALLED -- .NET says so in a comment of its own,
// describing its internal forwarders as existing "to avoid `protected internal` signatures in the
// public docs" (`UriSyntax.cs:245-246`).
//
// A behavioural test cannot see this: a hook that is public and a hook that is protected behave
// identically wherever both compile. THE ONLY INSTRUMENT THAT CAN REPORT IT IS THE COMPILER, which
// is what this fixture is.
//
// WITH NO SITE SELECTED THIS FILE MUST COMPILE CLEAN -- the migrated spellings are in the `#else`
// branches, and they are the shape .NET itself uses: a subclass that wants its own hook reachable
// from outside publishes a forwarder.

// NEGATIVE-FIXTURE: component=Uri

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

#include <memory>
#include <string>

#include "System/Uri.hpp"
#include "System/UriComponents.hpp"
#include "System/UriFormat.hpp"
#include "System/UriParser.hpp"

namespace {

class ConsumerParser final : public System::UriParser {
public:
    // The migrated shape: forwarders, exactly as .NET's `InternalGetComponents` and friends are.
    bool CallIsBaseOf(const System::Uri& b, const System::Uri& r) { return IsBaseOf(b, r); }
    bool CallIsWellFormed(const System::Uri& u) { return IsWellFormedOriginalString(u); }
};

} // namespace

int main() {
    const System::Uri base("http://example.com/");
    const System::Uri rel("http://example.com/path");
    ConsumerParser parser;

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(call-is-base-of-from-outside): is protected within this context
    //     | 'IsBaseOf' is a protected member
    (void)parser.IsBaseOf(base, rel);
#else
    (void)parser.CallIsBaseOf(base, rel);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // The hook most likely to survive a careless migration, because a caller who only ever wanted
    // "does this parser accept the string" has no reason to think of it as an override point.
    // NEGATIVE(call-is-well-formed-from-outside): is protected within this context
    //     | 'IsWellFormedOriginalString' is a protected member
    (void)parser.IsWellFormedOriginalString(base);
#else
    (void)parser.CallIsWellFormed(base);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(call-get-components-from-outside): is protected within this context
    //     | 'GetComponents' is a protected member
    (void)parser.GetComponents(base, System::UriComponents::Scheme, System::UriFormat::Unescaped);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // OnRegister is NEW surface, and it is protected from the day it lands rather than being
    // published and narrowed later -- so this site pins a spelling that was never legal.
    // NEGATIVE(call-on-register-from-outside): is protected within this context
    //     | 'OnRegister' is a protected member
    parser.OnRegister("scheme", 80);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // The registration state is PRIVATE and its accessors are protected. A consumer reading a
    // parser's scheme from outside is the shape that would quietly re-widen the type.
    // NEGATIVE(read-scheme-name-from-outside): is protected within this context
    //     | 'getSchemeNameProperty' is a protected member
    (void)parser.getSchemeNameProperty();
#endif

    return 0;
}
