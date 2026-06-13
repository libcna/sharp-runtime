// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cctype>
#include <cwctype>
#include <stdexcept>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

class Char {
public:
    static constexpr SharpRuntime::charcs MaxValue = 0xFFFFu;
    static constexpr SharpRuntime::charcs MinValue = u'\0';

    static bool IsLetter(SharpRuntime::charcs c)       { return std::iswalpha(static_cast<wint_t>(c)) != 0; }
    static bool IsDigit(SharpRuntime::charcs c)        { return std::iswdigit(static_cast<wint_t>(c)) != 0; }
    static bool IsLetterOrDigit(SharpRuntime::charcs c){ return IsLetter(c) || IsDigit(c); }
    static bool IsWhiteSpace(SharpRuntime::charcs c)   { return std::iswspace(static_cast<wint_t>(c)) != 0; }
    static bool IsUpper(SharpRuntime::charcs c)        { return std::iswupper(static_cast<wint_t>(c)) != 0; }
    static bool IsLower(SharpRuntime::charcs c)        { return std::iswlower(static_cast<wint_t>(c)) != 0; }
    static bool IsPunctuation(SharpRuntime::charcs c)  { return std::iswpunct(static_cast<wint_t>(c)) != 0; }
    static bool IsSymbol(SharpRuntime::charcs c)       { return !IsLetter(c) && !IsDigit(c) && !IsWhiteSpace(c) && !IsPunctuation(c) && !IsControl(c) && std::iswprint(static_cast<wint_t>(c)); }
    static bool IsControl(SharpRuntime::charcs c)      { return std::iswcntrl(static_cast<wint_t>(c)) != 0; }
    static bool IsNumber(SharpRuntime::charcs c)       { return std::iswdigit(static_cast<wint_t>(c)) != 0; }
    static bool IsSeparator(SharpRuntime::charcs c)    { return std::iswspace(static_cast<wint_t>(c)) != 0; }

    /// @brief Returns true if @p c is in the ASCII range (U+0000..U+007F).
    static bool IsAscii(SharpRuntime::charcs c)             { return c < 0x80u; }
    /// @brief Returns true if @p c is an ASCII decimal digit ('0'–'9').
    static bool IsAsciiDigit(SharpRuntime::charcs c)        { return c >= u'0' && c <= u'9'; }
    /// @brief Returns true if @p c is an ASCII uppercase letter ('A'–'Z').
    static bool IsAsciiUpper(SharpRuntime::charcs c)        { return c >= u'A' && c <= u'Z'; }
    /// @brief Returns true if @p c is an ASCII lowercase letter ('a'–'z').
    static bool IsAsciiLower(SharpRuntime::charcs c)        { return c >= u'a' && c <= u'z'; }
    /// @brief Returns true if @p c is an ASCII letter ('A'–'Z' or 'a'–'z').
    static bool IsAsciiLetter(SharpRuntime::charcs c)       { return IsAsciiUpper(c) || IsAsciiLower(c); }
    /// @brief Returns true if @p c is an ASCII letter or decimal digit.
    static bool IsAsciiLetterOrDigit(SharpRuntime::charcs c){ return IsAsciiLetter(c) || IsAsciiDigit(c); }
    /// @brief Returns true if @p c is an ASCII uppercase letter ('A'–'Z'). Alias for IsAsciiUpper.
    static bool IsAsciiLetterUpper(SharpRuntime::charcs c)  { return IsAsciiUpper(c); }
    /// @brief Returns true if @p c is an ASCII lowercase letter ('a'–'z'). Alias for IsAsciiLower.
    static bool IsAsciiLetterLower(SharpRuntime::charcs c)  { return IsAsciiLower(c); }
    /// @brief Returns true if @p c is a valid ASCII hexadecimal digit (0–9, A–F, a–f).
    static bool IsAsciiHexDigit(SharpRuntime::charcs c) {
        return IsAsciiDigit(c) || (c >= u'A' && c <= u'F') || (c >= u'a' && c <= u'f');
    }
    /// @brief Returns true if @p c is a lowercase ASCII hex digit (0–9, a–f).
    static bool IsAsciiHexDigitLower(SharpRuntime::charcs c) {
        return IsAsciiDigit(c) || (c >= u'a' && c <= u'f');
    }
    /// @brief Returns true if @p c is an uppercase ASCII hex digit (0–9, A–F).
    static bool IsAsciiHexDigitUpper(SharpRuntime::charcs c) {
        return IsAsciiDigit(c) || (c >= u'A' && c <= u'F');
    }

    static bool IsHighSurrogate(SharpRuntime::charcs c) { return c >= 0xD800 && c <= 0xDBFF; }
    static bool IsLowSurrogate(SharpRuntime::charcs c)  { return c >= 0xDC00 && c <= 0xDFFF; }
    static bool IsSurrogate(SharpRuntime::charcs c)     { return c >= 0xD800 && c <= 0xDFFF; }
    static bool IsSurrogatePair(SharpRuntime::charcs high, SharpRuntime::charcs low) {
        return IsHighSurrogate(high) && IsLowSurrogate(low);
    }

    static SharpRuntime::charcs ToUpper(SharpRuntime::charcs c) {
        return static_cast<SharpRuntime::charcs>(std::towupper(static_cast<wint_t>(c)));
    }
    static SharpRuntime::charcs ToLower(SharpRuntime::charcs c) {
        return static_cast<SharpRuntime::charcs>(std::towlower(static_cast<wint_t>(c)));
    }

    /// @brief Parses a UTF-8 string that contains exactly one Unicode code point and returns it
    ///        as a @c char16_t. Throws @c std::invalid_argument for empty input, invalid UTF-8,
    ///        or input that encodes more than one code point. Throws @c std::overflow_error for
    ///        code points above U+FFFF (outside the Basic Multilingual Plane).
    static SharpRuntime::charcs Parse(const std::string& s) {
        if (s.empty()) throw std::invalid_argument("String must be exactly one character long.");
        auto b0 = static_cast<unsigned char>(s[0]);
        uint32_t cp;
        size_t   bytes;
        if      (b0 < 0x80u) { cp = b0;        bytes = 1; }
        else if (b0 < 0xE0u) { cp = b0 & 0x1F; bytes = 2; }
        else if (b0 < 0xF0u) { cp = b0 & 0x0F; bytes = 3; }
        else                  { cp = b0 & 0x07; bytes = 4; }
        if (s.size() != bytes) throw std::invalid_argument("String must be exactly one character long.");
        for (size_t i = 1; i < bytes; ++i) {
            auto bi = static_cast<unsigned char>(s[i]);
            if ((bi & 0xC0u) != 0x80u) throw std::invalid_argument("Invalid UTF-8 sequence.");
            cp = (cp << 6) | (bi & 0x3Fu);
        }
        if (cp > 0xFFFFu) throw std::overflow_error("Character is outside BMP; cannot fit in char16_t.");
        return static_cast<SharpRuntime::charcs>(cp);
    }

    /// @brief Converts @p c to a UTF-8 encoded @c std::string (1–3 bytes for BMP code points).
    static std::string ToString(SharpRuntime::charcs c) {
        uint32_t cp = static_cast<uint32_t>(c);
        std::string r;
        if (cp < 0x80u) {
            r += static_cast<char>(cp);
        } else if (cp < 0x800u) {
            r += static_cast<char>(0xC0u | (cp >> 6));
            r += static_cast<char>(0x80u | (cp & 0x3Fu));
        } else {
            r += static_cast<char>(0xE0u | (cp >> 12));
            r += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
            r += static_cast<char>(0x80u | (cp & 0x3Fu));
        }
        return r;
    }

    static int GetNumericValue(SharpRuntime::charcs c) {
        if (c >= u'0' && c <= u'9') return c - u'0';
        return -1;
    }

    static int ConvertToUtf32(SharpRuntime::charcs high, SharpRuntime::charcs low) {
        if (!IsSurrogatePair(high, low)) throw std::invalid_argument("Not a valid surrogate pair.");
        return ((high - 0xD800) << 10) + (low - 0xDC00) + 0x10000;
    }
};

} // namespace System
