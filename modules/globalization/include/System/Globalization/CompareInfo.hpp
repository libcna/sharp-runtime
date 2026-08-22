// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Globalization/CompareOptions.hpp"
#include "System/Globalization/SortKey.hpp"
#include "System/Globalization/detail/InvariantCase.hpp"
#include "System/NotSupportedException.hpp"

namespace System::Globalization {

using SharpRuntime::charcs;
using SharpRuntime::intcs;

/**
 * @brief Implements a set of methods for culture-sensitive string comparisons.
 *
 * C++ counterpart of .NET System.Globalization.CompareInfo.
 * Instances are obtained via the GetCompareInfo factory methods.
 * This practical subset provides deterministic ordinal/invariant comparison, including Unicode
 * simple-case folding for IgnoreCase and OrdinalIgnoreCase. The culture name is descriptive only:
 * ICU-style collation and the linguistic IgnoreNonSpace/IgnoreSymbols/IgnoreKanaType/IgnoreWidth,
 * NumericOrdering and StringSort options are not implemented and throw NotSupportedException
 * instead of silently producing a byte-comparison result.
 */
class CompareInfo {
public:
    /**
     * @brief Constructs a CompareInfo for the given culture name.
     *
     * C++ counterpart of .NET CompareInfo obtained via CultureInfo.CompareInfo.
     * @param name The culture name (e.g. "en-US"); defaults to "en-US".
     */
    explicit CompareInfo(const std::string& name = "en-US") : name_(name) {}

    /**
     * @brief Returns a CompareInfo for the given culture name.
     *
     * C++ counterpart of .NET CompareInfo.GetCompareInfo(string).
     * @param name The culture name.
     * @return A CompareInfo instance for @p name.
     */
    static CompareInfo GetCompareInfo(const std::string& name) { return CompareInfo(name); }

    /**
     * @brief Returns a CompareInfo for the given LCID.
     *
     * C++ counterpart of .NET CompareInfo.GetCompareInfo(int).
     * @param culture The locale identifier; ignored — returns the default "en-US" instance.
     * @return A default CompareInfo instance.
     */
    static CompareInfo GetCompareInfo(intcs /*culture*/) { return CompareInfo("en-US"); }

    /**
     * @brief Gets the culture name for this CompareInfo.
     *
     * C++ counterpart of .NET CompareInfo.Name.
     * @return The culture name string.
     */
    [[nodiscard]] const std::string& getNameProperty() const { return name_; }

    /**
     * @brief Gets the locale identifier (LCID) for this CompareInfo.
     *
     * C++ counterpart of .NET CompareInfo.LCID.
     * @return Always 0 in this stub implementation.
     */
    [[nodiscard]] intcs getLCIDProperty() const { return 0; }

    /**
     * @brief Compares two strings using the specified options.
     *
     * C++ counterpart of .NET CompareInfo.Compare(string, string, CompareOptions).
     * @param s1      The first string.
     * @param s2      The second string.
     * @param options Comparison options (default None).
     * @return Negative if s1 < s2, zero if equal, positive if s1 > s2.
     */
    intcs Compare(const std::string& s1, const std::string& s2,
                  CompareOptions options = CompareOptions::None) const {
        return validateOptions(options) ? compareIgnoreCase(s1, s2) : compareOrdinal(s1, s2);
    }

    /**
     * @brief Compares substrings of two strings using the specified options.
     *
     * C++ counterpart of .NET CompareInfo.Compare(string, int, int, string, int, int, CompareOptions).
     * @param s1      The first string.
     * @param off1    Starting offset in @p s1.
     * @param len1    Number of characters from @p s1.
     * @param s2      The second string.
     * @param off2    Starting offset in @p s2.
     * @param len2    Number of characters from @p s2.
     * @param options Comparison options (default None).
     * @return Negative, zero, or positive.
     */
    intcs Compare(const std::string& s1, intcs off1, intcs len1,
                  const std::string& s2, intcs off2, intcs len2,
                  CompareOptions options = CompareOptions::None) const {
        CheckRange(s1, off1, len1, "offset1");
        CheckRange(s2, off2, len2, "offset2");
        return Compare(s1.substr(static_cast<size_t>(off1), static_cast<size_t>(len1)),
                       s2.substr(static_cast<size_t>(off2), static_cast<size_t>(len2)), options);
    }

    /**
     * @brief Determines whether @p source starts with @p prefix using the specified options.
     *
     * C++ counterpart of .NET CompareInfo.IsPrefix(string, string, CompareOptions).
     * @param source  The string to search.
     * @param prefix  The prefix to test.
     * @param options Comparison options (default None).
     * @return true if @p source starts with @p prefix.
     */
    [[nodiscard]] bool IsPrefix(const std::string& source, const std::string& prefix,
                                CompareOptions options = CompareOptions::None) const {
        const bool ignoreCase = validateOptions(options);
        if (!ignoreCase) {
            return prefix.size() <= source.size() && source.compare(0, prefix.size(), prefix) == 0;
        }
        const auto sourceFolded = detail::FoldUtf8OrdinalIgnoreCase(source);
        const auto prefixFolded = detail::FoldUtf8OrdinalIgnoreCase(prefix);
        return prefixFolded.scalars.size() <= sourceFolded.scalars.size() &&
               foldedSubsequenceAt(sourceFolded.scalars, 0, prefixFolded.scalars);
    }

    /**
     * @brief Determines whether @p source ends with @p suffix using the specified options.
     *
     * C++ counterpart of .NET CompareInfo.IsSuffix(string, string, CompareOptions).
     * @param source  The string to search.
     * @param suffix  The suffix to test.
     * @param options Comparison options (default None).
     * @return true if @p source ends with @p suffix.
     */
    [[nodiscard]] bool IsSuffix(const std::string& source, const std::string& suffix,
                                CompareOptions options = CompareOptions::None) const {
        const bool ignoreCase = validateOptions(options);
        if (!ignoreCase) {
            return suffix.size() <= source.size() &&
                   source.compare(source.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        const auto sourceFolded = detail::FoldUtf8OrdinalIgnoreCase(source);
        const auto suffixFolded = detail::FoldUtf8OrdinalIgnoreCase(suffix);
        if (suffixFolded.scalars.size() > sourceFolded.scalars.size()) return false;
        return foldedSubsequenceAt(sourceFolded.scalars,
                                   sourceFolded.scalars.size() - suffixFolded.scalars.size(),
                                   suffixFolded.scalars);
    }

    /**
     * @brief Searches for @p value in @p source and returns its zero-based index, or -1.
     *
     * C++ counterpart of .NET CompareInfo.IndexOf(string, string, CompareOptions).
     * @param source  The string to search within.
     * @param value   The string to locate.
     * @param options Comparison options (default None).
     * @return The zero-based index of the first occurrence, or -1 if not found.
     */
    [[nodiscard]] intcs IndexOf(const std::string& source, const std::string& value,
                                CompareOptions options = CompareOptions::None) const {
        if (!validateOptions(options)) {
            const auto pos = source.find(value);
            return pos == std::string::npos ? -1 : static_cast<intcs>(pos);
        }
        if (value.empty()) return 0;
        const auto sourceFolded = detail::FoldUtf8OrdinalIgnoreCase(source);
        const auto valueFolded = detail::FoldUtf8OrdinalIgnoreCase(value);
        for (std::size_t i = 0; i + valueFolded.scalars.size() <= sourceFolded.scalars.size(); ++i) {
            if (foldedSubsequenceAt(sourceFolded.scalars, i, valueFolded.scalars)) {
                return static_cast<intcs>(sourceFolded.byteOffsets[i]);
            }
        }
        return -1;
    }

    /**
     * @brief Searches for the last occurrence of @p value in @p source, or -1.
     *
     * C++ counterpart of .NET CompareInfo.LastIndexOf(string, string, CompareOptions).
     * @param source  The string to search within.
     * @param value   The string to locate.
     * @param options Comparison options (default None).
     * @return The zero-based index of the last occurrence, or -1 if not found.
     */
    [[nodiscard]] intcs LastIndexOf(const std::string& source, const std::string& value,
                                    CompareOptions options = CompareOptions::None) const {
        if (!validateOptions(options)) {
            const auto pos = source.rfind(value);
            return pos == std::string::npos ? -1 : static_cast<intcs>(pos);
        }
        if (value.empty()) return static_cast<intcs>(source.size());
        const auto sourceFolded = detail::FoldUtf8OrdinalIgnoreCase(source);
        const auto valueFolded = detail::FoldUtf8OrdinalIgnoreCase(value);
        if (valueFolded.scalars.size() > sourceFolded.scalars.size()) return -1;
        std::size_t i = sourceFolded.scalars.size() - valueFolded.scalars.size();
        while (true) {
            if (foldedSubsequenceAt(sourceFolded.scalars, i, valueFolded.scalars)) {
                return static_cast<intcs>(sourceFolded.byteOffsets[i]);
            }
            if (i == 0) break;
            --i;
        }
        return -1;
    }

    /**
     * @brief Determines whether a character can be sorted.
     *
     * C++ counterpart of .NET CompareInfo.IsSortable(char).
     * @return Always true in this implementation.
     */
    static bool IsSortable(charcs /*ch*/) { return true; }

    /**
     * @brief Determines whether a string can be sorted.
     *
     * C++ counterpart of .NET CompareInfo.IsSortable(string).
     * @return Always true in this implementation.
     */
    static bool IsSortable(const std::string& /*text*/) { return true; }

    /**
     * @brief Gets the SortKey for a string using the specified comparison options.
     *
     * C++ counterpart of .NET CompareInfo.GetSortKey(string, CompareOptions).
     * In the supported invariant subset, a case-sensitive key contains the original UTF-8 bytes
     * and an ignore-case key contains fixed-width folded scalars. Consequently strings equal under
     * the corresponding Compare operation produce equal keys.
     * @param source  The string to create a sort key for.
     * @param options Comparison options (default None).
     * @return A SortKey object for @p source.
     * @throws System::ArgumentException if @p options contains Ordinal or OrdinalIgnoreCase.
     */
    [[nodiscard]] SortKey GetSortKey(const std::string& source,
                                     CompareOptions options = CompareOptions::None) const {
        const bool ignoreCase = validateSortKeyOptions(options);
        std::vector<bytecs> key;
        if (ignoreCase) {
            key = foldedKey(detail::FoldUtf8OrdinalIgnoreCase(source).scalars);
        } else {
            key.assign(source.begin(), source.end());
        }
        return SortKey(source, key);
    }

    /**
     * @brief Gets a hash code for the given string under the specified comparison options.
     *
     * C++ counterpart of .NET CompareInfo.GetHashCode(string, CompareOptions).
     * When @p options includes IgnoreCase, the hash is computed from the case-folded
     * string so that strings equal under IgnoreCase comparison also hash equal.
     * @param source  The source string.
     * @param options Comparison options.
     * @return A hash code derived from the string value.
     */
    [[nodiscard]] intcs GetHashCode(const std::string& source, CompareOptions options) const {
        if (!validateOptions(options)) {
            return static_cast<intcs>(std::hash<std::string>{}(source));
        }
        const auto key = foldedKey(detail::FoldUtf8OrdinalIgnoreCase(source).scalars);
        const std::string hashSource(key.begin(), key.end());
        return static_cast<intcs>(std::hash<std::string>{}(hashSource));
    }

    /**
     * @brief Returns true if both CompareInfo instances have the same culture name.
     *
     * C++ counterpart of .NET CompareInfo.Equals(object).
     * @param other The CompareInfo to compare.
     * @return true if the culture names are equal.
     */
    bool operator==(const CompareInfo& other) const { return name_ == other.name_; }

    /**
     * @brief Returns a string representation of this CompareInfo.
     *
     * C++ counterpart of .NET CompareInfo.ToString().
     * @return A string in the form "CompareInfo - <name>".
     */
    [[nodiscard]] std::string ToString() const { return "CompareInfo - " + name_; }

private:
    std::string name_;

    static void CheckRange(const std::string& s, intcs offset, intcs length, const char* paramName) {
        if (offset < 0) throw System::ArgumentOutOfRangeException(paramName);
        if (length < 0) throw System::ArgumentOutOfRangeException("length");
        if (static_cast<size_t>(offset) > s.size() ||
            static_cast<size_t>(length) > s.size() - static_cast<size_t>(offset)) {
            throw System::ArgumentOutOfRangeException(paramName);
        }
    }

    /**
     * Validates the shared comparison-option policy and returns whether it folds case.
     * Ordinal flags are valid only alone, matching .NET. Linguistic options are known public enum
     * values but outside this no-ICU subset, so they fail explicitly rather than being ignored.
     * GetSortKey applies its narrower per-door mask before entering this policy.
     */
    static bool validateOptions(CompareOptions options) {
        constexpr std::uint32_t IgnoreCase = 0x00000001u;
        constexpr std::uint32_t LinguisticUnsupported = 0x2000003Eu;
        constexpr std::uint32_t OrdinalIgnoreCase = 0x10000000u;
        constexpr std::uint32_t Ordinal = 0x40000000u;
        constexpr std::uint32_t Known = IgnoreCase | LinguisticUnsupported |
                                        OrdinalIgnoreCase | Ordinal;
        const auto raw = static_cast<std::uint32_t>(static_cast<int>(options));
        if ((raw & ~Known) != 0) {
            throw System::ArgumentException("The CompareOptions value contains an unknown flag.",
                                            "options");
        }
        if ((raw & (Ordinal | OrdinalIgnoreCase)) != 0) {
            if (raw == Ordinal) return false;
            if (raw == OrdinalIgnoreCase) return true;
            throw System::ArgumentException(
                "Ordinal and OrdinalIgnoreCase may only be used by themselves.", "options");
        }
        if ((raw & LinguisticUnsupported) != 0) {
            throw System::NotSupportedException(
                "This practical subset has no ICU collation data for the requested CompareOptions.");
        }
        return (raw & IgnoreCase) != 0;
    }

    /**
     * GetSortKey uses .NET's narrower ValidCompareMaskOffFlags contract: the linguistic flags
     * belong to that door (and are rejected below only because this subset has no collation
     * database), but neither ordinal mode is a valid SortKey option.
     */
    static bool validateSortKeyOptions(CompareOptions options) {
        constexpr std::uint32_t OrdinalModes = 0x50000000u;
        const auto raw = static_cast<std::uint32_t>(static_cast<int>(options));
        if ((raw & OrdinalModes) != 0) {
            throw System::ArgumentException(
                "Ordinal and OrdinalIgnoreCase are not valid GetSortKey options.", "options");
        }
        return validateOptions(options);
    }

    static intcs compareOrdinal(const std::string& a, const std::string& b) {
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }

    static intcs compareIgnoreCase(const std::string& a, const std::string& b) {
        const auto aFolded = detail::FoldUtf8OrdinalIgnoreCase(a).scalars;
        const auto bFolded = detail::FoldUtf8OrdinalIgnoreCase(b).scalars;
        const std::size_t count = std::min(aFolded.size(), bFolded.size());
        for (std::size_t i = 0; i < count; ++i) {
            if (aFolded[i] < bFolded[i]) return -1;
            if (aFolded[i] > bFolded[i]) return 1;
        }
        if (aFolded.size() < bFolded.size()) return -1;
        if (aFolded.size() > bFolded.size()) return 1;
        return 0;
    }

    static bool foldedSubsequenceAt(const std::vector<std::uint32_t>& source,
                                    std::size_t offset,
                                    const std::vector<std::uint32_t>& value) {
        if (offset > source.size() || value.size() > source.size() - offset) return false;
        return std::equal(value.begin(), value.end(), source.begin() + static_cast<std::ptrdiff_t>(offset));
    }

    static std::vector<bytecs> foldedKey(const std::vector<std::uint32_t>& scalars) {
        std::vector<bytecs> key;
        key.reserve(scalars.size() * 4);
        for (std::uint32_t scalar : scalars) {
            key.push_back(static_cast<bytecs>((scalar >> 24) & 0xFFu));
            key.push_back(static_cast<bytecs>((scalar >> 16) & 0xFFu));
            key.push_back(static_cast<bytecs>((scalar >> 8) & 0xFFu));
            key.push_back(static_cast<bytecs>(scalar & 0xFFu));
        }
        return key;
    }
};

} // namespace System::Globalization
