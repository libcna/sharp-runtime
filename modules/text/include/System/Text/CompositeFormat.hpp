// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/FormatException.hpp"

namespace System::Text {

    /** Represents a parsed composite format string (e.g. "Hello, {0}!") that avoids re-parsing on each use. */
    class CompositeFormat {
        std::string format_;
        SharpRuntime::intcs minArgCount_ = 0;

        // Real .NET's CompositeFormat.Parse validates the format string and throws FormatException
        // for an invalid format item -- unterminated "{"/stray "}", or an empty/non-numeric index.
        // An earlier version of this parser silently accepted any malformed input (e.g. "Hello {"
        // or a stray "}") and just returned MinimumArgumentCount 0, masking bugs in caller-supplied
        // format strings instead of surfacing them like real .NET does.
        //
        // Ticket #2010 (SR-AUD-298, cause T-D of docs/SystemTextNamespaceReviewPlan.md) closed
        // two defects in one expression, both reachable from ordinary public input:
        //
        //   * `std::stoi(idxStr)` threw std::out_of_range for "{2147483648}" -- a std:: exception
        //     escaping a System-shaped public API whose own doc-comment promises FormatException,
        //     the same class #1882 removed from String::Format; and
        //   * one value earlier, "{2147483647}" SUCCEEDED and returned
        //     getMinimumArgumentCountProperty() == -2147483648, because `maxIdx + 1` on a signed
        //     intcs already holding INT32_MAX is undefined behaviour. A negative minimum argument
        //     count is a silent wrong result, which the finding does not name and which is worse
        //     than the leak it does.
        //
        // The digits are therefore accumulated in a wider type with an explicit bound. The bound
        // is INT32_MAX - 1, i.e. the largest index whose `+ 1` is representable -- deliberately
        // NOT .NET's kCompositeIndexLimit of 1,000,000, which would additionally reject
        // "{1000000}".."{2147483646}", inputs this parser accepts today with a defined positive
        // answer. That narrowing belongs to the approval-gated ticket #2020, which adopts
        // System::detail::runCompositeFormat's grammar wholesale; #2010 stops exactly one value
        // short of it.
        static constexpr long long kMaxRepresentableIndex =
            static_cast<long long>(SharpRuntime::INTCS_MAX) - 1;

        static SharpRuntime::intcs countPlaceholders(const std::string& fmt) {
            long long maxIdx = -1;
            for (std::size_t i = 0; i < fmt.size(); ) {
                char c = fmt[i];
                if (c == '{') {
                    if (i + 1 < fmt.size() && fmt[i + 1] == '{') { i += 2; continue; }
                    std::size_t j = i + 1;
                    while (j < fmt.size() && fmt[j] != '}' && fmt[j] != ',' && fmt[j] != ':') ++j;
                    if (j >= fmt.size())
                        throw System::FormatException("Input string was not in a correct format.");
                    std::string idxStr = fmt.substr(i + 1, j - i - 1);
                    if (idxStr.empty() ||
                        !std::all_of(idxStr.begin(), idxStr.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; }))
                        throw System::FormatException("Input string was not in a correct format.");
                    // Bounded accumulation: no std:: exception can escape, and the loop stops as
                    // soon as the value can no longer be represented, so a long digit run costs
                    // no more than a short one.
                    long long idx = 0;
                    for (unsigned char ch : idxStr) {
                        idx = idx * 10 + (ch - '0');
                        if (idx > kMaxRepresentableIndex)
                            throw System::FormatException("Input string was not in a correct format.");
                    }
                    if (idx > maxIdx) maxIdx = idx;
                    while (j < fmt.size() && fmt[j] != '}') ++j;
                    if (j >= fmt.size())
                        throw System::FormatException("Input string was not in a correct format.");
                    i = j + 1;
                } else if (c == '}') {
                    if (i + 1 < fmt.size() && fmt[i + 1] == '}') { i += 2; continue; }
                    throw System::FormatException("Input string was not in a correct format.");
                } else {
                    ++i;
                }
            }
            return static_cast<SharpRuntime::intcs>(maxIdx + 1);
        }

    public:
        /**
         * @brief Parses a composite format string and returns a CompositeFormat instance.
         * @throws System::FormatException if @p format contains an invalid format item
         *         (unterminated placeholder, stray '}', or a non-numeric/empty argument index).
         */
        static CompositeFormat Parse(const std::string& format) {
            CompositeFormat cf;
            cf.format_ = format;
            cf.minArgCount_ = countPlaceholders(format);
            return cf;
        }

        /** Gets the original format string. */
        [[nodiscard]] const std::string& getFormatProperty() const { return format_; }
        /** Gets the minimum number of arguments required to satisfy the format string. */
        [[nodiscard]] SharpRuntime::intcs getMinimumArgumentCountProperty() const { return minArgCount_; }
    };

} // namespace System::Text
