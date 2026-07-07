// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Text/Json/Utf8JsonWriter.hpp"
#include <cstdio>
#include "System/InvalidOperationException.hpp"
#include "System/Text/Json/JsonException.hpp"
#include "nlohmann/json.hpp"

namespace System::Text::Json {

    void Utf8JsonWriter::writeIndentIfNeeded() {
        if (!options_.Indented) return;
        buffer_ += options_.NewLine;
        buffer_.append(stack_.size() * static_cast<size_t>(options_.IndentSize), options_.IndentCharacter);
    }

    void Utf8JsonWriter::beforeWritingElement() {
        if (!stack_.empty()) {
            Frame& top = stack_.back();
            if (top.hasWrittenItem) buffer_ += ',';
            writeIndentIfNeeded();
            top.hasWrittenItem = true;
        }
    }

    // Called after a value (scalar or nested container) has been fully written: clears the
    // enclosing object's "awaiting a property value" state, or marks the root scalar as written.
    void Utf8JsonWriter::markValueWritten() {
        if (stack_.empty()) {
            rootValueWritten_ = true;
        } else if (stack_.back().isObject) {
            stack_.back().awaitingPropertyValue = false;
        }
    }

    void Utf8JsonWriter::appendEscapedString(const std::string& value) {
        buffer_ += '"';
        for (char c : value) {
            switch (c) {
                case '"': buffer_ += "\\\""; break;
                case '\\': buffer_ += "\\\\"; break;
                case '\b': buffer_ += "\\b"; break;
                case '\f': buffer_ += "\\f"; break;
                case '\n': buffer_ += "\\n"; break;
                case '\r': buffer_ += "\\r"; break;
                case '\t': buffer_ += "\\t"; break;
                case '<': case '>': case '&': case '\'': {
                    char hex[8];
                    std::snprintf(hex, sizeof(hex), "\\u%04x", static_cast<unsigned char>(c));
                    buffer_ += hex;
                    break;
                }
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char hex[8];
                        std::snprintf(hex, sizeof(hex), "\\u%04x", static_cast<unsigned char>(c));
                        buffer_ += hex;
                    } else {
                        buffer_ += c;
                    }
                    break;
            }
        }
        buffer_ += '"';
    }

    void Utf8JsonWriter::validateCanWriteContainerStart() const {
        if (options_.SkipValidation) return;
        bool ok = stack_.empty() ? !rootValueWritten_
                                  : (!stack_.back().isObject || stack_.back().awaitingPropertyValue);
        if (!ok) throw System::InvalidOperationException("Cannot write the start of an array or object here.");
        if (options_.MaxDepth > 0 && static_cast<intcs>(stack_.size()) >= options_.MaxDepth)
            throw System::InvalidOperationException("The depth of the JSON exceeds the maximum configured depth.");
    }

    void Utf8JsonWriter::validateCanWritePropertyName() const {
        if (options_.SkipValidation) return;
        if (stack_.empty() || !stack_.back().isObject || stack_.back().awaitingPropertyValue)
            throw System::InvalidOperationException("Cannot write a property name here.");
    }

    void Utf8JsonWriter::validateCanWriteValue() const {
        if (options_.SkipValidation) return;
        bool ok = stack_.empty() ? !rootValueWritten_
                                  : (!stack_.back().isObject || stack_.back().awaitingPropertyValue);
        if (!ok) throw System::InvalidOperationException("Cannot write a value here.");
    }

    void Utf8JsonWriter::validateCanWriteEnd(bool isObject) const {
        if (options_.SkipValidation) return;
        if (stack_.empty() || stack_.back().isObject != isObject || stack_.back().awaitingPropertyValue)
            throw System::InvalidOperationException(
                std::string("Cannot write the end of an ") + (isObject ? "object" : "array") + " here.");
    }

    void Utf8JsonWriter::WriteStartObject() {
        validateCanWriteContainerStart();
        beforeWritingElement();
        buffer_ += '{';
        markValueWritten();
        stack_.push_back(Frame{true, false, false});
    }

    void Utf8JsonWriter::WriteStartObject(const std::string& propertyName) {
        WritePropertyName(propertyName);
        if (!options_.SkipValidation && options_.MaxDepth > 0 && static_cast<intcs>(stack_.size()) >= options_.MaxDepth)
            throw System::InvalidOperationException("The depth of the JSON exceeds the maximum configured depth.");
        buffer_ += '{';
        markValueWritten();
        stack_.push_back(Frame{true, false, false});
    }

    void Utf8JsonWriter::WriteEndObject() {
        validateCanWriteEnd(true);
        bool hadItems = stack_.back().hasWrittenItem;
        stack_.pop_back();
        if (hadItems) writeIndentIfNeeded();
        buffer_ += '}';
    }

    void Utf8JsonWriter::WriteStartArray() {
        validateCanWriteContainerStart();
        beforeWritingElement();
        buffer_ += '[';
        markValueWritten();
        stack_.push_back(Frame{false, false, false});
    }

    void Utf8JsonWriter::WriteStartArray(const std::string& propertyName) {
        WritePropertyName(propertyName);
        if (!options_.SkipValidation && options_.MaxDepth > 0 && static_cast<intcs>(stack_.size()) >= options_.MaxDepth)
            throw System::InvalidOperationException("The depth of the JSON exceeds the maximum configured depth.");
        buffer_ += '[';
        markValueWritten();
        stack_.push_back(Frame{false, false, false});
    }

    void Utf8JsonWriter::WriteEndArray() {
        validateCanWriteEnd(false);
        bool hadItems = stack_.back().hasWrittenItem;
        stack_.pop_back();
        if (hadItems) writeIndentIfNeeded();
        buffer_ += ']';
    }

    void Utf8JsonWriter::WritePropertyName(const std::string& propertyName) {
        validateCanWritePropertyName();
        beforeWritingElement();
        appendEscapedString(propertyName);
        buffer_ += options_.Indented ? ": " : ":";
        stack_.back().awaitingPropertyValue = true;
    }

    void Utf8JsonWriter::WriteString(const std::string& propertyName, const std::string& value) {
        WritePropertyName(propertyName);
        WriteStringValue(value);
    }

    void Utf8JsonWriter::WriteStringValue(const std::string& value) {
        validateCanWriteValue();
        if (stack_.empty() || !stack_.back().isObject) beforeWritingElement();
        appendEscapedString(value);
        markValueWritten();
    }

    void Utf8JsonWriter::WriteNumber(const std::string& propertyName, intcs value) {
        WritePropertyName(propertyName);
        WriteNumberValue(value);
    }
    void Utf8JsonWriter::WriteNumber(const std::string& propertyName, longcs value) {
        WritePropertyName(propertyName);
        WriteNumberValue(value);
    }
    void Utf8JsonWriter::WriteNumber(const std::string& propertyName, double value) {
        WritePropertyName(propertyName);
        WriteNumberValue(value);
    }

    void Utf8JsonWriter::WriteNumberValue(intcs value) { WriteNumberValue(static_cast<longcs>(value)); }

    void Utf8JsonWriter::WriteNumberValue(longcs value) {
        validateCanWriteValue();
        if (stack_.empty() || !stack_.back().isObject) beforeWritingElement();
        buffer_ += std::to_string(value);
        markValueWritten();
    }

    void Utf8JsonWriter::WriteNumberValue(double value) {
        validateCanWriteValue();
        if (stack_.empty() || !stack_.back().isObject) beforeWritingElement();
        buffer_ += nlohmann::json(value).dump();
        markValueWritten();
    }

    void Utf8JsonWriter::WriteBoolean(const std::string& propertyName, bool value) {
        WritePropertyName(propertyName);
        WriteBooleanValue(value);
    }

    void Utf8JsonWriter::WriteBooleanValue(bool value) {
        validateCanWriteValue();
        if (stack_.empty() || !stack_.back().isObject) beforeWritingElement();
        buffer_ += value ? "true" : "false";
        markValueWritten();
    }

    void Utf8JsonWriter::WriteNull(const std::string& propertyName) {
        WritePropertyName(propertyName);
        WriteNullValue();
    }

    void Utf8JsonWriter::WriteNullValue() {
        validateCanWriteValue();
        if (stack_.empty() || !stack_.back().isObject) beforeWritingElement();
        buffer_ += "null";
        markValueWritten();
    }

    void Utf8JsonWriter::WriteRawValue(const std::string& json, bool skipInputValidation) {
        if (!skipInputValidation && !options_.SkipValidation) {
            try {
                auto parsed = nlohmann::json::parse(json);
                (void)parsed;
            } catch (const nlohmann::json::parse_error& e) {
                throw JsonException(std::string("Invalid JSON passed to WriteRawValue: ") + e.what());
            }
        }
        validateCanWriteValue();
        if (stack_.empty() || !stack_.back().isObject) beforeWritingElement();
        buffer_ += json;
        markValueWritten();
    }

} // namespace System::Text::Json
