// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include "System/Text/Json/JsonElement.hpp"
#include "System/IDisposable.hpp"
#include "nlohmann/json.hpp"

namespace System::Text::Json {

    class JsonDocument : public System::IDisposable {
        std::shared_ptr<JsonElement> root_;
        bool disposed_ = false;

        explicit JsonDocument(std::shared_ptr<JsonElement> root) : root_(std::move(root)) {}

        static std::shared_ptr<JsonElement> fromNlohmann(const nlohmann::json& j) {
            if (j.is_null())
                return std::make_shared<JsonElement>(JsonValueKind::Null, "null");
            if (j.is_boolean()) {
                bool b = j.get<bool>();
                return std::make_shared<JsonElement>(
                    b ? JsonValueKind::True : JsonValueKind::False,
                    b ? "true" : "false");
            }
            if (j.is_number())
                return std::make_shared<JsonElement>(JsonValueKind::Number, j.dump());
            if (j.is_string())
                return std::make_shared<JsonElement>(JsonValueKind::String, j.get<std::string>());
            if (j.is_array()) {
                auto el = std::make_shared<JsonElement>(JsonValueKind::Array, "");
                for (const auto& item : j)
                    el->addArrayItemForTesting(fromNlohmann(item));
                return el;
            }
            if (j.is_object()) {
                auto el = std::make_shared<JsonElement>(JsonValueKind::Object, "");
                for (const auto& [key, val] : j.items())
                    el->addPropertyForTesting(key, fromNlohmann(val));
                return el;
            }
            return std::make_shared<JsonElement>(JsonValueKind::Undefined);
        }

    public:
        ~JsonDocument() override = default;

        void Dispose() override { disposed_ = true; root_.reset(); }

        [[nodiscard]] const JsonElement& getRootElementProperty() const {
            if (!root_) throw std::runtime_error("Document disposed.");
            return *root_;
        }

        static std::shared_ptr<JsonDocument> Parse(const std::string& json) {
            auto j = nlohmann::json::parse(json); // throws nlohmann::json::parse_error on bad input
            return std::shared_ptr<JsonDocument>(new JsonDocument(fromNlohmann(j)));
        }

        static std::shared_ptr<JsonDocument> ParseValue(const std::string& json) {
            return Parse(json);
        }
    };

} // namespace System::Text::Json
