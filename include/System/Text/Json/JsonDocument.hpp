// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include "System/Text/Json/JsonElement.hpp"
#include "System/IDisposable.hpp"

namespace System::Text::Json {

    class JsonDocument : public System::IDisposable {
        std::shared_ptr<JsonElement> root_;
        bool disposed_ = false;

        explicit JsonDocument(std::shared_ptr<JsonElement> root) : root_(std::move(root)) {}

    public:
        ~JsonDocument() override = default;

        void Dispose() override { disposed_ = true; root_.reset(); }

        [[nodiscard]] const JsonElement& getRootElementProperty() const {
            if (!root_) throw std::runtime_error("Document disposed.");
            return *root_;
        }

        // Stub — real implementation requires JSON parser (nlohmann/json, rapidjson, etc.)
        static std::shared_ptr<JsonDocument> Parse(const std::string& json) {
            (void)json;
            // Returns an empty object stub; integrate JSON backend for real parsing
            return std::shared_ptr<JsonDocument>(
                new JsonDocument(std::make_shared<JsonElement>(JsonValueKind::Object, json)));
        }

        static std::shared_ptr<JsonDocument> ParseValue(const std::string& json) {
            return Parse(json);
        }
    };

} // namespace System::Text::Json
