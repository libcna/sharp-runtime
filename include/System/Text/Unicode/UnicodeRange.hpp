// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>

namespace System::Text::Unicode {

    /** Represents a contiguous range of Unicode code points in the BMP. */
    class UnicodeRange {
        int firstCodePoint_;
        int length_;

    public:
        /** Constructs a UnicodeRange starting at firstCodePoint with the given length; throws if out of BMP bounds. */
        UnicodeRange(int firstCodePoint, int length)
            : firstCodePoint_(firstCodePoint), length_(length) {
            if (firstCodePoint < 0 || firstCodePoint > 0xFFFF)
                throw std::out_of_range("firstCodePoint must be in BMP (0x0000–0xFFFF).");
            if (length < 0 || static_cast<long long>(firstCodePoint) + length > 0x10000)
                throw std::out_of_range("length out of range.");
        }

        /** Gets the first code point in the range. */
        [[nodiscard]] int getFirstCodePointProperty() const { return firstCodePoint_; }
        /** Gets the number of code points in the range. */
        [[nodiscard]] int getLengthProperty()         const { return length_; }

        /** Creates a UnicodeRange spanning from firstCharacter to lastCharacter (inclusive). */
        static UnicodeRange Create(char16_t firstCharacter, char16_t lastCharacter) {
            if (firstCharacter > lastCharacter)
                throw std::invalid_argument("firstCharacter must be <= lastCharacter.");
            return UnicodeRange(firstCharacter, lastCharacter - firstCharacter + 1);
        }
    };

} // namespace System::Text::Unicode
