#pragma once

#include <string>
#include <vector>

#include "CppDotNet/CppDotNetHelper.hpp"

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
        String() = delete;
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

        static std::string Format(const std::string& format, CppDotNet::intcs arg0);
        static std::string Format(const std::string& format, const std::string& arg0);

        static std::string ToString(CppDotNet::intcs value, int width, char fill = '0');
    };
}
