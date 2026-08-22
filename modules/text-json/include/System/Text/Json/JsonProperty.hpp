// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <utility>
#include "System/Text/Json/JsonElement.hpp"

namespace System::Text::Json {

    /**
     * @brief Represents a single property (name/value pair) for a JSON object.
     *
     * C++ counterpart of .NET System.Text.Json.JsonProperty.
     */
    class JsonProperty {
        std::string name_;
        JsonElement value_;

    public:
        JsonProperty() = default;
        JsonProperty(std::string name, JsonElement value) : name_(std::move(name)), value_(std::move(value)) {}

        /** @return The name of this property. */
        [[nodiscard]] const std::string& getNameProperty() const { return name_; }
        /** @return The value of this property. */
        [[nodiscard]] const JsonElement& getValueProperty() const { return value_; }

        /** @return true if @p text matches the name of this property. */
        [[nodiscard]] bool NameEquals(const std::string& text) const { return name_ == text; }

        /**
         * @return This complete property (escaped name, separator and value) re-rendered as
         *         canonical JSON text.
         *
         * .NET returns the original property source slice. This DOM has no source spans, so the
         * representation-level limitation declared for JsonElement::GetRawText() in ticket #2118
         * applies here too; unlike the old implementation, the property name and separator are
         * never omitted.
         */
        [[nodiscard]] std::string ToString() const {
            return nlohmann::ordered_json(name_).dump() + ":" + value_.GetRawText();
        }
    };

} // namespace System::Text::Json
