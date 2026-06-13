// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/String.hpp"

namespace System::Text
{
    using SharpRuntime::intcs;

    /// <summary>
    /// Provides a mutable string buffer for efficient string construction.
    ///
    /// Lightweight C++ emulation of .NET System.Text.StringBuilder, intended
    /// primarily for source-porting convenience in the SharpRuntime layer.
    /// Only a practical subset of the original .NET API is provided.
    /// </summary>
    class StringBuilder
    {
    private:
        std::string buffer; ///< Internal text buffer.

    public:
        /// Initializes a new empty instance of the StringBuilder class.
        StringBuilder();

        /// Initializes a new instance of the StringBuilder class with the specified initial text.
        /// @param value Initial text.
        explicit StringBuilder(const std::string& value);

        /// Removes all characters from the current instance.
        void Clear();

        /// Appends the specified string to this instance.
        /// @param value String to append.
        /// @return Reference to this instance.
        StringBuilder& Append(const std::string& value);

        /// Appends the specified null-terminated C string to this instance.
        /// If @p value is nullptr, nothing is appended.
        /// @param value C string to append.
        /// @return Reference to this instance.
        StringBuilder& Append(const char* value);

        /// Appends the specified character to this instance.
        /// @param value Character to append.
        /// @return Reference to this instance.
        StringBuilder& Append(char value);

        /// Appends the string representation of the specified integer value.
        /// @param value Integer value to append.
        /// @return Reference to this instance.
        StringBuilder& Append(intcs value);

        /// Appends the string representation of the specified double value.
        /// @param value Double value to append.
        /// @return Reference to this instance.
        StringBuilder& Append(double value);

        /// Appends the string representation of the specified boolean value.
        /// The appended text is "True" or "False" to match .NET behavior.
        /// @param value Boolean value to append.
        /// @return Reference to this instance.
        StringBuilder& Append(bool value);

        /// Appends a line terminator (newline) to this instance.
        /// @return Reference to this instance.
        StringBuilder& AppendLine();

        /// Appends the specified string followed by a line terminator.
        /// @param value String to append.
        /// @return Reference to this instance.
        StringBuilder& AppendLine(const std::string& value);

        /// Returns the current contents of this instance as a string.
        /// @return The accumulated string.
        [[nodiscard]] std::string ToString() const;

        /// Gets the number of characters in this instance (.NET Length property).
        /// @return Number of characters in the internal buffer.
        [[nodiscard]] intcs getLengthProperty() const;

        /// Returns true if the internal buffer is empty.
        [[nodiscard]] bool Empty() const;

        /// Appends the string representation of the specified 64-bit integer value.
        /// @param value Long integer value to append.
        /// @return Reference to this instance.
        StringBuilder& Append(SharpRuntime::longcs value);

        /// Inserts the specified string at the given character position.
        /// @param index Zero-based position at which to insert.
        /// @param value String to insert.
        /// @return Reference to this instance.
        StringBuilder& Insert(intcs index, const std::string& value);

        /// Removes a range of characters from this instance.
        /// @param startIndex Zero-based position of the first character to remove.
        /// @param count Number of characters to remove.
        /// @return Reference to this instance.
        StringBuilder& Remove(intcs startIndex, intcs count);

        /// Replaces all occurrences of @p oldValue with @p newValue.
        /// @param oldValue The string to replace.
        /// @param newValue The replacement string.
        /// @return Reference to this instance.
        StringBuilder& Replace(const std::string& oldValue, const std::string& newValue);

        /// @brief Appends a formatted string (delegates to String::Format overloads).
        StringBuilder& AppendFormat(const std::string& format, SharpRuntime::intcs arg0)
            { return Append(System::String::Format(format, arg0)); }
        /// @brief Appends a formatted string with a double argument.
        StringBuilder& AppendFormat(const std::string& format, double arg0)
            { return Append(System::String::Format(format, arg0)); }
        /// @brief Appends a formatted string with a string argument.
        StringBuilder& AppendFormat(const std::string& format, const std::string& arg0)
            { return Append(System::String::Format(format, arg0)); }
        /// @brief Appends a formatted string with two integer arguments.
        StringBuilder& AppendFormat(const std::string& format, SharpRuntime::intcs arg0, SharpRuntime::intcs arg1)
            { return Append(System::String::Format(format, arg0, arg1)); }
        /// @brief Appends a formatted string with an integer and a string argument.
        StringBuilder& AppendFormat(const std::string& format, SharpRuntime::intcs arg0, const std::string& arg1)
            { return Append(System::String::Format(format, arg0, arg1)); }
        /// @brief Appends a formatted string with a string and an integer argument.
        StringBuilder& AppendFormat(const std::string& format, const std::string& arg0, SharpRuntime::intcs arg1)
            { return Append(System::String::Format(format, arg0, arg1)); }
        /// @brief Appends a formatted string with two string arguments.
        StringBuilder& AppendFormat(const std::string& format, const std::string& arg0, const std::string& arg1)
            { return Append(System::String::Format(format, arg0, arg1)); }
        /// @brief Appends a formatted string with two double arguments.
        StringBuilder& AppendFormat(const std::string& format, double arg0, double arg1)
            { return Append(System::String::Format(format, arg0, arg1)); }

        /// @brief Appends elements of @p values joined by @p separator.
        StringBuilder& AppendJoin(const std::string& separator, const std::vector<std::string>& values)
            { return Append(System::String::Join(separator, values)); }
    };
}
