// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Text/Unicode/UnicodeRange.hpp"

namespace System::Text::Unicode {

    /// Provides static factory methods for well-known Unicode BMP block ranges.
    class UnicodeRanges {
    public:
        UnicodeRanges() = delete;

        /// Returns a range covering the entire BMP (U+0000–U+FFFF).
        static UnicodeRange All()                      { return UnicodeRange(0x0000, 0x10000); }
        /// Returns an empty range.
        static UnicodeRange None()                     { return UnicodeRange(0x0000, 0); }
        /// Returns the Basic Latin block (U+0000–U+007F).
        static UnicodeRange BasicLatin()               { return UnicodeRange(0x0000, 128); }
        /// Returns the Latin-1 Supplement block (U+0080–U+00FF).
        static UnicodeRange Latin1Supplement()         { return UnicodeRange(0x0080, 128); }
        /// Returns the Latin Extended-A block (U+0100–U+017F).
        static UnicodeRange LatinExtendedA()           { return UnicodeRange(0x0100, 128); }
        /// Returns the Latin Extended-B block (U+0180–U+024F).
        static UnicodeRange LatinExtendedB()           { return UnicodeRange(0x0180, 208); }
        /// Returns the IPA Extensions block (U+0250–U+02AF).
        static UnicodeRange IPAExtensions()            { return UnicodeRange(0x0250,  96); }
        /// Returns the Spacing Modifier Letters block (U+02B0–U+02FF).
        static UnicodeRange SpacingModifierLetters()   { return UnicodeRange(0x02B0,  80); }
        /// Returns the Combining Diacritical Marks block (U+0300–U+036F).
        static UnicodeRange CombiningDiacriticalMarks(){ return UnicodeRange(0x0300, 112); }
        /// Returns the Greek and Coptic block (U+0370–U+03FF).
        static UnicodeRange GreekandCoptic()           { return UnicodeRange(0x0370, 144); }
        /// Returns the Cyrillic block (U+0400–U+04FF).
        static UnicodeRange Cyrillic()                 { return UnicodeRange(0x0400, 256); }
        /// Returns the Cyrillic Supplement block (U+0500–U+052F).
        static UnicodeRange CyrillicSupplement()       { return UnicodeRange(0x0500,  48); }
        /// Returns the Armenian block (U+0530–U+058F).
        static UnicodeRange Armenian()                 { return UnicodeRange(0x0530,  96); }
        /// Returns the Hebrew block (U+0590–U+05FF).
        static UnicodeRange Hebrew()                   { return UnicodeRange(0x0590, 112); }
        /// Returns the Arabic block (U+0600–U+06FF).
        static UnicodeRange Arabic()                   { return UnicodeRange(0x0600, 256); }
        /// Returns the Syriac block (U+0700–U+074F).
        static UnicodeRange Syriac()                   { return UnicodeRange(0x0700,  80); }
        /// Returns the Thaana block (U+0780–U+07BF).
        static UnicodeRange Thaana()                   { return UnicodeRange(0x0780,  64); }
        /// Returns the Devanagari block (U+0900–U+097F).
        static UnicodeRange Devanagari()               { return UnicodeRange(0x0900, 128); }
        /// Returns the Bengali block (U+0980–U+09FF).
        static UnicodeRange Bengali()                  { return UnicodeRange(0x0980, 128); }
        /// Returns the Gurmukhi block (U+0A00–U+0A7F).
        static UnicodeRange Gurmukhi()                 { return UnicodeRange(0x0A00, 128); }
        /// Returns the Gujarati block (U+0A80–U+0AFF).
        static UnicodeRange Gujarati()                 { return UnicodeRange(0x0A80, 128); }
        /// Returns the Oriya block (U+0B00–U+0B7F).
        static UnicodeRange Oriya()                    { return UnicodeRange(0x0B00, 128); }
        /// Returns the Tamil block (U+0B80–U+0BFF).
        static UnicodeRange Tamil()                    { return UnicodeRange(0x0B80, 128); }
        /// Returns the Telugu block (U+0C00–U+0C7F).
        static UnicodeRange Telugu()                   { return UnicodeRange(0x0C00, 128); }
        /// Returns the Kannada block (U+0C80–U+0CFF).
        static UnicodeRange Kannada()                  { return UnicodeRange(0x0C80, 128); }
        /// Returns the Malayalam block (U+0D00–U+0D7F).
        static UnicodeRange Malayalam()                { return UnicodeRange(0x0D00, 128); }
        /// Returns the Sinhala block (U+0D80–U+0DFF).
        static UnicodeRange Sinhala()                  { return UnicodeRange(0x0D80, 128); }
        /// Returns the Thai block (U+0E00–U+0E7F).
        static UnicodeRange Thai()                     { return UnicodeRange(0x0E00, 128); }
        /// Returns the Lao block (U+0E80–U+0EFF).
        static UnicodeRange Lao()                      { return UnicodeRange(0x0E80, 128); }
        /// Returns the Tibetan block (U+0F00–U+0FFF).
        static UnicodeRange Tibetan()                  { return UnicodeRange(0x0F00, 256); }
        /// Returns the CJK Unified Ideographs block (U+4E00–U+9FFF).
        static UnicodeRange CJKUnifiedIdeographs()     { return UnicodeRange(0x4E00, 0x9FFF - 0x4E00 + 1); }
        /// Returns the Hangul Syllables block (U+AC00–U+D7A3).
        static UnicodeRange HangulSyllables()          { return UnicodeRange(0xAC00, 0xD7A3 - 0xAC00 + 1); }
        /// Returns the Specials block (U+FFF0–U+FFFF).
        static UnicodeRange Specials()                 { return UnicodeRange(0xFFF0,  16); }
    };

} // namespace System::Text::Unicode
