// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <string>

namespace System::Collections {

/**
 * @brief Defines a dictionary key/value pair that can be set or retrieved.
 *
 * C++ counterpart of .NET System.Collections.DictionaryEntry.
 * Used by IDictionary and IDictionaryEnumerator to represent a non-generic entry.
 */
struct DictionaryEntry {
private:
    std::any key_;
    std::any value_;

public:
    /**
     * @brief Default-constructs a DictionaryEntry with empty key and value.
     *
     * C++ counterpart of the implicit default constructor.
     */
    DictionaryEntry() = default;

    /**
     * @brief Constructs a DictionaryEntry with the given key and value.
     *
     * C++ counterpart of .NET DictionaryEntry(object key, object? value).
     * @param key   The key of the entry.
     * @param value The value of the entry.
     */
    DictionaryEntry(std::any key, std::any value)
        : key_(std::move(key)), value_(std::move(value)) {}

    /**
     * @brief Gets the key of the entry.
     *
     * C++ counterpart of .NET DictionaryEntry.Key getter.
     * @return The key stored in this entry.
     */
    [[nodiscard]] const std::any& getKeyProperty() const { return key_; }

    /**
     * @brief Sets the key of the entry.
     *
     * C++ counterpart of .NET DictionaryEntry.Key setter.
     * @param key The new key value.
     */
    void setKeyProperty(std::any key) { key_ = std::move(key); }

    /**
     * @brief Gets the value of the entry.
     *
     * C++ counterpart of .NET DictionaryEntry.Value getter.
     * @return The value stored in this entry.
     */
    [[nodiscard]] const std::any& getValueProperty() const { return value_; }

    /**
     * @brief Sets the value of the entry.
     *
     * C++ counterpart of .NET DictionaryEntry.Value setter.
     * @param value The new value.
     */
    void setValueProperty(std::any value) { value_ = std::move(value); }

    /**
     * @brief Deconstructs the entry into its key and value components.
     *
     * C++ counterpart of .NET DictionaryEntry.Deconstruct(out object, out object?).
     * @param key   Receives the key.
     * @param value Receives the value.
     */
    void Deconstruct(std::any& key, std::any& value) const {
        key   = key_;
        value = value_;
    }

    /**
     * @brief Returns a string representation of the key/value pair.
     *
     * C++ counterpart of .NET DictionaryEntry.ToString().
     * @return String in the form "[key, value]".
     */
    [[nodiscard]] std::string ToString() const {
        std::string k = key_.has_value()   ? key_.type().name()   : "(null)";
        std::string v = value_.has_value() ? value_.type().name() : "(null)";
        return "[" + k + ", " + v + "]";
    }
};

} // namespace System::Collections
