// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Text/Unicode/UnicodeRange.hpp"

namespace System::Text::Unicode {

    // Well-known Unicode BMP block ranges
    class UnicodeRanges {
    public:
        UnicodeRanges() = delete;

        static UnicodeRange All()                      { return UnicodeRange(0x0000, 0x10000); }
        static UnicodeRange None()                     { return UnicodeRange(0x0000, 0); }
        static UnicodeRange BasicLatin()               { return UnicodeRange(0x0000, 128); }
        static UnicodeRange Latin1Supplement()         { return UnicodeRange(0x0080, 128); }
        static UnicodeRange LatinExtendedA()           { return UnicodeRange(0x0100, 128); }
        static UnicodeRange LatinExtendedB()           { return UnicodeRange(0x0180, 208); }
        static UnicodeRange IPAExtensions()            { return UnicodeRange(0x0250,  96); }
        static UnicodeRange SpacingModifierLetters()   { return UnicodeRange(0x02B0,  80); }
        static UnicodeRange CombiningDiacriticalMarks(){ return UnicodeRange(0x0300, 112); }
        static UnicodeRange GreekandCoptic()           { return UnicodeRange(0x0370, 144); }
        static UnicodeRange Cyrillic()                 { return UnicodeRange(0x0400, 256); }
        static UnicodeRange CyrillicSupplement()       { return UnicodeRange(0x0500,  48); }
        static UnicodeRange Armenian()                 { return UnicodeRange(0x0530,  96); }
        static UnicodeRange Hebrew()                   { return UnicodeRange(0x0590, 112); }
        static UnicodeRange Arabic()                   { return UnicodeRange(0x0600, 256); }
        static UnicodeRange Syriac()                   { return UnicodeRange(0x0700,  80); }
        static UnicodeRange Thaana()                   { return UnicodeRange(0x0780,  64); }
        static UnicodeRange Devanagari()               { return UnicodeRange(0x0900, 128); }
        static UnicodeRange Bengali()                  { return UnicodeRange(0x0980, 128); }
        static UnicodeRange Gurmukhi()                 { return UnicodeRange(0x0A00, 128); }
        static UnicodeRange Gujarati()                 { return UnicodeRange(0x0A80, 128); }
        static UnicodeRange Oriya()                    { return UnicodeRange(0x0B00, 128); }
        static UnicodeRange Tamil()                    { return UnicodeRange(0x0B80, 128); }
        static UnicodeRange Telugu()                   { return UnicodeRange(0x0C00, 128); }
        static UnicodeRange Kannada()                  { return UnicodeRange(0x0C80, 128); }
        static UnicodeRange Malayalam()                { return UnicodeRange(0x0D00, 128); }
        static UnicodeRange Sinhala()                  { return UnicodeRange(0x0D80, 128); }
        static UnicodeRange Thai()                     { return UnicodeRange(0x0E00, 128); }
        static UnicodeRange Lao()                      { return UnicodeRange(0x0E80, 128); }
        static UnicodeRange Tibetan()                  { return UnicodeRange(0x0F00, 256); }
        static UnicodeRange CJKUnifiedIdeographs()     { return UnicodeRange(0x4E00, 0x9FFF - 0x4E00 + 1); }
        static UnicodeRange HangulSyllables()          { return UnicodeRange(0xAC00, 0xD7A3 - 0xAC00 + 1); }
        static UnicodeRange Specials()                 { return UnicodeRange(0xFFF0,  16); }
    };

} // namespace System::Text::Unicode
