// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/XmlConvert.hpp"
#include "System/DateTimeKind.hpp"
#include "System/Globalization/DateTimeStyles.hpp"
#include "System/TimeZone.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <limits>

#include "SharpRuntime/PortableScan.hpp"
#include "System/Boolean.hpp"
#include "System/Byte.hpp"
#include "System/Char.hpp"
#include "System/FormatException.hpp"
#include "System/Double.hpp"
#include "System/Int16.hpp"
#include "System/Int32.hpp"
#include "System/Int64.hpp"
#include "System/SByte.hpp"
#include "System/Single.hpp"
#include "System/UInt16.hpp"
#include "System/UInt32.hpp"
#include "System/UInt64.hpp"
#include "System/ArgumentException.hpp"
#include "System/Xml/XmlException.hpp"

namespace System::Xml {

    namespace {

        // ===================================================================================
        // Ticket #2080 -- the XML Schema `duration` lexical form.
        //
        // `XmlConvert.ToString(TimeSpan)` is `new XsdDuration(value).ToString()` and
        // `XmlConvert.ToTimeSpan(string)` is `new XsdDuration(s).ToTimeSpan()`
        // (XmlConvert.cs:686-689 and :1109-1127). Neither touches .NET's native colon form.
        // This port used TimeSpan::ToString()/Parse() for both, so it emitted "1.00:00:00.0000000"
        // where XML says "P1D", and rejected "P1D" and "PT1H30M" outright.
        //
        // Both halves are transcribed from XsdDuration.cs.
        // ===================================================================================

        /** @brief The decomposed XSD duration components. Years and months are parse-only. */
        struct XsdDuration {
            bool          negative = false;
            long long     years    = 0;
            long long     months   = 0;
            long long     days     = 0;
            long long     hours    = 0;
            long long     minutes  = 0;
            long long     seconds  = 0;
            long long     nanoseconds = 0;   // 0..999,999,999
        };

        /**
         * @brief Reads a run of digits, as XsdDuration.TryParseDigits does.
         * @param eatDigits When true, digits past what fits are discarded rather than an error --
         *        which is what lets a fractional second carry more than nine digits.
         */
        bool TryParseDurationDigits(const std::string& s, std::size_t& pos, bool eatDigits,
                                    long long& value, int& numDigits) {
            const std::size_t start = pos;
            value = 0;
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
                const int digit = s[pos] - '0';
                if (value > (2147483647LL - digit) / 10) {
                    if (!eatDigits) return false;   // XsdDuration raises OverflowException here
                    // Discard: the value is already at full precision.
                    ++pos;
                    continue;
                }
                value = value * 10 + digit;
                ++pos;
            }
            numDigits = static_cast<int>(pos - start);
            return true;
        }

        /**
         * @brief Parses the XSD `duration` lexical form, transcribed from XsdDuration.TryParse.
         *
         * Grammar: `'-'? 'P' (nY)? (nM)? (nD)? ('T' (nH)? (nM)? (n('.'n*)? 'S')?)?`, with at
         * least one component present, no trailing characters, and no trailing digits.
         */
        bool TryParseXsdDuration(const std::string& s, XsdDuration& result) {
            result = XsdDuration{};
            std::size_t pos = 0;
            const std::size_t length = s.size();
            bool anyPart = false;
            long long value = 0;
            int numDigits = 0;

            if (pos < length && s[pos] == '-') { result.negative = true; ++pos; }
            if (pos >= length || s[pos] != 'P') return false;
            ++pos;

            if (!TryParseDurationDigits(s, pos, false, value, numDigits)) return false;
            if (pos >= length) return false;

            if (s[pos] == 'Y') {
                if (numDigits == 0) return false;
                anyPart = true; result.years = value;
                if (++pos == length) return anyPart;
                if (!TryParseDurationDigits(s, pos, false, value, numDigits)) return false;
                if (pos >= length) return false;
            }
            if (s[pos] == 'M') {
                if (numDigits == 0) return false;
                anyPart = true; result.months = value;
                if (++pos == length) return anyPart;
                if (!TryParseDurationDigits(s, pos, false, value, numDigits)) return false;
                if (pos >= length) return false;
            }
            if (s[pos] == 'D') {
                if (numDigits == 0) return false;
                anyPart = true; result.days = value;
                if (++pos == length) return anyPart;
                if (!TryParseDurationDigits(s, pos, false, value, numDigits)) return false;
                if (pos >= length) return false;
            }

            if (s[pos] == 'T') {
                // "P1T..." is invalid: the digits before T belong to no component.
                if (numDigits != 0) return false;
                ++pos;
                if (!TryParseDurationDigits(s, pos, false, value, numDigits)) return false;
                if (pos >= length) return false;

                if (s[pos] == 'H') {
                    if (numDigits == 0) return false;
                    anyPart = true; result.hours = value;
                    if (++pos == length) return anyPart;
                    if (!TryParseDurationDigits(s, pos, false, value, numDigits)) return false;
                    if (pos >= length) return false;
                }
                if (s[pos] == 'M') {
                    if (numDigits == 0) return false;
                    anyPart = true; result.minutes = value;
                    if (++pos == length) return anyPart;
                    if (!TryParseDurationDigits(s, pos, false, value, numDigits)) return false;
                    if (pos >= length) return false;
                }
                if (s[pos] == '.') {
                    ++pos;
                    anyPart = true; result.seconds = value;
                    if (!TryParseDurationDigits(s, pos, true, value, numDigits)) return false;
                    if (numDigits == 0) value = 0;   // "1.S" means no fraction
                    for (; numDigits > 9; --numDigits) value /= 10;
                    for (; numDigits < 9; ++numDigits) value *= 10;
                    result.nanoseconds = value;
                    if (pos >= length || s[pos] != 'S') return false;
                    if (++pos == length) return anyPart;
                    numDigits = 0;
                } else if (s[pos] == 'S') {
                    if (numDigits == 0) return false;
                    anyPart = true; result.seconds = value;
                    if (++pos == length) return anyPart;
                    numDigits = 0;
                }
            }

            // .NET has `if (numDigits != 0) goto InvalidFormat;` here as well. It is omitted
            // deliberately: it is UNREACHABLE, in this port and in the reference alike. Every
            // path that falls through to this point has already run `if (pos >= length) return
            // false` after its digit parse, so `pos < length` holds and the next line rejects
            // regardless of the digit count. A mutation removing it changed nothing, which is
            // how that was established rather than assumed.
            if (pos != length) return false;    // no trailing characters
            return anyPart;                     // at least one component must be present
        }

        /**
         * @brief Converts parsed components to ticks, transcribed from XsdDuration.TryToTimeSpan.
         *
         * @note Years and months have no fixed length, so .NET **estimates**: 365 days to the
         *       year and 30 to the month (`XsdDuration.cs:243-244` says so in as many words).
         *       That estimate is reproduced rather than improved on -- a "better" one would
         *       disagree with every .NET-produced value.
         */
        bool TryXsdDurationToTicks(const XsdDuration& d, long long& ticks) {
            unsigned long long t = 0;
            t += (static_cast<unsigned long long>(d.years) +
                  static_cast<unsigned long long>(d.months) / 12) * 365;
            t += (static_cast<unsigned long long>(d.months) % 12) * 30;
            t += static_cast<unsigned long long>(d.days);
            t *= 24;  t += static_cast<unsigned long long>(d.hours);
            t *= 60;  t += static_cast<unsigned long long>(d.minutes);
            t *= 60;  t += static_cast<unsigned long long>(d.seconds);
            t *= static_cast<unsigned long long>(System::TimeSpan::TicksPerSecond);
            t += static_cast<unsigned long long>(d.nanoseconds) / 100;

            constexpr unsigned long long kMaxTicks =
                static_cast<unsigned long long>(SharpRuntime::LONGCS_MAX);
            if (d.negative) {
                if (t == kMaxTicks + 1) { ticks = SharpRuntime::LONGCS_MIN; return true; }
                if (t > kMaxTicks) return false;
                ticks = -static_cast<long long>(t);
            } else {
                if (t > kMaxTicks) return false;
                ticks = static_cast<long long>(t);
            }
            return true;
        }

        /**
         * @brief Formats a TimeSpan in the XSD `duration` lexical form, transcribed from
         *        XsdDuration's TimeSpan constructor and TryFormat.
         *
         * A TimeSpan carries no years or months, so the output is
         * `[-]P[nD][T[nH][nM][n[.fffffffff]S]]`, and zero is `PT0S`. The fractional second is
         * nine digits with trailing zeros stripped -- and since a tick is 100 ns, the last two
         * are always zero, so at most seven significant digits appear.
         */
        std::string FormatXsdDuration(const System::TimeSpan& value) {
            const long long ticks = value.getTicksProperty();
            const bool negative = ticks < 0;
            // Negating LONGCS_MIN is undefined; the unsigned magnitude is exact for every input.
            const unsigned long long magnitude =
                negative ? (~static_cast<unsigned long long>(ticks) + 1ULL)
                         : static_cast<unsigned long long>(ticks);

            const unsigned long long perSecond =
                static_cast<unsigned long long>(System::TimeSpan::TicksPerSecond);
            const long long nanoseconds = static_cast<long long>((magnitude % perSecond) * 100);
            const long long days    = static_cast<long long>(
                magnitude / static_cast<unsigned long long>(System::TimeSpan::TicksPerDay));
            const long long hours   = static_cast<long long>(
                (magnitude / static_cast<unsigned long long>(System::TimeSpan::TicksPerHour)) % 24);
            const long long minutes = static_cast<long long>(
                (magnitude / static_cast<unsigned long long>(System::TimeSpan::TicksPerMinute)) % 60);
            const long long seconds = static_cast<long long>((magnitude / perSecond) % 60);

            std::string out;
            if (negative) out += '-';
            out += 'P';
            if (days != 0) out += std::to_string(days) + "D";
            if (hours != 0 || minutes != 0 || seconds != 0 || nanoseconds != 0) {
                out += 'T';
                if (hours != 0) out += std::to_string(hours) + "H";
                if (minutes != 0) out += std::to_string(minutes) + "M";
                if (seconds != 0 || nanoseconds != 0) {
                    out += std::to_string(seconds);
                    if (nanoseconds != 0) {
                        std::string fraction(9, '0');
                        long long remaining = nanoseconds;
                        for (int i = 8; i >= 0; --i) {
                            fraction[static_cast<std::size_t>(i)] =
                                static_cast<char>('0' + remaining % 10);
                            remaining /= 10;
                        }
                        while (!fraction.empty() && fraction.back() == '0') fraction.pop_back();
                        out += '.' + fraction;
                    }
                    out += 'S';
                }
            }
            if (out.back() == 'P') out += "T0S";   // zero is "PT0S"
            return out;
        }

        // Matches XmlConvert.cs's private WhitespaceChars array exactly.
        constexpr const char* WhitespaceChars = " \t\n\r";

        std::string TrimXmlWhitespace(const std::string& s) {
            auto begin = s.find_first_not_of(WhitespaceChars);
            if (begin == std::string::npos) return "";
            auto end = s.find_last_not_of(WhitespaceChars);
            return s.substr(begin, end - begin + 1);
        }
    } // namespace

    // --- Character classification (practical ASCII-range subset; see class doc-comment) ------

    bool XmlConvert::IsStartNCNameChar(SharpRuntime::charcs ch) {
        auto c = static_cast<unsigned int>(ch);
        if (c >= 128) return true; // permissive: treat all non-ASCII as valid name chars
        return std::isalpha(static_cast<int>(c)) || ch == '_';
    }

    bool XmlConvert::IsNCNameChar(SharpRuntime::charcs ch) {
        auto c = static_cast<unsigned int>(ch);
        if (c >= 128) return true;
        return std::isalnum(static_cast<int>(c)) || ch == '_' || ch == '-' || ch == '.';
    }

    bool XmlConvert::IsXmlChar(SharpRuntime::charcs ch) {
        auto c = static_cast<unsigned int>(ch);
        // XML 1.0 Char production: #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | [#x10000-#x10FFFF]
        return c == 0x9 || c == 0xA || c == 0xD ||
               (c >= 0x20 && c <= 0xD7FF) ||
               (c >= 0xE000 && c <= 0xFFFD);
    }

    bool XmlConvert::IsXmlSurrogatePair(SharpRuntime::charcs lowChar, SharpRuntime::charcs highChar) {
        auto low = static_cast<unsigned int>(lowChar);
        auto high = static_cast<unsigned int>(highChar);
        if (high < 0xD800 || high > 0xDBFF || low < 0xDC00 || low > 0xDFFF) return false;
        unsigned int codePoint = 0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00);
        return codePoint <= 0x10FFFF;
    }

    bool XmlConvert::IsPublicIdChar(SharpRuntime::charcs ch) {
        static const std::string extra = " \r\n-'()+,./:=?;!*#@$_%";
        return std::isalnum(static_cast<unsigned char>(ch)) != 0 ||
               extra.find(static_cast<char>(ch)) != std::string::npos;
    }

    bool XmlConvert::IsWhitespaceChar(SharpRuntime::charcs ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    }

    // --- Name encoding -------------------------------------------------------------------

    namespace {
        std::string EncodeNameImpl(const std::string& name, bool isNmToken, bool isLocal) {
            if (name.empty()) return name;
            std::string result;
            result.reserve(name.size());
            for (size_t i = 0; i < name.size(); ++i) {
                char ch = name[i];
                bool isFirst = (i == 0) && !isLocal && !isNmToken;
                bool valid = isFirst ? XmlConvert::IsStartNCNameChar(ch) : XmlConvert::IsNCNameChar(ch);
                // A literal underscore that looks like the start of an escape ("_x....") must
                // itself be escaped, or DecodeName could misinterpret it on round-trip.
                bool looksLikeEscape = ch == '_' && i + 6 < name.size() && name[i + 1] == 'x' &&
                                        std::isxdigit(static_cast<unsigned char>(name[i + 2])) &&
                                        std::isxdigit(static_cast<unsigned char>(name[i + 3])) &&
                                        std::isxdigit(static_cast<unsigned char>(name[i + 4])) &&
                                        std::isxdigit(static_cast<unsigned char>(name[i + 5])) &&
                                        name[i + 6] == '_';
                if (valid && !looksLikeEscape) {
                    result += ch;
                } else {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "_x%04X_", static_cast<unsigned int>(static_cast<unsigned char>(ch)));
                    result += buf;
                }
            }
            return result;
        }
    }

    std::string XmlConvert::EncodeName(const std::string& name) { return EncodeNameImpl(name, false, false); }
    std::string XmlConvert::EncodeNmToken(const std::string& name) { return EncodeNameImpl(name, true, false); }
    std::string XmlConvert::EncodeLocalName(const std::string& name) { return EncodeNameImpl(name, false, true); }

    std::string XmlConvert::DecodeName(const std::string& name) {
        if (name.empty()) return name;
        std::string result;
        result.reserve(name.size());
        size_t i = 0;
        while (i < name.size()) {
            if (name[i] == '_' && i + 6 < name.size() && name[i + 1] == 'x' && name[i + 6] == '_' &&
                std::isxdigit(static_cast<unsigned char>(name[i + 2])) &&
                std::isxdigit(static_cast<unsigned char>(name[i + 3])) &&
                std::isxdigit(static_cast<unsigned char>(name[i + 4])) &&
                std::isxdigit(static_cast<unsigned char>(name[i + 5]))) {
                unsigned int code = 0;
                SHARP_RUNTIME_SSCANF(name.c_str() + i + 2, "%4x", &code);
                result += static_cast<char>(code);
                i += 7;
            } else {
                result += name[i];
                ++i;
            }
        }
        return result;
    }

    // --- Verification --------------------------------------------------------------------

    std::string XmlConvert::VerifyName(const std::string& name) {
        if (name.empty()) throw System::ArgumentException("The value cannot be an empty string.", "name");
        if (!IsStartNCNameChar(name[0]))
            throw XmlException("Invalid XML name: '" + name + "'.");
        for (size_t i = 1; i < name.size(); ++i)
            if (!IsNCNameChar(name[i]) && name[i] != ':')
                throw XmlException("Invalid XML name: '" + name + "'.");
        return name;
    }

    std::string XmlConvert::VerifyNCName(const std::string& name) {
        if (name.empty()) throw System::ArgumentException("The value cannot be an empty string.", "name");
        if (!IsStartNCNameChar(name[0]))
            throw XmlException("Invalid XML NCName: '" + name + "'.");
        for (size_t i = 1; i < name.size(); ++i)
            if (!IsNCNameChar(name[i]))
                throw XmlException("Invalid XML NCName: '" + name + "'.");
        return name;
    }

    std::string XmlConvert::VerifyTOKEN(const std::string& token) {
        if (token.empty()) return token;
        bool startsOrEndsWithSpace = token.front() == ' ' || token.back() == ' ';
        bool hasControlChar = token.find_first_of("\t\n\r") != std::string::npos;
        bool hasDoubleSpace = token.find("  ") != std::string::npos;
        if (startsOrEndsWithSpace || hasControlChar || hasDoubleSpace)
            throw XmlException(
                "line-feed (#xA) or tab (#x9) characters, leading or trailing spaces and sequences of "
                "one or more spaces (#x20) are not allowed in 'xs:token'.");
        return token;
    }

    std::string XmlConvert::VerifyNMTOKEN(const std::string& name) {
        if (name.empty()) throw XmlException("Invalid NmToken value '" + name + "'.");
        for (char ch : name)
            if (!IsNCNameChar(ch) && ch != ':')
                throw XmlException("Invalid NmToken value '" + name + "'.");
        return name;
    }

    std::string XmlConvert::VerifyXmlChars(const std::string& content) {
        for (char ch : content)
            if (!IsXmlChar(ch))
                throw XmlException("Invalid XML character in content.");
        return content;
    }

    std::string XmlConvert::VerifyPublicId(const std::string& publicId) {
        for (char ch : publicId)
            if (!IsPublicIdChar(ch))
                throw XmlException("Invalid character in public identifier: '" + publicId + "'.");
        return publicId;
    }

    std::string XmlConvert::VerifyWhitespace(const std::string& content) {
        for (char ch : content)
            if (!IsWhitespaceChar(ch))
                throw XmlException("Invalid whitespace character in content.");
        return content;
    }

    // --- ToString --------------------------------------------------------------------

    std::string XmlConvert::ToString(bool value) { return value ? "true" : "false"; }
    std::string XmlConvert::ToString(SharpRuntime::charcs value) { return System::Char::ToString(value); }
#if SHARP_RUNTIME_HAS_NATIVE_INT128
    std::string XmlConvert::ToString(System::Decimal value) { return value.ToString(); }
#endif
    std::string XmlConvert::ToString(SharpRuntime::sbytecs value) { return System::SByte::ToString(value); }
    std::string XmlConvert::ToString(SharpRuntime::shortcs value) { return System::Int16::ToString(value); }
    std::string XmlConvert::ToString(SharpRuntime::intcs value) { return System::Int32::ToString(value); }
    std::string XmlConvert::ToString(SharpRuntime::longcs value) { return System::Int64::ToString(value); }
    std::string XmlConvert::ToString(SharpRuntime::bytecs value) { return System::Byte::ToString(value); }
    std::string XmlConvert::ToString(SharpRuntime::ushortcs value) { return System::UInt16::ToString(value); }
    std::string XmlConvert::ToString(SharpRuntime::uintcs value) { return System::UInt32::ToString(value); }
    std::string XmlConvert::ToString(SharpRuntime::ulongcs value) { return System::UInt64::ToString(value); }
    // Verified against XmlConvert.cs's ToString(float)/ToString(double): real XmlConvert uses
    // the XML Schema lexical-space tokens "INF"/"-INF" for infinity, not .NET Double/Single's
    // general-purpose ToString() tokens "Infinity"/"-Infinity" -- output using the latter is
    // invalid per the XML Schema `double`/`float` lexical space.
    std::string XmlConvert::ToString(float value) {
        if (std::isinf(value)) return value < 0 ? "-INF" : "INF";
        return System::Single::ToString(value);
    }
    std::string XmlConvert::ToString(double value) {
        if (std::isinf(value)) return value < 0 ? "-INF" : "INF";
        return System::Double::ToString(value);
    }
    // Ticket #2080. This used to emit .NET's NATIVE colon form -- "1.00:00:00.0000000" for one
    // day -- where XmlConvert.ToString(TimeSpan) is `new XsdDuration(value).ToString()`
    // (XmlConvert.cs:686-689) and produces "P1D". See FormatXsdDuration above.
    std::string XmlConvert::ToString(const System::TimeSpan& value) {
        return FormatXsdDuration(value);
    }

namespace {

    // ---------------------------------------------------------------------------
    // #1945 -- the DateTimeKind matrix, and where this module can reach a zone
    // ---------------------------------------------------------------------------
    //
    // FOUR MEMBERS OF THIS CLASS ACCEPTED A SECOND ARGUMENT AND DISCARDED IT: two `format`
    // parameters and two `XmlDateTimeSerializationMode` parameters, each spelled `/*format*/` or
    // `/*mode*/`. A caller could hand `ToDateTime(s, "HH:mm:ss")` a date and get it back, because
    // the value was parsed by an entirely different grammar with no diagnostic -- the SR-AUD-168
    // shape, four times over.
    //
    // THE MODE HALF CARRIED A PREMISE THAT HAD STOPPED BEING TRUE. Its comment read "System::
    // DateTime does not track DateTimeKind (see its own doc-comment), so Local/Utc/Unspecified/
    // RoundtripKind cannot be distinguished here". #1941 phase 1 gave `DateTime` a `Kind` and
    // phase 2 made it convert by that kind, so the sentence describes a runtime that no longer
    // exists.
    //
    // AND THIS MODULE CAN REACH A ZONE WHERE `Core.Base` CANNOT. At the #1945 checkpoint that was
    // why this work could land before #1942; #1942 has since completed. #1941 phase 2 had to take
    // an `ILocalTimeZone` as a
    // PARAMETER, because `Core.Base` cannot name a time zone and .NET reaches `TimeZoneInfo.Local`
    // internally. `modules/xml` is under no such constraint: `TimeZone` depends on `Core.Base`
    // alone, so taking it as a PRIVATE dependency is not a cycle, and
    // `System::TimeZone::CurrentTimeZone()` is already an `ILocalTimeZone`. So the deviation
    // #1941 recorded is resolved HERE, by the module that can actually name the zone, and
    // `XmlConvert`'s signatures stay exactly .NET's -- no zone parameter, because none is needed.

    /** @brief .NET's `SwitchToLocalTime` (`XmlConvert.cs`), transcribed cell by cell. */
    System::DateTime switchToLocalTime(const System::DateTime& value) {
        switch (value.getKindProperty()) {
            case System::DateTimeKind::Local:       return value;
            // STAMP, not convert: an unspecified value is DECLARED local, its ticks untouched.
            case System::DateTimeKind::Unspecified:
                return System::DateTime::SpecifyKind(value, System::DateTimeKind::Local);
            // CONVERT: this is one of only two cells in the whole matrix that moves the ticks.
            case System::DateTimeKind::Utc:
                return value.ToLocalTime(System::TimeZone::CurrentTimeZone());
            default:                                return value;
        }
    }

    /** @brief .NET's `SwitchToUtcTime`. Mirror of the above, and NOT its inverse cell for cell. */
    System::DateTime switchToUtcTime(const System::DateTime& value) {
        switch (value.getKindProperty()) {
            case System::DateTimeKind::Utc:         return value;
            case System::DateTimeKind::Unspecified:
                return System::DateTime::SpecifyKind(value, System::DateTimeKind::Utc);
            case System::DateTimeKind::Local:
                return value.ToUniversalTime(System::TimeZone::CurrentTimeZone());
            default:                                return value;
        }
    }

    /**
     * @brief .NET's `XsdDateTime` rendering for `XmlDateTimeFlags.DateTime` (#1945, SA-16.5).
     *
     * TWO CHANGES FROM WHAT THIS FILE USED TO EMIT, NOT ONE, and that is why SA-16.5 had to be
     * asked rather than assumed. `value.ToString()` renders `2024-06-15 12:00:00` -- a SPACE
     * separator and no kind marker. An XSD `dateTime` literal requires the `T`, so appending only
     * the marker would have repaired the round trip and still left the document wrong.
     *
     * THE MARKER IS WHAT MAKES `RoundtripKind` REAL. #1945 measured that a kind could not cross a
     * string in this port, because nothing wrote one and nothing read one; SA-16.3 closed both
     * halves, and this is the writing half.
     */
    std::string renderXsdDateTime(const System::DateTime& value) {
        char buffer[64];
        const long fractionTicks = static_cast<long>(value.getTicksProperty() % 10000000LL);
        int written = std::snprintf(
            buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d",
            static_cast<int>(value.getYearProperty()), static_cast<int>(value.getMonthProperty()),
            static_cast<int>(value.getDayProperty()), static_cast<int>(value.getHourProperty()),
            static_cast<int>(value.getMinuteProperty()),
            static_cast<int>(value.getSecondProperty()));
        std::string out(buffer, static_cast<std::size_t>(written));

        // .NET emits the fraction only when it is non-zero and TRIMS trailing zeroes -- `.5`
        // rather than `.5000000`. Always writing seven digits would be a different literal for
        // the same instant.
        if (fractionTicks != 0) {
            written = std::snprintf(buffer, sizeof(buffer), "%07ld", fractionTicks);
            std::string frac(buffer, static_cast<std::size_t>(written));
            while (!frac.empty() && frac.back() == '0') frac.pop_back();
            out += '.';
            out += frac;
        }

        // The kind marker. `Unspecified` writes NOTHING, which is what lets an unspecified value
        // round-trip as unspecified rather than acquiring a kind it never had.
        switch (value.getKindProperty()) {
            case System::DateTimeKind::Utc:   out += 'Z'; break;
            case System::DateTimeKind::Local: {
                const System::TimeSpan offset =
                    System::TimeZone::CurrentTimeZone().GetUtcOffset(value);
                const SharpRuntime::longcs totalMinutes = offset.getTicksProperty() / 600000000LL;
                const char sign = totalMinutes < 0 ? '-' : '+';
                const SharpRuntime::longcs magnitude =
                    totalMinutes < 0 ? -totalMinutes : totalMinutes;
                written = std::snprintf(buffer, sizeof(buffer), "%c%02d:%02d", sign,
                                        static_cast<int>(magnitude / 60),
                                        static_cast<int>(magnitude % 60));
                out.append(buffer, static_cast<std::size_t>(written));
                break;
            }
            case System::DateTimeKind::Unspecified: break;
        }
        return out;
    }

    /** @brief XSD `dateTime` rendering for a DateTimeOffset, preserving its explicit offset. */
    std::string renderXsdDateTimeOffset(const System::DateTimeOffset& value) {
        const System::DateTime clock = value.getDateTimeProperty();
        char buffer[64];
        int written = std::snprintf(
            buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d",
            static_cast<int>(clock.getYearProperty()), static_cast<int>(clock.getMonthProperty()),
            static_cast<int>(clock.getDayProperty()), static_cast<int>(clock.getHourProperty()),
            static_cast<int>(clock.getMinuteProperty()),
            static_cast<int>(clock.getSecondProperty()));
        std::string out(buffer, static_cast<std::size_t>(written));

        const long fractionTicks = static_cast<long>(
            clock.getTicksProperty() % System::DateTime::TicksPerSecond);
        if (fractionTicks != 0) {
            written = std::snprintf(buffer, sizeof(buffer), "%07ld", fractionTicks);
            std::string fraction(buffer, static_cast<std::size_t>(written));
            while (!fraction.empty() && fraction.back() == '0') fraction.pop_back();
            out += '.';
            out += fraction;
        }

        SharpRuntime::longcs totalMinutes =
            value.getOffsetProperty().getTicksProperty() / System::TimeSpan::TicksPerMinute;
        if (totalMinutes == 0) {
            // XsdDateTime classifies a zero DateTimeOffset offset as Zulu. `+00:00` names the
            // same instant, but XmlConvert's canonical lexical form is the single `Z` marker.
            out += 'Z';
            return out;
        }
        const char sign = totalMinutes < 0 ? '-' : '+';
        if (totalMinutes < 0) totalMinutes = -totalMinutes; // offset is validated within +/-14h
        written = std::snprintf(buffer, sizeof(buffer), "%c%02d:%02d", sign,
                                static_cast<int>(totalMinutes / 60),
                                static_cast<int>(totalMinutes % 60));
        out.append(buffer, static_cast<std::size_t>(written));
        return out;
    }

    /**
     * @brief Splits a trailing XSD kind marker off @p text (#1945, SA-16.3's reading half).
     *
     * **THIS DOES NOT GO THROUGH `DateTime::Parse`, AND THAT IS DELIBERATE RATHER THAN A
     * WORKAROUND.** SA-16.4 left the general `Parse` alone -- it still parses a zone and
     * **discards** it, which is #1929's recorded behaviour -- so a round trip built on it could
     * never carry a kind however the writing half rendered. **.NET does not use `DateTime.Parse`
     * here either**: `XmlConvert.ToDateTime` builds an `XsdDateTime`, which parses the zone
     * itself. This is that split, in the smallest form that expresses it.
     *
     * @param text The rendered value; the marker is removed in place.
     * @param kind Receives `Utc` for `Z`, `Local` for a numeric offset, `Unspecified` for neither.
     * @param offsetMinutes Receives the numeric offset in minutes; 0 otherwise.
     */
    void splitXsdKindMarker(std::string& text, System::DateTimeKind& kind, int& offsetMinutes) {
        kind = System::DateTimeKind::Unspecified;
        offsetMinutes = 0;
        if (text.empty()) return;

        if (text.back() == 'Z') {
            text.pop_back();
            kind = System::DateTimeKind::Utc;
            return;
        }
        const auto invalidTimezone = [] {
            throw System::FormatException(
                "The string is not a valid XSD dateTime value: the timezone must be 'Z' or "
                "+/-hh:mm.");
        };
        // DateTime's intentionally broader general grammar accepts lower-case `z`; XSD's lexical
        // marker is case-sensitive, so it must not reach that parser and silently lose its Kind.
        if (text.back() == 'z') invalidTimezone();

        // A numeric offset is exactly six characters, `+hh:mm` or `-hh:mm`. Matching a SHAPE
        // rather than scanning backwards for a sign is what stops a date's own `-` separator
        // from being read as one -- `2024-06-15` ends in `06-15`, which a looser rule accepts.
        auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
        if (text.size() >= 6) {
            const std::string tail = text.substr(text.size() - 6);
            if ((tail[0] == '+' || tail[0] == '-') && tail[3] == ':' &&
                isDigit(tail[1]) && isDigit(tail[2]) &&
                isDigit(tail[4]) && isDigit(tail[5])) {
                const int hours = (tail[1] - '0') * 10 + (tail[2] - '0');
                const int minutes = (tail[4] - '0') * 10 + (tail[5] - '0');
                const int magnitudeMinutes = hours * 60 + minutes;
                if (minutes > 59 || magnitudeMinutes > 14 * 60) {
                    // XSD's numeric timezone bound is exactly +/-14:00. Leaving the marker
                    // attached and falling through to DateTime::Parse would be unsafe here
                    // because that broader practical-subset grammar deliberately accepts and
                    // discards offsets such as +14:59.
                    throw System::FormatException(
                        "The string is not a valid XSD dateTime value: timezone offset is outside "
                        "+/-14:00.");
                }
                offsetMinutes = (tail[0] == '-' ? -1 : 1) * magnitudeMinutes;
                kind = System::DateTimeKind::Local;
                text.erase(text.size() - 6);
                return;
            }
        }

        // No canonical marker was found. A plus is never part of the supported date/time body;
        // a final minus beyond the date separators is likewise a timezone attempt. Reject these
        // before the broader DateTime parser accepts `+8`, `+2:5`, `+800` or `+0800` and discards
        // them. Ordinary dates such as 2024-06-15 and 06-15-2024 keep their two separators.
        const std::size_t plus = text.rfind('+');
        const std::size_t minus = text.rfind('-');
        if (plus != std::string::npos || (minus != std::string::npos && minus > 7))
            invalidTimezone();
    }

    /**
     * @brief The four-way mode switch shared by `ToString(value, mode)` and `ToDateTime(s, mode)`.
     *
     * ONE DEFINITION, because .NET writes the same switch twice and two copies of one matrix is
     * how the two doors come to disagree. The `default:` arm's message is .NET's verbatim
     * (`Sch_InvalidDateTimeOption`), and it is reachable only by casting a value in from outside
     * the enumeration -- which is exactly why it needs a test.
     */
    System::DateTime applyDateTimeMode(const System::DateTime& value,
                                       System::Xml::XmlDateTimeSerializationMode mode) {
        using System::Xml::XmlDateTimeSerializationMode;
        switch (mode) {
            case XmlDateTimeSerializationMode::Local:  return switchToLocalTime(value);
            case XmlDateTimeSerializationMode::Utc:    return switchToUtcTime(value);
            // STRIPS the kind while keeping the ticks -- `new DateTime(value.Ticks, Unspecified)`.
            case XmlDateTimeSerializationMode::Unspecified:
                return System::DateTime::SpecifyKind(value, System::DateTimeKind::Unspecified);
            // The one arm that does NOTHING, which is what "roundtrip" means: the kind survives.
            // `renderXsdDateTime` and `splitXsdKindMarker` are the matching write/read halves, so
            // this is observably different from Unspecified for both Utc (`Z`) and Local
            // (numeric offset) values. This path deliberately does not depend on general
            // DateTime::Parse, whose practical-subset contract still consumes but does not apply
            // an implicit local-zone designator.
            case XmlDateTimeSerializationMode::RoundtripKind: return value;
        }
        throw System::ArgumentException(
            "The '" + std::to_string(static_cast<int>(mode)) + "' value for the 'dateTimeOption' "
            "parameter is not an allowed value for the 'XmlDateTimeSerializationMode' enumeration.",
            "dateTimeOption");
    }

} // namespace

    std::string XmlConvert::ToString(const System::DateTime& value) {
        return renderXsdDateTime(value);
    }
    std::string XmlConvert::ToString(const System::DateTime& value, const std::string& format) { return value.ToString(format); }

    std::string XmlConvert::ToString(const System::DateTime& value,
                                     XmlDateTimeSerializationMode mode) {
        return renderXsdDateTime(applyDateTimeMode(value, mode));
    }

    std::string XmlConvert::ToString(const System::DateTimeOffset& value) {
        return renderXsdDateTimeOffset(value);
    }
    std::string XmlConvert::ToString(const System::DateTimeOffset& value, const std::string& format) { return value.ToString(format); }
    std::string XmlConvert::ToString(const System::Guid& value) { return value.ToString(); }

    // --- ToXxx -----------------------------------------------------------------------

    bool XmlConvert::ToBoolean(const std::string& s) {
        auto notws = [](unsigned char c) { return !std::isspace(c); };
        auto b = std::find_if(s.begin(), s.end(), notws);
        auto e = std::find_if(s.rbegin(), s.rend(), notws).base();
        std::string t(b, e < b ? b : e);
        if (t == "1" || t == "true") return true;
        if (t == "0" || t == "false") return false;
        throw System::FormatException("String '" + s + "' was not recognized as a valid Boolean.");
    }
    SharpRuntime::charcs XmlConvert::ToChar(const std::string& s) { return System::Char::Parse(s); }
#if SHARP_RUNTIME_HAS_NATIVE_INT128
    // Verified against XmlConvert.cs's ToDecimal(string), which passes NumberStyles.
    // AllowLeadingWhite | AllowTrailingWhite to decimal.Parse. XML decimal content with
    // surrounding whitespace -- common from document formatting/indentation -- previously
    // threw FormatException; same bug class as ToSingle/ToDouble below. (As of ticket #1857
    // System::Decimal::TryParse itself now also skips leading/trailing whitespace, but this
    // explicit TrimXmlWhitespace is retained: it also governs the INF/-INF token check and
    // keeps XmlConvert independent of the exact whitespace set Decimal happens to accept.)
    System::Decimal XmlConvert::ToDecimal(const std::string& s) { return System::Decimal::Parse(TrimXmlWhitespace(s)); }
#endif
    SharpRuntime::sbytecs XmlConvert::ToSByte(const std::string& s) { return System::SByte::Parse(s); }
    SharpRuntime::shortcs XmlConvert::ToInt16(const std::string& s) { return System::Int16::Parse(s); }
    SharpRuntime::intcs XmlConvert::ToInt32(const std::string& s) { return System::Int32::Parse(s); }
    SharpRuntime::longcs XmlConvert::ToInt64(const std::string& s) { return System::Int64::Parse(s); }
    SharpRuntime::bytecs XmlConvert::ToByte(const std::string& s) { return System::Byte::Parse(s); }
    SharpRuntime::ushortcs XmlConvert::ToUInt16(const std::string& s) { return System::UInt16::Parse(s); }
    SharpRuntime::uintcs XmlConvert::ToUInt32(const std::string& s) { return System::UInt32::Parse(s); }
    SharpRuntime::ulongcs XmlConvert::ToUInt64(const std::string& s) { return System::UInt64::Parse(s); }
    // Verified against XmlConvert.cs's ToSingle(string)/ToDouble(string): real XmlConvert trims
    // XML whitespace, then recognizes "-INF"/"INF" as the XML Schema lexical-space infinity
    // tokens before falling through to ordinary numeric parsing -- valid schema input like
    // "INF" previously failed to parse (Single::Parse/Double::Parse only recognize .NET's own
    // "Infinity"/"-Infinity" spelling).
    //
    // Also fixed here: the fallback path called Parse(s) with the ORIGINAL untrimmed string
    // instead of the already-computed `trimmed` one. Single::Parse/Double::Parse delegate to
    // std::from_chars, which -- unlike .NET's float.Parse/double.Parse -- does NOT skip leading
    // or trailing whitespace at all (confirmed via a standalone repro: from_chars fails outright
    // on " 3.14 ", never even reaching the digits). Since XML element/attribute text content
    // commonly has surrounding whitespace from document formatting/indentation (e.g.
    // "<value> 3.14 </value>"), this silently threw FormatException for extremely common,
    // perfectly valid XML Schema float/double content.
    float XmlConvert::ToSingle(const std::string& s) {
        std::string trimmed = TrimXmlWhitespace(s);
        if (trimmed == "-INF") return -std::numeric_limits<float>::infinity();
        if (trimmed == "INF") return std::numeric_limits<float>::infinity();
        return System::Single::Parse(trimmed);
    }
    double XmlConvert::ToDouble(const std::string& s) {
        std::string trimmed = TrimXmlWhitespace(s);
        if (trimmed == "-INF") return -std::numeric_limits<double>::infinity();
        if (trimmed == "INF") return std::numeric_limits<double>::infinity();
        return System::Double::Parse(trimmed);
    }
    /**
     * @brief Parses the XML Schema `duration` lexical form.
     *
     * Ticket #2080. This used to call `TimeSpan::Parse`, so `"P1D"` and `"PT1H30M"` were
     * rejected while `"1.00:00:00"` was accepted -- the exact inverse of
     * `XmlConvert.ToTimeSpan`, which is `new XsdDuration(s).ToTimeSpan()`
     * (`XmlConvert.cs:1109-1127`) and never looks at the colon form.
     *
     * **The colon form is no longer accepted**, and that narrowing is .NET's: `XsdDuration`'s
     * parser requires a leading `P`. Accepting both would make this method's contract "either
     * grammar", which no reference or schema defines.
     *
     * @throws System::FormatException for anything that is not a well-formed duration, with
     *         .NET's own message shape (`XmlConvert_BadFormat`: *"The string '{0}' is not a
     *         valid TimeSpan value."*). `XsdDuration` raises `OverflowException` for a component
     *         too large to hold; this remaps to `FormatException` exactly as
     *         `XmlConvert.ToTimeSpan` does -- its `catch (Exception)` is deliberately broad,
     *         "Remap exception for v1 compatibility".
     */
    System::TimeSpan XmlConvert::ToTimeSpan(const std::string& s) {
        XsdDuration duration;
        long long ticks = 0;
        if (!TryParseXsdDuration(s, duration) || !TryXsdDurationToTicks(duration, ticks)) {
            throw System::FormatException("The string '" + s + "' is not a valid TimeSpan value.");
        }
        return System::TimeSpan::FromTicks(ticks);
    }
    System::DateTime XmlConvert::ToDateTime(const std::string& s) {
        // XML Schema's whitespace facet is applied before the lexical timezone marker is read.
        // Splitting against the original physical end lost `Z` / `+hh:mm` whenever legal outer
        // whitespace followed it, and could let an invalid +14:01 marker bypass the XSD bound.
        std::string body = TrimXmlWhitespace(s);
        System::DateTimeKind kind = System::DateTimeKind::Unspecified;
        int offsetMinutes = 0;
        splitXsdKindMarker(body, kind, offsetMinutes);
        System::DateTime parsed = System::DateTime::Parse(body);
        if (kind == System::DateTimeKind::Local) {
            // A numeric offset names an INSTANT, so the value is converted to this process's zone
            // rather than merely stamped -- otherwise `+05:00` and `+02:00` would produce the same
            // local wall-clock time and the offset would have been read and thrown away again.
            // XsdDateTime has explicit compatibility behavior at DateTime's range boundaries:
            // if converting the named offset to UTC under/overflows, it applies the process-local
            // offset directly to the original clock and clamps only if that still lies outside.
            // Calling AddMinutes here used to leak ArgumentOutOfRangeException from a Try-style
            // parsing path instead of reproducing that result.
            const auto& zone = System::TimeZone::CurrentTimeZone();
            const SharpRuntime::longcs offsetTicks =
                static_cast<SharpRuntime::longcs>(offsetMinutes) *
                System::TimeSpan::TicksPerMinute;
            SharpRuntime::longcs utcTicks = parsed.getTicksProperty() - offsetTicks;
            if (utcTicks < 0 || utcTicks > System::DateTime::MaxTicks) {
                const SharpRuntime::longcs localOffset =
                    zone.GetUtcOffset(parsed).getTicksProperty();
                SharpRuntime::longcs compatibleTicks = utcTicks + localOffset;
                if (compatibleTicks < 0) compatibleTicks = 0;
                if (compatibleTicks > System::DateTime::MaxTicks)
                    compatibleTicks = System::DateTime::MaxTicks;
                parsed = System::DateTime(
                    compatibleTicks, System::DateTimeKind::Local);
            } else {
                parsed = System::DateTime(utcTicks, System::DateTimeKind::Utc)
                             .ToLocalTime(zone);
            }
        } else if (kind == System::DateTimeKind::Utc) {
            parsed = System::DateTime::SpecifyKind(parsed, System::DateTimeKind::Utc);
        }
        return parsed;
    }
    System::DateTime XmlConvert::ToDateTime(const std::string& s, const std::string& format) {
        // XmlConvert.cs routes this door through invariant ParseExact with leading/trailing
        // whitespace enabled. Supplying the process zone is the C++ signature adaptation recorded
        // by #1942: it is needed when the exact format carries z/K or its styles require a local
        // conversion, and modules/xml can name the TimeZone component without creating a cycle.
        const auto styles =
            System::Globalization::DateTimeStyles::AllowLeadingWhite |
            System::Globalization::DateTimeStyles::AllowTrailingWhite;
        return System::DateTime::ParseExact(
            s, format, nullptr, styles, &System::TimeZone::CurrentTimeZone());
    }
    System::DateTime XmlConvert::ToDateTime(const std::string& s, XmlDateTimeSerializationMode mode) {
        // THROUGH THE ONE-ARGUMENT DOOR, not DateTime::Parse: the marker must be split off before
        // the mode is applied, or every parsed value would still arrive Unspecified and the mode
        // would stamp rather than convert. Two doors reading one grammar two ways is the #2393
        // shape, and it is the exact defect this ticket exists to end.
        return applyDateTimeMode(ToDateTime(s), mode);
    }
    System::DateTimeOffset XmlConvert::ToDateTimeOffset(const std::string& s) {
        // The XSD door must choose an offset for the parsed DATE, not Core.Base's documented
        // current-offset-only DateTimeOffset::Parse fallback. Splitting the marker here also
        // keeps all XmlConvert date-time doors on the same XSD numeric-offset bound.
        std::string body = TrimXmlWhitespace(s);
        System::DateTimeKind markerKind = System::DateTimeKind::Unspecified;
        int offsetMinutes = 0;
        splitXsdKindMarker(body, markerKind, offsetMinutes);
        const System::DateTime clock = System::DateTime::Parse(body);
        System::TimeSpan offset = System::TimeSpan::Zero;
        if (markerKind == System::DateTimeKind::Local) {
            offset = System::TimeSpan::FromMinutes(offsetMinutes);
        } else if (markerKind == System::DateTimeKind::Unspecified) {
            offset = System::TimeZone::CurrentTimeZone().GetUtcOffset(clock);
        }
        return System::DateTimeOffset(clock, offset);
    }
    System::DateTimeOffset XmlConvert::ToDateTimeOffset(const std::string& s,
                                                        const std::string& format) {
        // Keep this paired with the DateTime door above. #1943 added the real DateTimeOffset exact
        // grammar, including zzz/K offset capture; the former DateTime-plus-local-offset
        // composition predated it and silently made every explicit-offset format impossible.
        // XmlConvert itself permits outer whitespace, and a missing offset defaults from the
        // explicit process zone as required by this port's Core.Base dependency boundary.
        const auto styles =
            System::Globalization::DateTimeStyles::AllowLeadingWhite |
            System::Globalization::DateTimeStyles::AllowTrailingWhite;
        return System::DateTimeOffset::ParseExact(
            s, format, nullptr, styles, &System::TimeZone::CurrentTimeZone());
    }
    System::Guid XmlConvert::ToGuid(const std::string& s) { return System::Guid::Parse(s); }

} // namespace System::Xml
