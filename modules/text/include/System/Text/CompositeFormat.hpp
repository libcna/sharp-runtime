// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/FormatException.hpp"
#include "System/detail/CompositeFormat.hpp"

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
        // Ticket #2020 (the grammar half of the same finding, cause T-N, CCF-012) removed the
        // hand-written scanner that used to live here. It was a THIRD composite-format grammar --
        // exactly what CCF-012 warns about -- and it skipped everything between the index and the
        // closing brace, so `{0,not-a-width}` and `{0,-}` parsed happily. The scan is now
        // System::detail::scanCompositeFormat, shared with String::Format and
        // FormattableString::ToString, under the digit policy .NET's own CompositeFormat.Parse
        // uses.
        //
        // THE POLICY MATTERS AND THE PLAN HAD IT BACKWARDS. #2020's description asserts that
        // `Parse("{1500000}")` should be rejected "where .NET's AppendFormatHelper index limit is
        // 1,000,000". .NET's CompositeFormat.Parse has NO index limit: TryParseLiterals writes
        // `while (char.IsAsciiDigit(ch))` (CompositeFormat.cs:201,258) where AppendFormatHelper
        // writes `while (char.IsAsciiDigit(ch) && index < IndexLimit)`
        // (ValueStringBuilder.AppendFormat.cs:99,140), and nothing else between the two differs.
        // Adopting the formatter's limits here would have introduced a new divergence while
        // claiming to remove one, so `{1000000}`..`{2147483646}` keep the answers #2010 gave them.

    public:
        /**
         * @brief Parses a composite format string and returns a CompositeFormat instance.
         *
         * Transcribed from .NET's `CompositeFormat.TryParseLiterals`
         * (`CompositeFormat.cs:112-352`) by way of the shared
         * `System::detail::scanCompositeFormat`: an item is an opening brace, at least one
         * ASCII digit, optional spaces, an optional `,`-introduced alignment (itself optional
         * spaces, an optional `-`, at least one digit, optional spaces), an optional
         * `:`-introduced specifier that may not contain `{`, and a closing brace. `{{` and `}}`
         * are escapes; any other unescaped `}` is an error.
         *
         * `MinimumArgumentCount` is one more than the largest index any item names, which is
         * .NET's `_argsRequired` (`CompositeFormat.cs:55`).
         *
         * @throws System::FormatException if @p format contains an invalid format item, or an
         *         argument index too large for `intcs` — see
         *         `System::detail::kCompositeMaxRepresentableIndex`, which records why that last
         *         case deviates from .NET 11 on purpose.
         */
        static CompositeFormat Parse(const std::string& format) {
            CompositeFormat cf;
            cf.format_ = format;

            long long maxIdx = -1;
            System::detail::scanCompositeFormat(
                format, System::detail::CompositeDigitPolicy::UnboundedLikeParse,
                [](std::string_view) {},  // Parse keeps the original text, so literals are dropped.
                [&](long long index, long long, bool, std::string_view) {
                    if (index > maxIdx) maxIdx = index;
                });

            cf.minArgCount_ = static_cast<SharpRuntime::intcs>(maxIdx + 1);
            return cf;
        }

        /** Gets the original format string. */
        [[nodiscard]] const std::string& getFormatProperty() const { return format_; }
        /** Gets the minimum number of arguments required to satisfy the format string. */
        [[nodiscard]] SharpRuntime::intcs getMinimumArgumentCountProperty() const { return minArgCount_; }
    };

} // namespace System::Text
