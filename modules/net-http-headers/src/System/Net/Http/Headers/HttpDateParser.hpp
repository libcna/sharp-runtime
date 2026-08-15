// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

// IMPLEMENTATION-ONLY header. It sits beside the sources under src/, not under include/, so it
// never becomes part of this component's public surface and is included with a plain relative
// path -- the same placement and spelling `modules/xml/src` already uses for
// `XPathAstInternal.hpp` and `XmlNodeChangeEvents.hpp`.
//
// Ticket #2125 (SR-AUD-321, cause NH-H, docs/SystemNetHttpHeadersNamespaceReviewPlan.md §4.3).
//
// The module carried **seven** byte-identical copies of an `sscanf`-based HTTP-date parser --
// §4.3 counted six and did not name `WarningHeaderValue`'s date field -- and none of them
// checked that the whole value had been consumed. So
//
//     RetryConditionHeaderValue::Parse("Sun, 06 Nov 1994 08:49:37 GMT trailing")
//
// succeeded and silently discarded ` trailing`, at every one of the seven doors. The control
// that proves this is a *consumption* defect rather than a permissive grammar is that
// `"garbage"` was, and still is, rejected.
//
// **What this deliberately does NOT change: the accepted grammar.** The `sscanf` conversion
// string is kept verbatim and `%n` is appended, so a value that parsed before parses to exactly
// the same instant now, and a value that failed before still fails. Rewriting this as a
// hand-written fixed-width scanner would additionally reject things `sscanf` accepts (a signed
// or over-wide day field, for instance), and with `/rv/tmp/runtime/` absent there is no evidence
// in this repository for what .NET does with those. #2125's job is full consumption; narrowing
// the grammar is not authorised and is not done here.
//
// **The obsolete formats, measured rather than assumed** (this is the pin #2125's acceptance
// criteria require and the measurement #2130 asked for on the port side):
//
//   | Format | Example | Before #2125 | After #2125 |
//   |---|---|---|---|
//   | IMF-fixdate (preferred) | `Sun, 06 Nov 1994 08:49:37 GMT` | parsed | parsed |
//   | RFC 850 (obsolete)      | `Sunday, 06-Nov-94 08:49:37 GMT` | **rejected** | **rejected** |
//   | ANSI C asctime (obsolete)| `Sun Nov  6 08:49:37 1994`      | **rejected** | **rejected** |
//
// Neither obsolete form was ever accepted -- the conversion string requires a comma immediately
// after a three-letter day name, which both obsolete forms fail -- so #2125 cannot have narrowed
// one away. RFC 9110 §5.6.7 requires a *recipient* to accept all three; whether .NET's parser
// does is the still-open question **#2130**, and it is a widening, not something this ticket
// may guess at.

#include "SharpRuntime/PortableScan.hpp"
#include "System/DateTimeOffset.hpp"
#include "System/TimeSpan.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace System::Net::Http::Headers::detail {

    /**
     * @brief Parses one HTTP-date in the preferred IMF-fixdate form, consuming the whole value.
     *
     * @param s The candidate field value.
     * @param result Receives the parsed instant (UTC) on success.
     * @return true only if @p s is a complete HTTP-date. Trailing text after the `GMT` token
     * makes the value invalid; trailing *whitespace* does not, because whitespace was accepted
     * before #2125 and is not what the finding is about.
     */
    inline bool TryParseHttpDate(const std::string& s, System::DateTimeOffset& result) {
        static constexpr std::array<const char*, 12> months = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

        char dayName[4] = {};
        char monthName[4] = {};
        int day = 0, year = 0, hour = 0, minute = 0, second = 0;
        char gmt[4] = {};
        int consumed = -1;

        // An embedded NUL would let sscanf stop early and report a complete match over a prefix,
        // so it is rejected before c_str() hides the rest of the value.
        if (s.find('\0') != std::string::npos) return false;

        // The three character-array arguments carry their buffer size on MSVC, which its secure
        // scanf requires for %[ and %s; %n and the numeric conversions take no extra argument
        // there, so the conversion string and its result are the same on every compiler.
        const int matched = SHARP_RUNTIME_SSCANF(
            s.c_str(), "%3[A-Za-z], %d %3[A-Za-z] %d %d:%d:%d %3s%n",
            SHARP_RUNTIME_SCANF_BUFFER(dayName), &day, SHARP_RUNTIME_SCANF_BUFFER(monthName),
            &year, &hour, &minute, &second, SHARP_RUNTIME_SCANF_BUFFER(gmt), &consumed);
        if (matched != 8) return false;
        if (consumed < 0) return false;
        if (std::strcmp(gmt, "GMT") != 0) return false;

        // #2125: the whole value must be consumed. Anything but whitespace after the GMT token
        // means this was not one HTTP-date, and silently discarding it is how a cache key, a
        // conditional request or a log line ends up disagreeing with the sender.
        for (size_t i = static_cast<size_t>(consumed); i < s.size(); ++i) {
            if (std::isspace(static_cast<unsigned char>(s[i])) == 0) return false;
        }

        int monthIndex = -1;
        for (int i = 0; i < 12; ++i) {
            if (std::strcmp(monthName, months[static_cast<size_t>(i)]) == 0) { monthIndex = i; break; }
        }
        if (monthIndex < 0) return false;

        try {
            result = System::DateTimeOffset(year, monthIndex + 1, day, hour, minute, second,
                                            System::TimeSpan::Zero);
            return true;
        } catch (...) {
            return false;
        }
    }

} // namespace System::Net::Http::Headers::detail
