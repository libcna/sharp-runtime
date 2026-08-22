// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Globalization/detail/InvariantCase.hpp"
#include "System/InvalidOperationException.hpp"

namespace System::Globalization {

using SharpRuntime::charcs;
using SharpRuntime::intcs;

/**
 * @brief Defines text properties and behaviors, such as casing, that are specific to a writing system.
 *
 * C++ counterpart of .NET System.Globalization.TextInfo.
 * This practical subset performs deterministic invariant Unicode simple casing using the pinned
 * UCD 16.0 tables. The retained culture name does not select culture-tailored casing: Turkish I,
 * context-sensitive sigma and multi-scalar expansions remain outside the supported contract.
 * ToTitleCase uses the same invariant mappings and Unicode category boundaries.
 */
class TextInfo {
public:
    /**
     * @brief Constructs a TextInfo for the given culture name.
     *
     * C++ counterpart of .NET CultureInfo.TextInfo.
     * @param cultureName The culture name (e.g. "en-US"); defaults to "en-US".
     */
    explicit TextInfo(const std::string& cultureName = "en-US")
        : cultureName_(cultureName) {}

    /**
     * @brief Gets the culture name associated with this TextInfo.
     *
     * C++ counterpart of .NET TextInfo.CultureName.
     * @return The culture name string.
     */
    [[nodiscard]] const std::string& getCultureNameProperty() const { return cultureName_; }

    /**
     * @brief Gets a value indicating whether this TextInfo is read-only.
     *
     * C++ counterpart of .NET TextInfo.IsReadOnly.
     * @return true if this instance is read-only; otherwise false.
     */
    [[nodiscard]] bool getIsReadOnlyProperty() const { return isReadOnly_; }

    /**
     * @brief Gets a value indicating whether the writing system is right-to-left.
     *
     * C++ counterpart of .NET TextInfo.IsRightToLeft.
     * Stub — always returns false.
     * @return Always false.
     */
    [[nodiscard]] bool getIsRightToLeftProperty() const { return false; }

    /**
     * @brief Gets the ANSI code page for this writing system.
     *
     * C++ counterpart of .NET TextInfo.ANSICodePage.
     * Stub — always returns 0.
     * @return Always 0.
     */
    [[nodiscard]] intcs getANSICodePageProperty() const { return 0; }

    /**
     * @brief Gets the EBCDIC code page for this writing system.
     *
     * C++ counterpart of .NET TextInfo.EBCDICCodePage.
     * Stub — always returns 0.
     * @return Always 0.
     */
    [[nodiscard]] intcs getEBCDICCodePageProperty() const { return 0; }

    /**
     * @brief Gets the culture locale identifier.
     *
     * C++ counterpart of .NET TextInfo.LCID.
     * Stub — always returns 0.
     * @return Always 0.
     */
    [[nodiscard]] intcs getLCIDProperty() const { return 0; }

    /**
     * @brief Gets the Macintosh code page for this writing system.
     *
     * C++ counterpart of .NET TextInfo.MacCodePage.
     * Stub — always returns 0.
     * @return Always 0.
     */
    [[nodiscard]] intcs getMacCodePageProperty() const { return 0; }

    /**
     * @brief Gets the OEM code page for this writing system.
     *
     * C++ counterpart of .NET TextInfo.OEMCodePage.
     * Stub — always returns 0.
     * @return Always 0.
     */
    [[nodiscard]] intcs getOEMCodePageProperty() const { return 0; }

    /**
     * @brief Gets the list separator for this writing system.
     *
     * C++ counterpart of .NET TextInfo.ListSeparator.
     * @return The list separator string (default ",").
     */
    [[nodiscard]] std::string getListSeparatorProperty() const { return listSeparator_; }

    /**
     * @brief Sets the list separator for this writing system.
     *
     * C++ counterpart of .NET TextInfo.ListSeparator setter.
     * @param value The new list separator string.
     * @throws System::InvalidOperationException if this instance is read-only.
     */
    void setListSeparatorProperty(const std::string& value) {
        VerifyWritable();
        listSeparator_ = value;
    }

    /**
     * @brief Converts a UTF-16 character to its lowercase equivalent.
     *
     * C++ counterpart of .NET TextInfo.ToLower(char).
     * @param c The character to convert.
     * @return The lowercase equivalent.
     */
    [[nodiscard]] charcs ToLower(charcs c) const {
        const auto codePoint = static_cast<std::uint32_t>(c);
        if (codePoint >= 0xD800u && codePoint <= 0xDFFFu) return c;
        return static_cast<charcs>(detail::LookupToLowerInvariant(codePoint));
    }

    /**
     * @brief Converts a UTF-16 string to lowercase.
     *
     * C++ counterpart of .NET TextInfo.ToLower(string) (UTF-16 variant).
     * @param str The string to convert.
     * @return The lowercase string.
     */
    [[nodiscard]] std::u16string ToLower(const std::u16string& str) const {
        return detail::MapUtf16Invariant(str, detail::InvariantCaseMapping::Lower);
    }

    /**
     * @brief Converts a UTF-8 string to lowercase.
     *
     * C++ counterpart of .NET TextInfo.ToLower(string).
     * @param str The string to convert.
     * @return The lowercase string.
     */
    [[nodiscard]] std::string ToLower(const std::string& str) const {
        return detail::MapUtf8Invariant(str, detail::InvariantCaseMapping::Lower);
    }

    /**
     * @brief Converts a UTF-16 character to its uppercase equivalent.
     *
     * C++ counterpart of .NET TextInfo.ToUpper(char).
     * @param c The character to convert.
     * @return The uppercase equivalent.
     */
    [[nodiscard]] charcs ToUpper(charcs c) const {
        const auto codePoint = static_cast<std::uint32_t>(c);
        if (codePoint >= 0xD800u && codePoint <= 0xDFFFu) return c;
        return static_cast<charcs>(detail::LookupToUpperInvariant(codePoint));
    }

    /**
     * @brief Converts a UTF-16 string to uppercase.
     *
     * C++ counterpart of .NET TextInfo.ToUpper(string) (UTF-16 variant).
     * @param str The string to convert.
     * @return The uppercase string.
     */
    [[nodiscard]] std::u16string ToUpper(const std::u16string& str) const {
        return detail::MapUtf16Invariant(str, detail::InvariantCaseMapping::Upper);
    }

    /**
     * @brief Converts a UTF-8 string to uppercase.
     *
     * C++ counterpart of .NET TextInfo.ToUpper(string).
     * @param str The string to convert.
     * @return The uppercase string.
     */
    [[nodiscard]] std::string ToUpper(const std::string& str) const {
        return detail::MapUtf8Invariant(str, detail::InvariantCaseMapping::Upper);
    }

    /**
     * @brief Converts the specified string to title case.
     *
     * C++ counterpart of .NET TextInfo.ToTitleCase(string).
     * Each word starts with an invariant simple-uppercase scalar; remaining scalars are lowercased --
     * EXCEPT a word that is entirely uppercase (no lowercase letters at all, e.g. "USA",
     * "NASA") is left unchanged, matching .NET's real behavior of preserving acronyms
     * (TextInfo.cs's `hasLowerCase` flag, "in line with Word 2000 behavior of
     * titlecasing"). A word whose first letter is lowercase is always normally
     * title-cased even if the rest happens to be uppercase (e.g. "uSA" -> "Usa"), since
     * .NET's hasLowerCase check covers the first letter too.
     * @param str The string to convert.
     * @return The title-cased string.
     */
    [[nodiscard]] std::string ToTitleCase(const std::string& str) const {
        struct Token {
            std::uint32_t codePoint;
            char rawByte;
            bool valid;
        };
        std::vector<Token> tokens;
        tokens.reserve(str.size());
        for (std::size_t offset = 0; offset < str.size();) {
            std::uint32_t codePoint = 0;
            std::size_t length = 0;
            if (System::detail::TryDecodeUtf8Scalar(str, offset, codePoint, length)) {
                tokens.push_back({codePoint, 0, true});
                offset += length;
            } else {
                tokens.push_back({0, str[offset], false});
                ++offset;
            }
        }

        const auto categoryOf = [](const Token& token) {
            return detail::LookupUnicodeCategory(token.codePoint);
        };
        const auto isLetter = [&](const Token& token) {
            return token.valid && categoryOf(token) <= UnicodeCategory::OtherLetter;
        };
        const auto isWordBoundary = [&](const Token& token) {
            if (!token.valid) return true;
            const auto category = categoryOf(token);
            return (category >= UnicodeCategory::SpaceSeparator &&
                    category <= UnicodeCategory::Format) ||
                   (category >= UnicodeCategory::ConnectorPunctuation &&
                    category <= UnicodeCategory::OtherSymbol);
        };

        std::string result;
        result.reserve(str.size());
        const auto appendOriginal = [&](const Token& token) {
            if (token.valid) System::detail::AppendUtf8Scalar(result, token.codePoint);
            else result.push_back(token.rawByte);
        };
        const auto appendRange = [&](std::size_t begin, std::size_t end, bool lower) {
            for (std::size_t j = begin; j < end; ++j) {
                if (!tokens[j].valid) {
                    result.push_back(tokens[j].rawByte);
                    continue;
                }
                const std::uint32_t mapped = lower
                    ? detail::LookupToLowerInvariant(tokens[j].codePoint)
                    : tokens[j].codePoint;
                System::detail::AppendUtf8Scalar(result, mapped);
            }
        };

        std::size_t i = 0;
        while (i < tokens.size()) {
            // .NET starts a title-cased word at a letter, not merely at the first scalar after a
            // separator. Thus digits and combining marks are copied until a later letter starts.
            if (!isLetter(tokens[i])) {
                appendOriginal(tokens[i]);
                ++i;
                continue;
            }

            const auto firstCategory = categoryOf(tokens[i]);
            System::detail::AppendUtf8Scalar(
                result, detail::LookupToUpperInvariant(tokens[i].codePoint));
            bool hasLower = firstCategory == UnicodeCategory::LowercaseLetter;
            ++i;
            std::size_t lowercaseStart = i;

            while (i < tokens.size()) {
                if (isLetter(tokens[i])) {
                    if (categoryOf(tokens[i]) == UnicodeCategory::LowercaseLetter) {
                        hasLower = true;
                    }
                    ++i;
                    continue;
                }

                // .NET flushes the prefix at an apostrophe and always lowercases the remainder.
                // That preserves an acronym before it ("USA'S" -> "USA's") while handling an
                // uppercase surname tail ("O'BRIEN" -> "O'brien").
                if (tokens[i].valid && tokens[i].codePoint == U'\'') {
                    appendRange(lowercaseStart, i + 1, hasLower);
                    ++i;
                    lowercaseStart = i;
                    hasLower = true;
                    continue;
                }

                if (isWordBoundary(tokens[i])) break;
                ++i;
            }

            appendRange(lowercaseStart, i, hasLower);
        }
        return result;
    }

    /**
     * @brief Returns a mutable copy of this TextInfo.
     *
     * C++ counterpart of .NET TextInfo.Clone().
     * @return A modifiable copy.
     */
    [[nodiscard]] TextInfo Clone() const {
        TextInfo copy = *this;
        copy.isReadOnly_ = false;
        return copy;
    }

    /**
     * @brief Returns a read-only copy of the given TextInfo.
     *
     * C++ counterpart of .NET TextInfo.ReadOnly(TextInfo).
     * @param textInfo The source instance.
     * @return A read-only copy.
     */
    static TextInfo ReadOnly(const TextInfo& textInfo) {
        TextInfo copy = textInfo;
        copy.isReadOnly_ = true;
        return copy;
    }

    /**
     * @brief Returns true if both TextInfo instances have the same culture name.
     *
     * C++ counterpart of .NET TextInfo.Equals(object).
     * @param other The TextInfo to compare.
     * @return true if the culture names are equal.
     */
    bool operator==(const TextInfo& other) const { return cultureName_ == other.cultureName_; }

    /**
     * @brief Returns a string representation of this TextInfo.
     *
     * C++ counterpart of .NET TextInfo.ToString().
     * @return A string in the form "TextInfo - <cultureName>".
     */
    [[nodiscard]] std::string ToString() const { return "TextInfo - " + cultureName_; }

private:
    std::string cultureName_;
    std::string listSeparator_{"," };
    bool isReadOnly_{false};

    void VerifyWritable() const {
        if (isReadOnly_) throw System::InvalidOperationException("Instance is read-only.");
    }
};

} // namespace System::Globalization
