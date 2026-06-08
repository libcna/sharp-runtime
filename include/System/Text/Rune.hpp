// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace System::Text {

    class Rune {
        uint32_t value_;

        static bool IsSurrogate(uint32_t v) { return v >= 0xD800 && v <= 0xDFFF; }
        static bool IsValidCodePoint(uint32_t v) { return v <= 0x10FFFF && !IsSurrogate(v); }

    public:
        static const Rune ReplacementChar;

        explicit Rune(uint32_t value) : value_(value) {
            if (!IsValidCodePoint(value))
                throw std::out_of_range("Invalid Unicode scalar value.");
        }
        explicit Rune(char c) : Rune(static_cast<uint32_t>(static_cast<unsigned char>(c))) {}

        [[nodiscard]] uint32_t getValueProperty()       const { return value_; }
        [[nodiscard]] int      getUtf16SequenceLengthProperty() const { return value_ >= 0x10000 ? 2 : 1; }
        [[nodiscard]] int      getUtf8SequenceLengthProperty()  const {
            if (value_ < 0x80)   return 1;
            if (value_ < 0x800)  return 2;
            if (value_ < 0x10000) return 3;
            return 4;
        }
        [[nodiscard]] bool getIsAsciiProperty()         const { return value_ < 0x80; }
        [[nodiscard]] bool getIsBmpProperty()           const { return value_ < 0x10000; }

        static bool IsValid(uint32_t value) { return IsValidCodePoint(value); }

        static bool IsAscii(Rune r) { return r.value_ < 0x80; }
        static bool IsWhiteSpace(Rune r) {
            static const uint32_t ws[] = {0x9,0xA,0xB,0xC,0xD,0x20,0x85,0xA0,0x1680,
                0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,0x2009,
                0x200A,0x2028,0x2029,0x202F,0x205F,0x3000,0xFEFF};
            for (uint32_t w : ws) if (r.value_ == w) return true;
            return false;
        }
        static bool IsLetter(Rune r)        { return (r.value_ >= 'A' && r.value_ <= 'Z') || (r.value_ >= 'a' && r.value_ <= 'z'); }
        static bool IsDigit(Rune r)         { return r.value_ >= '0' && r.value_ <= '9'; }
        static bool IsLetterOrDigit(Rune r) { return IsLetter(r) || IsDigit(r); }
        static bool IsUpper(Rune r)         { return r.value_ >= 'A' && r.value_ <= 'Z'; }
        static bool IsLower(Rune r)         { return r.value_ >= 'a' && r.value_ <= 'z'; }

        static Rune ToUpper(Rune r) {
            if (IsLower(r)) return Rune(r.value_ - 32);
            return r;
        }
        static Rune ToLower(Rune r) {
            if (IsUpper(r)) return Rune(r.value_ + 32);
            return r;
        }

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

        bool operator==(const Rune& o) const { return value_ == o.value_; }
        bool operator!=(const Rune& o) const { return value_ != o.value_; }
        bool operator< (const Rune& o) const { return value_ <  o.value_; }
        bool operator<=(const Rune& o) const { return value_ <= o.value_; }
        bool operator> (const Rune& o) const { return value_ >  o.value_; }
        bool operator>=(const Rune& o) const { return value_ >= o.value_; }
    };

    inline const Rune Rune::ReplacementChar{uint32_t(0xFFFD)};

} // namespace System::Text
