#pragma once

#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Text
{
    using SharpRuntime::intcs;

    /**
     * @brief Provides a mutable string buffer for efficient string construction.
     *
     * This class is a lightweight C++ emulation of the .NET
     * System.Text.StringBuilder class. It is intended primarily
     * for source-porting convenience and practical use in the
     * SharpRuntime layer.
     *
     * Only a small useful subset of the original .NET API is provided.
     *
     * @note Status: PARTIAL
     */
    class StringBuilder
    {
    private:
        /**
         * @brief Internal text buffer.
         *
         * @note Status: IMPLEMENTED
         */
        std::string buffer;

    public:
        /**
         * @brief Initializes a new empty instance of the StringBuilder class.
         *
         * @note Status: IMPLEMENTED
         */
        StringBuilder();

        /**
         * @brief Initializes a new instance of the StringBuilder class
         * with the specified initial text.
         *
         * @param value Initial text.
         *
         * @note Status: IMPLEMENTED
         */
        explicit StringBuilder(const std::string& value);

        /**
         * @brief Removes all characters from the current instance.
         *
         * @note Status: IMPLEMENTED
         */
        void Clear();

        /**
         * @brief Appends the specified string to this instance.
         *
         * @param value String to append.
         * @return Reference to this instance.
         *
         * @note Status: IMPLEMENTED
         */
        StringBuilder& Append(const std::string& value);

        /**
         * @brief Appends the specified null-terminated C string to this instance.
         *
         * If @p value is nullptr, nothing is appended.
         *
         * @param value C string to append.
         * @return Reference to this instance.
         *
         * @note Status: IMPLEMENTED
         */
        StringBuilder& Append(const char* value);

        /**
         * @brief Appends the specified character to this instance.
         *
         * @param value Character to append.
         * @return Reference to this instance.
         *
         * @note Status: IMPLEMENTED
         */
        StringBuilder& Append(char value);

        /**
         * @brief Appends the string representation of the specified integer value.
         *
         * @param value Integer value to append.
         * @return Reference to this instance.
         *
         * @note Status: IMPLEMENTED
         */
        StringBuilder& Append(intcs value);

        /**
         * @brief Appends the string representation of the specified double value.
         *
         * @param value Double value to append.
         * @return Reference to this instance.
         *
         * @note Status: IMPLEMENTED
         */
        StringBuilder& Append(double value);

        /**
         * @brief Appends the string representation of the specified boolean value.
         *
         * The appended text is "True" or "False" to better match .NET behavior.
         *
         * @param value Boolean value to append.
         * @return Reference to this instance.
         *
         * @note Status: IMPLEMENTED
         */
        StringBuilder& Append(bool value);

        /**
         * @brief Appends a line terminator to this instance.
         *
         * This method appends '\n'.
         *
         * @return Reference to this instance.
         *
         * @note Status: IMPLEMENTED
         */
        StringBuilder& AppendLine();

        /**
         * @brief Appends the specified string followed by a line terminator.
         *
         * This method appends the string and then '\n'.
         *
         * @param value String to append.
         * @return Reference to this instance.
         *
         * @note Status: IMPLEMENTED
         */
        StringBuilder& AppendLine(const std::string& value);

        /**
         * @brief Returns the current contents of this instance as a string.
         *
         * @return The accumulated string.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] std::string ToString() const;

        /**
         * @brief Gets the number of characters contained in this instance.
         *
         * This method follows the project's property-porting naming convention
         * and corresponds conceptually to the .NET Length property.
         *
         * @return Number of characters in the internal buffer.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] intcs getLengthProperty() const;

        /**
         * @brief Returns true if the internal buffer is empty.
         *
         * @return True if empty, otherwise false.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] bool Empty() const;
    };
}