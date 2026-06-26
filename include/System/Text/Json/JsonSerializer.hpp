// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include "System/Text/Json/JsonDocument.hpp"
#include "System/Text/Json/JsonSerializerOptions.hpp"

namespace System::Text::Json {

    /** Provides static methods for serializing and deserializing JSON (stub — requires JSON backend). */
    class JsonSerializer {
    public:
        JsonSerializer() = delete;

        /** Serializes a value to a JSON string (stub — always throws). */
        static std::string Serialize(const void* /*value*/, const JsonSerializerOptions& /*opts*/ = JsonSerializerOptions::Default()) {
            throw std::runtime_error("JsonSerializer::Serialize requires JSON backend integration.");
        }

        /** Deserializes a JSON string to a JsonDocument. */
        static std::shared_ptr<JsonDocument> Deserialize(const std::string& json,
                                                          const JsonSerializerOptions& /*opts*/ = JsonSerializerOptions::Default()) {
            return JsonDocument::Parse(json);
        }

        /** Typed serialization overload (stub — always throws). */
        template<typename T>
        static std::string Serialize(const T& /*value*/, const JsonSerializerOptions& opts = JsonSerializerOptions::Default()) {
            return Serialize(static_cast<const void*>(nullptr), opts);
        }
    };

} // namespace System::Text::Json
