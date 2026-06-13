// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>

namespace System::Globalization {

/// <summary>Provides iteration over and retrieval of text elements in a string.</summary>
class StringInfo {
    std::string string_;

public:
    /// Constructs an empty StringInfo.
    StringInfo() = default;
    /// Constructs a StringInfo wrapping @p str.
    explicit StringInfo(const std::string& str) : string_(str) {}

    /// @return The underlying string.
    [[nodiscard]] const std::string& getStringProperty() const { return string_; }
    /// Sets the underlying string to @p v.
    void setStringProperty(const std::string& v)               { string_ = v; }

    /// @return The number of text elements (bytes in this stub implementation).
    [[nodiscard]] int getLengthInTextElementsProperty() const {
        return static_cast<int>(string_.size());
    }

    /// @return Substring starting at text element @p startingTextElement to the end.
    [[nodiscard]] std::string SubstringByTextElements(int startingTextElement) const {
        return string_.substr(startingTextElement);
    }

    /// @return Substring of @p lengthInTextElements elements starting at @p startingTextElement.
    [[nodiscard]] std::string SubstringByTextElements(int startingTextElement, int lengthInTextElements) const {
        return string_.substr(startingTextElement, lengthInTextElements);
    }

    /// @return The text element at position @p index in @p str (single character in this stub).
    static std::string GetNextTextElement(const std::string& str, int index = 0) {
        if (index >= static_cast<int>(str.size())) return {};
        return std::string(1, str[index]);
    }

    /// @return The length of the text element at @p index in @p str (always 1 in this stub).
    static int GetNextTextElementLength(const std::string& str, int index = 0) {
        if (index >= static_cast<int>(str.size())) return 0;
        return 1;
    }

    /// @return A vector where each entry is a single character from @p str.
    static std::vector<std::string> ParseCombiningCharacters(const std::string& str) {
        std::vector<std::string> result;
        for (char c : str) result.push_back(std::string(1, c));
        return result;
    }
};

} // namespace System::Globalization
