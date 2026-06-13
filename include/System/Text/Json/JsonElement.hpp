// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <charconv>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "System/Text/Json/JsonValueKind.hpp"

namespace System::Text::Json {

    /// Represents a single JSON value (object, array, string, number, boolean, or null).
    class JsonElement {
        JsonValueKind kind_ = JsonValueKind::Undefined;
        std::string rawText_;
        std::vector<std::pair<std::string, std::shared_ptr<JsonElement>>> properties_;
        std::vector<std::shared_ptr<JsonElement>> arrayItems_;

    public:
        /// Constructs an undefined JsonElement.
        JsonElement() = default;
        /// Constructs a JsonElement with the given kind and raw text representation.
        explicit JsonElement(JsonValueKind kind, const std::string& raw = "")
            : kind_(kind), rawText_(raw) {}

        /// Gets the kind of this JSON value.
        [[nodiscard]] JsonValueKind getValueKindProperty() const { return kind_; }

        /// Returns the value as a string; throws if the element is not a JSON string.
        [[nodiscard]] std::string GetString() const {
            if (kind_ != JsonValueKind::String)
                throw std::runtime_error("Element is not a string.");
            return rawText_;
        }

        /// Returns the value as a 32-bit integer; throws if the element is not a JSON number.
        [[nodiscard]] int GetInt32() const {
            if (kind_ != JsonValueKind::Number)
                throw std::runtime_error("Element is not a number.");
            return std::stoi(rawText_);
        }

        /// Returns the value as a 64-bit integer; throws if the element is not a JSON number.
        [[nodiscard]] long long GetInt64() const {
            if (kind_ != JsonValueKind::Number)
                throw std::runtime_error("Element is not a number.");
            return std::stoll(rawText_);
        }

        /// Returns the value as a double; throws if the element is not a JSON number.
        [[nodiscard]] double GetDouble() const {
            if (kind_ != JsonValueKind::Number)
                throw std::runtime_error("Element is not a number.");
            double result{};
            auto [ptr, ec] = std::from_chars(rawText_.data(), rawText_.data() + rawText_.size(), result);
            if (ec != std::errc{}) throw std::runtime_error("JSON number cannot be parsed as double.");
            return result;
        }

        /// Returns the value as a boolean (true if kind is True, false otherwise).
        [[nodiscard]] bool GetBoolean() const {
            return kind_ == JsonValueKind::True;
        }

        /// Returns the raw JSON text of this element.
        [[nodiscard]] std::string GetRawText() const { return rawText_; }

        /// Tries to get a named object property; returns false if not found.
        [[nodiscard]] bool TryGetProperty(const std::string& name, JsonElement& out) const {
            for (auto& [k, v] : properties_) {
                if (k == name) { out = *v; return true; }
            }
            return false;
        }

        /// Gets a named object property; throws if not found.
        [[nodiscard]] JsonElement GetProperty(const std::string& name) const {
            JsonElement result;
            if (!TryGetProperty(name, result))
                throw std::runtime_error("Property '" + name + "' not found.");
            return result;
        }

        /// Returns the items of this JSON array; throws if the element is not an array.
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

        /// Returns the raw text representation of this element.
        [[nodiscard]] std::string ToString() const { return rawText_; }
    };

} // namespace System::Text::Json
