// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Text/Json/JsonElement.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"
#include "System/FormatException.hpp"
#include "System/IndexOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Text/Json/JsonProperty.hpp"

namespace System::Text::Json {

    namespace {
        const char* kindName(JsonValueKind kind) {
            switch (kind) {
                case JsonValueKind::Undefined: return "Undefined";
                case JsonValueKind::Object: return "Object";
                case JsonValueKind::Array: return "Array";
                case JsonValueKind::String: return "String";
                case JsonValueKind::Number: return "Number";
                case JsonValueKind::True: return "True";
                case JsonValueKind::False: return "False";
                case JsonValueKind::Null: return "Null";
            }
            return "Undefined";
        }
    } // namespace

    const nlohmann::ordered_json& JsonElement::require(JsonValueKind expected, const char* what) const {
        if (getValueKindProperty() != expected) {
            throw System::InvalidOperationException(
                std::string("The requested operation requires an element of type '") + what +
                "', but the target element has type '" + kindName(getValueKindProperty()) + "'.");
        }
        return *node_;
    }

    intcs JsonElement::GetInt32() const {
        const auto& n = require(JsonValueKind::Number, "Number");
        if (!n.is_number_integer() && !n.is_number_unsigned()) {
            double d = n.get<double>();
            if (d != static_cast<double>(static_cast<intcs>(d)))
                throw System::FormatException("The JSON value could not be converted to Int32.");
        }
        auto value = n.get<long long>();
        if (value < static_cast<long long>(SharpRuntime::INTCS_MIN) || value > static_cast<long long>(SharpRuntime::INTCS_MAX))
            throw System::FormatException("The JSON value could not be converted to Int32.");
        return static_cast<intcs>(value);
    }

    longcs JsonElement::GetInt64() const {
        const auto& n = require(JsonValueKind::Number, "Number");
        if (!n.is_number_integer() && !n.is_number_unsigned()) {
            double d = n.get<double>();
            if (d != static_cast<double>(static_cast<longcs>(d)))
                throw System::FormatException("The JSON value could not be converted to Int64.");
        }
        return n.get<longcs>();
    }

    bool JsonElement::GetBoolean() const {
        JsonValueKind kind = getValueKindProperty();
        if (kind == JsonValueKind::True) return true;
        if (kind == JsonValueKind::False) return false;
        throw System::InvalidOperationException(
            std::string("The requested operation requires an element of type 'Boolean', but the target element has type '") +
            kindName(kind) + "'.");
    }

    bool JsonElement::TryGetInt32(intcs& value) const {
        if (!node_ || !node_->is_number()) { value = 0; return false; }
        double d = node_->get<double>();
        if (d != static_cast<double>(static_cast<intcs>(d))) { value = 0; return false; }
        value = static_cast<intcs>(d);
        return true;
    }

    bool JsonElement::TryGetInt64(longcs& value) const {
        if (!node_ || !node_->is_number()) { value = 0; return false; }
        double d = node_->get<double>();
        if (d != static_cast<double>(static_cast<longcs>(d))) { value = 0; return false; }
        value = static_cast<longcs>(d);
        return true;
    }

    JsonElement JsonElement::GetProperty(const std::string& name) const {
        JsonElement result;
        if (!TryGetProperty(name, result))
            throw System::Collections::Generic::KeyNotFoundException("The given key '" + name + "' was not present.");
        return result;
    }

    JsonElement JsonElement::operator[](intcs index) const {
        const auto& arr = require(JsonValueKind::Array, "Array");
        if (index < 0 || static_cast<size_t>(index) >= arr.size())
            throw System::IndexOutOfRangeException("Index was outside the bounds of the array.");
        return JsonElement(std::shared_ptr<const nlohmann::ordered_json>(node_, &arr[static_cast<size_t>(index)]));
    }

    std::vector<JsonProperty> JsonElement::EnumerateObject() const {
        const auto& obj = require(JsonValueKind::Object, "Object");
        std::vector<JsonProperty> result;
        result.reserve(obj.size());
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            JsonElement value(std::shared_ptr<const nlohmann::ordered_json>(node_, &(*it)));
            result.push_back(JsonProperty(it.key(), std::move(value)));
        }
        return result;
    }

} // namespace System::Text::Json
