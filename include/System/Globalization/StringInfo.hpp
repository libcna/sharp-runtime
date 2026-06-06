// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>

namespace System::Globalization {

    class StringInfo {
        std::string string_;

    public:
        StringInfo() = default;
        explicit StringInfo(const std::string& str) : string_(str) {}

        [[nodiscard]] const std::string& getStringProperty() const { return string_; }
        void setStringProperty(const std::string& v)               { string_ = v; }

        [[nodiscard]] int getLengthInTextElementsProperty() const {
            return static_cast<int>(string_.size());
        }

        [[nodiscard]] std::string SubstringByTextElements(int startingTextElement) const {
            return string_.substr(startingTextElement);
        }

        [[nodiscard]] std::string SubstringByTextElements(int startingTextElement, int lengthInTextElements) const {
            return string_.substr(startingTextElement, lengthInTextElements);
        }

        static std::string GetNextTextElement(const std::string& str, int index = 0) {
            if (index >= static_cast<int>(str.size())) return {};
            return std::string(1, str[index]);
        }

        static int GetNextTextElementLength(const std::string& str, int index = 0) {
            if (index >= static_cast<int>(str.size())) return 0;
            return 1;
        }

        static std::vector<std::string> ParseCombiningCharacters(const std::string& str) {
            std::vector<std::string> result;
            for (char c : str) result.push_back(std::string(1, c));
            return result;
        }
    };

} // namespace System::Globalization
