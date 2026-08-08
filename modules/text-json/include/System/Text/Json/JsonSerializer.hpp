// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include <type_traits>
#include "System/Text/Json/JsonDocument.hpp"
#include "System/Text/Json/JsonException.hpp"
#include "System/Text/Json/JsonSerializerOptions.hpp"
#include "System/Text/Json/detail/JsonNumberConversion.hpp"
#include "nlohmann/json.hpp"

namespace System::Text::Json {

    /**
     * @brief Provides static methods for serializing objects to JSON and deserializing JSON to
     * objects.
     *
     * C++ counterpart of .NET System.Text.Json.JsonSerializer.
     *
     * @note Real .NET `Serialize<T>`/`Deserialize<T>` walk an arbitrary type's members via
     * reflection (or a source-generated `JsonTypeInfo`) — both are out of scope here (see
     * CLAUDE.md's parity philosophy: reflection is a permanent deviation). The template overloads
     * below instead rely on `nlohmann::ordered_json`'s compile-time ADL customization points
     * (`to_json`/`from_json` free functions, or `NLOHMANN_DEFINE_TYPE_INTRUSIVE`), which already
     * gives real, working serialization for primitives, `std::string`, `std::vector<T>`,
     * `std::map<std::string, T>`, `std::optional<T>`, and any user type that defines those
     * functions — a compile-time customization point standing in for reflection, not a stub.
     */
    class JsonSerializer {
    public:
        JsonSerializer() = delete;

        /** @brief Serializes @p value to a JSON string. @throws JsonException on serialization failure. */
        template <typename T>
        static std::string Serialize(const T& value, const JsonSerializerOptions& opts = JsonSerializerOptions::Default()) {
            try {
                nlohmann::ordered_json j = value;
                return opts.getWriteIndentedProperty() ? j.dump(static_cast<int>(opts.getIndentSizeProperty()),
                                                                  opts.getIndentCharacterProperty())
                                                        : j.dump();
            } catch (const nlohmann::ordered_json::exception& e) {
                throw JsonException(e.what());
            }
        }

        /** @brief Serializes @p element to a JSON string (re-formatted per @p opts's indentation settings). */
        static std::string Serialize(const JsonElement& element, const JsonSerializerOptions& opts = JsonSerializerOptions::Default()) {
            if (!opts.getWriteIndentedProperty()) return element.GetRawText();
            try {
                auto j = nlohmann::ordered_json::parse(element.GetRawText());
                return j.dump(static_cast<int>(opts.getIndentSizeProperty()), opts.getIndentCharacterProperty());
            } catch (const nlohmann::ordered_json::exception& e) {
                throw JsonException(e.what());
            }
        }

        /**
         * @brief Deserializes @p json into a value of type @p T.
         * @throws JsonException on invalid input or a shape mismatch, and — when @p T is a
         * non-`bool` integral type — when the JSON number does not convert to it exactly.
         *
         * Ticket #2114 (SR-AUD-328, cause TJ-F): for integral @p T this used to reach
         * `nlohmann`'s `get<T>()` directly, so `Deserialize<int>("1.5")` returned `1`,
         * `Deserialize<int>("2147483648")` returned `-2147483648`, and
         * `Deserialize<int>("99999999999999999999")` executed **undefined behaviour** (a bare
         * `static_cast<int>(double)`, reported by UBSan's `float-cast-overflow`). It now shares
         * `JsonElement`'s already-correct rule through
         * `detail::TryConvertJsonNumberToIntegral`.
         *
         * @note The guard applies to a **top-level** integral @p T only. An integer nested inside
         * an aggregate — `Deserialize<std::vector<int>>("[1.5]")` — still reaches `nlohmann`'s own
         * conversion, because the aggregate is deserialized by `nlohmann`'s ADL customization
         * points (see the class note) and this port has no hook between them. That residual is
         * measured, pinned by a test, and recorded in the review plan rather than left implicit;
         * closing it would mean replacing the customization-point design, not fixing a defect.
         */
        template <typename T>
        static T Deserialize(const std::string& json, const JsonSerializerOptions& /*opts*/ = JsonSerializerOptions::Default()) {
            try {
                auto parsed = nlohmann::ordered_json::parse(json);
                if constexpr (std::is_integral_v<T> && !std::is_same_v<std::remove_cv_t<T>, bool>) {
                    if (parsed.is_number()) {
                        T value{};
                        if (!detail::TryConvertJsonNumberToIntegral(parsed, value))
                            throw JsonException("The JSON value could not be converted to the requested integral type.");
                        return value;
                    }
                }
                return parsed.get<T>();
            } catch (const nlohmann::ordered_json::exception& e) {
                throw JsonException(e.what());
            }
        }

        /** @brief Deserializes @p json into a JsonDocument (untyped tree). */
        static std::shared_ptr<JsonDocument> Deserialize(const std::string& json,
                                                          const JsonSerializerOptions& /*opts*/ = JsonSerializerOptions::Default()) {
            return JsonDocument::Parse(json);
        }
    };

} // namespace System::Text::Json
