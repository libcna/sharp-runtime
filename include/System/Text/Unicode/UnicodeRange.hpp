// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>

namespace System::Text::Unicode {

    class UnicodeRange {
        int firstCodePoint_;
        int length_;

    public:
        UnicodeRange(int firstCodePoint, int length)
            : firstCodePoint_(firstCodePoint), length_(length) {
            if (firstCodePoint < 0 || firstCodePoint > 0xFFFF)
                throw std::out_of_range("firstCodePoint must be in BMP (0x0000–0xFFFF).");
            if (length < 0 || static_cast<long long>(firstCodePoint) + length > 0x10000)
                throw std::out_of_range("length out of range.");
        }

        [[nodiscard]] int getFirstCodePointProperty() const { return firstCodePoint_; }
        [[nodiscard]] int getLengthProperty()         const { return length_; }

        static UnicodeRange Create(char16_t firstCharacter, char16_t lastCharacter) {
            if (firstCharacter > lastCharacter)
                throw std::invalid_argument("firstCharacter must be <= lastCharacter.");
            return UnicodeRange(firstCharacter, lastCharacter - firstCharacter + 1);
        }
    };

} // namespace System::Text::Unicode
