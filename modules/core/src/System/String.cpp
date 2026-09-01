// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/String.hpp"
#include "System/TimeSpan.hpp"
#include "System/detail/CompositeFormat.hpp"
#include "System/detail/FloatTextFormat.hpp"
#include "System/Double.hpp"
#include "System/Int32.hpp"
#include "System/Single.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"
#include "System/OutOfMemoryException.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <new>
#include <sstream>
#include <stdexcept>

namespace System
{
    std::vector<std::string> String::Split(const std::string& value, char delimiter)
    {
        // Manual find/substr scan instead of std::stringstream + std::getline: measured ~2.6x
        // faster for typical short game-code strings (e.g. tokenizing a CSV-ish line), since
        // stringstream carries locale-facet and virtual-dispatch overhead getline doesn't need
        // for a single-character delimiter. Matches the approach String::Split(value, string)
        // already used below (this Split overload was the odd one out).
        //
        // Side effect verified against real .NET (String.Manipulation.cs's
        // CreateSplitArrayOfThisAsSoleValue): this also fixes a pre-existing correctness bug --
        // the old getline-based loop returned an EMPTY vector for an empty @p value, but real
        // .NET's "".Split(',') returns a one-element array containing "". This scan naturally
        // produces the correct {""} result without a special case (the trailing push_back below
        // always executes at least once).
        std::vector<std::string> result;
        std::size_t pos = 0, found;
        while ((found = value.find(delimiter, pos)) != std::string::npos)
        {
            result.push_back(value.substr(pos, found - pos));
            pos = found + 1;
        }
        result.push_back(value.substr(pos));
        return result;
    }

    std::vector<std::string> String::Split(const std::string& value, const std::vector<char>& delimiters)
    {
        std::vector<std::string> result;
        std::string current;
        for (char c : value) {
            bool isDelim = false;
            for (char d : delimiters) if (c == d) { isDelim = true; break; }
            if (isDelim) { result.push_back(current); current.clear(); }
            else current += c;
        }
        result.push_back(current);
        return result;
    }

    std::vector<std::string> String::Split(const std::string& value, const std::string& delimiter)
    {
        std::vector<std::string> result;
        if (delimiter.empty()) { result.push_back(value); return result; }
        size_t pos = 0, found;
        while ((found = value.find(delimiter, pos)) != std::string::npos) {
            result.push_back(value.substr(pos, found - pos));
            pos = found + delimiter.size();
        }
        result.push_back(value.substr(pos));
        return result;
    }

    bool String::IsEmpty(const std::string& value)
    {
        return value.empty();
    }

    bool String::StartsWith(const std::string& value, const std::string& prefix)
    {
        return value.size() >= prefix.size() &&
               value.compare(0, prefix.size(), prefix) == 0;
    }

    bool String::IsNullOrEmpty(const std::string& value)
    {
        return value.empty();
    }

    // --- Format helpers (file-internal) ---
    //
    // Ticket #1882 (SR-AUD-015, CCF-012) replaced a sequential text-replacement engine with the
    // single-pass parse below. The previous engine produced its output by repeatedly mutating a
    // buffer that already held substituted argument text, which is the single root cause of four
    // measured defects (docs/CompositeFormatBoundaryPlan.md sections 5 and 8):
    //
    //   * it did not terminate. replaceArg reset its scan cursor to 0 after every substitution,
    //     so it re-read text it had just inserted; Format("{0}", "{0}") looped forever. This
    //     needed no malformed format string -- Format("{0}", userText) hung whenever userText
    //     contained "{0}".
    //   * it re-read an argument's literal text as format syntax, so a later argument overwrote
    //     an earlier one: Format("{0}{1}", "{1}", "X") returned "XX" instead of "{1}X", and
    //     Format("{0}{1}", "{1}", "{0}") threw FormatException on input .NET formats fine.
    //   * extractSpec returned the FIRST occurrence's specifier for every occurrence of an
    //     index, so Format("{0:X}/{0:D3}", 255) returned "FF/FF" instead of "FF/255".
    //   * validation ran after substitution (FinalizeFormat), so it could only classify what
    //     survived and reported an index error for an unclosed item.
    //
    // The parse below walks the format string exactly once and appends to a separate output, the
    // property that makes .NET's own AppendFormatHelper immune to all four by construction
    // (System.Private.CoreLib/src/System/Text/ValueStringBuilder.AppendFormat.cs).
    //
    // The ACCEPTED GRAMMAR IS DELIBERATELY UNCHANGED: "{{"/"}}" are still not escapes, a stray
    // "}" is still literal text, and "{N,width}" still parses but does not pad. Adopting .NET's
    // grammar changes what currently-succeeding calls return and is gated on explicit user
    // approval as ticket #1884 (plan section 20).
    namespace {
        // The one bounded backward substring search for `String::LastIndexOf` (#2224,
        // SR-AUD-016).
        //
        // `std::string::rfind(substr, startIndex)` treats `startIndex` as the latest permitted
        // match **start**, but .NET's bounded LastIndexOf defines a *searched section* and the
        // whole match must lie inside it. Both substring overloads used to check only the lower
        // bound, so a match that began inside the section and ran off its end was returned:
        // `LastIndexOf("abcde", "de", 3, 4)` answered 3 although the section is indices 0..3 and
        // the match consumes index 4, and `LastIndexOf("abcde", "abcde", 2, 3)` answered 0,
        // overrunning `startIndex` by three positions.
        //
        // Searching from `searchEnd = startIndex + 1 - substr.size()` — the last position at
        // which the match still *fits* — is what makes the upper bound hold, and the size
        // comparison is done in `size_t` so a substring longer than the section cannot produce a
        // negative intermediate. The single-character overloads do not need this: a one-character
        // match can never overrun its own start.
        SharpRuntime::intcs lastIndexOfBounded(const std::string& value, const std::string& substr,
                                               SharpRuntime::intcs startIndex,
                                               SharpRuntime::intcs lowerBound) {
            const size_t inclusiveEnd = static_cast<size_t>(startIndex) + 1u;
            if (substr.size() > inclusiveEnd) return -1;  // cannot fit at or before startIndex
            const size_t searchEnd = inclusiveEnd - substr.size();
            const auto pos = value.rfind(substr, searchEnd);
            if (pos == std::string::npos) return -1;
            return static_cast<SharpRuntime::intcs>(pos) >= lowerBound
                       ? static_cast<SharpRuntime::intcs>(pos)
                       : -1;
        }

        // Every Format overload's arguments, type-erased just enough to be indexed by a parsed
        // format item. `text` points at the caller's own std::string parameter, which outlives
        // the call; nothing here owns or copies it.
        struct FormatArg {
            enum class Kind { Int, Long, Float, Double, Text, TimeSpan };
            Kind                 kind = Kind::Int;
            SharpRuntime::intcs  i    = 0;
            SharpRuntime::longcs l    = 0;
            float                f    = 0.0f;
            double               d    = 0.0;
            const std::string*   text = nullptr;
            const System::TimeSpan* timeSpan = nullptr;
        };

        FormatArg argOf(SharpRuntime::intcs v)   { FormatArg a; a.kind = FormatArg::Kind::Int;    a.i = v; return a; }
        FormatArg argOf(SharpRuntime::longcs v)  { FormatArg a; a.kind = FormatArg::Kind::Long;   a.l = v; return a; }
        // A float is NOT widened to double. Single and Double round-trip through a different
        // number of digits, so Format("{0}", 59.4f) came out as "59.400001525878906" -- the
        // double text of the widened float -- where .NET prints "59.4", because .NET formats the
        // argument with its OWN Single.ToString().
        FormatArg argOf(float v)                 { FormatArg a; a.kind = FormatArg::Kind::Float;  a.f = v; return a; }
        FormatArg argOf(double v)                { FormatArg a; a.kind = FormatArg::Kind::Double; a.d = v; return a; }
        FormatArg argOf(const std::string& v)    { FormatArg a; a.kind = FormatArg::Kind::Text;   a.text = &v; return a; }
        FormatArg argOf(const System::TimeSpan& v) { FormatArg a; a.kind = FormatArg::Kind::TimeSpan; a.timeSpan = &v; return a; }

        // Largest specifier number accepted. This is .NET's own bound: ParseFormatSpecifier
        // (Common/src/System/Number.Formatting.Common.cs:93-105) throws
        // FormatException(SR.Argument_BadFormatSpecifier) as soon as the accumulator would reach
        // 100'000'000, so the accepted range is 0..99'999'999.
        constexpr long long kSpecNumberLimit = 100'000'000LL;

        enum class SpecNumberKind { Absent, Value, TooLarge };

        struct SpecNumber {
            SpecNumberKind kind  = SpecNumberKind::Absent;
            long long      value = 0;
        };

        // Parses the numeric tail of a format specifier ("3" in "D3", "-2" in "F-2").
        //
        // This deliberately reproduces std::stoi's PREFIX semantics -- optional sign, digits,
        // trailing junk ignored -- because that is what the previous implementation did and every
        // currently-succeeding specifier must keep its exact result. What it does not reproduce
        // is std::stoi's failure modes, which were the defect: std::stoi THREW std::out_of_range
        // or std::invalid_argument straight out of a System-shaped public API for inputs such as
        // "{0:D99999999999}" and "{0:DX}", and the std::abs() applied to its result was
        // UBSan-confirmed undefined behaviour for "{0:D-2147483648}" (negation of INT_MIN).
        SpecNumber parseSpecNumber(std::string_view spec) {
            if (spec.size() <= 1) return {};
            std::size_t i = 1;
            bool negative = false;
            if (spec[i] == '+' || spec[i] == '-') { negative = (spec[i] == '-'); ++i; }
            if (i >= spec.size() || !std::isdigit(static_cast<unsigned char>(spec[i]))) return {};
            long long n = 0;
            for (; i < spec.size() && std::isdigit(static_cast<unsigned char>(spec[i])); ++i) {
                // The bound is checked BEFORE the multiply, exactly as the reference does, so the
                // accepted range is the reference's own 0..999'999'999 rather than a stricter one
                // this port would be inventing without authority.
                if (n >= kSpecNumberLimit) return {SpecNumberKind::TooLarge, 0};
                n = n * 10 + (spec[i] - '0');
            }
            return {SpecNumberKind::Value, negative ? -n : n};
        }

        [[noreturn]] void throwBadFormatSpecifier() {
            // .NET: SR.Argument_BadFormatSpecifier, raised as a FormatException by
            // ThrowHelper.ThrowFormatException_BadFormatSpecifier().
            throw System::FormatException("Format specifier was invalid.");
        }

        // Format integer with .NET-style specifier (X/x=hex, D=decimal padded, else plain).
        std::string fmtInt(SharpRuntime::intcs value, std::string_view spec) {
            if (spec.empty()) return std::to_string(value);
            // A format that is not one letter plus an optional precision is a CUSTOM numeric
            // format string -- "00", "0.00", "#,##0" -- a separate .NET grammar the standard
            // specifiers below cannot express. This function knew only X/x/D/d, so every custom
            // format fell through to a plain decimal and was silently DROPPED:
            // Format("{0:00}:{1:00}", 3, 7) returned "3:7" where .NET returns "03:07". The
            // grammar is already implemented once, in the type's own ToString, so this defers to
            // it rather than growing a second copy. Standard specifiers keep taking the path
            // below unchanged, which is what preserves the specifier-tail hardening of tickets
            // #1847/#1849 -- ToString's own tail parsing is stricter in ways Format's callers
            // are already tested against ("{0:DX}" must yield "42", not throw).
            if (System::detail::isCustomNumericPlaceholderFormat(std::string(spec))) {
                return System::Int32::ToString(value, std::string(spec));
            }
            const char sc = spec[0];
            const SpecNumber num = parseSpecNumber(spec);
            if (num.kind == SpecNumberKind::TooLarge) throwBadFormatSpecifier();
            // The magnitude, as the previous std::abs() intended. The bound above makes the
            // negation exact for every accepted value, so INT_MIN can no longer be reached.
            const long long width = num.kind == SpecNumberKind::Value
                                        ? (num.value < 0 ? -num.value : num.value)
                                        : 0;
            if (sc == 'X' || sc == 'x') {
                try {
                    std::ostringstream oss;
                    oss.imbue(std::locale::classic());
                    if (sc == 'X') oss << std::uppercase;
                    oss << std::hex;
                    // std::setw takes an int, so the streamsize cast only widened the value on
                    // its way back down again -- a 64-to-32 narrowing MSVC /W4 reports as C4244.
                    // parseSpecNumber accepts at most 999'999'999, so the int cast is exact for
                    // every value that can reach here, exactly like the precision cast below.
                    if (width > 0) oss << std::setw(static_cast<int>(width)) << std::setfill('0');
                    oss << static_cast<unsigned int>(value);
                    return oss.str();
                } catch (const std::bad_alloc&) {
                    throw System::OutOfMemoryException();
                } catch (const std::length_error&) {
                    throw System::OutOfMemoryException();
                }
            }
            if (sc == 'D' || sc == 'd') {
                const bool neg = value < 0;
                std::string s = std::to_string(neg ? -static_cast<long long>(value) : static_cast<long long>(value));
                // One allocation. The previous "s = \"0\" + s" loop copied the whole string once
                // per padding character, so Format("{0:D999999999}", 7) was ~10^18 byte copies --
                // a second, independent way this function failed to terminate.
                //
                // A width the reference accepts can still be large enough that the allocation
                // fails. .NET raises OutOfMemoryException there; letting std::bad_alloc or
                // std::length_error escape would reintroduce exactly the defect this ticket
                // removes -- a non-System exception leaving a System-shaped public API.
                if (static_cast<long long>(s.size()) < width) {
                    try {
                        s.insert(s.begin(), static_cast<std::size_t>(width - static_cast<long long>(s.size())), '0');
                    } catch (const std::bad_alloc&) {
                        throw System::OutOfMemoryException();
                    } catch (const std::length_error&) {
                        throw System::OutOfMemoryException();
                    }
                }
                return neg ? "-" + s : s;
            }
            return std::to_string(value);
        }

        // Format double with .NET-style specifier (F=fixed, G=general, E=scientific).
        std::string fmtDouble(double value, std::string_view spec) {
            if (spec.empty()) {
                std::array<char, 64> buf;
                auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
                return ec == std::errc{} ? std::string(buf.data(), ptr) : std::to_string(value);
            }
            // Same custom-format gap as fmtInt above, in the floating-point half:
            // Format("{0:0.00}", 59.4) returned "59.4" instead of "59.40".
            if (System::detail::isCustomNumericPlaceholderFormat(std::string(spec))) {
                return System::Double::ToString(value, std::string(spec));
            }
            const char sc = spec[0];
            const SpecNumber num = parseSpecNumber(spec);
            if (num.kind == SpecNumberKind::TooLarge) throwBadFormatSpecifier();
            // Absent numeric tail keeps the previous default of 2, and a negative value is passed
            // through unchanged, so every specifier that formatted successfully before still
            // produces the identical text.
            const int prec = num.kind == SpecNumberKind::Value ? static_cast<int>(num.value) : 2;
            try {
                std::ostringstream oss;
                oss.imbue(std::locale::classic());
                if (sc == 'F' || sc == 'f') {
                    oss << std::fixed << std::setprecision(prec) << value;
                } else if (sc == 'G' || sc == 'g') {
                    oss << std::defaultfloat << std::setprecision(prec > 0 ? prec : 6) << value;
                } else if (sc == 'E' || sc == 'e') {
                    if (sc == 'E') oss << std::uppercase;
                    oss << std::scientific << std::setprecision(prec) << value;
                } else {
                    oss << value;
                }
                return oss.str();
            } catch (const std::bad_alloc&) {
                throw System::OutOfMemoryException();
            } catch (const std::length_error&) {
                throw System::OutOfMemoryException();
            }
        }

        // Format a float as .NET does: through Single's own ToString, never through Double's.
        // The specifier tail is validated here first, so an oversized one is still the
        // FormatException ticket #1849 pinned rather than a 10^9-digit precision request.
        std::string fmtFloat(float value, std::string_view spec) {
            if (spec.empty()) return System::Single::ToString(value);
            const SpecNumber num = parseSpecNumber(spec);
            if (num.kind == SpecNumberKind::TooLarge) throwBadFormatSpecifier();
            return System::Single::ToString(value, std::string(spec));
        }

        std::string renderArg(const FormatArg& arg, std::string_view spec) {
            switch (arg.kind) {
                case FormatArg::Kind::Int:    return fmtInt(arg.i, spec);
                case FormatArg::Kind::Float:  return fmtFloat(arg.f, spec);
                case FormatArg::Kind::Double: return fmtDouble(arg.d, spec);
                case FormatArg::Kind::Long:   return std::to_string(arg.l);
                case FormatArg::Kind::Text:   return *arg.text;
                case FormatArg::Kind::TimeSpan:
                    return spec.empty() ? arg.timeSpan->ToString()
                                        : arg.timeSpan->ToString(std::string(spec));
            }
            return {};
        }

        // One left-to-right pass over @p format, driven by the shared .NET
        // composite-format grammar in System/detail/CompositeFormat.hpp. Argument
        // text is appended to the output and is never re-examined, so an argument
        // containing its own placeholder terminates and is emitted verbatim
        // (ticket #1882); the grammar the scan accepts became .NET's in #1884.
        std::string formatCore(const std::string& format, const FormatArg* args, std::size_t argCount) {
            return System::detail::runCompositeFormat(
                format, argCount,
                [args](std::size_t index, std::string_view spec) {
                    return renderArg(args[index], spec);
                });
        }
    }

    std::string String::Format(const std::string& format, SharpRuntime::intcs arg0)
    {
        const FormatArg args[] = {argOf(arg0)};
        return formatCore(format, args, 1);
    }

    std::string String::Format(const std::string& format, double arg0)
    {
        const FormatArg args[] = {argOf(arg0)};
        return formatCore(format, args, 1);
    }

    std::string String::Format(const std::string& format, const std::string& arg0)
    {
        const FormatArg args[] = {argOf(arg0)};
        return formatCore(format, args, 1);
    }

    std::string String::Format(const std::string& format, SharpRuntime::intcs arg0, SharpRuntime::intcs arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    std::string String::Format(const std::string& format, SharpRuntime::intcs arg0, const std::string& arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    std::string String::Format(const std::string& format, const std::string& arg0, SharpRuntime::intcs arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    std::string String::Format(const std::string& format, const std::string& arg0, const std::string& arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    std::string String::Format(const std::string& format, double arg0, double arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    std::string String::Format(const std::string& format, float arg0, float arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    std::string String::Format(const std::string& format, bool arg0)
    {
        return Format(format, arg0 ? std::string("True") : std::string("False"));
    }
    std::string String::Format(const std::string& format, char arg0)
    {
        return Format(format, std::string(1, arg0));
    }
    std::string String::ToString(SharpRuntime::intcs value, SharpRuntime::intcs width, char fill)
    {
        std::ostringstream oss;
        oss << std::setw(width) << std::setfill(fill) << value;
        return oss.str();
    }

    bool String::IsNullOrWhiteSpace(const std::string& value)
    {
        for (char c : value)
            if (!std::isspace(static_cast<unsigned char>(c))) return false;
        return true;
    }

    bool String::EndsWith(const std::string& value, const std::string& suffix)
    {
        if (suffix.size() > value.size()) return false;
        return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    bool String::Contains(const std::string& value, const std::string& substr)
    {
        return value.find(substr) != std::string::npos;
    }

    std::string String::Replace(const std::string& value, const std::string& oldValue, const std::string& newValue)
    {
        if (oldValue.empty()) return value;
        std::string result;
        result.reserve(value.size());
        size_t pos = 0, found;
        while ((found = value.find(oldValue, pos)) != std::string::npos)
        {
            result.append(value, pos, found - pos);
            result.append(newValue);
            pos = found + oldValue.size();
        }
        result.append(value, pos, std::string::npos);
        return result;
    }

    std::string String::Replace(const std::string& value, char oldChar, char newChar)
    {
        std::string result = value;
        for (char& c : result)
            if (c == oldChar) c = newChar;
        return result;
    }

    std::string String::Substring(const std::string& value, SharpRuntime::intcs startIndex)
    {
        if (startIndex < 0 || static_cast<size_t>(startIndex) > value.size())
            throw System::ArgumentOutOfRangeException("startIndex", "String::Substring: startIndex must be within the bounds of the string.");
        return value.substr(static_cast<size_t>(startIndex));
    }

    std::string String::Substring(const std::string& value, SharpRuntime::intcs startIndex, SharpRuntime::intcs length)
    {
        if (startIndex < 0 || length < 0 ||
            static_cast<size_t>(startIndex) + static_cast<size_t>(length) > value.size())
            throw System::ArgumentOutOfRangeException("startIndex", "String::Substring: startIndex and length must refer to a location within the string.");
        return value.substr(static_cast<size_t>(startIndex), static_cast<size_t>(length));
    }

    std::string String::Trim(const std::string& value)
    {
        auto first = value.find_first_not_of(" \t\n\r\f\v");
        if (first == std::string::npos) return {};
        auto last = value.find_last_not_of(" \t\n\r\f\v");
        return value.substr(first, last - first + 1);
    }

    std::string String::TrimStart(const std::string& value)
    {
        auto first = value.find_first_not_of(" \t\n\r\f\v");
        return first == std::string::npos ? std::string{} : value.substr(first);
    }

    std::string String::TrimEnd(const std::string& value)
    {
        auto last = value.find_last_not_of(" \t\n\r\f\v");
        return last == std::string::npos ? std::string{} : value.substr(0, last + 1);
    }

    std::string String::Concat(const std::string& a, const std::string& b)
    {
        return a + b;
    }

    std::string String::Concat(const std::string& a, const std::string& b, const std::string& c)
    {
        return a + b + c;
    }

    std::string String::Concat(const std::string& a, const std::string& b, const std::string& c, const std::string& d)
    {
        return a + b + c + d;
    }

    std::string String::Concat(const std::vector<std::string>& values)
    {
        // Pre-computing the total size and reserving once avoids the handful of geometric
        // reallocations std::string's amortized-growth += would otherwise perform -- measured
        // ~1.4x faster for a realistic multi-item join/concat (see Join's identical rationale
        // below; the two functions share the same append-loop shape).
        std::size_t total = 0;
        for (const auto& s : values) total += s.size();
        std::string result;
        result.reserve(total);
        for (const auto& s : values) result += s;
        return result;
    }

    std::string String::Join(const std::string& separator, const std::vector<std::string>& values)
    {
        // See Concat(vector<string>)'s comment above -- same upfront-reserve rationale, measured
        // ~1.4x faster for a realistic 50-item join.
        std::size_t total = 0;
        for (const auto& s : values) total += s.size();
        if (!values.empty()) total += separator.size() * (values.size() - 1);
        std::string result;
        result.reserve(total);
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0) result += separator;
            result += values[i];
        }
        return result;
    }

    std::string String::ToUpper(const std::string& value)
    {
        std::string result = value;
        for (char& c : result) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return result;
    }

    std::string String::ToLower(const std::string& value)
    {
        std::string result = value;
        for (char& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return result;
    }

    SharpRuntime::intcs String::IndexOf(const std::string& value, const std::string& substr)
    {
        auto pos = value.find(substr);
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    SharpRuntime::intcs String::IndexOf(const std::string& value, char ch)
    {
        auto pos = value.find(ch);
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    SharpRuntime::intcs String::LastIndexOf(const std::string& value, const std::string& substr)
    {
        auto pos = value.rfind(substr);
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    SharpRuntime::intcs String::LastIndexOf(const std::string& value, char ch)
    {
        auto pos = value.rfind(ch);
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    std::string String::PadLeft(const std::string& value, SharpRuntime::intcs totalWidth)
    {
        return PadLeft(value, totalWidth, ' ');
    }

    std::string String::PadLeft(const std::string& value, SharpRuntime::intcs totalWidth, char paddingChar)
    {
        if (totalWidth < 0)
            throw System::ArgumentOutOfRangeException("totalWidth", "String::PadLeft: totalWidth must not be negative.");
        if (static_cast<SharpRuntime::intcs>(value.size()) >= totalWidth) return value;
        return std::string(static_cast<size_t>(totalWidth) - value.size(), paddingChar) + value;
    }

    std::string String::PadRight(const std::string& value, SharpRuntime::intcs totalWidth)
    {
        return PadRight(value, totalWidth, ' ');
    }

    std::string String::PadRight(const std::string& value, SharpRuntime::intcs totalWidth, char paddingChar)
    {
        if (totalWidth < 0)
            throw System::ArgumentOutOfRangeException("totalWidth", "String::PadRight: totalWidth must not be negative.");
        if (static_cast<SharpRuntime::intcs>(value.size()) >= totalWidth) return value;
        return value + std::string(static_cast<size_t>(totalWidth) - value.size(), paddingChar);
    }

    SharpRuntime::intcs String::IndexOfAny(const std::string& value, const std::vector<char>& anyOf)
    {
        for (SharpRuntime::intcs i = 0; i < static_cast<SharpRuntime::intcs>(value.size()); ++i)
            for (char c : anyOf)
                if (value[static_cast<size_t>(i)] == c) return i;
        return -1;
    }

    SharpRuntime::intcs String::LastIndexOfAny(const std::string& value, const std::vector<char>& anyOf)
    {
        for (SharpRuntime::intcs i = static_cast<SharpRuntime::intcs>(value.size()) - 1; i >= 0; --i)
            for (char c : anyOf)
                if (value[static_cast<size_t>(i)] == c) return i;
        return -1;
    }

    bool String::Contains(const std::string& value, char ch)
    {
        return value.find(ch) != std::string::npos;
    }

    SharpRuntime::intcs String::IndexOf(const std::string& value, const std::string& substr, SharpRuntime::intcs startIndex)
    {
        auto len = static_cast<SharpRuntime::intcs>(value.size());
        if (startIndex < 0 || startIndex > len)
            throw System::ArgumentOutOfRangeException("startIndex", "String::IndexOf: startIndex must be within the bounds of the string.");
        auto pos = value.find(substr, static_cast<size_t>(startIndex));
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    SharpRuntime::intcs String::IndexOf(const std::string& value, char ch, SharpRuntime::intcs startIndex)
    {
        auto len = static_cast<SharpRuntime::intcs>(value.size());
        if (startIndex < 0 || startIndex > len)
            throw System::ArgumentOutOfRangeException("startIndex", "String::IndexOf: startIndex must be within the bounds of the string.");
        auto pos = value.find(ch, static_cast<size_t>(startIndex));
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    SharpRuntime::intcs String::LastIndexOf(const std::string& value, const std::string& substr, SharpRuntime::intcs startIndex)
    {
        if (value.empty()) return substr.empty() ? 0 : -1;
        auto len = static_cast<SharpRuntime::intcs>(value.size());
        // Real .NET's CompareInfo.LastIndexOf explicitly special-cases startIndex == source.Length
        // as valid (a documented back-compat off-by-one fixup: "The caller likely had an off-by-one
        // error when invoking the API") by silently treating it as startIndex == length - 1, rather
        // than throwing -- a natural "search the whole string backward" idiom deserves to work, not
        // throw ArgumentOutOfRangeException.
        if (startIndex == len) startIndex = len - 1;
        if (startIndex < 0 || startIndex >= len)
            throw System::ArgumentOutOfRangeException("startIndex", "String::LastIndexOf: startIndex must be within the bounds of the string.");
        return lastIndexOfBounded(value, substr, startIndex, 0);
    }

    SharpRuntime::intcs String::LastIndexOf(const std::string& value, char ch, SharpRuntime::intcs startIndex)
    {
        if (value.empty()) return -1;
        auto len = static_cast<SharpRuntime::intcs>(value.size());
        // See the (string, string, int) overload above for why startIndex == length is special-cased
        // rather than rejected, matching real .NET's CompareInfo.LastIndexOf fixup.
        if (startIndex == len) startIndex = len - 1;
        if (startIndex < 0 || startIndex >= len)
            throw System::ArgumentOutOfRangeException("startIndex", "String::LastIndexOf: startIndex must be within the bounds of the string.");
        auto pos = value.rfind(ch, static_cast<size_t>(startIndex));
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    std::string String::Create(SharpRuntime::intcs count, char ch)
    {
        if (count < 0)
            throw System::ArgumentOutOfRangeException("count", "String::Create: count must not be negative.");
        return std::string(static_cast<size_t>(count), ch);
    }

    SharpRuntime::intcs String::Compare(const std::string& a, const std::string& b)
    {
        return static_cast<SharpRuntime::intcs>(a.compare(b));
    }

    SharpRuntime::intcs String::Compare(const std::string& a, const std::string& b, bool ignoreCase)
    {
        if (!ignoreCase) return Compare(a, b);
        std::string la = ToLower(a), lb = ToLower(b);
        return static_cast<SharpRuntime::intcs>(la.compare(lb));
    }

    bool String::Equals(const std::string& a, const std::string& b)
    {
        return a == b;
    }

    bool String::Equals(const std::string& a, const std::string& b, bool ignoreCase)
    {
        if (!ignoreCase) return a == b;
        return ToLower(a) == ToLower(b);
    }

    std::string String::Format(const std::string& format, SharpRuntime::intcs arg0, SharpRuntime::intcs arg1, SharpRuntime::intcs arg2)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1), argOf(arg2)};
        return formatCore(format, args, 3);
    }

    std::string String::Format(const std::string& format, const std::string& arg0, const std::string& arg1, const std::string& arg2)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1), argOf(arg2)};
        return formatCore(format, args, 3);
    }

    std::string String::Format(const std::string& format, SharpRuntime::longcs arg0)
    {
        return Format(format, std::to_string(arg0));
    }

    std::string String::Format(const std::string& format, const TimeSpan& arg0)
    {
        const FormatArg args[] = {argOf(arg0)};
        return formatCore(format, args, 1);
    }

    std::string String::Format(const std::string& format, double arg0, const std::string& arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    std::string String::Format(const std::string& format, const std::string& arg0, double arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    std::string String::Format(const std::string& format, SharpRuntime::longcs arg0, SharpRuntime::intcs arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    std::string String::Format(const std::string& format, SharpRuntime::intcs arg0, SharpRuntime::longcs arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    bool String::StartsWith(const std::string& value, char ch)
    {
        return !value.empty() && value.front() == ch;
    }

    bool String::EndsWith(const std::string& value, char ch)
    {
        return !value.empty() && value.back() == ch;
    }

    std::string String::Remove(const std::string& value, SharpRuntime::intcs startIndex)
    {
        if (startIndex < 0 || static_cast<size_t>(startIndex) > value.size())
            throw System::ArgumentOutOfRangeException("startIndex", "String::Remove: startIndex must be within the bounds of the string.");
        return value.substr(0, static_cast<size_t>(startIndex));
    }

    std::string String::Remove(const std::string& value, SharpRuntime::intcs startIndex, SharpRuntime::intcs count)
    {
        if (startIndex < 0 || count < 0 ||
            static_cast<size_t>(startIndex) + static_cast<size_t>(count) > value.size())
            throw System::ArgumentOutOfRangeException("startIndex", "String::Remove: startIndex and count must refer to a location within the string.");
        std::string result = value;
        result.erase(static_cast<size_t>(startIndex), static_cast<size_t>(count));
        return result;
    }

    std::string String::Insert(const std::string& value, SharpRuntime::intcs startIndex, const std::string& insertValue)
    {
        if (startIndex < 0 || static_cast<size_t>(startIndex) > value.size())
            throw System::ArgumentOutOfRangeException("startIndex", "String::Insert: startIndex must be within the bounds of the string.");
        std::string result = value;
        result.insert(static_cast<size_t>(startIndex), insertValue);
        return result;
    }

    std::string String::Join(const std::string& separator, const std::vector<SharpRuntime::intcs>& values)
    {
        std::string result;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) result += separator;
            result += std::to_string(values[i]);
        }
        return result;
    }

    std::string String::Join(const std::string& separator, const std::vector<double>& values)
    {
        std::string result;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) result += separator;
            result += std::to_string(values[i]);
        }
        return result;
    }

    std::vector<char> String::ToCharArray(const std::string& value)
    {
        return std::vector<char>(value.begin(), value.end());
    }

    std::string String::Trim(const std::string& value, const std::vector<char>& trimChars)
    {
        return TrimEnd(TrimStart(value, trimChars), trimChars);
    }

    std::string String::TrimStart(const std::string& value, const std::vector<char>& trimChars)
    {
        size_t start = 0;
        while (start < value.size()) {
            bool found = false;
            for (char c : trimChars) if (value[start] == c) { found = true; break; }
            if (!found) break;
            ++start;
        }
        return value.substr(start);
    }

    std::string String::TrimEnd(const std::string& value, const std::vector<char>& trimChars)
    {
        size_t end = value.size();
        while (end > 0) {
            bool found = false;
            for (char c : trimChars) if (value[end - 1] == c) { found = true; break; }
            if (!found) break;
            --end;
        }
        return value.substr(0, end);
    }

    static std::vector<std::string> applySplitOptions(std::vector<std::string> parts, StringSplitOptions options)
    {
        bool removeEmpty = (static_cast<int>(options) & static_cast<int>(StringSplitOptions::RemoveEmptyEntries)) != 0;
        bool trimEntries = (static_cast<int>(options) & static_cast<int>(StringSplitOptions::TrimEntries)) != 0;
        std::vector<std::string> result;
        for (auto& p : parts) {
            if (trimEntries) p = String::Trim(p);
            if (removeEmpty && p.empty()) continue;
            result.push_back(std::move(p));
        }
        return result;
    }

    std::vector<std::string> String::Split(const std::string& value, char delimiter, StringSplitOptions options)
    {
        return applySplitOptions(Split(value, delimiter), options);
    }

    std::vector<std::string> String::Split(const std::string& value, const std::string& delimiter, StringSplitOptions options)
    {
        return applySplitOptions(Split(value, delimiter), options);
    }

    std::string String::Format(const std::string& format, SharpRuntime::intcs arg0, double arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    std::string String::Format(const std::string& format, SharpRuntime::intcs arg0, float arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    std::string String::Format(const std::string& format, double arg0, SharpRuntime::intcs arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    const std::string String::Empty{};

    std::string String::Format(const std::string& format, float arg0)
    {
        const FormatArg args[] = {argOf(arg0)};
        return formatCore(format, args, 1);
    }

    std::string String::Format(const std::string& format, SharpRuntime::longcs arg0, SharpRuntime::longcs arg1)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1)};
        return formatCore(format, args, 2);
    }

    std::string String::Format(const std::string& format,
                               const std::string& arg0, const std::string& arg1,
                               const std::string& arg2, const std::string& arg3)
    {
        const FormatArg args[] = {argOf(arg0), argOf(arg1), argOf(arg2), argOf(arg3)};
        return formatCore(format, args, 4);
    }

    std::string String::ToUpperInvariant(const std::string& value)
    {
        return ToUpper(value); // ASCII subset is locale-invariant with std::toupper
    }

    std::string String::ToLowerInvariant(const std::string& value)
    {
        return ToLower(value);
    }

    SharpRuntime::intcs String::CompareOrdinal(const std::string& a, const std::string& b)
    {
        if (a < b) return -1;
        if (a > b) return  1;
        return 0;
    }

    SharpRuntime::intcs String::IndexOf(const std::string& value, char ch, SharpRuntime::intcs startIndex, SharpRuntime::intcs count)
    {
        auto len = static_cast<SharpRuntime::intcs>(value.size());
        if (startIndex < 0 || startIndex > len)
            throw System::ArgumentOutOfRangeException("startIndex", "String::IndexOf: startIndex must be within the bounds of the string.");
        if (count < 0 || count > len - startIndex)
            throw System::ArgumentOutOfRangeException("count", "String::IndexOf: count must refer to a location within the string.");
        SharpRuntime::intcs end = startIndex + count;
        for (SharpRuntime::intcs i = startIndex; i < end; ++i)
            if (value[static_cast<size_t>(i)] == ch) return i;
        return -1;
    }

    SharpRuntime::intcs String::IndexOf(const std::string& value, const std::string& substr, SharpRuntime::intcs startIndex, SharpRuntime::intcs count)
    {
        auto len = static_cast<SharpRuntime::intcs>(value.size());
        if (startIndex < 0 || startIndex > len)
            throw System::ArgumentOutOfRangeException("startIndex", "String::IndexOf: startIndex must be within the bounds of the string.");
        if (count < 0 || count > len - startIndex)
            throw System::ArgumentOutOfRangeException("count", "String::IndexOf: count must refer to a location within the string.");
        if (substr.empty()) return startIndex;
        SharpRuntime::intcs end = startIndex + count;
        auto pos = value.find(substr, static_cast<size_t>(startIndex));
        if (pos == std::string::npos) return -1;
        return static_cast<SharpRuntime::intcs>(pos) < end ? static_cast<SharpRuntime::intcs>(pos) : -1;
    }

    SharpRuntime::intcs String::LastIndexOf(const std::string& value, char ch, SharpRuntime::intcs startIndex, SharpRuntime::intcs count)
    {
        if (value.empty()) return -1;
        auto len = static_cast<SharpRuntime::intcs>(value.size());
        // Real .NET's CompareInfo.LastIndexOf off-by-one fixup: startIndex == length is treated as
        // startIndex == length - 1 with count reduced by one to match (not just startIndex alone) --
        // see the 3-arg overload above for the non-count-adjusted rationale.
        if (startIndex == len) { startIndex = len - 1; if (count > 0) count -= 1; }
        if (startIndex < 0 || startIndex >= len)
            throw System::ArgumentOutOfRangeException("startIndex", "String::LastIndexOf: startIndex must be within the bounds of the string.");
        if (count < 0 || count > startIndex + 1)
            throw System::ArgumentOutOfRangeException("count", "String::LastIndexOf: count must refer to a location within the string.");
        SharpRuntime::intcs begin = startIndex - count + 1;
        for (SharpRuntime::intcs i = startIndex; i >= begin && i >= 0; --i)
            if (value[static_cast<size_t>(i)] == ch) return i;
        return -1;
    }

    SharpRuntime::intcs String::LastIndexOf(const std::string& value, const std::string& substr, SharpRuntime::intcs startIndex, SharpRuntime::intcs count)
    {
        if (value.empty()) return substr.empty() ? 0 : -1;
        auto len = static_cast<SharpRuntime::intcs>(value.size());
        // See the (string, char, int, int) overload above for the off-by-one fixup rationale.
        if (startIndex == len) { startIndex = len - 1; if (count > 0) count -= 1; }
        if (startIndex < 0 || startIndex >= len)
            throw System::ArgumentOutOfRangeException("startIndex", "String::LastIndexOf: startIndex must be within the bounds of the string.");
        if (count < 0 || count > startIndex + 1)
            throw System::ArgumentOutOfRangeException("count", "String::LastIndexOf: count must refer to a location within the string.");
        if (substr.empty()) return startIndex;
        return lastIndexOfBounded(value, substr, startIndex, startIndex - count + 1);
    }

    std::vector<char> String::ToCharArray(const std::string& value, SharpRuntime::intcs startIndex, SharpRuntime::intcs length)
    {
        // startIndex+length (both intcs/int32) can itself signed-overflow for large inputs,
        // silently bypassing this check instead of throwing -- confirmed real UB via a
        // standalone UBSan repro on the identical pattern in Span<T>::Slice (ticket 265/1487);
        // fixed the same way real .NET's Span<T>.Slice(int,int) does: unsigned comparison,
        // subtraction instead of addition.
        auto sz = static_cast<SharpRuntime::intcs>(value.size());
        if (static_cast<SharpRuntime::uintcs>(startIndex) > static_cast<SharpRuntime::uintcs>(sz) ||
            static_cast<SharpRuntime::uintcs>(length) > static_cast<SharpRuntime::uintcs>(sz - startIndex))
            throw System::ArgumentOutOfRangeException("index", "String::ToCharArray: index out of range");
        return std::vector<char>(value.begin() + startIndex, value.begin() + startIndex + length);
    }

    SharpRuntime::intcs String::GetHashCode(const std::string& value) noexcept
    {
        // Widen to a fixed 64-bit type before shifting by 32 - std::size_t is only 32 bits
        // on some targets (e.g. Emscripten's wasm32), where "h >> 32" would shift by the
        // full width of the type.
        const auto h = static_cast<std::uint64_t>(std::hash<std::string>{}(value));
        return static_cast<SharpRuntime::intcs>(h ^ (h >> 32));
    }

    // -----------------------------------------------------------------------
    // StringComparison helpers
    // -----------------------------------------------------------------------

    static std::string sc_toLower(const std::string& s) {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(),
            [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        return r;
    }

    static bool sc_isIgnoreCase(StringComparison c) {
        return c == StringComparison::CurrentCultureIgnoreCase  ||
               c == StringComparison::InvariantCultureIgnoreCase ||
               c == StringComparison::OrdinalIgnoreCase;
    }

    SharpRuntime::intcs String::Compare(const std::string& a, const std::string& b, StringComparison comparisonType) {
        return Compare(a, b, sc_isIgnoreCase(comparisonType));
    }

    bool String::Equals(const std::string& a, const std::string& b, StringComparison comparisonType) {
        return Equals(a, b, sc_isIgnoreCase(comparisonType));
    }

    bool String::Contains(const std::string& value, const std::string& substr, StringComparison comparisonType) {
        if (sc_isIgnoreCase(comparisonType))
            return sc_toLower(value).find(sc_toLower(substr)) != std::string::npos;
        return value.find(substr) != std::string::npos;
    }

    bool String::StartsWith(const std::string& value, const std::string& prefix, StringComparison comparisonType) {
        if (sc_isIgnoreCase(comparisonType))
            return StartsWith(sc_toLower(value), sc_toLower(prefix));
        return StartsWith(value, prefix);
    }

    bool String::EndsWith(const std::string& value, const std::string& suffix, StringComparison comparisonType) {
        if (sc_isIgnoreCase(comparisonType))
            return EndsWith(sc_toLower(value), sc_toLower(suffix));
        return EndsWith(value, suffix);
    }

    SharpRuntime::intcs String::IndexOf(const std::string& value, const std::string& substr, StringComparison comparisonType) {
        if (sc_isIgnoreCase(comparisonType)) {
            auto pos = sc_toLower(value).find(sc_toLower(substr));
            return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
        }
        return IndexOf(value, substr);
    }

    SharpRuntime::intcs String::LastIndexOf(const std::string& value, const std::string& substr, StringComparison comparisonType) {
        if (sc_isIgnoreCase(comparisonType)) {
            auto pos = sc_toLower(value).rfind(sc_toLower(substr));
            return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
        }
        return LastIndexOf(value, substr);
    }

    std::string String::Join(char separator, const std::vector<std::string>& values) {
        return Join(std::string(1, separator), values);
    }

}
