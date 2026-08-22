// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Globalization/TextElementEnumerator.hpp"

namespace System::Globalization {

using SharpRuntime::intcs;

/**
 * @brief Provides iteration over and retrieval of text elements in a string.
 *
 * C++ counterpart of .NET System.Globalization.StringInfo.
 * In this practical subset, a text element is one Unicode scalar encoded as UTF-8 rather than a
 * full extended grapheme cluster. Combining sequences, emoji ZWJ sequences and regional-indicator
 * pairs therefore remain separate elements. Every entry point shares the same scalar boundary
 * rule with TextElementEnumerator: a valid multi-byte scalar is never split, while each malformed
 * input byte is a deterministic one-byte element.
 */
class StringInfo {
    std::string string_;

    /** Byte offsets of every scalar-based element, followed by the string-end sentinel. */
    static std::vector<std::size_t> elementOffsets(const std::string& str) {
        std::vector<std::size_t> offsets;
        for (std::size_t i = 0; i < str.size(); i += detail::Utf8TextElementLength(str, i)) {
            offsets.push_back(i);
        }
        offsets.push_back(str.size());
        return offsets;
    }

public:
    /**
     * @brief Constructs an empty StringInfo.
     *
     * C++ counterpart of .NET StringInfo().
     */
    StringInfo() = default;

    /**
     * @brief Constructs a StringInfo wrapping the given string.
     *
     * C++ counterpart of .NET StringInfo(string).
     * @param str The string to wrap.
     */
    explicit StringInfo(const std::string& str) : string_(str) {}

    /**
     * @brief Gets the underlying string.
     *
     * C++ counterpart of .NET StringInfo.String.
     * @return A const reference to the wrapped string.
     */
    [[nodiscard]] const std::string& getStringProperty() const { return string_; }

    /**
     * @brief Sets the underlying string.
     *
     * C++ counterpart of .NET StringInfo.String setter.
     * @param v The new string value.
     */
    void setStringProperty(const std::string& v) { string_ = v; }

    /**
     * @brief Gets the number of text elements in the string.
     *
     * C++ counterpart of .NET StringInfo.LengthInTextElements.
     * Counts scalar-based elements (see the class doc-comment) — a valid multi-byte scalar counts
     * as one element, not one per byte.
     * @return The number of text elements in the string.
     */
    [[nodiscard]] intcs getLengthInTextElementsProperty() const {
        return static_cast<intcs>(elementOffsets(string_).size() - 1);
    }

    /**
     * @brief Returns a substring starting at the given text element index.
     *
     * C++ counterpart of .NET StringInfo.SubstringByTextElements(int).
     * @param startingTextElement The zero-based index of the first text element.
     * @return The substring from @p startingTextElement to the end.
     * @throws System::ArgumentOutOfRangeException if @p startingTextElement is out of range.
     */
    [[nodiscard]] std::string SubstringByTextElements(intcs startingTextElement) const {
        const auto offsets = elementOffsets(string_);
        const std::size_t count = offsets.size() - 1;
        if (startingTextElement < 0 || static_cast<std::size_t>(startingTextElement) >= count) {
            throw System::ArgumentOutOfRangeException("startingTextElement");
        }
        return string_.substr(offsets[static_cast<std::size_t>(startingTextElement)]);
    }

    /**
     * @brief Returns a substring of the given number of text elements.
     *
     * C++ counterpart of .NET StringInfo.SubstringByTextElements(int, int).
     * @param startingTextElement   The zero-based index of the first text element.
     * @param lengthInTextElements  The number of text elements to include.
     * @return The specified substring.
     * @throws System::ArgumentOutOfRangeException if @p startingTextElement or
     *         @p lengthInTextElements is negative or out of range for the string.
     */
    [[nodiscard]] std::string SubstringByTextElements(intcs startingTextElement,
                                                       intcs lengthInTextElements) const {
        const auto offsets = elementOffsets(string_);
        const std::size_t count = offsets.size() - 1;
        if (startingTextElement < 0 || static_cast<std::size_t>(startingTextElement) >= count) {
            throw System::ArgumentOutOfRangeException("startingTextElement");
        }
        const std::size_t start = static_cast<std::size_t>(startingTextElement);
        if (lengthInTextElements < 0 ||
            static_cast<std::size_t>(lengthInTextElements) > count - start) {
            throw System::ArgumentOutOfRangeException("lengthInTextElements");
        }
        const std::size_t end = start + static_cast<std::size_t>(lengthInTextElements);
        return string_.substr(offsets[start], offsets[end] - offsets[start]);
    }

    /**
     * @brief Returns the text element at the specified index in the given string.
     *
     * C++ counterpart of .NET StringInfo.GetNextTextElement(string, int).
     * Returns the full UTF-8 byte sequence starting at @p index (see the class doc-comment) —
     * consistent with GetTextElementEnumerator, so a multi-byte character is never truncated to
     * its leading byte.
     * @param str   The source string.
     * @param index The zero-based byte index (default 0).
     * @return The text element (1-4 bytes) starting at @p index, or an empty string if @p index
     *         is exactly str.size() (the end of the string).
     * @throws System::ArgumentOutOfRangeException if @p index is negative, greater than
     *         str.size(), or in the middle of a valid UTF-8 scalar.
     */
    static std::string GetNextTextElement(const std::string& str, intcs index = 0) {
        if (index < 0 || index > static_cast<int>(str.size()))
            throw System::ArgumentOutOfRangeException("index");
        if (index == static_cast<int>(str.size())) return {};
        const size_t i = static_cast<size_t>(index);
        if (!detail::IsUtf8TextElementBoundary(str, i))
            throw System::ArgumentOutOfRangeException("index");
        return str.substr(i, detail::Utf8TextElementLength(str, i));
    }

    /**
     * @brief Returns the length of the text element at the specified index.
     *
     * C++ counterpart of .NET StringInfo.GetNextTextElementLength(string, int).
     * Returns the byte length of the UTF-8 sequence starting at @p index (1-4), consistent with
     * GetNextTextElement/GetTextElementEnumerator.
     * @param str   The source string.
     * @param index The zero-based byte index (default 0).
     * @return The element's byte length (1-4) if @p index is a valid element position; 0 if
     *         @p index is exactly str.size() (the end of the string).
     * @throws System::ArgumentOutOfRangeException if @p index is negative, greater than
     *         str.size(), or in the middle of a valid UTF-8 scalar.
     */
    static intcs GetNextTextElementLength(const std::string& str, intcs index = 0) {
        if (index < 0 || index > static_cast<int>(str.size()))
            throw System::ArgumentOutOfRangeException("index");
        if (index == static_cast<int>(str.size())) return 0;
        if (!detail::IsUtf8TextElementBoundary(str, static_cast<std::size_t>(index)))
            throw System::ArgumentOutOfRangeException("index");
        return static_cast<intcs>(detail::Utf8TextElementLength(str, static_cast<size_t>(index)));
    }

    /**
     * @brief Returns the starting byte indices of each text element in the given string.
     *
     * C++ counterpart of .NET StringInfo.ParseCombiningCharacters(string).
     * Each entry is the starting byte index of one UTF-8 element (no combining-character
     * grouping) — consistent with GetNextTextElement/GetTextElementEnumerator, so a multi-byte
     * character contributes exactly one entry (its starting byte), not one per byte.
     * @param str The source string.
     * @return A vector of zero-based starting byte indices, one per text element in @p str.
     */
    static std::vector<intcs> ParseCombiningCharacters(const std::string& str) {
        std::vector<intcs> result;
        const auto offsets = elementOffsets(str);
        result.reserve(offsets.size() - 1);
        for (std::size_t i = 0; i + 1 < offsets.size(); ++i)
            result.push_back(static_cast<intcs>(offsets[i]));
        return result;
    }

    /**
     * @brief Returns a TextElementEnumerator for the entire string.
     *
     * C++ counterpart of .NET StringInfo.GetTextElementEnumerator(string).
     * @param str The string to enumerate.
     * @return A TextElementEnumerator positioned before the first element.
     */
    static TextElementEnumerator GetTextElementEnumerator(const std::string& str) {
        return TextElementEnumerator(str);
    }

    /**
     * @brief Returns a TextElementEnumerator starting at the given index.
     *
     * C++ counterpart of .NET StringInfo.GetTextElementEnumerator(string, int).
     * @param str   The string to enumerate.
     * @param index The zero-based byte index at which to start enumeration.
     * @return A TextElementEnumerator positioned before the first element at @p index.
     * @throws System::ArgumentOutOfRangeException if @p index is negative, greater than the
     *         string length, or in the middle of a valid UTF-8 scalar.
     */
    static TextElementEnumerator GetTextElementEnumerator(const std::string& str, intcs index) {
        return TextElementEnumerator(str, index);
    }
};

} // namespace System::Globalization
