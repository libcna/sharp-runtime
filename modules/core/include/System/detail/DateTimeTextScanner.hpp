// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <string>
#include <string_view>

/**
 * @file
 * @brief The full-consumption cursor the four date/time parsers scan with
 *        (ticket #1879, SR-AUD-007b / SR-AUD-009 / SR-AUD-061, CCF-002 class D).
 *
 * `DateTime`, `DateTimeOffset`, `TimeOnly` and `DateOnly` used to verify a few
 * separator *positions* and then run a single `std::sscanf` **prefix**
 * conversion, never asserting that the conversion consumed the whole string.
 * That accepted `"2024-06-15junk"` as a date and turned `"2024-06-15 10:xx:00"`
 * into **midnight** — wrong answers a caller cannot detect. .NET's
 * `DateTimeParse.cs` is a state-machine lexer that fails on any unconsumed
 * token; this cursor is the small piece of that contract the port needs.
 *
 * It also removes `std::sscanf`'s `%d` leniencies, which are not part of any
 * documented grammar: `%d` skips leading whitespace and accepts an explicit
 * sign, so `" 024-06-15"`, `"+024-06-15"` and `"2024-06-15 +1:20:30"` all
 * parsed. And it removes `%d`'s formally undefined behaviour on a numeral
 * outside `int` by never reading more digits than fit.
 *
 * Deliberately **not** a general lexer: the port implements a narrow ISO-8601
 * subset by prior decision (`DateTime.hpp`, `TimeOnly.hpp`), and widening it is
 * new API surface, not a validation repair
 * (`docs/DateTimeValidationBoundaryPlan.md` §16.4).
 */
namespace System::detail {

/** @brief A forward-only cursor over a candidate date/time string. */
class DateTimeTextScanner {
public:
    /** @brief Begins scanning @p text, which must outlive the scanner. */
    explicit DateTimeTextScanner(std::string_view text) noexcept : text_(text) {}

    /** @brief True when every character has been consumed. */
    [[nodiscard]] bool atEnd() const noexcept { return index_ >= text_.size(); }

    /** @brief Consumes @p c if it is the next character; otherwise consumes nothing. */
    bool take(char c) noexcept {
        if (index_ < text_.size() && text_[index_] == c) {
            ++index_;
            return true;
        }
        return false;
    }

    /**
     * @brief Consumes between @p minDigits and @p maxDigits ASCII digits.
     *
     * Digits only — no sign, no whitespace, no locale involvement, which is the
     * whole point of not using `std::sscanf`. Consumes nothing and returns
     * @c false unless at least @p minDigits digits are available; stops after
     * @p maxDigits so the accumulated value cannot overflow @c int (callers pass
     * at most 4).
     *
     * @param minDigits  Fewest digits that make the field well-formed.
     * @param maxDigits  Most digits the field may hold (<= 9).
     * @param value      Receives the parsed value on success; untouched on failure.
     * @param digitCount Optionally receives how many digits were consumed, which
     *                   a fractional-second field needs in order to scale.
     */
    bool takeDigits(int minDigits, int maxDigits, int& value,
                    int* digitCount = nullptr) noexcept {
        const std::size_t start = index_;
        int accumulated = 0, count = 0;
        while (index_ < text_.size() && count < maxDigits &&
               text_[index_] >= '0' && text_[index_] <= '9') {
            accumulated = accumulated * 10 + (text_[index_] - '0');
            ++index_;
            ++count;
        }
        if (count < minDigits) {
            index_ = start;  // a partial field consumes nothing
            return false;
        }
        value = accumulated;
        if (digitCount != nullptr) *digitCount = count;
        return true;
    }

private:
    std::string_view text_;
    std::size_t      index_ = 0;
};

} // namespace System::detail
