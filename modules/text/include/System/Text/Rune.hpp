// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Text/detail/Utf8Scalar.hpp"
#include "System/Globalization/detail/UnicodeCategoryLookup.hpp"

namespace System::Text {

    /** Represents a Unicode scalar value (a valid code point excluding surrogates). */
    class Rune {
        uint32_t value_;

        static bool IsSurrogate(uint32_t v) { return v >= 0xD800 && v <= 0xDFFF; }
        static bool IsValidCodePoint(uint32_t v) { return v <= 0x10FFFF && !IsSurrogate(v); }

    public:
        /** The Unicode replacement character U+FFFD. */
        static const Rune ReplacementChar;

        /** Default-constructs a Rune representing U+0000 NUL (matches .NET's default(Rune)). */
        Rune() : value_(0) {}

        /** Constructs a Rune from a Unicode code point; throws if the value is a surrogate or > U+10FFFF. */
        explicit Rune(uint32_t value) : value_(value) {
            if (!IsValidCodePoint(value))
                throw System::ArgumentOutOfRangeException("value", "Invalid Unicode scalar value.");
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

        /**
         * @brief Decodes the Rune starting at UTF-8 byte offset @p index in @p value.
         *
         * C++ counterpart of .NET Rune.TryGetRuneAt(string, int, out Rune) — adapted for this
         * runtime's UTF-8 `std::string` representation instead of .NET's UTF-16 `string`
         * (so @p index is a byte offset, not a UTF-16 code-unit offset).
         * @param value The UTF-8 source string.
         * @param index Byte offset within @p value at which to start decoding.
         * @param result Receives the decoded Rune on success.
         * @param bytesConsumed Receives the number of UTF-8 bytes the Rune occupied.
         * @return true if a valid Rune was decoded at @p index; false if the byte sequence there is invalid.
         */
        static bool TryGetRuneAt(const std::string& value, size_t index, Rune& result, size_t& bytesConsumed) {
            if (index >= value.size()) {
                bytesConsumed = 0;
                return false;
            }
            // Ticket #2354 (2026-08-18): this was the third and last header-inline COPY of
            // the one UTF-8 scalar decode (System/detail/Utf8Scalar.hpp, factored out by #2014).
            // Rune needs the REPORTING form rather than the substituting one every Encoding
            // uses, and the two differ on exactly one input class: a structurally valid encoding
            // of a non-scalar -- a surrogate, or a value above U+10FFFF. Rune reports the
            // sequence's own length there, so a caller can skip the whole thing; an Encoding
            // reports 1, so it emits one U+FFFD per byte. That difference is why the shared
            // header has two entry points instead of one, and it is preserved exactly.
            uint32_t cp = 0;
            if (!System::detail::TryDecodeUtf8Scalar(value, index, cp, bytesConsumed))
                return false;
            result = Rune(cp);
            return true;
        }

        /** Returns true if the Rune is in the ASCII range. */
        static bool IsAscii(Rune r) { return r.value_ < 0x80; }
        /** Returns true if the Rune is a Unicode white-space character. */
        static bool IsWhiteSpace(Rune r) {
            static const uint32_t ws[] = {0x9,0xA,0xB,0xC,0xD,0x20,0x85,0xA0,0x1680,
                0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,0x2009,
                0x200A,0x2028,0x2029,0x202F,0x205F,0x3000};
            for (uint32_t w : ws) if (r.value_ == w) return true;
            return false;
        }
        // -------------------------------------------------------------------------------
        // #2018 (SR-AUD-294, cause T-L). These members were ASCII-only while IsWhiteSpace above
        // was Unicode-aware, so the type contradicted itself. They now answer from the
        // generated UCD 16.0 tables, as .NET does; the same close-out also removed U+FEFF from
        // IsWhiteSpace because .NET's Rune/Char white-space set excludes it.
        //
        // Each keeps .NET's OWN ASCII fast path (Rune.cs:1341-1426) rather than going straight
        // to the table. HONEST NOTE ON THE EVIDENCE: this is a proven EQUIVALENCE and a
        // mutation removing it is NOT caught -- measured over all 128 ASCII code points, the
        // fast path and the table agree on all six members, 0 disagreements. It is kept because
        // .NET's ASCII branch and its category branch are two statements and only the second is
        // a table lookup; collapsing them would be a simplification of the reference rather than
        // a port of it, and it is exactly the place a future divergence would be introduced
        // without anything noticing.
        // -------------------------------------------------------------------------------

        /** Returns true if the Rune is a Unicode letter. C++ counterpart of .NET Rune.IsLetter. */
        static bool IsLetter(Rune r) {
            if (IsAscii(r)) return (r.value_ >= 'A' && r.value_ <= 'Z') || (r.value_ >= 'a' && r.value_ <= 'z');
            const auto c = System::Globalization::detail::LookupUnicodeCategory(r.value_);
            return c == System::Globalization::UnicodeCategory::UppercaseLetter
                || c == System::Globalization::UnicodeCategory::LowercaseLetter
                || c == System::Globalization::UnicodeCategory::TitlecaseLetter
                || c == System::Globalization::UnicodeCategory::ModifierLetter
                || c == System::Globalization::UnicodeCategory::OtherLetter;
        }
        /** Returns true if the Rune is a Unicode decimal digit. C++ counterpart of .NET Rune.IsDigit. */
        static bool IsDigit(Rune r) {
            if (IsAscii(r)) return r.value_ >= '0' && r.value_ <= '9';
            return System::Globalization::detail::LookupUnicodeCategory(r.value_)
                 == System::Globalization::UnicodeCategory::DecimalDigitNumber;
        }
        /** Returns true if the Rune is a Unicode letter or decimal digit. */
        static bool IsLetterOrDigit(Rune r) { return IsLetter(r) || IsDigit(r); }
        /** Returns true if the Rune is a Unicode uppercase letter. */
        static bool IsUpper(Rune r) {
            if (IsAscii(r)) return r.value_ >= 'A' && r.value_ <= 'Z';
            return System::Globalization::detail::LookupUnicodeCategory(r.value_)
                 == System::Globalization::UnicodeCategory::UppercaseLetter;
        }
        /** Returns true if the Rune is a Unicode lowercase letter. */
        static bool IsLower(Rune r) {
            if (IsAscii(r)) return r.value_ >= 'a' && r.value_ <= 'z';
            return System::Globalization::detail::LookupUnicodeCategory(r.value_)
                 == System::Globalization::UnicodeCategory::LowercaseLetter;
        }

        /**
         * Converts the Rune to its invariant uppercase form.
         *
         * C++ counterpart of .NET Rune.ToUpperInvariant. This is the **simple** mapping only,
         * one code point to one code point: U+00DF SHARP S stays itself rather than becoming
         * "SS", because a full mapping can produce more than one Rune and the return type is
         * one Rune -- which is .NET's constraint too, not a shortcut taken here.
         */
        static Rune ToUpper(Rune r) {
            if (IsAscii(r)) return IsLower(r) ? Rune(r.value_ - 32) : r;
            return Rune(System::Globalization::detail::LookupToUpperInvariant(r.value_));
        }
        /** Converts the Rune to its invariant lowercase form. See ToUpper for the simple-mapping note. */
        static Rune ToLower(Rune r) {
            if (IsAscii(r)) return IsUpper(r) ? Rune(r.value_ + 32) : r;
            return Rune(System::Globalization::detail::LookupToLowerInvariant(r.value_));
        }

        /** Returns the UTF-8 encoded string representation of this Rune. */
        [[nodiscard]] std::string ToString() const {
            std::string s;
            System::detail::AppendUtf8Scalar(s, value_);
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
