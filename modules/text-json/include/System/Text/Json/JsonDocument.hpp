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
#include "System/Text/Json/detail/JsonParseCore.hpp"
#include "nlohmann/json.hpp"

namespace System::Text::Json {

    /**
     * @brief Represents the in-memory tree of a parsed JSON document, providing read-only access
     * via a root JsonElement.
     *
     * C++ counterpart of .NET System.Text.Json.JsonDocument.
     *
     * @note **`Dispose()` is not propagated to elements handed out earlier** — SR-AUD-324, cause
     * TJ-H, ticket **#2117**, blocked on an object-layout change. It is **not** a use-after-free:
     * `JsonElement` holds an **owning** aliasing `shared_ptr`, so an element captured before
     * `Dispose()` keeps the tree alive and reads **live** storage, where .NET's
     * `CheckUseAfterDispose` would throw. `getRootElementProperty()` after `Dispose()` already
     * throws and double `Dispose()` is already safe; it is only the previously handed-out elements
     * that keep working. Both halves are pinned by
     * `JsonGatedBehaviourPins.PIN2117TheDisposalFlagIsNotPropagatedToElementsHandedOutEarlier`.
     * The cost of the safety is *retention*: `Dispose()` does not free the tree while an element
     * survives. That is not a leak, and LSan agrees.
     */
    class JsonDocument : public System::IDisposable {
        std::shared_ptr<const nlohmann::ordered_json> root_;
        bool disposed_ = false;

        explicit JsonDocument(std::shared_ptr<const nlohmann::ordered_json> root) : root_(std::move(root)) {}

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
            try {
                // #2116/#2121: option validation, the #2112 embedded-NUL guard, the parse itself
                // and the depth check all live in ONE place now, because JsonSerializer had a
                // second, drifted copy of this sequence. See detail::ParseDocumentText.
                return std::shared_ptr<JsonDocument>(new JsonDocument(
                    std::make_shared<const nlohmann::ordered_json>(detail::ParseDocumentText(json, options))));
            // #2111: this caught only parse_error, so a number literal that overflows a
            // double -- which raises out_of_range, NOT parse_error -- escaped as a std::
            // exception that no caller writing catch(const System::Exception&) could see.
            // JsonSerializer::Deserialize<T> already caught the BASE and was already correct.
            } catch (const nlohmann::ordered_json::exception& e) {
                throw JsonException(std::string("'") + e.what() + "'.");
            }
        }

        /** @brief Parses a JSON value (alias for Parse). */
        static std::shared_ptr<JsonDocument> ParseValue(const std::string& json, JsonDocumentOptions options = {}) {
            return Parse(json, options);
        }
    };

} // namespace System::Text::Json
