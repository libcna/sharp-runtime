// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>

namespace System::Globalization {

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
    [[nodiscard]] int getLengthInTextElementsProperty() const {
        return static_cast<int>(string_.size());
    }

    /**
     * @brief Returns a substring starting at the given text element index.
     *
     * C++ counterpart of .NET StringInfo.SubstringByTextElements(int).
     * @param startingTextElement The zero-based index of the first text element.
     * @return The substring from @p startingTextElement to the end.
     */
    [[nodiscard]] std::string SubstringByTextElements(int startingTextElement) const {
        return string_.substr(startingTextElement);
    }

    /**
     * @brief Returns a substring of the given number of text elements.
     *
     * C++ counterpart of .NET StringInfo.SubstringByTextElements(int, int).
     * @param startingTextElement   The zero-based index of the first text element.
     * @param lengthInTextElements  The number of text elements to include.
     * @return The specified substring.
     */
    [[nodiscard]] std::string SubstringByTextElements(int startingTextElement,
                                                       int lengthInTextElements) const {
        return string_.substr(startingTextElement, lengthInTextElements);
    }

    /**
     * @brief Returns the text element at the specified index in the given string.
     *
     * C++ counterpart of .NET StringInfo.GetNextTextElement(string, int).
     * Stub — returns a single character at @p index.
     * @param str   The source string.
     * @param index The zero-based character index (default 0).
     * @return A single-character string, or an empty string if @p index is out of range.
     */
    static std::string GetNextTextElement(const std::string& str, int index = 0) {
        if (index >= static_cast<int>(str.size())) return {};
        return std::string(1, str[index]);
    }

    /**
     * @brief Returns the length of the text element at the specified index.
     *
     * C++ counterpart of .NET StringInfo.GetNextTextElementLength(string, int).
     * Stub — always returns 1 (single-byte elements).
     * @param str   The source string.
     * @param index The zero-based character index (default 0).
     * @return 1 if @p index is in range; otherwise 0.
     */
    static int GetNextTextElementLength(const std::string& str, int index = 0) {
        if (index >= static_cast<int>(str.size())) return 0;
        return 1;
    }

    /**
     * @brief Returns the text elements of the given string as a vector of strings.
     *
     * C++ counterpart of .NET StringInfo.ParseCombiningCharacters(string).
     * Stub — each entry is a single character (no combining-character grouping).
     * @param str The source string.
     * @return A vector where each entry contains one character from @p str.
     */
    static std::vector<std::string> ParseCombiningCharacters(const std::string& str) {
        std::vector<std::string> result;
        for (char c : str) result.push_back(std::string(1, c));
        return result;
    }
};

} // namespace System::Globalization
