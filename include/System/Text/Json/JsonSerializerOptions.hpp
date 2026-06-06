// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Text::Json {

    enum class JsonNamingPolicy {
        CamelCase           = 0,
        SnakeCaseLower      = 1,
        SnakeCaseUpper      = 2,
        KebabCaseLower      = 3,
        KebabCaseUpper      = 4,
    };

    enum class JsonCommentHandling : uint8_t {
        Disallow = 0,
        Skip     = 1,
        Allow    = 2,
    };

    enum class JsonNumberHandling {
        Strict                      = 0,
        AllowReadingFromString      = 1,
        WriteAsString               = 2,
        AllowNamedFloatingPointLiterals = 4,
    };

    class JsonSerializerOptions {
        bool writeIndented_ = false;
        bool allowTrailingCommas_ = false;
        bool propertyNameCaseInsensitive_ = false;
        int maxDepth_ = 0;
        JsonCommentHandling readCommentHandling_ = JsonCommentHandling::Disallow;

    public:
        JsonSerializerOptions() = default;

        [[nodiscard]] bool getWriteIndentedProperty()              const { return writeIndented_; }
        [[nodiscard]] bool getAllowTrailingCommasProperty()        const { return allowTrailingCommas_; }
        [[nodiscard]] bool getPropertyNameCaseInsensitiveProperty() const { return propertyNameCaseInsensitive_; }
        [[nodiscard]] int  getMaxDepthProperty()                   const { return maxDepth_; }
        [[nodiscard]] JsonCommentHandling getReadCommentHandlingProperty() const { return readCommentHandling_; }

        void setWriteIndentedProperty(bool v)               { writeIndented_ = v; }
        void setAllowTrailingCommasProperty(bool v)         { allowTrailingCommas_ = v; }
        void setPropertyNameCaseInsensitiveProperty(bool v) { propertyNameCaseInsensitive_ = v; }
        void setMaxDepthProperty(int v)                     { maxDepth_ = v; }
        void setReadCommentHandlingProperty(JsonCommentHandling v) { readCommentHandling_ = v; }

        static const JsonSerializerOptions& Default() {
            static JsonSerializerOptions opts;
            return opts;
        }
    };

} // namespace System::Text::Json
