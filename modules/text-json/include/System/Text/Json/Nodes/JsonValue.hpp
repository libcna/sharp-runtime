// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Text/Json/Nodes/JsonNode.hpp"
#include "System/Text/Json/detail/JsonNumberConversion.hpp"
#include "nlohmann/json.hpp"

namespace System::Text::Json::Nodes {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /**
     * @brief Represents a mutable JSON value that isn't a JsonObject or JsonArray (a string,
     * number, boolean, or an underlying JsonElement scalar).
     *
     * C++ counterpart of .NET System.Text.Json.Nodes.JsonValue.
     *
     * @note Real .NET's `JsonValue.Create<T>` is generic and dispatches on `T`'s `JsonTypeInfo`
     * (reflection/source-gen); this port instead provides overloads for the practically portable
     * primitive types (bool, intcs, longcs, double, std::string) — see JsonSerializer's doc comment
     * on the same reflection-out-of-scope rationale.
     */
    class JsonValue : public JsonNode {
        nlohmann::ordered_json value_;

        explicit JsonValue(nlohmann::ordered_json value, JsonNodeOptions options = {})
            : JsonNode(options), value_(std::move(value)) {}

    public:
        /** @return true if the underlying value is a JSON string. */
        [[nodiscard]] bool IsString() const { return value_.is_string(); }
        /** @return true if the underlying value is a JSON number. */
        [[nodiscard]] bool IsNumber() const { return value_.is_number(); }
        /** @return true if the underlying value is a JSON boolean. */
        [[nodiscard]] bool IsBoolean() const { return value_.is_boolean(); }

        /**
         * @return The value as a string.
         * @throws System::InvalidOperationException if this isn't a JSON string.
         * @note Verified against JsonValueOfElement.cs's GetValue<T>: real .NET throws
         * InvalidOperationException (via ThrowInvalidOperationException_NodeUnableToConvertElement)
         * when the underlying value can't convert to the requested type, not FormatException.
         */
        [[nodiscard]] std::string GetString() const {
            if (!value_.is_string()) throw System::InvalidOperationException("The JsonValue is not a string.");
            return value_.get<std::string>();
        }
        /**
         * @return The value as a 32-bit integer.
         * @throws System::InvalidOperationException if this isn't a JSON number, or if it is a
         * number that does not convert exactly — a JSON float literal (`1.5`, but also `1.0` and
         * `2e1`, whose numeric values *are* integral), or an integer outside `Int32`'s range.
         *
         * Ticket #2114 (SR-AUD-328, cause TJ-F). This accessor used to hand the value straight to
         * `nlohmann`'s `get<intcs>()`, which truncated `1.5` to `1`, wrapped `2147483648` to
         * `-2147483648`, and executed **undefined behaviour** — a bare `static_cast<int>(double)`
         * — for `99999999999999999999`, `1e100` and NaN. `JsonElement` had already fixed exactly
         * this; the rule now lives in `detail::TryConvertJsonNumberToIntegral` and both share it.
         *
         * @note The exception is `InvalidOperationException`, **not** the `FormatException`
         * `JsonElement::GetInt32` raises. That difference is deliberate and is .NET's: see
         * `GetString`'s note above, transcribed from `JsonValueOfElement.cs`, where a `JsonValue`
         * that cannot convert raises `ThrowInvalidOperationException_NodeUnableToConvertElement`.
         */
        [[nodiscard]] intcs GetInt32() const {
            if (!value_.is_number()) throw System::InvalidOperationException("The JsonValue is not a number.");
            intcs result = 0;
            if (!detail::TryConvertJsonNumberToIntegral(value_, result))
                throw System::InvalidOperationException("The JsonValue could not be converted to Int32.");
            return result;
        }
        /**
         * @return The value as a 64-bit integer.
         * @throws System::InvalidOperationException if this isn't a JSON number, or if it is a
         * number that does not convert exactly. See GetInt32 for the full contract and for why
         * the exception type differs from `JsonElement`'s.
         */
        [[nodiscard]] longcs GetInt64() const {
            if (!value_.is_number()) throw System::InvalidOperationException("The JsonValue is not a number.");
            longcs result = 0;
            if (!detail::TryConvertJsonNumberToIntegral(value_, result))
                throw System::InvalidOperationException("The JsonValue could not be converted to Int64.");
            return result;
        }
        /** @return The value as a double. @throws System::InvalidOperationException if this isn't a JSON number. */
        [[nodiscard]] double GetDouble() const {
            if (!value_.is_number()) throw System::InvalidOperationException("The JsonValue is not a number.");
            return value_.get<double>();
        }
        /** @return The value as a boolean. @throws System::InvalidOperationException if this isn't a JSON boolean. */
        [[nodiscard]] bool GetBoolean() const {
            if (!value_.is_boolean()) throw System::InvalidOperationException("The JsonValue is not a boolean.");
            return value_.get<bool>();
        }

        [[nodiscard]] JsonValueKind GetValueKind() const override {
            if (value_.is_string()) return JsonValueKind::String;
            if (value_.is_number()) return JsonValueKind::Number;
            if (value_.is_boolean()) return value_.get<bool>() ? JsonValueKind::True : JsonValueKind::False;
            return JsonValueKind::Null;
        }

        [[nodiscard]] nlohmann::ordered_json toNlohmann() const override { return value_; }

        [[nodiscard]] std::shared_ptr<JsonNode> DeepClone() const override {
            return std::shared_ptr<JsonValue>(new JsonValue(value_, getOptionsProperty()));
        }

        /** @brief Creates a JsonValue wrapping a string. */
        [[nodiscard]] static std::shared_ptr<JsonValue> Create(const std::string& value, JsonNodeOptions options = {}) {
            return std::shared_ptr<JsonValue>(new JsonValue(nlohmann::ordered_json(value), options));
        }
        /** @brief Creates a JsonValue wrapping a 32-bit integer. */
        [[nodiscard]] static std::shared_ptr<JsonValue> Create(intcs value, JsonNodeOptions options = {}) {
            return std::shared_ptr<JsonValue>(new JsonValue(nlohmann::ordered_json(value), options));
        }
        /** @brief Creates a JsonValue wrapping a 64-bit integer. */
        [[nodiscard]] static std::shared_ptr<JsonValue> Create(longcs value, JsonNodeOptions options = {}) {
            return std::shared_ptr<JsonValue>(new JsonValue(nlohmann::ordered_json(value), options));
        }
        /** @brief Creates a JsonValue wrapping a double. */
        [[nodiscard]] static std::shared_ptr<JsonValue> Create(double value, JsonNodeOptions options = {}) {
            return std::shared_ptr<JsonValue>(new JsonValue(nlohmann::ordered_json(value), options));
        }
        /** @brief Creates a JsonValue wrapping a boolean. */
        [[nodiscard]] static std::shared_ptr<JsonValue> Create(bool value, JsonNodeOptions options = {}) {
            return std::shared_ptr<JsonValue>(new JsonValue(nlohmann::ordered_json(value), options));
        }

        /** @brief Internal: wraps an already-built nlohmann::ordered_json scalar (used by JsonNode::Parse). Not part of .NET's public surface. */
        [[nodiscard]] static std::shared_ptr<JsonValue> FromNlohmann(const nlohmann::ordered_json& value, JsonNodeOptions options = {}) {
            return std::shared_ptr<JsonValue>(new JsonValue(value, options));
        }
    };

} // namespace System::Text::Json::Nodes
