// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cctype>
#include <string>
#include <cwctype>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Globalization {

using SharpRuntime::charcs;
using SharpRuntime::intcs;

/// <summary>Defines text properties and behaviors, such as casing, that are specific to a writing system.</summary>
class TextInfo {
public:
    explicit TextInfo(const std::string& cultureName = "en-US") : _cultureName(cultureName) {}

    [[nodiscard]] const std::string& getCultureNameProperty() const { return _cultureName; }
    [[nodiscard]] bool getIsReadOnlyProperty() const { return _isReadOnly; }
    [[nodiscard]] bool getIsRightToLeftProperty() const { return false; }
    [[nodiscard]] std::string getListSeparatorProperty() const { return _listSeparator; }
    void setListSeparatorProperty(const std::string& value) { _listSeparator = value; }

    /// <summary>Converts a character to lowercase.</summary>
    charcs ToLower(charcs c) const {
        if (c < 128) return static_cast<charcs>(std::tolower(static_cast<int>(c)));
        return static_cast<charcs>(std::towlower(static_cast<wint_t>(c)));
    }

    /// <summary>Converts a string to lowercase.</summary>
    std::u16string ToLower(const std::u16string& str) const {
        std::u16string result = str;
        for (auto& c : result) c = ToLower(c);
        return result;
    }

    std::string ToLower(const std::string& str) const {
        std::string result = str;
        for (auto& c : result)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return result;
    }

    /// <summary>Converts a character to uppercase.</summary>
    charcs ToUpper(charcs c) const {
        if (c < 128) return static_cast<charcs>(std::toupper(static_cast<int>(c)));
        return static_cast<charcs>(std::towupper(static_cast<wint_t>(c)));
    }

    /// <summary>Converts a string to uppercase.</summary>
    std::u16string ToUpper(const std::u16string& str) const {
        std::u16string result = str;
        for (auto& c : result) c = ToUpper(c);
        return result;
    }

    std::string ToUpper(const std::string& str) const {
        std::string result = str;
        for (auto& c : result)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return result;
    }

    /// <summary>Converts a string to title case (each word starts with uppercase).</summary>
    std::string ToTitleCase(const std::string& str) const {
        std::string result = str;
        bool newWord = true;
        for (auto& c : result) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                newWord = true;
            } else if (newWord) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                newWord = false;
            } else {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        return result;
    }

    TextInfo Clone() const { return *this; }

    static TextInfo ReadOnly(const TextInfo& textInfo) {
        TextInfo copy = textInfo;
        copy._isReadOnly = true;
        return copy;
    }

    bool operator==(const TextInfo& other) const { return _cultureName == other._cultureName; }
    std::string ToString() const { return "TextInfo - " + _cultureName; }

private:
    std::string _cultureName;
    std::string _listSeparator{","};
    bool _isReadOnly{false};
};

} // namespace System::Globalization
