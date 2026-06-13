// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/StringSplitOptions.hpp"

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
        /// @brief Formats a string replacing `{0}` with @p arg0 as "True" or "False".
        static std::string Format(const std::string& format, bool arg0);
        /// @brief Formats a string replacing `{0}` with @p arg0 as a single character.
        static std::string Format(const std::string& format, char arg0);

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

        /// @brief Returns the zero-based index of the first occurrence of any character in @p anyOf, or -1.
        static SharpRuntime::intcs IndexOfAny(const std::string& value, const std::vector<char>& anyOf);

        /// @brief Returns the zero-based index of the last occurrence of any character in @p anyOf, or -1.
        static SharpRuntime::intcs LastIndexOfAny(const std::string& value, const std::vector<char>& anyOf);

        /// @brief Returns true if @p value contains the character @p ch.
        static bool Contains(const std::string& value, char ch);

        /// @brief Returns the zero-based index of the first occurrence of @p substr starting at @p startIndex, or -1.
        static SharpRuntime::intcs IndexOf(const std::string& value, const std::string& substr, SharpRuntime::intcs startIndex);

        /// @brief Returns the zero-based index of the first occurrence of @p ch starting at @p startIndex, or -1.
        static SharpRuntime::intcs IndexOf(const std::string& value, char ch, SharpRuntime::intcs startIndex);

        /// @brief Returns the zero-based index of the last occurrence of @p substr searching backward from @p startIndex, or -1.
        static SharpRuntime::intcs LastIndexOf(const std::string& value, const std::string& substr, SharpRuntime::intcs startIndex);

        /// @brief Returns the zero-based index of the last occurrence of @p ch searching backward from @p startIndex, or -1.
        static SharpRuntime::intcs LastIndexOf(const std::string& value, char ch, SharpRuntime::intcs startIndex);

        /// @brief Returns a new string of @p count copies of character @p ch.
        static std::string Create(SharpRuntime::intcs count, char ch);

        /// @brief Returns true if @p value starts with the character @p ch.
        static bool StartsWith(const std::string& value, char ch);

        /// @brief Returns true if @p value ends with the character @p ch.
        static bool EndsWith(const std::string& value, char ch);

        /// @brief Returns a new string with all characters from @p startIndex to the end removed.
        static std::string Remove(const std::string& value, SharpRuntime::intcs startIndex);

        /// @brief Returns a new string with @p count characters removed starting at @p startIndex.
        static std::string Remove(const std::string& value, SharpRuntime::intcs startIndex, SharpRuntime::intcs count);

        /// @brief Inserts @p insertValue into @p value at @p startIndex.
        static std::string Insert(const std::string& value, SharpRuntime::intcs startIndex, const std::string& insertValue);

        /// @brief Joins all strings in @p values (initializer_list form) with @p separator between them.
        static std::string Join(const std::string& separator, std::initializer_list<std::string> values)
        {
            return Join(separator, std::vector<std::string>(values));
        }

        /// @brief Joins all integers in @p values with @p separator between them.
        static std::string Join(const std::string& separator, const std::vector<SharpRuntime::intcs>& values);

        /// @brief Joins all doubles in @p values with @p separator between them.
        static std::string Join(const std::string& separator, const std::vector<double>& values);

        /// @brief Returns the characters of @p value as a vector of chars.
        static std::vector<char> ToCharArray(const std::string& value);

        /// @brief Returns a copy of @p value with all leading and trailing characters in @p trimChars removed.
        static std::string Trim(const std::string& value, const std::vector<char>& trimChars);

        /// @brief Returns a copy of @p value with all leading characters in @p trimChars removed.
        static std::string TrimStart(const std::string& value, const std::vector<char>& trimChars);

        /// @brief Returns a copy of @p value with all trailing characters in @p trimChars removed.
        static std::string TrimEnd(const std::string& value, const std::vector<char>& trimChars);

        /// @brief Splits @p value on @p delimiter with the specified @p options.
        static std::vector<std::string> Split(const std::string& value, char delimiter, StringSplitOptions options);

        /// @brief Splits @p value on @p delimiter string with the specified @p options.
        static std::vector<std::string> Split(const std::string& value, const std::string& delimiter, StringSplitOptions options);

        /// @brief Compares @p a and @p b ordinally; returns negative/zero/positive.
        static SharpRuntime::intcs Compare(const std::string& a, const std::string& b);

        /// @brief Compares @p a and @p b with optional case-insensitivity; returns negative/zero/positive.
        static SharpRuntime::intcs Compare(const std::string& a, const std::string& b, bool ignoreCase);

        /// @brief Returns true if @p a and @p b are equal (case-sensitive).
        static bool Equals(const std::string& a, const std::string& b);

        /// @brief Returns true if @p a and @p b are equal with optional case-insensitivity.
        static bool Equals(const std::string& a, const std::string& b, bool ignoreCase);

        /// @brief Formats a string with three integer arguments.
        static std::string Format(const std::string& format, SharpRuntime::intcs arg0, SharpRuntime::intcs arg1, SharpRuntime::intcs arg2);

        /// @brief Formats a string with three string arguments.
        static std::string Format(const std::string& format, const std::string& arg0, const std::string& arg1, const std::string& arg2);

        /// @brief Formats a string replacing `{0}` with @p arg0 (long integer).
        static std::string Format(const std::string& format, SharpRuntime::longcs arg0);
        /// @brief Formats a string with a double arg0 and string arg1.
        static std::string Format(const std::string& format, double arg0, const std::string& arg1);
        /// @brief Formats a string with a string arg0 and double arg1.
        static std::string Format(const std::string& format, const std::string& arg0, double arg1);
        /// @brief Formats a string with a longcs arg0 and intcs arg1.
        static std::string Format(const std::string& format, SharpRuntime::longcs arg0, SharpRuntime::intcs arg1);
        /// @brief Formats a string with an intcs arg0 and longcs arg1.
        static std::string Format(const std::string& format, SharpRuntime::intcs arg0, SharpRuntime::longcs arg1);

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
