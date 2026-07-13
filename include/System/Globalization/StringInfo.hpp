// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
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
 * In this stub, text elements are treated as individual bytes (not Unicode grapheme
 * clusters); full grapheme-cluster support is not implemented.
 */
class StringInfo {
    std::string string_;

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
     * Stub — returns the byte length of the string.
     * @return The number of text elements (bytes in this implementation).
     */
    [[nodiscard]] intcs getLengthInTextElementsProperty() const {
        return static_cast<intcs>(string_.size());
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
        return SubstringByTextElements(startingTextElement,
                                       static_cast<intcs>(string_.size()) - startingTextElement);
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
        size_t length = string_.size();
        if (static_cast<unsigned int>(startingTextElement) >= static_cast<unsigned int>(length)) {
            throw System::ArgumentOutOfRangeException("startingTextElement");
        }
        if (static_cast<unsigned int>(lengthInTextElements) >
            static_cast<unsigned int>(length - static_cast<size_t>(startingTextElement))) {
            throw System::ArgumentOutOfRangeException("lengthInTextElements");
        }
        return string_.substr(static_cast<size_t>(startingTextElement),
                              static_cast<size_t>(lengthInTextElements));
    }

    /**
     * @brief Returns the text element at the specified index in the given string.
     *
     * C++ counterpart of .NET StringInfo.GetNextTextElement(string, int).
     * Stub — returns a single character at @p index (byte, not grapheme cluster).
     * @param str   The source string.
     * @param index The zero-based character index (default 0).
     * @return A single-character string, or an empty string if @p index is exactly str.size()
     *         (the end of the string).
     * @throws System::ArgumentOutOfRangeException if @p index is negative or greater than
     *         str.size(). Previously only checked `index >= size()`, with no negative-index
     *         check at all, so a negative index fell through to `str[index]` -- an
     *         out-of-bounds/undefined-behavior read (verified against StringInfo.cs's
     *         `(uint)index > (uint)str.Length` check, which relies on unsigned wraparound to
     *         catch negative values too).
     */
    static std::string GetNextTextElement(const std::string& str, intcs index = 0) {
        if (index < 0 || index > static_cast<int>(str.size()))
            throw System::ArgumentOutOfRangeException("index");
        if (index == static_cast<int>(str.size())) return {};
        return std::string(1, str[static_cast<size_t>(index)]);
    }

    /**
     * @brief Returns the length of the text element at the specified index.
     *
     * C++ counterpart of .NET StringInfo.GetNextTextElementLength(string, int).
     * Stub — always returns 1 (single-byte elements, not real grapheme-cluster length).
     * @param str   The source string.
     * @param index The zero-based character index (default 0).
     * @return 1 if @p index is a valid element position; 0 if @p index is exactly str.size()
     *         (the end of the string).
     * @throws System::ArgumentOutOfRangeException if @p index is negative or greater than
     *         str.size(). Previously a negative index fell through to `return 1` instead of
     *         throwing (StringInfo.cs validates the same way as GetNextTextElement).
     */
    static intcs GetNextTextElementLength(const std::string& str, intcs index = 0) {
        if (index < 0 || index > static_cast<int>(str.size()))
            throw System::ArgumentOutOfRangeException("index");
        if (index == static_cast<int>(str.size())) return 0;
        return 1;
    }

    /**
     * @brief Returns the starting byte indices of each text element in the given string.
     *
     * C++ counterpart of .NET StringInfo.ParseCombiningCharacters(string).
     * Stub — each entry is the byte index of a character (no combining-character grouping).
     * @param str The source string.
     * @return A vector of zero-based byte indices, one per character in @p str.
     */
    static std::vector<intcs> ParseCombiningCharacters(const std::string& str) {
        std::vector<intcs> result;
        result.reserve(str.size());
        for (intcs i = 0; i < static_cast<intcs>(str.size()); ++i)
            result.push_back(i);
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
     * @throws System::ArgumentOutOfRangeException if @p index is negative or greater than the string length.
     */
    static TextElementEnumerator GetTextElementEnumerator(const std::string& str, intcs index) {
        if (index < 0 || index > static_cast<int>(str.size()))
            throw System::ArgumentOutOfRangeException("index");
        return TextElementEnumerator(str.substr(index));
    }
};

} // namespace System::Globalization
