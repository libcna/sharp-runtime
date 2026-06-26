// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace System::Text {

    /** Represents a Unicode scalar value (a valid code point excluding surrogates). */
    class Rune {
        uint32_t value_;

        static bool IsSurrogate(uint32_t v) { return v >= 0xD800 && v <= 0xDFFF; }
        static bool IsValidCodePoint(uint32_t v) { return v <= 0x10FFFF && !IsSurrogate(v); }

    public:
        /** The Unicode replacement character U+FFFD. */
        static const Rune ReplacementChar;

        /** Constructs a Rune from a Unicode code point; throws if the value is a surrogate or > U+10FFFF. */
        explicit Rune(uint32_t value) : value_(value) {
            if (!IsValidCodePoint(value))
                throw std::out_of_range("Invalid Unicode scalar value.");
        }
        /** Constructs a Rune from a plain ASCII char. */
        explicit Rune(char c) : Rune(static_cast<uint32_t>(static_cast<unsigned char>(c))) {}

        /** Gets the Unicode scalar value as a uint32_t. */
        [[nodiscard]] uint32_t getValueProperty()       const { return value_; }
        /** Gets the number of UTF-16 code units needed to encode this Rune (1 or 2). */
        [[nodiscard]] int      getUtf16SequenceLengthProperty() const { return value_ >= 0x10000 ? 2 : 1; }
        /** Gets the number of UTF-8 bytes needed to encode this Rune (1–4). */
        [[nodiscard]] int      getUtf8SequenceLengthProperty()  const {
            if (value_ < 0x80)   return 1;
            if (value_ < 0x800)  return 2;
            if (value_ < 0x10000) return 3;
            return 4;
        }
        /** Returns true if this Rune is in the ASCII range (U+0000–U+007F). */
        [[nodiscard]] bool getIsAsciiProperty()         const { return value_ < 0x80; }
        /** Returns true if this Rune is in the Basic Multilingual Plane (U+0000–U+FFFF). */
        [[nodiscard]] bool getIsBmpProperty()           const { return value_ < 0x10000; }

        /** Returns true if the given code point is a valid Unicode scalar value. */
        static bool IsValid(uint32_t value) { return IsValidCodePoint(value); }

        /** Returns true if the Rune is in the ASCII range. */
        static bool IsAscii(Rune r) { return r.value_ < 0x80; }
        /** Returns true if the Rune is a Unicode white-space character. */
        static bool IsWhiteSpace(Rune r) {
            static const uint32_t ws[] = {0x9,0xA,0xB,0xC,0xD,0x20,0x85,0xA0,0x1680,
                0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,0x2009,
                0x200A,0x2028,0x2029,0x202F,0x205F,0x3000,0xFEFF};
            for (uint32_t w : ws) if (r.value_ == w) return true;
            return false;
        }
        /** Returns true if the Rune is a letter (ASCII range only). */
        static bool IsLetter(Rune r)        { return (r.value_ >= 'A' && r.value_ <= 'Z') || (r.value_ >= 'a' && r.value_ <= 'z'); }
        /** Returns true if the Rune is an ASCII decimal digit. */
        static bool IsDigit(Rune r)         { return r.value_ >= '0' && r.value_ <= '9'; }
        /** Returns true if the Rune is a letter or digit (ASCII range only). */
        static bool IsLetterOrDigit(Rune r) { return IsLetter(r) || IsDigit(r); }
        /** Returns true if the Rune is an ASCII uppercase letter. */
        static bool IsUpper(Rune r)         { return r.value_ >= 'A' && r.value_ <= 'Z'; }
        /** Returns true if the Rune is an ASCII lowercase letter. */
        static bool IsLower(Rune r)         { return r.value_ >= 'a' && r.value_ <= 'z'; }

        /** Converts an ASCII lowercase letter to uppercase; other Runes are returned unchanged. */
        static Rune ToUpper(Rune r) {
            if (IsLower(r)) return Rune(r.value_ - 32);
            return r;
        }
        /** Converts an ASCII uppercase letter to lowercase; other Runes are returned unchanged. */
        static Rune ToLower(Rune r) {
            if (IsUpper(r)) return Rune(r.value_ + 32);
            return r;
        }

        /** Returns the UTF-8 encoded string representation of this Rune. */
        [[nodiscard]] std::string ToString() const {
            std::string s;
            if (value_ < 0x80) {
                s += static_cast<char>(value_);
            } else if (value_ < 0x800) {
                s += static_cast<char>(0xC0 | (value_ >> 6));
                s += static_cast<char>(0x80 | (value_ & 0x3F));
            } else if (value_ < 0x10000) {
                s += static_cast<char>(0xE0 | (value_ >> 12));
                s += static_cast<char>(0x80 | ((value_ >> 6) & 0x3F));
                s += static_cast<char>(0x80 | (value_ & 0x3F));
            } else {
                s += static_cast<char>(0xF0 | (value_ >> 18));
                s += static_cast<char>(0x80 | ((value_ >> 12) & 0x3F));
                s += static_cast<char>(0x80 | ((value_ >> 6) & 0x3F));
                s += static_cast<char>(0x80 | (value_ & 0x3F));
            }
            return s;
        }

        /** Equality operator. */
        bool operator==(const Rune& o) const { return value_ == o.value_; }
        /** Inequality operator. */
        bool operator!=(const Rune& o) const { return value_ != o.value_; }
        /** Less-than comparison by code point. */
        bool operator< (const Rune& o) const { return value_ <  o.value_; }
        /** Less-than-or-equal comparison by code point. */
        bool operator<=(const Rune& o) const { return value_ <= o.value_; }
        /** Greater-than comparison by code point. */
        bool operator> (const Rune& o) const { return value_ >  o.value_; }
        /** Greater-than-or-equal comparison by code point. */
        bool operator>=(const Rune& o) const { return value_ >= o.value_; }
    };

    inline const Rune Rune::ReplacementChar{uint32_t(0xFFFD)};

} // namespace System::Text
