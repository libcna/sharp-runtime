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
     * @note Status: PARTIAL
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
        static std::vector<std::string> Split(const std::string& value, char delimiter);

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

        /// @brief Formats a string with a single integer argument, replacing the first "{0}" placeholder.
        /// @param format Format string containing "{0}".
        /// @param arg0 Integer value to substitute.
        static std::string Format(const std::string& format, SharpRuntime::intcs arg0);
        /// @brief Formats a string with a single string argument, replacing the first "{0}" placeholder.
        /// @param format Format string containing "{0}".
        /// @param arg0 String value to substitute.
        static std::string Format(const std::string& format, const std::string& arg0);

        /// @brief Converts an integer to a zero-padded string of at least @p width characters.
        /// @param value Integer value to convert.
        /// @param width Minimum field width.
        /// @param fill  Padding character (default '0').
        static std::string ToString(SharpRuntime::intcs value, int width, char fill = '0');
    };
}
