// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Portable floating-point std::from_chars: Apple's libc++ only exposes the float/double
// overloads of std::from_chars/std::to_chars when the build targets macOS 13.3+ / iOS 16.4+ (the
// underlying implementation was only added to the OS-shipped libc++.dylib at that OS version, and
// Apple's headers omit the declaration entirely below that deployment target rather than merely
// deprecating it -- confirmed via a real build: Clang reports "no viable overload" against only
// the *integer* from_chars template, meaning the floating-point overload isn't in the candidate
// set at all under an older deployment target, not just unavailable-if-called).
//
// PortableFromCharsFloat<T> uses the real std::from_chars when the compiler's own overload set
// actually contains a floating-point candidate (detected via a `requires` expression, not a
// version/platform guess), and falls back to strtof/strtod otherwise -- so this stays correct on
// every platform/deployment-target combination without forcing a higher minimum runtime OS.
#pragma once

#include <charconv>
#include <cerrno>
#include <cstdlib>
#include <type_traits>

namespace SharpRuntime
{
    template <class T>
    concept HasFromCharsOverload = requires(const char* first, const char* last, T& value)
    {
        std::from_chars(first, last, value);
    };

    // strtof/strtod require a NUL-terminated C string. Every real call site here passes
    // `s.data()`/`s.data() + s.size()` from a std::string, which is guaranteed NUL-terminated
    // since C++11 (s[s.size()] == '\0'), so `first` itself is always safe to hand directly to
    // strtof/strtod without an extra copy -- `last` is used only to detect trailing garbage via
    // the returned end pointer, matching std::from_chars's own "ptr != last means not fully
    // consumed" contract exactly.
    //
    // strtof/strtod are NOT a drop-in behavioral match for std::from_chars, and this function
    // corrects for the two known real differences before delegating, rather than silently
    // inheriting them:
    //  1. strtof/strtod skip leading whitespace per the C standard; std::from_chars does not
    //     skip any whitespace at all (this codebase's own callers rely on that strictness --
    //     see System/Xml/XmlConvert.cpp's own comment on this exact point).
    //  2. strtof/strtod accept a leading '+' sign per the C standard; std::from_chars's
    //     floating-point grammar does not ("a minus sign is parsed, but a plus sign is not",
    //     a well-known, deliberate asymmetry from most other numeric parsers).
    // Both are rejected up front as std::errc::invalid_argument, matching what a real
    // std::from_chars call would do for the same input, before strtof/strtod ever run.
    template <class T>
    inline std::from_chars_result PortableFromCharsFloat(const char* first, const char* last, T& value)
    {
        if (first == last || *first == '+' ||
            *first == ' ' || *first == '\t' || *first == '\n' || *first == '\r' ||
            *first == '\f' || *first == '\v')
            return {first, std::errc::invalid_argument};

        errno = 0;
        char* endPtr = nullptr;
        T parsed;
        if constexpr (std::is_same_v<T, float>)
            parsed = std::strtof(first, &endPtr);
        else
            parsed = static_cast<T>(std::strtod(first, &endPtr));

        if (endPtr == first)
            return {first, std::errc::invalid_argument};
        if (errno == ERANGE)
            return {endPtr, std::errc::result_out_of_range};
        value = parsed;
        return {endPtr, std::errc{}};
    }

    // Drop-in replacement for `std::from_chars(first, last, value)` (the 3-argument
    // chars_format::general-equivalent overload) that degrades gracefully to the strtof/strtod
    // fallback above when the platform's own std::from_chars has no floating-point overload at
    // all. Same {ptr, ec} return shape as std::from_chars, so existing
    // `auto [ptr, ec] = ...` call sites need only the function name changed.
    template <class T>
    inline std::from_chars_result FromCharsFloat(const char* first, const char* last, T& value)
    {
        if constexpr (HasFromCharsOverload<T>)
            return std::from_chars(first, last, value);
        else
            return PortableFromCharsFloat(first, last, value);
    }
}
