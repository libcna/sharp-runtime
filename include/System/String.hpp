// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System
{
    /**
     * @brief Provides utility methods similar to selected members of .NET String.
     *
     * This class is not a wrapper around std::string. Instead, it offers
     * static helper methods useful for source-porting C# code to C++.
     *
     * @note Status: DONE
     */
    class String
    {
    public:
        /// Deleted constructor — all members are static.
        String() = delete;
        /// Deleted destructor — class is not instantiable.
        ~String() = delete;

        /**
         * @brief Splits the specified string by a single character delimiter.
         *
         * @param value Input string.
         * @param delimiter Delimiter character.
         * @return Vector of split parts.
         *
         * @note Status: IMPLEMENTED
         */
        /// @brief Splits @p value on @p delimiter (single character).
        static std::vector<std::string> Split(const std::string& value, char delimiter);

        /// @brief Splits @p value on any character in @p delimiters.
        static std::vector<std::string> Split(const std::string& value, const std::vector<char>& delimiters);

        /// @brief Splits @p value on a string @p delimiter.
        static std::vector<std::string> Split(const std::string& value, const std::string& delimiter);

        /**
         * @brief Returns true if the specified string is empty.
         *
         * @param value Input string.
         * @return True if the string is empty, otherwise false.
         *
         * @note Status: IMPLEMENTED
         */
        static bool IsEmpty(const std::string& value);

        /**
         * @brief Returns true if the specified string starts with the given prefix.
         *
         * @param value Input string.
         * @param prefix Prefix to test.
         * @return True if @p value starts with @p prefix.
         *
         * @note Status: IMPLEMENTED
         */
        static bool StartsWith(const std::string& value, const std::string& prefix);

        /**
         * @brief Returns true if the specified string is empty.
         *
         * This method exists mainly for naming similarity with .NET.
         *
         * @param value Input string.
         * @return True if empty, otherwise false.
         *
         * @note Status: IMPLEMENTED
         */
        static bool IsNullOrEmpty(const std::string& value);

        /// @brief Formats a string replacing `{0}` with @p arg0 (supports `{0:X}`, `{0:D3}` specifiers).
        static std::string Format(const std::string& format, SharpRuntime::intcs arg0);
        /// @brief Formats a string replacing `{0}` with @p arg0 (supports `{0:F2}`, `{0:G}` specifiers).
        static std::string Format(const std::string& format, double arg0);
        /// @brief Formats a string replacing `{0}` with @p arg0 (plain string substitution).
        static std::string Format(const std::string& format, const std::string& arg0);
        /// @brief Formats a string with two integer arguments (`{0}` and `{1}`).
        static std::string Format(const std::string& format, SharpRuntime::intcs arg0, SharpRuntime::intcs arg1);
        /// @brief Formats a string with an integer arg0 and string arg1.
        static std::string Format(const std::string& format, SharpRuntime::intcs arg0, const std::string& arg1);
        /// @brief Formats a string with a string arg0 and integer arg1.
        static std::string Format(const std::string& format, const std::string& arg0, SharpRuntime::intcs arg1);
        /// @brief Formats a string with two string arguments.
        static std::string Format(const std::string& format, const std::string& arg0, const std::string& arg1);
        /// @brief Formats a string with two double arguments (supports `{0:F2}` specifiers).
        static std::string Format(const std::string& format, double arg0, double arg1);

        /// @brief Converts an integer to a zero-padded string of at least @p width characters.
        /// @param value Integer value to convert.
        /// @param width Minimum field width.
        /// @param fill  Padding character (default '0').
        static std::string ToString(SharpRuntime::intcs value, int width, char fill = '0');

        /// @brief Returns true if @p value consists only of whitespace characters (or is empty).
        static bool IsNullOrWhiteSpace(const std::string& value);

        /// @brief Returns true if @p value ends with @p suffix.
        static bool EndsWith(const std::string& value, const std::string& suffix);

        /// @brief Returns true if @p value contains @p substr.
        static bool Contains(const std::string& value, const std::string& substr);

        /// @brief Returns a copy of @p value with every occurrence of @p oldValue replaced by @p newValue.
        static std::string Replace(const std::string& value, const std::string& oldValue, const std::string& newValue);

        /// @brief Returns a copy of @p value with every occurrence of @p oldChar replaced by @p newChar.
        static std::string Replace(const std::string& value, char oldChar, char newChar);

        /// @brief Returns the substring of @p value starting at @p startIndex to the end.
        static std::string Substring(const std::string& value, SharpRuntime::intcs startIndex);

        /// @brief Returns the substring of @p value starting at @p startIndex with the given @p length.
        static std::string Substring(const std::string& value, SharpRuntime::intcs startIndex, SharpRuntime::intcs length);

        /// @brief Returns a copy of @p value with leading and trailing whitespace removed.
        static std::string Trim(const std::string& value);

        /// @brief Returns a copy of @p value with leading whitespace removed.
        static std::string TrimStart(const std::string& value);

        /// @brief Returns a copy of @p value with trailing whitespace removed.
        static std::string TrimEnd(const std::string& value);

        /// @brief Concatenates two strings.
        static std::string Concat(const std::string& a, const std::string& b);

        /// @brief Concatenates three strings.
        static std::string Concat(const std::string& a, const std::string& b, const std::string& c);

        /// @brief Concatenates four strings.
        static std::string Concat(const std::string& a, const std::string& b, const std::string& c, const std::string& d);

        /// @brief Concatenates all strings in @p values with no separator.
        static std::string Concat(const std::vector<std::string>& values);

        /// @brief Joins all elements of @p values with @p separator between them.
        static std::string Join(const std::string& separator, const std::vector<std::string>& values);

        /// @brief Returns a copy of @p value with all characters converted to uppercase.
        static std::string ToUpper(const std::string& value);

        /// @brief Returns a copy of @p value with all characters converted to lowercase.
        static std::string ToLower(const std::string& value);

        /// @brief Returns the zero-based index of the first occurrence of @p substr in @p value, or -1 if not found.
        static SharpRuntime::intcs IndexOf(const std::string& value, const std::string& substr);

        /// @brief Returns the zero-based index of the first occurrence of @p ch in @p value, or -1 if not found.
        static SharpRuntime::intcs IndexOf(const std::string& value, char ch);

        /// @brief Returns the zero-based index of the last occurrence of @p substr in @p value, or -1 if not found.
        static SharpRuntime::intcs LastIndexOf(const std::string& value, const std::string& substr);

        /// @brief Returns the zero-based index of the last occurrence of @p ch in @p value, or -1 if not found.
        static SharpRuntime::intcs LastIndexOf(const std::string& value, char ch);

        /// @brief Returns @p value right-aligned in a field of @p totalWidth, padded with spaces on the left.
        static std::string PadLeft(const std::string& value, SharpRuntime::intcs totalWidth);

        /// @brief Returns @p value right-aligned in a field of @p totalWidth, padded with @p paddingChar on the left.
        static std::string PadLeft(const std::string& value, SharpRuntime::intcs totalWidth, char paddingChar);

        /// @brief Returns @p value left-aligned in a field of @p totalWidth, padded with spaces on the right.
        static std::string PadRight(const std::string& value, SharpRuntime::intcs totalWidth);

        /// @brief Returns @p value left-aligned in a field of @p totalWidth, padded with @p paddingChar on the right.
        static std::string PadRight(const std::string& value, SharpRuntime::intcs totalWidth, char paddingChar);
    };
}
