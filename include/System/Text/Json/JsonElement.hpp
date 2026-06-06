// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "System/Text/Json/JsonValueKind.hpp"

namespace System::Text::Json {

    class JsonElement {
        JsonValueKind kind_ = JsonValueKind::Undefined;
        std::string rawText_;
        std::vector<std::pair<std::string, std::shared_ptr<JsonElement>>> properties_;
        std::vector<std::shared_ptr<JsonElement>> arrayItems_;

    public:
        JsonElement() = default;
        explicit JsonElement(JsonValueKind kind, const std::string& raw = "")
            : kind_(kind), rawText_(raw) {}

        [[nodiscard]] JsonValueKind getValueKindProperty() const { return kind_; }

        [[nodiscard]] std::string GetString() const {
            if (kind_ != JsonValueKind::String)
                throw std::runtime_error("Element is not a string.");
            return rawText_;
        }

        [[nodiscard]] int GetInt32() const {
            if (kind_ != JsonValueKind::Number)
                throw std::runtime_error("Element is not a number.");
            return std::stoi(rawText_);
        }

        [[nodiscard]] long long GetInt64() const {
            if (kind_ != JsonValueKind::Number)
                throw std::runtime_error("Element is not a number.");
            return std::stoll(rawText_);
        }

        [[nodiscard]] double GetDouble() const {
            if (kind_ != JsonValueKind::Number)
                throw std::runtime_error("Element is not a number.");
            return std::stod(rawText_);
        }

        [[nodiscard]] bool GetBoolean() const {
            return kind_ == JsonValueKind::True;
        }

        [[nodiscard]] std::string GetRawText() const { return rawText_; }

        [[nodiscard]] bool TryGetProperty(const std::string& name, JsonElement& out) const {
            for (auto& [k, v] : properties_) {
                if (k == name) { out = *v; return true; }
            }
            return false;
        }

        [[nodiscard]] JsonElement GetProperty(const std::string& name) const {
            JsonElement result;
            if (!TryGetProperty(name, result))
                throw std::runtime_error("Property '" + name + "' not found.");
            return result;
        }

        [[nodiscard]] const std::vector<std::shared_ptr<JsonElement>>& EnumerateArray() const {
            if (kind_ != JsonValueKind::Array)
                throw std::runtime_error("Element is not an array.");
            return arrayItems_;
        }

        void addPropertyForTesting(const std::string& key, std::shared_ptr<JsonElement> val) {
            properties_.emplace_back(key, std::move(val));
        }
        void addArrayItemForTesting(std::shared_ptr<JsonElement> item) {
            arrayItems_.push_back(std::move(item));
        }

        [[nodiscard]] std::string ToString() const { return rawText_; }
    };

} // namespace System::Text::Json
