// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Text::Json {

    /** Specifies naming policy for JSON property names. */
    enum class JsonNamingPolicy {
        /** camelCase naming (e.g. "myProperty"). */
        CamelCase           = 0,
        /** snake_case lowercase naming. */
        SnakeCaseLower      = 1,
        /** SNAKE_CASE uppercase naming. */
        SnakeCaseUpper      = 2,
        /** kebab-case lowercase naming. */
        KebabCaseLower      = 3,
        /** KEBAB-CASE uppercase naming. */
        KebabCaseUpper      = 4,
    };

    /** Controls how JSON comments are handled during reading. */
    enum class JsonCommentHandling : uint8_t {
        /** Comments are not allowed; an exception is thrown. */
        Disallow = 0,
        /** Comments are ignored. */
        Skip     = 1,
        /** Comments are treated as valid tokens. */
        Allow    = 2,
    };

    /** Controls how numbers are handled during JSON serialization/deserialization. */
    enum class JsonNumberHandling {
        /** Strict numeric handling (no string conversion). */
        Strict                      = 0,
        /** Numbers may be read from JSON strings. */
        AllowReadingFromString      = 1,
        /** Numbers are written as JSON strings. */
        WriteAsString               = 2,
        /** Named floating-point literals (NaN, Infinity) are allowed. */
        AllowNamedFloatingPointLiterals = 4,
    };

    /** Provides options to control JSON serialization and deserialization behavior. */
    class JsonSerializerOptions {
        bool writeIndented_ = false;
        bool allowTrailingCommas_ = false;
        bool propertyNameCaseInsensitive_ = false;
        int maxDepth_ = 0;
        JsonCommentHandling readCommentHandling_ = JsonCommentHandling::Disallow;

    public:
        /** Constructs default options (no indentation, strict comments, case-sensitive). */
        JsonSerializerOptions() = default;

        /** Returns true if JSON output should be indented. */
        [[nodiscard]] bool getWriteIndentedProperty()              const { return writeIndented_; }
        /** Returns true if trailing commas in JSON are allowed. */
        [[nodiscard]] bool getAllowTrailingCommasProperty()        const { return allowTrailingCommas_; }
        /** Returns true if property name matching is case-insensitive. */
        [[nodiscard]] bool getPropertyNameCaseInsensitiveProperty() const { return propertyNameCaseInsensitive_; }
        /** Gets the maximum allowed depth when reading JSON (0 = unlimited). */
        [[nodiscard]] int  getMaxDepthProperty()                   const { return maxDepth_; }
        /** Gets the comment-handling mode used when reading JSON. */
        [[nodiscard]] JsonCommentHandling getReadCommentHandlingProperty() const { return readCommentHandling_; }

        /** Sets whether JSON output should be indented. */
        void setWriteIndentedProperty(bool v)               { writeIndented_ = v; }
        /** Sets whether trailing commas in JSON are allowed. */
        void setAllowTrailingCommasProperty(bool v)         { allowTrailingCommas_ = v; }
        /** Sets whether property name matching is case-insensitive. */
        void setPropertyNameCaseInsensitiveProperty(bool v) { propertyNameCaseInsensitive_ = v; }
        /** Sets the maximum allowed depth when reading JSON. */
        void setMaxDepthProperty(int v)                     { maxDepth_ = v; }
        /** Sets the comment-handling mode used when reading JSON. */
        void setReadCommentHandlingProperty(JsonCommentHandling v) { readCommentHandling_ = v; }

        /** Returns the default JsonSerializerOptions instance. */
        static const JsonSerializerOptions& Default() {
            static JsonSerializerOptions opts;
            return opts;
        }
    };

} // namespace System::Text::Json
