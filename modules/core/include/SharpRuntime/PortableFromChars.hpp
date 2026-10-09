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
//
// The `requires` probe alone stopped being sufficient with LLVM 20's libc++ (Xcode 26+): the
// floating-point from_chars is now DECLARED at every deployment target, carrying
// `availability(macos/ios, strict, introduced=26.0)`. Availability is not a substitution
// failure, so the probe succeeds and the call is then a hard "unavailable" error below
// macOS/iOS 26. libc++ publishes the same decision as `_LIBCPP_AVAILABILITY_HAS_FROM_CHARS_
// FLOATING_POINT` (0 or 1); StdFromCharsFloatUsable folds it in, and is simply true on every
// standard library that does not define it.
#pragma once

#include <charconv>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>
#include <locale.h>
#if defined(__APPLE__)
#include <xlocale.h>
#endif

namespace SharpRuntime
{
    template <class T>
    concept HasFromCharsOverload = requires(const char* first, const char* last, T& value)
    {
        std::from_chars(first, last, value);
    };

#if defined(_LIBCPP_AVAILABILITY_HAS_FROM_CHARS_FLOATING_POINT)
    inline constexpr bool StdFromCharsFloatUsable = _LIBCPP_AVAILABILITY_HAS_FROM_CHARS_FLOATING_POINT;
#else
    inline constexpr bool StdFromCharsFloatUsable = true;
#endif

    // strtof/strtod take NO end pointer: they scan until a NUL and there is no way to tell them
    // where the caller's range stops. This function used to hand them `first` and simply discard
    // `last`, which is SR-AUD-180: the C parser ran straight past the declared range.
    //
    //   * `"12"` restricted to `[0,1)` returned value 12 and a pointer TWO past a one-character
    //     range, where a real from_chars returns 1 and stops at offset 1;
    //   * `"1e3"` restricted to `[0,1)` returned 1000 rather than 1;
    //   * with the range placed flush against a PROT_NONE guard page, the read at `last` is a
    //     SEGFAULT -- while std::from_chars on the same layout survives.
    //
    // AddressSanitizer cannot see any of it: the over-read happens inside the C library's own
    // strtod, which is neither instrumented nor an ASan interceptor, so a heap buffer with no NUL
    // in it produces no report at all. That is why the evidence for this repair is a guard page
    // rather than a sanitizer (docs/CoreDefinedArithmeticBoundedParseFamilyPlan.md section 5.3.1).
    //
    // The header also used to claim that "every real call site here passes `s.data()`/`s.data() +
    // s.size()` from a std::string". That is false and was measured false: Single::tryParseCore
    // and Double::tryParseCore trim surrounding ASCII whitespace (ticket #1864) BEFORE forming the
    // range, so for `" 1.5 "` they hand over a subrange whose `last` is a space, not the string
    // terminator. Only the XPath caller passes a whole string.
    //
    // The repair is to give the C parser a range it cannot leave: copy `[first, last)` into a
    // NUL-terminated local buffer, parse the copy, and rebase the returned pointer into the
    // caller's range. The copy is exactly `len + 1` bytes -- no multiplication, no amplification
    // of a caller-controlled length -- and stays on the stack for every realistic numeric literal.
    //
    // strtof/strtod are still NOT a drop-in behavioral match for std::from_chars, and this
    // function corrects for the known real differences before delegating rather than silently
    // inheriting them:
    //  1. strtof/strtod skip leading whitespace per the C standard; std::from_chars does not
    //     skip any whitespace at all (this codebase's own callers rely on that strictness --
    //     see System/Xml/XmlConvert.cpp's own comment on this exact point).
    //  2. strtof/strtod accept a leading '+' sign per the C standard; std::from_chars's
    //     floating-point grammar does not ("a minus sign is parsed, but a plus sign is not",
    //     a well-known, deliberate asymmetry from most other numeric parsers).
    // Both are rejected up front as std::errc::invalid_argument, matching what a real
    // std::from_chars call would do for the same input, before strtof/strtod ever run.
    //  3. strtof/strtod accept C99 hexadecimal floating literals; std::from_chars with
    //     chars_format::general does not, and stops at the 'x' (ticket #2222). Handled in the
    //     body, where the copy is truncated so the C parser sees only what the standard grammar
    //     would have consumed.
    //
    // noexcept is ADDED rather than dropped, and it is load-bearing: std::from_chars is itself
    // noexcept, and Single::tryParseCore / Single::TryParse are noexcept, so on the fallback
    // platform a throwing helper would have called std::terminate. The heap path for an
    // over-long range therefore uses a nothrow allocation and reports std::errc::not_enough_memory
    // -- the one errc a real std::from_chars never returns, reachable only when a copy of the
    // caller's own range cannot be allocated, and treated as failure by every caller here.
    template <class T>
    inline std::from_chars_result PortableFromCharsFloat(const char* first, const char* last, T& value) noexcept
    {
        if (first == last || *first == '+' ||
            *first == ' ' || *first == '\t' || *first == '\n' || *first == '\r' ||
            *first == '\f' || *first == '\v')
            return {first, std::errc::invalid_argument};

        // Large enough for every representable decimal literal plus a long digit run; anything
        // longer takes the nothrow heap path rather than being truncated, because truncating a
        // digit run changes the value (a 600-digit integer is not its first 511 digits).
        constexpr std::size_t stackCapacity = 512;
        std::size_t length = static_cast<std::size_t>(last - first);

        // Difference 3, ticket #2222: strtof/strtod accept C99 HEXADECIMAL floating literals and
        // std::from_chars with chars_format::general does not. Measured, the standard function
        // stops at the 'x' and reports the leading zero it did consume: "0x10" -> value 0 with
        // ptr at offset 1, "-0x10" -> value -0 with ptr at offset 2. Truncating the copy right
        // after that zero reproduces exactly that, for every measured shape, including "0x",
        // "0X1p3" and "0xg". Sequences where the 'x' does not immediately follow the FIRST digit
        // ("00x1", "0.0x1") are not hexadecimal prefixes and are deliberately left alone -- the C
        // parser already stops in the right place there, which the tests pin.
        {
            const std::size_t digitStart = (length > 0 && *first == '-') ? 1u : 0u;
            if (length > digitStart + 1 && first[digitStart] == '0' &&
                (first[digitStart + 1] == 'x' || first[digitStart + 1] == 'X'))
                length = digitStart + 1;
        }
        char stackBuffer[stackCapacity];
        std::unique_ptr<char[]> heapBuffer;
        char* buffer = stackBuffer;
        if (length >= stackCapacity) {
            heapBuffer.reset(new (std::nothrow) char[length + 1]);
            if (!heapBuffer)
                return {first, std::errc::not_enough_memory};
            buffer = heapBuffer.get();
        }
        std::memcpy(buffer, first, length);
        buffer[length] = '\0';

        errno = 0;
        char* endPtr = nullptr;
        T parsed;
#if defined(__APPLE__)
        // AM4-112: strtof/strtod follow the process's LC_NUMERIC, so under a comma-decimal locale
        // ("de_DE", "cs_CZ") a host application had set, "1.5" read as 1 -- std::from_chars is
        // locale-independent. Apple is where production reaches this fallback (libc++ below
        // macOS/iOS 26), and its xlocale *_l functions parse in an explicit C locale, created once.
        static const locale_t cLocale = ::newlocale(LC_ALL_MASK, "C", nullptr);
        if constexpr (std::is_same_v<T, float>)
            parsed = ::strtof_l(buffer, &endPtr, cLocale);
        else
            parsed = static_cast<T>(::strtod_l(buffer, &endPtr, cLocale));
#elif defined(_WIN32)
        // AM4-253: the same LC_NUMERIC dependence as on Apple, through the CRT's explicit-locale
        // parsers.
        static const _locale_t cLocale = ::_create_locale(LC_NUMERIC, "C");
        if constexpr (std::is_same_v<T, float>)
            parsed = ::_strtof_l(buffer, &endPtr, cLocale);
        else
            parsed = static_cast<T>(::_strtod_l(buffer, &endPtr, cLocale));
#else
        // AM4-253: the same LC_NUMERIC dependence as on Apple, measured on Linux once CI had a
        // comma-decimal locale to run PortableFromCharsLocaleTests under. POSIX.1-2008's per-thread
        // locale is what glibc, musl, bionic and Emscripten all offer; the thread's own locale,
        // and the errno the parse left, are restored before anything else reads them.
        static const locale_t cLocale = ::newlocale(LC_ALL_MASK, "C", static_cast<locale_t>(0));
        const locale_t previousLocale =
            cLocale != static_cast<locale_t>(0) ? ::uselocale(cLocale) : static_cast<locale_t>(0);
        errno = 0;
        if constexpr (std::is_same_v<T, float>)
            parsed = std::strtof(buffer, &endPtr);
        else
            parsed = static_cast<T>(std::strtod(buffer, &endPtr));
        if (previousLocale != static_cast<locale_t>(0))
        {
            const int parseErrno = errno;
            ::uselocale(previousLocale);
            errno = parseErrno;
        }
#endif

        // Rebase into the caller's range. The consumed count cannot exceed `length`, because the
        // copy is exactly that long and its terminator stops the parse.
        const char* const consumedEnd = first + (endPtr - buffer);

        if (endPtr == buffer)
            return {first, std::errc::invalid_argument};
        // strtod/strtof also set ERANGE when the result underflows into the SUBNORMAL range, but
        // a subnormal is representable and std::from_chars returns it as an ordinary value
        // ("1e-320" -> 9.99989e-321, errc{}). Only overflow (+/-HUGE_VAL) and underflow all the
        // way to zero are out of range there, so only those are reported as such here.
        if (errno == ERANGE && (parsed == T{} || !std::isfinite(parsed)))
            return {consumedEnd, std::errc::result_out_of_range};
        value = parsed;
        return {consumedEnd, std::errc{}};
    }

    // Drop-in replacement for `std::from_chars(first, last, value)` (the 3-argument
    // chars_format::general-equivalent overload) that degrades gracefully to the strtof/strtod
    // fallback above when the platform's own std::from_chars has no floating-point overload at
    // all. Same {ptr, ec} return shape as std::from_chars, so existing
    // `auto [ptr, ec] = ...` call sites need only the function name changed.
    template <class T>
    inline std::from_chars_result FromCharsFloat(const char* first, const char* last, T& value) noexcept
    {
        if constexpr (StdFromCharsFloatUsable && HasFromCharsOverload<T>)
            return std::from_chars(first, last, value);
        else
            return PortableFromCharsFloat(first, last, value);
    }
}
