// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include "System/IDisposable.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Text/Json/JsonDocumentOptions.hpp"
#include "System/Text/Json/JsonElement.hpp"
#include "System/Text/Json/JsonException.hpp"
#include "nlohmann/json.hpp"

namespace System::Text::Json {

    /**
     * @brief Represents the in-memory tree of a parsed JSON document, providing read-only access
     * via a root JsonElement.
     *
     * C++ counterpart of .NET System.Text.Json.JsonDocument.
     */
    class JsonDocument : public System::IDisposable {
        std::shared_ptr<const nlohmann::json> root_;
        bool disposed_ = false;

        explicit JsonDocument(std::shared_ptr<const nlohmann::json> root) : root_(std::move(root)) {}

    public:
        ~JsonDocument() override = default;

        /** @brief Releases the root element and marks the document as disposed. */
        void Dispose() override {
            disposed_ = true;
            root_.reset();
        }

        /** @return The root JsonElement of this document. @throws System::ObjectDisposedException if disposed. */
        [[nodiscard]] JsonElement getRootElementProperty() const {
            if (disposed_) throw System::ObjectDisposedException("JsonDocument");
            return JsonElement(root_);
        }

        /**
         * @brief Parses UTF-8/ASCII JSON text and returns a JsonDocument.
         * @throws JsonException if the input is not valid JSON.
         */
        static std::shared_ptr<JsonDocument> Parse(const std::string& json, JsonDocumentOptions options = {}) {
            options.Validate();
            try {
                auto parsed = std::make_shared<const nlohmann::json>(
                    nlohmann::json::parse(json, /*callback=*/nullptr, /*allow_exceptions=*/true,
                                          /*ignore_comments=*/options.CommentHandling != JsonCommentHandling::Disallow));
                return std::shared_ptr<JsonDocument>(new JsonDocument(std::move(parsed)));
            } catch (const nlohmann::json::parse_error& e) {
                throw JsonException(std::string("'") + e.what() + "'.");
            }
        }

        /** @brief Parses a JSON value (alias for Parse). */
        static std::shared_ptr<JsonDocument> ParseValue(const std::string& json, JsonDocumentOptions options = {}) {
            return Parse(json, options);
        }
    };

} // namespace System::Text::Json
